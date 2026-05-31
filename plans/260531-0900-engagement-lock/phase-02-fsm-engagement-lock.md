# Phase 02 — FSM Engagement Lock Logic

**Priority:** P1
**Status:** pending
**Effort:** 45m
**Depends on:** Phase 01

## Overview

Rewrite click-gating inside `stepAttacking()` so only one shift+right-click fires per engagement window. Add `engagementUntil_` timestamp + per-engagement randomized lock duration.

## Key Insights

- Current code path (`src/combat/combat-fsm.cpp:91-115` approx):
  - Computes `dwell`, `minDwellOk`, `forceRepick`.
  - Has block "Continue hitting same spot at cooldown" gated by `attackCooldownMs` — this is the spam source.
- `attackCooldownMs` (350ms) must remain as anti-burst floor between repick clicks (not within engagement).
- `activity_.mobLikelyDead()` already requires `repickMinDwellMs` to have elapsed, so early-death false positives are bounded.
- Existing FSM uses `std::chrono::steady_clock`. Reuse the `ms(int)` helper already in the file.

## Requirements

- New field `engagementUntil_` (default-init = epoch → starts unlocked, first click fires immediately on entering Attacking).
- Skip click iff `inLock && !mobDead && !forceRepick`.
- On click decision (repick): sample `lock = engagementLockMs + uniform_int(0, engagementLockJitterMs)`, set `engagementUntil_ = now + lock`.
- Keep `attackCooldownMs` as hard floor between two consecutive repick clicks.
- Remove the "Continue hitting same spot at cooldown" block entirely.
- Reset/clear lock when leaving Attacking state is NOT required (engagementUntil_ is a moment in time; re-entry recomputes naturally on first click).

## Architecture / Data Flow

```
tick() → stepAttacking(v, now)
  ├─ update activity_ from VisionState
  ├─ dwell = now - lastPickAt_
  ├─ inLock = now < engagementUntil_
  ├─ mobDead = minDwellOk && activity_.mobLikelyDead()
  ├─ forceRepick = dwell >= repickMaxDwellMs
  ├─ if (inLock && !mobDead && !forceRepick) return;        // SILENT
  ├─ if (now - lastAttackAt_ < attackCooldownMs) return;    // anti-burst floor
  ├─ pos = sweep_.pickAttackPosition(...)
  ├─ schedule shift+right-click
  ├─ lastPickAt_ = now; lastAttackAt_ = now;
  └─ engagementUntil_ = now + engagementLockMs + jitter
```

## Related Files

Modify:
- `src/combat/combat-fsm.h` — add `std::chrono::steady_clock::time_point engagementUntil_{};`
- `src/combat/combat-fsm.cpp` — rewrite `stepAttacking()` click gate.

Read for context:
- `src/combat/combat-activity-monitor.h` — confirm `mobLikelyDead()` signature.
- `src/state/game-state.h` — new config fields from P1.

## Implementation Steps

1. In `combat-fsm.h`, add private field:
   ```cpp
   std::chrono::steady_clock::time_point engagementUntil_{};
   ```
2. In `combat-fsm.cpp` `stepAttacking()`:
   - After computing `dwell`, `minDwellOk`, `forceRepick`, add:
     ```cpp
     bool inLock = now < engagementUntil_;
     bool mobDead = minDwellOk && activity_.mobLikelyDead();
     if (inLock && !mobDead && !forceRepick) return;
     ```
   - Delete the existing block (currently ~lines 109-112):
     ```cpp
     // Continue hitting same spot at cooldown.
     if ((now - lastAttackAt_) < ms(cfg_.attackCooldownMs)) return;
     ```
     Replace with hard-floor between repicks:
     ```cpp
     if ((now - lastAttackAt_) < ms(cfg_.attackCooldownMs)) return;
     ```
     (Keep this AFTER the inLock gate — semantics now: floor only matters at repick moment.)
   - After click is scheduled and `lastPickAt_/lastAttackAt_` set, append:
     ```cpp
     int jitter = cfg_.engagementLockJitterMs > 0
         ? (std::rand() % (cfg_.engagementLockJitterMs + 1))
         : 0;
     engagementUntil_ = now + ms(cfg_.engagementLockMs + jitter);
     ```
     (If file already uses a better RNG helper, prefer it; `std::rand` acceptable given existing conventions — verify before adding `<cstdlib>` include.)
3. Confirm the original `minDwellOk && activity_.mobLikelyDead()` check that used to gate the "wait for mob to die" branch is now consolidated into the new `mobDead` variable — drop any duplicated old conditional.
4. `cmake --build` and fix any include/compile errors.

## Todo

- [ ] Add `engagementUntil_` field
- [ ] Insert inLock/mobDead skip gate
- [ ] Remove old "Continue hitting same spot" cooldown branch
- [ ] Set `engagementUntil_` after each click
- [ ] Compile clean

## Success Criteria

- Build green.
- Code review: only ONE place in `stepAttacking()` schedules a shift+right-click cmd.
- `engagementUntil_` set on the same line block as the click.
- `attackCooldownMs` still referenced exactly once (as repick floor).

## Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Activity monitor false-positive (mobLikelyDead too eager) → premature repick | Med | Med | `repickMinDwellMs` (2s) gates `mobDead`; if flaky bump to 3s in config |
| Lock too long when mob silently dies (no HP drain) | Low | Low | `repickMaxDwellMs` (15s) caps idle |
| Off-by-one: `engagementUntil_` left-over across state transitions | Low | Low | Time-point comparison is monotonic; stale value only matters if re-entering Attacking < lock duration after exit, which is desired behavior anyway |
| Forgot to remove old cooldown branch → still spams | Low | High | Manual test in P3 covers; success criteria #2 |
