---
phase: 5
title: main.cpp wiring + lifecycle events
status: pending
priority: P1
effort: 0.5d
---

# Phase 5 — main.cpp wiring

## Context
Wire tất cả component mới: lifecycle manager spawn/teardown context, monitor + pause gate, arbiter + scheduler per window. Khác plan cũ chỗ KHÔNG init N static — pipeline init/teardown trong callback.

## Files to modify
- `src/main.cpp`

## Outline

```cpp
// Shared singletons.
SendInputBackend backend;
ForegroundArbiter arbiter(std::chrono::milliseconds(70));
Humanizer human;
ProfileManager pm(exeDir() / "profiles");
pm.ensureDefaultProfile(cfg);
auto assignment = pm.loadAssignment();

UserActivityMonitor::Config umCfg;
umCfg.idleThreshold       = std::chrono::milliseconds(cfg.pause.idleMs);
umCfg.mouseIdleThreshold  = std::chrono::milliseconds(cfg.pause.mouseIdleMs);
UserActivityMonitor monitor(umCfg, [](bool active){
    LOG_INFO("UserActivity: %s", active ? "ACTIVE" : "IDLE");
});
monitor.start();
PauseGate pause(monitor);
backend.setSelfInputCallback([&]{ monitor.notifySelfInput(); });
arbiter.setPauseChecker([&](HWND h){ return pause.isPaused(h); });

// MainWindow created early; receives events.
MainWindow win(/* ... */);
win.setPauseGate(&pause);  // for manual toggle UI

// Lifecycle init/teardown closures.
auto initCtx = [&](PerWindowContext& ctx) -> bool {
    // Resolve profile: try by lastKnownRect + title match; fallback default.
    std::string key = pm.matchKey(ctx.title, ctx.lastKnownRect);  // see Phase 6
    auto it = assignment.find(key);
    ctx.profileName = (it != assignment.end()) ? it->second : "Default";
    if (!pm.load(ctx.profileName, ctx.cfg)) { ctx.cfg = cfg; ctx.profileName = "Default"; }
    ctx.bus.publish(std::make_shared<const AppConfig>(ctx.cfg));

    ctx.gate    = std::make_unique<OutputGate>();
    ctx.health  = std::make_unique<CaptureHealthFsm>();
    ctx.gate->setTarget(ctx.hwnd);
    ctx.gate->setHealth(ctx.health.get());
    ctx.gate->setRequireForeground(backend.requiresForeground());

    ctx.sched = std::make_unique<InputScheduler>(
        backend, human, *ctx.gate, arbiter, pause, ctx.hwnd);
    ctx.sched->start();

    ctx.combat = std::make_unique<CombatFsm>(*ctx.sched, ctx.hwnd, ctx.cfg.combat);
    ctx.combat->enable(ctx.cfg.combat.enabled);
    ctx.refill = std::make_unique<PotRefillScheduler>(
        *ctx.sched, *ctx.gate, ctx.hwnd, ctx.cfg.refill);
    ctx.refill->enable(ctx.cfg.refill.enabled);

    ctx.dispatcher = std::make_unique<ActionDispatcher>(*ctx.sched, *ctx.combat, ctx.cfg);
    ctx.dispatcher->setRefillScheduler(ctx.refill.get());

    ctx.capture = std::make_unique<WgcCapture>();
    if (!ctx.capture->start(ctx.hwnd)) {
        LOG_ERROR("Capture start failed for HWND %p", ctx.hwnd);
        return false;
    }
    ctx.vision = std::make_unique<VisionPipeline>(*ctx.capture, hp, mp, sp);
    int idx = ctx.index;
    CaptureHealthFsm* hf = ctx.health.get();
    ActionDispatcher* dp = ctx.dispatcher.get();
    ctx.vision->setCallback([hf, dp, idx](const VisionState& s){
        hf->notifyFrameArrived(s.seq);
        dp->onVisionTick(s);
    });
    ctx.vision->start();
    LOG_INFO("Window W%d initialized: %ls", idx, ctx.title.c_str());
    return true;
};

auto teardownCtx = [&](PerWindowContext& ctx) {
    LOG_INFO("Tearing down W%d", ctx.index);
    arbiter.cancelOwner(ctx.hwnd);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));  // let in-flight slot drain
    if (ctx.vision)  ctx.vision->stop();
    if (ctx.capture) ctx.capture->stop();
    if (ctx.sched)   ctx.sched->stop();
    // unique_ptrs free in dtor.
};

auto onAdded = [&](PerWindowContext* ctx) {
    win.onWindowAdded(ctx);   // Phase 7 UI hook
};
auto onRemoved = [&](HWND hwnd, int index) {
    win.onWindowRemoved(hwnd, index);
};

WindowLifecycleManager lifecycle(initCtx, teardownCtx, onAdded, onRemoved, /*max*/3, 2500ms);
lifecycle.start();

// Health tick (chỉ tick contexts còn sống — query qua lifecycle).
std::thread tickTh([&]{
    while (ticking.load()) {
        // Snapshot HWND list → iterate via UI's bound contexts.
        // Hoặc lifecycle expose `forEachContext(fn)` thread-safe.
        lifecycle.forEachContext([](PerWindowContext& c){ c.health->tick(); });
        std::this_thread::sleep_for(100ms);
    }
});

// ... event loop ...

lifecycle.stop();
arbiter.stop();
monitor.stop();
```

## Lifecycle manager API addition
```cpp
void WindowLifecycleManager::forEachContext(std::function<void(PerWindowContext&)> fn);
```

## Hotkey
- F8 toggle AUTO **cho tất cả window**. Phase 7 thêm per-window UI button.
- Add F9: toggle manual pause (cho user override).

## Single-instance
- Mutex global giữ — chỉ 1 process. 2nd launch → bring forward (giữ).

## Todo
- [ ] Refactor main.cpp.
- [ ] Add `lifecycle.forEachContext`.
- [ ] Wire monitor/pause/arbiter/backend.
- [ ] F9 manual pause hotkey.
- [ ] Compile + smoke test 0/1/2/3 windows.

## Success criteria
- Start app 0 PT mở → app chạy, UI shows "Waiting for PT window...".
- Mở 1 PT → tab xuất hiện ≤ 3s, log "W0 initialized".
- Mở thêm PT → W1, W2 xuất hiện.
- Đóng PT bất kỳ → tab biến mất ≤ 5s, không crash.
- F9 → toàn bộ auto pause; F9 lại → resume.

## Risks
- Race teardown vs vision callback: callback có thể fire sau khi vision->stop(). Cần `vision->stop()` block đến khi callback drain — verify WgcCapture impl.
- Cancel arbiter rồi vẫn cần delay trước free: 80ms chọn dựa slot 70ms + 10ms buffer; nếu thấy crash → tăng 150ms.
