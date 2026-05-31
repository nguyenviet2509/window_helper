# Phase 3 — PotRefillScheduler Module

## Overview
- **Priority**: P0
- **Status**: pending
- **Effort**: M
- **Depends on**: Phase 1, 2

Create new module `src/combat/pot-refill-scheduler.{h,cpp}` implementing the refill state machine.

## Files to Create
- `src/combat/pot-refill-scheduler.h`
- `src/combat/pot-refill-scheduler.cpp`

## Files to Modify
- `src/CMakeLists.txt` — add new sources

## API

```cpp
class PotRefillScheduler {
public:
    PotRefillScheduler(InputScheduler& sched, OutputGate& gate, HWND target,
                       const PotRefillConfig& cfg);
    void enable(bool on);
    bool enabled() const { return enabled_; }
    bool busy() const { return state_ != State::Idle; }
    void updateConfig(const PotRefillConfig& cfg);
    void setTarget(HWND h) { target_ = h; }
    void tick(const VisionState& v, std::chrono::steady_clock::time_point now);

    // For UI countdown (Phase 5)
    int secondsUntilNext(char which /*'h'|'s'|'m'*/, std::chrono::steady_clock::time_point now) const;
    const char* stateName() const;

private:
    enum class State { Idle, SaveCursor, OpenInv, MoveSlot, FireSlot,
                       CloseInv, Cleanup, AbortClose, AbortCleanup };
    enum class Slot { None, Hp, Sp, Mp };

    void scheduleStep(std::function<void(IInputBackend&)> action,
                      std::chrono::steady_clock::time_point fireAt);
    bool anyDue(std::chrono::steady_clock::time_point now) const;
    bool isDue(Slot s, std::chrono::steady_clock::time_point now) const;
    Slot nextSlot();   // returns next due slot in order HP→MP→SP, or None
    void enterAbort(std::chrono::steady_clock::time_point now);
    void doCleanup(std::chrono::steady_clock::time_point now, bool aborted);

    InputScheduler& sched_;
    OutputGate& gate_;
    HWND target_;
    PotRefillConfig cfg_;
    bool enabled_ = false;

    State state_ = State::Idle;
    Slot currentSlot_ = Slot::None;
    std::chrono::steady_clock::time_point nextStepAt_{};
    std::chrono::steady_clock::time_point refillStartedAt_{};
    std::chrono::steady_clock::time_point lastHpAt_{}, lastSpAt_{}, lastMpAt_{};
    std::chrono::steady_clock::time_point abortBackoffUntil_{};
    POINT savedCursorScreen_{0, 0};
    bool slotsPlanned_[3] = { false, false, false };  // HP,SP,MP for this refill round
};
```

## State Machine Logic (tick)

```
On each tick(v, now):
  1. If state == Idle:
       - If now < abortBackoffUntil_: return.
       - If !anyDue(now): return.
       - If v.valid && v.hpPct < cfg_.hpCriticalAbortThreshold: return.  // pre-check
       - Save cursor (GetCursorPos), set gate.setRefillActive(true).
       - Plan due slots into slotsPlanned_[].
       - Transition → OpenInv: schedule [tap V] at now, nextStepAt_=now+inventoryOpenDelayMs.
       - refillStartedAt_=now.

  2. Check global timeout: if (now - refillStartedAt_) > refillTimeoutMs && state != Idle:
       - Force doCleanup(now, aborted=true). Log warning.

  3. Check HP-critical abort: if state ∈ {OpenInv, MoveSlot, FireSlot, CloseInv} &&
       v.valid && v.hpPct < hpCriticalAbortThreshold:
       - enterAbort(now). Return.

  4. If now < nextStepAt_: return.

  5. Switch on state:
       OpenInv → pick nextSlot() into currentSlot_. If none → CloseInv branch.
                 Else schedule [mouse move to slot] (uses backend.sendMouseMove which respects Bezier path).
                 state=MoveSlot, nextStepAt_=now+mouseMoveDelayMs.
       MoveSlot → schedule atomic Shift+N for currentSlot_.
                  state=FireSlot, nextStepAt_=now+postHotkeyDelayMs.
       FireSlot → mark slot refilled (lastHpAt_/SpAt_/MpAt_=now), clear slotsPlanned_[idx].
                  → next slot via nextSlot(); if found, schedule mouse move, state=MoveSlot.
                  Else schedule [tap V close], state=CloseInv, nextStepAt_=now+inventoryCloseDelayMs.
       CloseInv → doCleanup(now, aborted=false).
       AbortClose → schedule [tap V close], state=AbortCleanup, nextStepAt_=now+inventoryCloseDelayMs.
       AbortCleanup → doCleanup(now, aborted=true). Set abortBackoffUntil_=now+abortBackoffMs.
```

## Atomic Shift+N Lambda

```cpp
auto fireSlot = [vk](IInputBackend& b) {
    b.sendKeyDown(VK_LSHIFT);
    b.sendKeyTap(vk, 30);
    b.sendKeyUp(VK_LSHIFT);
};
InputCmd cmd;
cmd.priority = P1_Refill;
cmd.fireAt = now;
cmd.action = fireSlot;
sched_.schedule(std::move(cmd));
```

VK mapping: HP→`'1'`, SP→`'2'`, MP→`'3'`.

## Cleanup

```cpp
void doCleanup(now, aborted) {
    // Defensive: release shift
    sched_.schedule(makeCmd(P1_Refill, now, [](IInputBackend& b) { b.sendKeyUp(VK_LSHIFT); }));
    // Restore cursor (convert screen→client→sendMouseMove via backend)
    POINT clientPt = savedCursorScreen_;
    ScreenToClient(target_, &clientPt);
    sched_.schedule(makeCmd(P1_Refill, now + ms(20),
        [x=clientPt.x, y=clientPt.y](IInputBackend& b) { b.sendMouseMove(x, y); }));
    // Clear gate
    gate_.setRefillActive(false);
    state_ = State::Idle;
    currentSlot_ = Slot::None;
    Logger::log("[refill] %s elapsedMs=%lld", aborted ? "ABORTED" : "DONE",
                durationMs(refillStartedAt_, now));
}
```

## Logging
Every state transition logs at Info level:
```
[refill] BEGIN slots=H,M,S cursor=(x,y)
[refill] OPEN_INV tap V
[refill] MOVE_HP → (1234, 567)
[refill] FIRE_HP shift+1
[refill] CLOSE_INV tap V
[refill] DONE elapsedMs=1840
```
Abort:
```
[refill] ABORT hpPct=0.27 < critical=0.30
[refill] backoff until +30000ms
```

## Todo
- [ ] Create `pot-refill-scheduler.h` with API above
- [ ] Implement state machine in `.cpp`
- [ ] Helper `makeCmd(prio, fireAt, action)` to reduce boilerplate
- [ ] Cursor save (GetCursorPos) and restore (ScreenToClient + sendMouseMove)
- [ ] Add to `src/CMakeLists.txt`
- [ ] Compile check

## Success Criteria
- Module compiles standalone.
- Unit-testable via mock InputScheduler if time permits (otherwise integration test in phase 6).

## Risks
- **Cursor restore via sendMouseMove (not SetCursorPos)** — backend uses PostMessage/SendInput respecting target. Verified by reading `i-input-backend.h:15`.
- **GetCursorPos returns screen coords** — must `ScreenToClient(target_, &pt)` before restore.
