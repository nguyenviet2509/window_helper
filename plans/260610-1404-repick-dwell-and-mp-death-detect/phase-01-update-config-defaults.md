# Phase 01 — Update Config Defaults

## Status: pending
## Priority: high
## Effort: 10 min

## Overview
Bump default values cho `repickMinDwellMs` (2000→6000) và `repickMaxDwellMs` (15000→22000) khớp với TTK manual 8-15s. Thêm 2 field mới `deathConfirmMs=2500`, `mpDropEpsilon=0.005` (hidden tunable, không UI).

## Files to Modify

### `src/state/game-state.h`
```diff
- int repickMinDwellMs = 1000;
- int repickMaxDwellMs = 8000;
+ int repickMinDwellMs = 6000;
+ int repickMaxDwellMs = 22000;
+ int deathConfirmMs   = 2500;   // MP-down-tick window for mob death detect
+ double mpDropEpsilon = 0.005;  // 0.5% threshold for "MP went down" tick
```

### `config.json`, `dist/config.json`, `dist-test/config.json`
```diff
   "repickMaxDwellMs": 15000,
   "repickMinDwellMs": 2000,
+  "deathConfirmMs": 2500,
+  "mpDropEpsilon": 0.005,
```
Update all 3 copies. Bump values to 6000/22000.

## Implementation Steps
1. Edit [src/state/game-state.h](../../src/state/game-state.h) lines 40-41, thêm 2 field mới ngay sau.
2. Edit 3 config.json files: bump 2 value cũ, thêm 2 field mới.

## Todo
- [ ] game-state.h: bump min/max + thêm deathConfirmMs/mpDropEpsilon
- [ ] config.json: bump + thêm
- [ ] dist/config.json: bump + thêm
- [ ] dist-test/config.json: bump + thêm

## Success Criteria
- Cả 3 config file có 4 field với value mới.
- game-state.h có 4 field với default mới.

## Next
Phase 02 — Redesign monitor.
