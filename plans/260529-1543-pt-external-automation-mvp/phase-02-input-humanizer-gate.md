# Phase 2 — Input Backend + Humanizer + OutputGate

**Est:** 2 ngày
**Priority:** P0
**Status:** code-ready (assumes PostMessage; revisit if Phase 0 probe says otherwise)
**Depends:** Phase 0 (backend decision), Phase 1 (frame source)

## Implemented Files
- `src/input/i-input-backend.h`
- `src/input/postmessage-backend.{h,cpp}` — primary backend (background-friendly)
- `src/input/send-input-backend.{h,cpp}` — foreground fallback, ready to swap
- `src/input/input-scheduler.{h,cpp}` — priority queue, applies humanizer jitter + gate
- `src/core/humanizer.{h,cpp}` — Gaussian jitter, miss-click, break/session-pause windows
- `src/core/output-gate.{h,cpp}` — single allow/deny decision
- `src/core/capture-health-fsm.{h,cpp}` — HEALTHY / DEGRADED / UNSAFE / STOPPED
- Wired in `src/main.cpp`: demo pot policy (HP/MP/SP < 30% → F1/F2/F3 tap)
- WTS session-lock notification registered (Win+L disables input)
- Mock logs `WM_KEYDOWN` / `WM_RBUTTONDOWN` via `OutputDebugString` for verification

## Validation (manual via DebugView)
1. Run `PtMockGame.exe` then `WindowHelper.exe`
2. Drag HP slider in mock to < 30% → DebugView should show `[sched] -> HP-pot vk=0x70` from WindowHelper and `[mock] WM_KEYDOWN vk=0x70` from PtMockGame
3. Minimize mock → gate=0 in logs, no more `[mock] WM_KEYDOWN` lines
4. Press `Win+L` → unlock → confirm input blocked during lock

## Mục Tiêu
- Implement `IInputBackend` cụ thể dựa trên kết quả Phase 0
- Humanizer module (Gaussian jitter, break, session pause, miss-click)
- OutputGate chặn input khi unsafe (capture/foreground/HWND/lock)

## Files Sẽ Tạo
```
src/input/
├── i-input-backend.h
├── postmessage-backend.h/.cpp        (nếu Phase 0 chọn PostMessage)
├── send-input-backend.h/.cpp         (nếu Phase 0 chọn SendInput)
└── input-scheduler.h/.cpp
src/core/
├── humanizer.h/.cpp
├── output-gate.h/.cpp
└── capture-health-fsm.h/.cpp
```

LOC ước tính: ~700.

## Implementation

### 2.1 IInputBackend Interface
```cpp
class IInputBackend {
public:
    virtual ~IInputBackend() = default;
    virtual void sendKeyTap(WORD vk, int holdMs) = 0;
    virtual void sendKeyDown(WORD vk) = 0;
    virtual void sendKeyUp(WORD vk) = 0;
    virtual void sendRightClick(int x, int y) = 0;
    virtual void sendShiftRightClick(int x, int y) = 0;
    virtual bool requiresForeground() const = 0;
};
```

### 2.2 PostMessageBackend (nếu Phase 0 OK)
```cpp
class PostMessageBackend : public IInputBackend {
    HWND target_;
public:
    void sendKeyTap(WORD vk, int holdMs) override {
        WORD scan = LOWORD(MapVirtualKey(vk, MAPVK_VK_TO_VSC));
        LPARAM down = MAKELPARAM(1, scan);
        LPARAM up   = MAKELPARAM(1, scan | KF_UP);
        PostMessage(target_, WM_KEYDOWN, vk, down);
        Sleep(holdMs);
        PostMessage(target_, WM_KEYUP, vk, up);
    }
    void sendShiftRightClick(int x, int y) override {
        LPARAM lp = MAKELPARAM(x, y);
        PostMessage(target_, WM_KEYDOWN, VK_LSHIFT, 0);
        PostMessage(target_, WM_RBUTTONDOWN, MK_RBUTTON|MK_SHIFT, lp);
        Sleep(20);
        PostMessage(target_, WM_RBUTTONUP, MK_SHIFT, lp);
        PostMessage(target_, WM_KEYUP, VK_LSHIFT, 0);
    }
    bool requiresForeground() const override { return false; }
};
```

