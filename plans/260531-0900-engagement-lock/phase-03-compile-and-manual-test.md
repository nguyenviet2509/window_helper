# Phase 03 — Compile + Manual Test

**Priority:** P1
**Status:** pending
**Effort:** 30m
**Depends on:** Phase 02

## Overview

Build, smoke-test, then 5-minute live farm session to confirm engagement lock behavior matches spec.

## Requirements

- Clean compile (no warnings introduced).
- Visible behavior change: single click per mob engagement.
- Defaults (5000ms ± 500ms) tunable via `config.json` without rebuild.

## Related Files

Read-only:
- `config.json` (root) — verify keys persisted after a run.
- Log output (stdout / log file if applicable).

## Implementation Steps

1. `cmake --build build --config Release` (or whatever convention exists — check `CMakeLists.txt`).
2. Launch tool, attach to game window, enable combat.
3. Stand near 1 mob. Observe:
   - Initial shift+right-click fires.
   - No further click for ~5s.
   - Character continues auto-attacking the same mob (game-driven chain).
4. After mob dies (HP bar gone / activity monitor flags dead):
   - Next click fires after `repickMinDwellMs` window passes.
5. Force a long engagement (tank mob) — confirm `repickMaxDwellMs` triggers repick around 15s.
6. Inspect `config.json` after exit — `engagementLockMs` / `engagementLockJitterMs` present.
7. Delete those keys from `config.json`, restart — defaults restored, behavior unchanged.

## Todo

- [ ] Build clean
- [ ] Smoke test: single click + 5s silence
- [ ] Verify mob-dead early exit
- [ ] Verify forceRepick at 15s
- [ ] Verify config round-trip + back-compat
- [ ] Verify no regression in Buffing / Arming states

## Success Criteria

- Brainstorm spec items met:
  - 0 extra clicks within a single engagement.
  - Avg gap between repicks ≈ time-to-kill.
  - No mid-mob target swap over 5-min session.
- No crash, no FSM lockup.

## Risks

| Risk | Mitigation |
|------|------------|
| Manual test environment unavailable | At minimum complete compile + cold-start with config; defer live test, mark phase DONE_WITH_CONCERNS |
| Behavior subtly wrong (e.g., still spams) | Re-open P2, inspect ordering of gates |

## Rollback

`git revert` the P1+P2 commits. No state migration. `config.json` extra keys harmless (loader ignores unknown).
