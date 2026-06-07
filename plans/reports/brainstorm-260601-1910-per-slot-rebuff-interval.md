# Brainstorm: Per-Slot Re-Buff Interval

**Date:** 2026-06-01 19:10
**Status:** Approved → implementation

## Problem
`CombatConfig.cycleDurationSec` (300s) global cho cả 4 slot F2-F5. Re-buff đồng loạt sai timing khi mỗi buff có duration khác nhau (F2 dài, F3 ngắn).

## Approaches

| # | Approach | Verdict |
|---|---|---|
| **A** | `rebuffIntervalSec` per slot, bỏ global. Track `lastCastAt_[i]`. Trigger Buffing khi bất kỳ slot due; trong Buffing chỉ cast slot due. | **Chọn** |
| B | Global tick + `minIntervalSec` per slot | Loại — 2 nguồn truth |
| C | `enabledEveryNCycles` per slot | Loại — khó tune |

## Final Solution (A)

**Schema:**
- `BuffSlotCfg.rebuffIntervalSec` (default 300) — mới
- `CombatConfig.cycleDurationSec` — bỏ; migration: nếu JSON cũ có, dùng làm default cho slot thiếu interval

**FSM state:** `std::vector<steady_clock::time_point> lastCastAt_` (init epoch zero = due lần đầu)

**Trigger (stepAttacking):** thay cycleStart check bằng:
```cpp
for (i) if (enabled && (now - lastCastAt_[i]) >= sec(intervalSec)) anyDue=true;
```

**Buffing step:** sequencer trả slot due tiếp theo; cast → `lastCastAt_[i] = now`. Exit khi không còn due.

**Migration:** JSON đọc `cycleDurationSec` làm fallback default per-slot, save lần kế ghi field mới, drop top-level.

## Files
- src/state/game-state.h
- src/config/config-loader.cpp
- src/combat/combat-fsm.h, .cpp
- src/combat/buff-sequencer.h
- src/ui/main-window.cpp

## Risks
- `lastCastAt_` resize khi cfg.buffs thay đổi runtime → sync size sau config reload
- Interval < 60s sẽ interrupt attack liên tục → UI warning ngưỡng thấp

## Success
- Set F2=1800, F3=300 → trong 1800s, F3 cast ~6 lần, F2 cast 1 lần
- Load config cũ auto-migrate
- Build sạch
