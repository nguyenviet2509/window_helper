---
phase: 3
title: InputScheduler ownerHwnd + PauseGate integration
status: pending
priority: P0
effort: 0.5d
---

# Phase 3 — InputScheduler owner + pause

## Context
Reuse Phase 3 plan cũ (`260610-0831/phase-03-input-scheduler-owner.md`) + thêm pause check trước drain.

## Files to modify
- `src/input/input-scheduler.h`
- `src/input/input-scheduler.cpp`

## Design diff vs plan cũ

### Header
```cpp
class ForegroundArbiter;
class PauseGate;  // NEW fwd

class InputScheduler {
public:
    InputScheduler(IInputBackend& backend, Humanizer& human, OutputGate& gate,
                   ForegroundArbiter& arbiter, PauseGate& pause, HWND owner);
    // ...
private:
    ForegroundArbiter& arbiter_;
    PauseGate& pause_;            // NEW
    HWND owner_;
};
```

### runLoop changes
```cpp
while (running_) {
    cv.wait until top ready or stop;
    if (q.empty()) continue;
    InputCmd cmd = q.top(); q.pop();
    unlock;

    // NEW: pause check trước acquire. Nếu paused → re-queue cmd (preserve priority).
    if (pause_.isPaused(owner_)) {
        // Re-queue và sleep ngắn để tránh busy loop.
        lock; q.push(cmd); unlock;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        continue;
    }

    if (!arbiter_.acquireSlot(owner_, cmd.priority, std::chrono::milliseconds(500))) {
        gatedDrops_++;
        continue;
    }
    backend_.setTarget(owner_);

    // Drain same-owner.
    do {
        // NEW: check pause mid-drain. Transaction-safe: chỉ break NẾU không trong transaction.
        if (pause_.isPaused(owner_) && !cmd.inTransaction) break;

        if (gate_.allow(cmd)) {
            human_.applyJitter(cmd);
            cmd.action(backend_);
            fired_++;
        } else {
            gatedDrops_++;
        }
        if (arbiter_.shouldYield(owner_)) break;

        lock;
        if (q.empty()) { unlock; break; }
        cmd = q.top(); q.pop();
        unlock;
    } while (true);

    arbiter_.releaseSlot(owner_);
}
```

### Transaction flag (NEW)
Add `bool inTransaction = false` to `InputCmd`. Set true cho:
- Mouse drag refill (down → moves → up phải atomic).
- Buff sequence multi-key.

Khi `inTransaction=true` → pause check skip giữa chừng. Sau slot release → pause check tiếp tục ở vòng ngoài.

## Files to modify (recap)
- `src/input/input-scheduler.h` + `.cpp`
- `src/input/input-cmd.h` (hoặc nơi định nghĩa) — add `inTransaction` field.
- `src/combat/pot-refill-scheduler.cpp` — set `cmd.inTransaction = true` trong drag sequence.

## Todo
- [ ] Add PauseGate ref to ctor.
- [ ] Pre-acquire pause check + re-queue with sleep.
- [ ] Mid-drain pause check (transaction-aware).
- [ ] Mark refill drag cmds as transactional.
- [ ] Compile check.

## Success criteria
- Pause ON → scheduler không acquire slot, queue grows nhưng không spin CPU (50ms sleep).
- Pause OFF resume → queue drain bình thường.
- Refill drag start → user move mouse giữa chừng → drag complete trước khi pause take effect.

## Risks
- Re-queue mất priority order nếu nhiều cmd lẫn lộn — KISS: dùng lại priority queue insert.
- Transaction quá dài → user thấy lag pause. Refill thường <500ms → ok.
