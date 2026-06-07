---
title: "Engagement Lock for shift+right-click attack"
description: "Single click per engagement window (~5s + jitter); skip cooldown spam; repick only on mob-dead or maxDwell."
status: implemented-pending-build-verify
supersededBy: 260531-1753-vision-mob-targeting  # phase-04 thay timer bằng vision targetLocked; giữ timer làm fallback
priority: P2
effort: 1.5h
branch: master
tags: [combat, fsm, anti-detect]
created: 2026-05-31
---

## Context

- Brainstorm: `plans/reports/brainstorm-260531-0900-engagement-lock.md` (Approach A approved)
- Problem: `stepAttacking()` spams shift+right-click every `attackCooldownMs` (350ms), each click re-targets and breaks the game's auto-chain that the first click already kicked off.
- Decision: introduce engagement lock window. After a click, stay silent for `engagementLockMs ± uniform(0, jitterMs)` unless mob dies or `repickMaxDwellMs` elapses.

## Phases

| # | Phase | Status | File |
|---|-------|--------|------|
| 1 | Config fields (state + loader) | done | [phase-01-config-fields.md](phase-01-config-fields.md) |
| 2 | FSM engagement lock logic | done | [phase-02-fsm-engagement-lock.md](phase-02-fsm-engagement-lock.md) |
| 3 | Compile + manual test | pending (cần user build) | [phase-03-compile-and-manual-test.md](phase-03-compile-and-manual-test.md) |
| 4 | UI expose slider + jitter + tooltip | done | [phase-04-ui-expose.md](phase-04-ui-expose.md) |

## Dependency Graph

- P1 → P2 (FSM reads cfg field)
- P1 → P4 (UI binds to cfg field)
- P2 + P4 → P3 (test full path including UI)

## File Ownership

| File | Phase |
|------|-------|
| `src/state/game-state.h` | P1 |
| `src/config/config-loader.cpp` | P1 |
| `src/combat/combat-fsm.h` | P2 |
| `src/combat/combat-fsm.cpp` | P2 |
| `src/ui/main-window.cpp` | P4 |

No overlap between parallel-eligible phases.

## Success Criteria (rollup)

- Within one engagement, only 1 shift+right-click sent (until mob dies or maxDwell hits).
- Default lock = 5000ms; jitter ±500ms observable across repicks.
- Existing `repickMinDwellMs` / `repickMaxDwellMs` / `attackCooldownMs` semantics preserved.
- `config.json` round-trip preserves new fields; missing fields fall back to defaults (backwards-compat).
- `cmake --build` clean. Manual 5-min farm: no mid-mob target swap.

## Rollback

- All changes are additive on config side (new optional JSON keys). Reverting commit restores 350ms spam behavior — no state migration needed.

## Unresolved Questions

- Log every engagement (start/end) for offline tuning? Punt to follow-up unless P3 manual test shows need.
