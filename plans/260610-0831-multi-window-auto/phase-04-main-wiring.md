---
phase: 4
title: main.cpp wiring N=2 + lifecycle
status: pending
priority: P1
effort: 0.5d
---

# Phase 4 — main.cpp wiring N windows

## Context
- Wire N=2 PerWindowContext lên ForegroundArbiter + shared backend.
- **Per-window cfg + ConfigBus** (Phase 4b ProfileManager load profile theo `lastAssignment.json`).
- Lifecycle: start order = sched → pipeline → health tick; stop ngược lại.

## Files to modify
- `src/main.cpp`.

## Implementation outline

```cpp
// After config load, after backend created.
SendInputBackend backend;                       // shared, 1 instance
ForegroundArbiter arbiter(std::chrono::milliseconds(120));
Humanizer human;                                // shared OK (stateless jitter)

ProfileManager pm(exeDir() / "profiles");        // Phase 4b
pm.ensureDefaultProfile(cfg);
auto assignment = pm.loadAssignment();

std::vector<HWND> targets;
FindAllTargets(targets);
if (targets.empty()) { /* error MsgBox; return 1 */ }
if (targets.size() > 2) {
    LOG_WARN("Found %zu PT windows; using first 2 only", targets.size());
    targets.resize(2);
}

// Amendment 09:08 — multi-window cursor strategy.
bool multiWindow = (targets.size() >= 2);
backend.setMousePathEnabled(multiWindow ? false : cfg.combat.enableMousePath);
if (multiWindow) LOG_WARN("Multi-window: Bezier mouse path disabled, teleport+park mode");
arbiter.setCursorPark(cfg.cursorPark.x, cfg.cursorPark.y,
                       multiWindow && cfg.cursorPark.enabled);

std::vector<std::unique_ptr<PerWindowContext>> ctxs;
for (size_t i = 0; i < targets.size(); ++i) {
    auto ctx = std::make_unique<PerWindowContext>();
    ctx->hwnd = targets[i];
    ctx->index = (int)i;
    wchar_t t[256]={}; GetWindowTextW(ctx->hwnd, t, 256); ctx->title = t;

    // Phase 4b: resolve profile per window.
    std::string key = "W" + std::to_string(i);
    auto it = assignment.find(key);
    ctx->profileName = (it != assignment.end()) ? it->second : "Default";
    if (!pm.load(ctx->profileName, ctx->cfg)) { ctx->cfg = cfg; ctx->profileName = "Default"; }
    ctx->bus.publish(std::make_shared<const AppConfig>(ctx->cfg));

    ctx->gate    = std::make_unique<OutputGate>();
    ctx->health  = std::make_unique<CaptureHealthFsm>();
    ctx->gate->setTarget(ctx->hwnd);
    ctx->gate->setHealth(ctx->health.get());
    ctx->gate->setRequireForeground(backend.requiresForeground());

    ctx->sched = std::make_unique<InputScheduler>(
        backend, human, *ctx->gate, arbiter, ctx->hwnd);
    ctx->sched->start();

    ctx->combat = std::make_unique<CombatFsm>(*ctx->sched, ctx->hwnd, ctx->cfg.combat);
    ctx->combat->enable(ctx->cfg.combat.enabled);
    ctx->refill = std::make_unique<PotRefillScheduler>(
        *ctx->sched, *ctx->gate, ctx->hwnd, ctx->cfg.refill);
    ctx->refill->enable(ctx->cfg.refill.enabled);

    ctx->dispatcher = std::make_unique<ActionDispatcher>(*ctx->sched, *ctx->combat, ctx->cfg);
    ctx->dispatcher->setRefillScheduler(ctx->refill.get());
    int idx = ctx->index;
    ctx->dispatcher->setLogger([idx](const char* tag, int prio, WORD vk) {
        Logger::instance().logf(LogLevel::Debug,
            "[w%d/dispatch] %s prio=P%d vk=0x%02X", idx, tag, prio, vk);
    });

    ctx->capture = std::make_unique<WgcCapture>();
    if (!ctx->capture->start(ctx->hwnd)) { /* fail; cleanup; return 2 */ }

    // BarConfig dùng chung (assume same resolution).
    ctx->vision = std::make_unique<VisionPipeline>(*ctx->capture, hp, mp, sp);
    CaptureHealthFsm* hf = ctx->health.get();
    ActionDispatcher* dp = ctx->dispatcher.get();
    int wIdx = idx;
    ctx->vision->setCallback([hf, dp, wIdx](const VisionState& s) {
        hf->notifyFrameArrived(s.seq);
        dp->onVisionTick(s);
        // throttled log per window — giữ tag [w0]/[w1].
    });
    ctx->vision->start();
    ctxs.push_back(std::move(ctx));
}

// Health tick thread loop tất cả ctx->health.
std::thread tickTh([&]{
    while (ticking.load()) {
        for (auto& c : ctxs) c->health->tick();
        std::this_thread::sleep_for(100ms);
    }
});

// UI nhận ctxs reference (Phase 5).
MainWindow win(bus, loader, configPath);
win.setContexts(ctxs);  // new API in Phase 5
// ... rest unchanged ...

// Shutdown reverse order.
for (auto& c : ctxs) { c->vision->stop(); c->capture->stop(); c->sched->stop(); }
arbiter.stop();
```

## Single-instance behavior
- Mutex global vẫn giữ — chỉ 1 process WindowHelper.
- 2nd launch → `BringOtherInstanceForward` (giữ).

## Hotkey
- F8 toggle AUTO **cho tất cả window** (gộp). Phase 5 thêm option per-window nếu cần.

## Todo
- [ ] Refactor main.cpp theo outline.
- [ ] Cleanup path khi capture fail tại window thứ 2 — vẫn cho phép chạy với window thứ 1 (graceful).
- [ ] Compile + smoke test 1 window (backward compat).

## Success criteria
- Mở 1 PT → chạy như cũ.
- Mở 2 PT → 2 capture chạy, 2 vision tick log [w0]/[w1], FG arbiter slots tăng.
- Shutdown sạch trong 2s.

## Risks
- WGC mở 2 capture session đồng thời → cần verify Windows cho phép. WGC mỗi HWND độc lập → OK theo doc.
- GPU/CPU load 2x → đo. Vision ~5%/window → OK.
