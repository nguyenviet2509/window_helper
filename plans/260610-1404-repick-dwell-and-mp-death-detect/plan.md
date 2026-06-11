---
name: Repick Dwell Tuning + MP-only Mob Death Detect
slug: repick-dwell-and-mp-death-detect
date: 260610-1404
status: complete
mode: fast
blockedBy: []
blocks: []
---

# Plan — Repick Dwell + MP Death Detect

## Source
Brainstorm: [brainstorm-260610-1404-repick-dwell-and-mp-death-detect.md](../reports/brainstorm-260610-1404-repick-dwell-and-mp-death-detect.md)

## Goal
1. Update default dwell numbers: `repickMinDwellMs=6000`, `repickMaxDwellMs=22000` (phù hợp TTK 8-15s).
2. Redesign mob-death detect: MP downward-tick only (bỏ HP tracking).
3. Add 2 hidden config fields: `deathConfirmMs=2500`, `mpDropEpsilon=0.005`.

## Constraints
- KISS, YAGNI, DRY.
- Không expose UI cho 2 field mới.
- Backward-compat: config-loader đã dùng `j.contains(...)`, user cũ tự nhận default mới.
- Không migration script.

## Phases

| # | Phase | Status | Files |
|---|---|---|---|
| 1 | Update config defaults | pending | game-state.h, config.json × 3 |
| 2 | Redesign CombatActivityMonitor | pending | combat-activity-monitor.{h,cpp} |
| 3 | Wire FSM call site + config loader | pending | combat-fsm.cpp, config-loader.cpp |
| 4 | Compile + update docs | pending | ui-parameters-guide.md, HUONG-DAN-CAU-HINH.md |
| 5 | Manual soak test | pending | (observation) |

## Key Files
- [src/state/game-state.h](../../src/state/game-state.h) — defaults
- [src/combat/combat-activity-monitor.h](../../src/combat/combat-activity-monitor.h) — class redesign
- [src/combat/combat-activity-monitor.cpp](../../src/combat/combat-activity-monitor.cpp) — impl rewrite
- [src/combat/combat-fsm.cpp:122,139](../../src/combat/combat-fsm.cpp) — `activity_.update()`, `mobLikelyDead()` call site
- [src/config/config-loader.cpp](../../src/config/config-loader.cpp) — load/save 2 field mới
- [config.json](../../config.json), [dist/config.json](../../dist/config.json), [dist-test/config.json](../../dist-test/config.json)
- [docs/ui-parameters-guide.md](../../docs/ui-parameters-guide.md), [dist/HUONG-DAN-CAU-HINH.md](../../dist/HUONG-DAN-CAU-HINH.md)

## Success Criteria
- Build pass (MSVC/clang) không warning mới.
- Default config sau update: min=6000, max=22000, deathConfirmMs=2500, mpDropEpsilon=0.005.
- Bot soak test 1 buổi: mob HP cao (TTK 12-15s) không bị cắt giữa chừng; mob stuck/immortal vẫn force-repick trong 22s.
- Docs đồng bộ default mới.
