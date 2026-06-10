---
phase: 2
title: ForegroundArbiter component
status: pending
priority: P0
effort: 1d
---

# Phase 2 — ForegroundArbiter

## Context
- Windows: 1 foreground tại 1 thời điểm. Cần component đảm bảo chỉ 1 InputScheduler được phép `SendInput` tại một thời điểm, với target đúng foreground.
- Phải support: fairness (round-robin), preempt (P0 emergency), recovery khi `SetForegroundWindow` fail.

## Files to create
- `src/dispatch/foreground-arbiter.h` (~60 LOC)
- `src/dispatch/foreground-arbiter.cpp` (~150 LOC)

## API design

```cpp
// foreground-arbiter.h
#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <windows.h>

class ForegroundArbiter {
public:
    struct Slot {
        HWND owner;
        std::chrono::steady_clock::time_point startedAt;
    };

    explicit ForegroundArbiter(std::chrono::milliseconds slotMs = std::chrono::milliseconds(120));

    // Blocking acquire: chờ đến lượt + đảm bảo `owner` là foreground.
    // Trả false nếu SetForegroundWindow fail sau retry hoặc shutdown.
    // priority: P0..P4 (giữ convention của priority.h).
    bool acquireSlot(HWND owner, int priority, std::chrono::milliseconds timeout);

    // Release slot hiện tại. Phải gọi sau khi xong chuỗi SendInput.
    void releaseSlot(HWND owner);

    // Preempt: nếu slot đang active có priority < newPriority (số nhỏ hơn = urgent hơn),
    // signal current holder qua flag để release sớm. acquire() đang chờ sẽ wake.
    // Caller dùng khi enqueue cmd P0.
    void notifyHigherPriorityWaiting(int priority);

    void stop();  // cancel all waiters

    // Stats
    uint64_t slotsServed() const { return slots_.load(); }
    uint64_t fgFailures()  const { return fgFails_.load(); }
    bool shouldYield(HWND owner) const;  // current holder check để cắt slot sớm

private:
    bool ensureForeground(HWND target);  // SetForegroundWindow + AttachThreadInput workaround

    std::mutex mu_;
    std::condition_variable cv_;
    Slot current_{nullptr, {}};
    int currentPrio_ = 99;
    int highestWaitingPrio_ = 99;
    std::atomic<bool> yieldRequested_{false};
    std::atomic<bool> running_{true};
    std::chrono::milliseconds slotMs_;
    std::atomic<uint64_t> slots_{0};
    std::atomic<uint64_t> fgFails_{0};
};
```

## Implementation notes

### `ensureForeground` — workaround Windows lock
```cpp
bool ForegroundArbiter::ensureForeground(HWND target) {
    if (GetForegroundWindow() == target) return true;
    DWORD curThread = GetCurrentThreadId();
    DWORD fgThread  = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    bool attached = false;
    if (fgThread && fgThread != curThread) {
        AttachThreadInput(curThread, fgThread, TRUE);
        attached = true;
    }
    BringWindowToTop(target);
    SetForegroundWindow(target);
    if (attached) AttachThreadInput(curThread, fgThread, FALSE);
    // Wait up to 50ms for confirm
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
    while (std::chrono::steady_clock::now() < deadline) {
        if (GetForegroundWindow() == target) return true;
        Sleep(5);
    }
    ++fgFails_;
    return false;
}
```

### Acquire flow
1. Lock + push wait entry với priority.
2. cv.wait_until: thoát khi (current.owner == nullptr) hoặc (running == false) hoặc timeout.
3. Set current = {owner, now}; currentPrio_ = priority; yieldRequested_ = false.
4. Unlock → call `ensureForeground(owner)`.
5. Nếu fail → relock, clear current, signal cv, return false.
6. Return true.

### Release flow
1. Lock.
2. current = {nullptr, ...}; currentPrio_ = 99; yieldRequested_ = false.
3. cv.notify_all (highest-priority waiter wins next).
4. ++slots_.
5. **Cursor park** (Amendment 09:08): nếu `cursorPark.enabled` và N>=2 → `SetCursorPos(park.x, park.y)`. Arbiter cần config snapshot (cursorPark x/y + enabled). API:
   ```cpp
   void setCursorPark(int x, int y, bool enabled);
   ```

### shouldYield
- Holder gọi định kỳ giữa các cmd. Trả true nếu:
  - `yieldRequested_` true (preempt), HOẶC
  - thời gian slot vượt `slotMs_` VÀ có waiter khác.

### notifyHigherPriorityWaiting
- Lock; nếu `priority < currentPrio_` → `yieldRequested_ = true`; cv.notify_all (hiện tại không ai chờ slot rảnh nhưng để evict logic của caller).

## Priority comparison (max-heap-equivalent)
- Lower number = more urgent (giống `priority.h`).
- Khi nhiều waiter cùng wait, đơn giản: tất cả wake → mỗi waiter tự check `currentPrio_` và priority của mình; chỉ highest-priority claim slot (re-lock loop). KISS — không dùng heap thực sự.

## Todo
- [ ] Tạo `foreground-arbiter.h/.cpp`.
- [ ] Unit test thủ công: 2 thread call acquire concurrently → verify round-robin.
- [ ] Test ensureForeground trên PT thật → log fgFailures.
- [ ] Add to CMakeLists.

## Success criteria
- 2 thread acquire alternating → log slots tăng đều.
- P0 acquire preempt: waiter P0 wake trong <50ms khi P4 đang giữ slot (qua shouldYield).
- `fgFailures` ≈ 0 trong 1000 switch trên PT thật.

## Risks
- AttachThreadInput có side-effect (queue input chung giữa 2 thread) — phải FALSE ngay sau call.
- SetForegroundWindow rate-limit (Windows ~10Hz cap với foreground lock timeout). Giữ slotMs ≥ 100ms.
- Anti-cheat detect FG switch flood → slot ≥ 100-150ms, không thấp hơn.

## Open
- Có cần `relinquish()` API cho holder voluntary yield không? Có thể bỏ — `releaseSlot` đã đủ.
