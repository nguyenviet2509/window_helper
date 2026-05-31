# Phase 6 — Manual Testing

## Overview
- **Priority**: P1
- **Status**: pending
- **Effort**: M
- **Depends on**: Phase 5

Manual smoke + edge-case validation against real game. No unit tests (vision/input mocking too costly — KISS).

## Test Scenarios

### T1: Disabled = no regression
- `refill.enabled = false`, run for 5 min of combat.
- Expect: zero refill logs, all existing behavior preserved.

### T2: Single-slot refill (HP only)
- `hp.intervalSec=30`, sp/mp.intervalSec=0 (disabled).
- Slot HP coords set correctly.
- Expect: every ~30s see full sequence: BEGIN → OPEN_INV → MOVE_HP → FIRE_HP → CLOSE_INV → DONE.
- Combat resumes attack within 1-2 ticks after DONE.

### T3: All 3 slots, gộp một lần mở kho
- All 3 intervalSec=30.
- Expect: 1 OPEN_INV → 3 MOVE/FIRE pairs (HP→MP→SP) → 1 CLOSE_INV.

### T4: HP critical mid-refill
- Set `hpCriticalAbortThreshold=0.50`; intentionally let HP drop while refill runs.
- Expect: `[refill] ABORT`, kho đóng, pot HP ingame fire (`[pot.hp] FIRE vk=49`).
- 30s backoff enforced — no retry trong window.

### T5: Cursor restore
- Note cursor position before refill (e.g., dùng external screen ruler).
- Trigger refill.
- Expect: cursor về vị trí cũ (±vài px do Bezier path).

### T6: Combat state preservation
- Mid-Buffing cycle when refill triggers.
- Expect: sau DONE, buff cycle tiếp tục đúng slot tiếp theo (không restart từ buff 1).

### T7: Foreground loss
- Alt-tab away mid-refill.
- Expect: state machine stuck → after `refillTimeoutMs` (10s) force CLEANUP → log warning.
- Refocus → next tick may retry refill nếu still due.

### T8: Wrong slot coords
- Set HP slot coords to (0,0) hoặc empty area in inventory.
- Expect: refill "succeeds" mechanically (no exception) but no pot consumed. User trách nhiệm calibrate.

### T9: Hot-reload config
- Toggle `enabled` true→false→true via config.json reload.
- Expect: no crash, no hang. Mid-refill toggle ignored until current refill completes.

## Acceptance
All scenarios pass → ship. Issues → file fix tasks per scenario.

## Risks
- **T4 unreliable**: HP drop timing hard to control. Workaround: temporarily set `hpCriticalAbortThreshold=0.95` to force abort.
- **T5 cursor drift via Bezier**: tolerate ±10 px.

## Todo
- [ ] Execute T1-T9
- [ ] Document any failures in `plans/reports/test-260531-pot-refill.md`
- [ ] File fix tasks if needed