### 2.3 SendInputBackend (fallback)
Implement scancode-based SendInput cho keys (`KEYEVENTF_SCANCODE`), mouse absolute coords normalized 0..65535, humanized cursor move (ease-out 8 step).

### 2.4 Humanizer
```cpp
struct HumanizerConfig {
    double jitter_sigma_ms = 45.0;
    int break_every_min = 25, break_every_max = 50;
    int break_dur_min_sec = 3, break_dur_max_sec = 8;
    int session_runtime_min = 40, session_runtime_max = 110;
    int session_pause_min = 6, session_pause_max = 14;
    double missclick_prob = 0.02;
    int missclick_offset_px = 5;
};

class Humanizer {
public:
    std::chrono::milliseconds gaussianJitter(int priority) const;
    bool shouldMissClick() const;
    cv::Point jitterPos(cv::Point p) const;
    bool inBreakWindow() const;
    bool inSessionPause() const;
    void notifyActionFired(int priority);   // tăng counter
private:
    int actionCount_ = 0;
    std::chrono::steady_clock::time_point sessionStart_, nextBreakAt_;
};
```

Rule: humanizer skip P0–P2 (HP/MP/SP/Recall), chỉ áp P3 (Combat) + P4 (Buff).

### 2.5 OutputGate
```cpp
class OutputGate {
public:
    void setTargetHwnd(HWND h) { hwnd_ = h; }
    void setCaptureHealth(CaptureHealth h) { health_ = h; }
    bool allowInput() const noexcept {
        return health_ == CaptureHealth::Healthy
            && IsWindow(hwnd_)
            && !IsIconic(hwnd_)
            && !sessionLocked_.load()
            && (backendRequiresForeground_
                ? GetForegroundWindow() == hwnd_
                : true);
    }
private:
    std::atomic<CaptureHealth> health_;
    HWND hwnd_ = nullptr;
    std::atomic<bool> sessionLocked_{false};
    bool backendRequiresForeground_ = false;
};
```

Đăng ký `WTSRegisterSessionNotification` để set `sessionLocked_` khi Win+L.

### 2.6 CaptureHealthFsm
States: HEALTHY → DEGRADED → UNSAFE → STOPPED (Section 22.4).
Inputs: frame luma, frame hash, AcquireNextFrame error.
Transitions theo timing thresholds config.

### 2.7 InputScheduler
```cpp
class InputScheduler {
public:
    void schedule(InputCmd cmd);  // priority queue
    void run(std::stop_token st);
private:
    std::priority_queue<InputCmd, ..., FireAtCompare> q_;
    IInputBackend& backend_;
    OutputGate& gate_;
    Humanizer& human_;
};
```

Single execution slot (Section 23.3) — cancel pending khi priority cao hơn đến (sẽ implement Phase 3 trong ActionDispatcher; Phase 2 chỉ basic scheduler).

## Test
- Unit-level: humanizer Gaussian distribution mean/sigma check (visual log)
- Manual: chạy InputScheduler với mock target → verify keys arrive via PtMockGame logger
- OutputGate: minimize mock → verify gate đóng → input dropped với log

## Acceptance
- [ ] InputBackend chốt từ Phase 0 implement xong
- [ ] Mock nhận đúng key/mouse từ tool
- [ ] OutputGate đóng đúng khi minimize / lock / not-foreground
- [ ] Humanizer jitter quan sát natural (visual log distribution)
- [ ] No input misfire khi capture unhealthy

## Risks
- PostMessage cooldown timing: nếu DOWN/UP gần nhau quá game không register → tune Sleep(20–40) ms
- `WTS_SESSION_LOCK` cần message loop handle → đảm bảo Main window pump WM_WTSSESSION_CHANGE

## Next
Phase 3 dùng InputScheduler + OutputGate + Humanizer.
