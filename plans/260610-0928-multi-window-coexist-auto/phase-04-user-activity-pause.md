---
phase: 4
title: UserActivityMonitor + PauseGate
status: pending
priority: P0
effort: 0.5d
---

# Phase 4 — UserActivityMonitor + PauseGate

## Context
Component cốt lõi cho "không phiền user". `UserActivityMonitor` poll `GetLastInputInfo` 200ms; emit pause/resume event cho `PauseGate`. PauseGate có API isPaused(HWND) cho InputScheduler + Arbiter consume.

## Files to create
- `src/core/user-activity-monitor.h` (~60 LOC)
- `src/core/user-activity-monitor.cpp` (~100 LOC)
- `src/core/pause-gate.h` (~50 LOC)
- `src/core/pause-gate.cpp` (~80 LOC)

## UserActivityMonitor API

```cpp
class UserActivityMonitor {
public:
    struct Config {
        std::chrono::milliseconds pollInterval{200};
        std::chrono::milliseconds idleThreshold{3000};      // resume after this idle duration
        std::chrono::milliseconds mouseIdleThreshold{5000}; // longer for mouse-required actions
    };

    using OnStateChange = std::function<void(bool active)>;

    UserActivityMonitor(Config cfg, OnStateChange cb);
    void start();
    void stop();

    bool isUserActive() const;                  // < idleThreshold since last input
    bool isUserActiveForMouse() const;          // < mouseIdleThreshold
    DWORD lastInputTickMs() const;

    // Tell monitor when WE injected input (to ignore self-input from idle calc).
    void notifySelfInput();

    void setConfig(Config cfg);

private:
    void pollLoop();
    Config cfg_;
    OnStateChange cb_;
    std::atomic<DWORD> lastSelfInputTick_{0};
    std::atomic<bool> active_{false};
    std::atomic<bool> running_{false};
    std::thread th_;
};
```

### Activity detection
- `GetLastInputInfo(&lii)` returns tick of last keyboard/mouse input (system-wide).
- Tự injected input cũng update lastInputInfo → cần phân biệt: `SendInputBackend` gọi `monitor->notifySelfInput()` ngay sau mỗi `SendInput`. Lưu `lastSelfInputTick_`.
- Considered active if: `(now - GetLastInputInfo) < threshold` AND `|lastInputTick - lastSelfInputTick_| > 100ms` (tức input không phải do mình).
- Edge case: user input ngay sau self input < 100ms → false negative (coi như self). Risk thấp vì user phản ứng > 100ms.

### Callback firing
- State change (active ↔ idle) → fire `cb_` 1 lần. UI nghe để update indicator.
- Trong active state: vẫn fire poll callback? KHÔNG — chỉ fire khi đổi state.

## PauseGate API

```cpp
class PauseGate {
public:
    explicit PauseGate(UserActivityMonitor& mon);

    // Per-owner pause check. Hiện tại: global pause (tất cả windows pause cùng lúc).
    // Tương lai: per-window pause (vd user focus window i → chỉ pause i).
    bool isPaused(HWND owner) const;

    // For mouse-required action (longer threshold).
    bool isPausedForMouse(HWND owner) const;

    // Manual override (UI button "Pause all" / "Resume all").
    void setManualPause(bool paused);
    bool isManuallyPaused() const;

    // Per-window manual toggle (Phase 7 UI).
    void setWindowPaused(HWND hwnd, bool paused);

private:
    UserActivityMonitor& mon_;
    std::atomic<bool> manualPause_{false};
    mutable std::mutex mu_;
    std::set<HWND> perWindowPaused_;
};
```

### Logic
- `isPaused(hwnd)` = `manualPause_` OR `perWindowPaused_.contains(hwnd)` OR `mon_.isUserActive()`.
- `isPausedForMouse(hwnd)` = pause cho mọi reason OR `mon_.isUserActiveForMouse()`.

## Integration

### main.cpp
```cpp
UserActivityMonitor::Config umCfg;
umCfg.idleThreshold = std::chrono::milliseconds(cfg.pause.idleMs);   // configurable
umCfg.mouseIdleThreshold = std::chrono::milliseconds(cfg.pause.mouseIdleMs);
UserActivityMonitor monitor(umCfg, [&](bool active){
    LOG_INFO("User activity: %s", active ? "ACTIVE" : "IDLE");
    // UI: refresh pause indicator. (Phase 7)
});
monitor.start();

PauseGate pause(monitor);

// Backend ↔ monitor wiring
backend.setSelfInputCallback([&]{ monitor.notifySelfInput(); });

// Arbiter ↔ pause check
arbiter.setPauseChecker([&](HWND h){ return pause.isPaused(h); });

// Scheduler ↔ pause
// Constructor: InputScheduler(..., arbiter, pause, hwnd)
```

### SendInputBackend
- Add `setSelfInputCallback(std::function<void()>)` — called after each SendInput.

## AppConfig additions

```cpp
struct PauseConfig {
    int idleMs = 3000;            // user idle threshold for general resume
    int mouseIdleMs = 5000;       // user idle threshold for mouse-required actions
    bool enabled = true;          // co-exist mode toggle (vs farm mode = false)
};

struct AppConfig {
    // ... existing
    PauseConfig pause;
};
```

## Todo
- [ ] `user-activity-monitor.h/.cpp`.
- [ ] `pause-gate.h/.cpp`.
- [ ] Backend `setSelfInputCallback` hook.
- [ ] PauseConfig in AppConfig + JSON load/save.
- [ ] Unit test: simulate self input + user input → state transitions.

## Success criteria
- User di chuột → monitor active trong < 250ms.
- User idle 3s → resume fired.
- Self SendInput không trigger active state (filter chính xác > 99%).
- PauseGate.isPaused() consistent với monitor state + manual override.

## Risks
- GetLastInputInfo include cả input từ SendInput → filter bằng notifySelfInput timing. 100ms window có thể đôi khi miss user input nhanh sau self → chấp nhận edge case.
- Manual pause toggle race với monitor auto state → manual luôn dominant (OR logic).
- Per-window pause future: hiện tại chỉ global; per-window cần track input target window — complex, defer.
