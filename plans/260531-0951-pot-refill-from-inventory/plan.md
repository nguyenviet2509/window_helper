---
title: "Pot Refill from Inventory"
description: "Periodic auto-refill HP/SP/MP pots from inventory via V toggle + Shift+1/2/3 at slot coords. Safe pause/resume + HP-critical abort."
status: implemented-pending-manual-test
priority: P2
created: 2026-05-31
slug: pot-refill-from-inventory
brainstorm: plans/reports/brainstorm-260531-0951-pot-refill-from-inventory.md
blockedBy: []
blocks: []
---

# Pot Refill from Inventory

## Goal
Khi character cạn pot hotbar, tool tự mở kho theo interval cấu hình và bơm lại HP/SP/MP bằng Shift+1/2/3 tại tọa độ slot trong kho. Pause toàn bộ combat trong lúc refill, abort an toàn nếu HP critical.

## Game Action Map
| Action | Key/Mouse |
|--------|-----------|
| Pot ingame | `1` HP, `2` SP, `3` MP |
| Attack | Shift + Right Mouse |
| Inventory toggle | `V` |
| Refill | mouse → slot + Shift+`1`/`2`/`3` |

## Key Design Decisions
- **Pause mechanism**: Hybrid — source skip (Combat/Pot) + gate flag (`OutputGate::refillActive_`) drops queued cmds.
- **Priority**: New `P1_Refill` bypasses gate flag; combat/pot at P2/P3 dropped.
- **Atomic Shift+N**: single lambda gói KeyDown LSHIFT → KeyTap → KeyUp LSHIFT (pattern giống `sendShiftRightClick`).
- **Refill order**: HP → MP → SP (gộp 1 lần mở kho).
- **Abort**: HP < 0.30 mid-refill → close inv → release shift → restore cursor → backoff 30s.
- **Timeout**: 10s force CLEANUP nếu state machine treo.
- **Coords**: client-space (theo game window), nhập trong `config.json`.
  - HP: (279, 473)  [bbox 268,462 - 290,484]
  - SP: (279, 492)  [bbox 268,484 - 291,501]
  - MP: (279, 517)  [bbox 268,506 - 290,528]

## Phases
| # | Phase | File | Status |
|---|-------|------|--------|
| 1 | Config schema + state | [phase-01-config-and-state.md](phase-01-config-and-state.md) | done |
| 2 | Gate flag + InputCmd.bypassRefillGate | [phase-02-gate-and-priority.md](phase-02-gate-and-priority.md) | done |
| 3 | PotRefillScheduler module | [phase-03-pot-refill-scheduler.md](phase-03-pot-refill-scheduler.md) | done |
| 4 | Combat + PotEvaluator skip | [phase-04-combat-pot-skip.md](phase-04-combat-pot-skip.md) | done |
| 5 | Dispatcher + main wiring | [phase-05-main-wiring-and-ui.md](phase-05-main-wiring-and-ui.md) | done (UI countdown deferred) |
| 6 | Manual testing | [phase-06-manual-testing.md](phase-06-manual-testing.md) | pending (needs real game) |

## Key Dependencies
- `src/input/input-scheduler.{h,cpp}` (existing)
- `src/core/output-gate.{h,cpp}` (existing — extended in phase 2)
- `src/input/mouse-path.{h,cpp}` (existing — used for Bezier move)
- `src/vision/roi.h` (`VisionState.hpPct` for abort check)

## Success Criteria
- `enabled=false` → zero regression.
- `enabled=true`, interval=60s → log `[refill] BEGIN ... CLEANUP elapsedMs<2000` mỗi 60s.
- HP < 0.30 mid-refill → `[refill] ABORT` → kho đóng → `[pot.hp] FIRE vk=49` <500ms.
- Cursor restore đúng vị trí farm; combat state preserved (Buffing/Attacking continues).

## Reports
- Brainstorm: [brainstorm-260531-0951-pot-refill-from-inventory.md](../reports/brainstorm-260531-0951-pot-refill-from-inventory.md)
