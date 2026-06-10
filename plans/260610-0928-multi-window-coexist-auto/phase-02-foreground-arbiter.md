---
phase: 2
title: ForegroundArbiter (N variable, skip paused, no cursor park)
status: pending
priority: P0
effort: 1d
---

# Phase 2 — ForegroundArbiter

## Context
Reuse design Phase 2 plan cũ (`260610-0831/phase-02-foreground-arbiter.md`) — copy + 3 thay đổi:

1. **Bỏ cursor park** — plan mới không di chuyển cursor sau release (user dùng máy song song).
2. **Slot mặc định 70ms** (thay 120ms) — N=3 cần slot ngắn hơn để mỗi window được service ~210ms cycle.
3. **Skip paused windows** — acquireSlot kiểm `pausedByUser` của owner; nếu paused → reject ngay (return false không block).

## Files to create
- `src/dispatch/foreground-arbiter.h` (~70 LOC)
- `src/dispatch/foreground-arbiter.cpp` (~170 LOC)

## API diff vs plan cũ

```cpp
class ForegroundArbiter {
public:
    explicit ForegroundArbiter(std::chrono::milliseconds slotMs = std::chrono::milliseconds(70));

    bool acquireSlot(HWND owner, int priority, std::chrono::milliseconds timeout);
    void releaseSlot(HWND owner);
    void notifyHigherPriorityWaiting(int priority);
    void stop();

    // NEW — pause integration.
    using PauseChecker = std::function<bool(HWND)>;  // return true if owner is paused
    void setPauseChecker(PauseChecker fn);

    // NEW — cancel pending requests for owner (called on window teardown).
    void cancelOwner(HWND owner);

    bool shouldYield(HWND owner) const;
    uint64_t slotsServed() const;
    uint64_t fgFailures() const;

private:
    bool ensureForeground(HWND target);  // same as plan cũ (AttachThreadInput workaround)
    // NO cursor park.

    PauseChecker pauseChecker_;
    std::set<HWND> cancelled_;
    // ... rest as plan cũ
};
```

## Behavior changes

### acquireSlot
- Trước khi vào wait loop: check `pauseChecker_ && pauseChecker_(owner)` → return false ngay.
- Trong wait loop: nếu HWND bị `cancelOwner` → return false.
- Khi awoken: re-check pause; nếu paused → release (đặt current = null) → return false.

### releaseSlot
- Same as plan cũ NHƯNG **không SetCursorPos**.
- Just clear current + notify.

### cancelOwner
- Lock; insert into cancelled set; cv.notify_all.
- Waiter của HWND này wake → return false.
- Holder của HWND này: yieldRequested = true → drain xong slot rồi release tự nhiên.
- Sau khi waiter clear: caller (lifecycle manager) wait ngắn (50ms) trước khi free context để chắc no pending.

## Implementation notes
- `ensureForeground` giống plan cũ — `SetForegroundWindow` + `AttachThreadInput` workaround + 50ms confirm window.
- Round-robin với N=3: tất cả waiter wake khi release; mỗi waiter check `currentPrio_ == 99 && !cancelled && !paused`; first-to-grab wins. Có thể bias bằng tracking last-served HWND để tránh starvation.
- Last-served bias (anti-starvation):
  ```cpp
  HWND lastServed_;
  // Trong acquire wake: nếu owner == lastServed_ và có waiter khác chờ → yield 5ms.
  ```

## Slot tuning
- N=2 → slot 70ms = 140ms cycle per window. Acceptable.
- N=3 → slot 70ms = 210ms cycle. PT combat density ~ 1-2 actions/sec → OK.
- N=1 → slot vẫn 70ms nhưng round-robin trivial; ~14 slots/s = nhiều hơn cần thiết. Không sao.
- Nếu fgFailures > 5%/min → tăng slot lên 80-100ms (POC measure Phase 9).

## Todo
- [ ] `foreground-arbiter.h/.cpp` (port từ plan cũ, bỏ cursor park).
- [ ] Add `setPauseChecker` + `cancelOwner`.
- [ ] Unit test: 3 thread acquire alternating; 1 thread paused → 2 còn lại chia slot.
- [ ] Cancel test: thread đang chờ slot → cancelOwner → return false trong <100ms.
- [ ] CMakeLists.

## Success criteria
- 3 thread acquire alternating → slot count chia đều ±10% sau 300 slots.
- P0 preempt latency < 100ms.
- Paused window → 0 slot served cho đến khi resume.
- Cancel → no slot served after cancel, waiter return false trong <100ms.
- fgFailures < 1% trong 1000 switch trên PT thật.

## Risks
- AttachThreadInput side-effect: queue input chung 2 thread — phải FALSE ngay sau call.
- SetForegroundWindow Windows rate-limit ~10Hz với foreground lock — slot ≥ 70ms ok.
- N=3 + slot 70ms quá nhanh → AC nghi ngờ? PT AC nhẹ; nếu thấy issue → tăng 100ms.
