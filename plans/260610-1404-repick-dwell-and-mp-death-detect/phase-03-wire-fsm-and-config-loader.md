# Phase 03 — Wire FSM Call Site + Config Loader

## Status: pending
## Priority: high
## Effort: 15 min

## Overview
Update FSM call sites theo API mới của `CombatActivityMonitor`. Thêm load/save 2 field `deathConfirmMs`/`mpDropEpsilon` trong config-loader. Propagate config xuống monitor instance (FSM phải set 2 field trên monitor khi init/khi config reload).

## File: `src/combat/combat-fsm.cpp`

**Line 122** (`stepAttacking`):
```diff
- activity_.update(v.hpPct, v.mpPct);
+ activity_.update(v.mpPct, now);
```

**Line 139** (mob death check):
```diff
- bool mobDead = minDwellOk && activity_.mobLikelyDead();
+ bool mobDead = minDwellOk && activity_.mobLikelyDead(now);
```

**Line 159** (after schedule attack):
```diff
- activity_.reset();
+ activity_.reset(now);
```

**Propagate config to monitor:**
Tìm chỗ FSM nhận `CombatConfig cfg_` (constructor/setConfig). Sau khi assign `cfg_`, set:
```cpp
activity_.deathConfirmMs = cfg_.deathConfirmMs;
activity_.mpDropEpsilon = cfg_.mpDropEpsilon;
```
(Vị trí: tìm grep `cfg_ =` hoặc constructor của `CombatFsm`.)

## File: `src/config/config-loader.cpp`

**Trong `toJson(CombatConfig)` (~line 76):**
```diff
   {"repickMinDwellMs", c.repickMinDwellMs},
   {"repickMaxDwellMs", c.repickMaxDwellMs},
+  {"deathConfirmMs", c.deathConfirmMs},
+  {"mpDropEpsilon", c.mpDropEpsilon},
```

**Trong `fromJson(CombatConfig)` (~line 102):**
```diff
   if (j.contains("repickMinDwellMs")) c.repickMinDwellMs = j["repickMinDwellMs"];
   if (j.contains("repickMaxDwellMs")) c.repickMaxDwellMs = j["repickMaxDwellMs"];
+  if (j.contains("deathConfirmMs")) c.deathConfirmMs = j["deathConfirmMs"];
+  if (j.contains("mpDropEpsilon")) c.mpDropEpsilon = j["mpDropEpsilon"];
```

## Implementation Steps
1. Edit `combat-fsm.cpp`: 3 call site update (lines 122, 139, 159).
2. Grep `cfg_ =` trong `combat-fsm.cpp` → tìm chỗ assign config, thêm 2 line set `activity_.deathConfirmMs/mpDropEpsilon` ngay sau.
3. Edit `config-loader.cpp`: thêm 2 line vào `toJson` + 2 line vào `fromJson`.
4. Run build (msbuild/cmake) → verify không lỗi compile.

## Todo
- [ ] combat-fsm.cpp: update update() call site (line 122)
- [ ] combat-fsm.cpp: update mobLikelyDead() call (line 139)
- [ ] combat-fsm.cpp: update reset() call (line 159)
- [ ] combat-fsm.cpp: propagate deathConfirmMs/mpDropEpsilon từ cfg_ xuống activity_
- [ ] config-loader.cpp: toJson thêm 2 field
- [ ] config-loader.cpp: fromJson thêm 2 field
- [ ] Build pass

## Success Criteria
- `combat-fsm.cpp` compile sạch với API mới.
- Save config → JSON có 2 field mới.
- Load config cũ (không có 2 field) → dùng default từ game-state.h.

## Risks
- Có thể có call site khác gọi `activity_.update(hp, mp)` hoặc `mobLikelyDead()` ngoài combat-fsm.cpp. **Mitigation:** grep toàn project trước khi build.

## Next
Phase 04 — Compile + docs.
