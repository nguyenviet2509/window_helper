# Phase 4 — Combat & PotEvaluator Skip Hook

## Overview
- **Priority**: P0
- **Status**: pending
- **Effort**: XS
- **Depends on**: Phase 3

Make CombatFsm and PotEvaluator skip work while refill is busy (source-level pause, complementary to gate drop).

## Approach
Inject a `std::function<bool()> refillBusyFn_` (or raw pointer to scheduler) into CombatFsm and PotEvaluator. Avoid tight coupling — function callback keeps the dependency one-way.

Alternative: pass a `const PotRefillScheduler*` directly. Simpler, but tighter coupling. **Recommend the pointer** since refill scheduler is owned by main and lifetime is clear (KISS over abstraction).

## Files to Modify
- `src/combat/combat-fsm.h` — add `setRefillScheduler(const PotRefillScheduler*)`
- `src/combat/combat-fsm.cpp` — early-return in `tick()` when `refill_->busy()`
- `src/combat/pot-evaluator.h` — same setter
- `src/combat/pot-evaluator.cpp` — early-return in evalHp/evalMpSp/evalRecall

## Changes

### `combat-fsm.h`
```cpp
class PotRefillScheduler;   // fwd decl
class CombatFsm {
public:
    void setRefillScheduler(const PotRefillScheduler* r) { refill_ = r; }
private:
    const PotRefillScheduler* refill_ = nullptr;
};
```

### `combat-fsm.cpp::tick()`
First line:
```cpp
void CombatFsm::tick(const VisionState& v, std::chrono::steady_clock::time_point now) {
    if (!enabled_) return;
    if (refill_ && refill_->busy()) return;    // NEW
    // ... existing logic ...
}
```

### `pot-evaluator.h`
Same setter pattern. Forward declare.

### `pot-evaluator.cpp`
At top of `evalHp`, `evalMpSp`, `evalRecall`:
```cpp
if (refill_ && refill_->busy()) return std::nullopt;
```

## Todo
- [ ] Add fwd decl + setter + member to CombatFsm
- [ ] Early-return in CombatFsm::tick
- [ ] Same for PotEvaluator (3 methods)
- [ ] Compile check

## Success Criteria
- When `refill->busy()=true`:
  - CombatFsm doesn't schedule new attack/buff cmds.
  - PotEvaluator doesn't fire pot ingame.
- When `false`: behavior unchanged.

## Risks
- **Forgetting one PotEvaluator method**: triple-check all 3 eval* methods.
- **CombatFsm internal state** (`buffs_`, `sweep_`, timers) untouched during skip → resumes naturally. Verified by reading `combat-fsm.cpp:8-19` (state preserved).
