# Phase 2 — OutputGate Flag & Priority Enum

## Overview
- **Priority**: P0
- **Status**: pending
- **Effort**: XS
- **Depends on**: Phase 1

Add `refillActive_` atomic flag to `OutputGate` and new `P1_Refill` priority. Gate drops commands ≥ P2 when flag set.

## Files to Modify
- `src/core/output-gate.h` — add flag + setter
- `src/core/output-gate.cpp` — modify `allowInput()`
- `src/dispatch/priority.h` — insert P1_Refill

## Changes

### `src/dispatch/priority.h`
Inspect current enum values. Insert `P1_Refill = 1` between `P0_Critical` and `P2_Pot`. Renumber existing if needed (P2_Pot=2, P3_Combat=3). If priority is just ints in InputCmd, no enum to update — just add constant.

### `src/core/output-gate.h`
```cpp
class OutputGate {
public:
    // ... existing setters ...
    void setRefillActive(bool v) { refillActive_.store(v); }
    bool refillActive() const noexcept { return refillActive_.load(); }
    bool allowInput(int priority) const noexcept;   // overload (keep old for compat?)
    bool allowInput() const noexcept;                // existing

private:
    // ... existing fields ...
    std::atomic<bool> refillActive_{ false };
};
```

### `src/core/output-gate.cpp`
- Existing `allowInput()` unchanged.
- Add overload `allowInput(int priority)`:
  - If `refillActive_ && priority >= P2_Pot` → return `false`.
  - Else delegate to `allowInput()`.

### Scheduler integration
Update `input-scheduler.cpp::runLoop`:
- Change `gate_.allowInput()` → `gate_.allowInput(cmd.priority)`.

## Todo
- [ ] Add `P1_Refill` priority
- [ ] Add `refillActive_` flag + setter/getter to OutputGate
- [ ] Add `allowInput(int priority)` overload
- [ ] Update scheduler to pass priority
- [ ] Compile check

## Success Criteria
- When `refillActive_=true`: commands with priority `P2_Pot`/`P3_Combat` dropped, log `[scheduler] DROPPED ... (gate denied)`.
- When `refillActive_=true`: `P1_Refill` commands fire normally.
- When `refillActive_=false`: behavior identical to current.

## Risks
- Other callers of `allowInput()` (without priority) must be reviewed — they'll continue using the no-arg version, fine.
