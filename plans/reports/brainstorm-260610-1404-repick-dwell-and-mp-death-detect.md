---
type: brainstorm
date: 260610-1404
slug: repick-dwell-and-mp-death-detect
status: approved
---

# Brainstorm — Repick Dwell Tuning + MP-only Mob Death Detect

## Problem Statement
Bot auto-combat hiện tại:
1. **Dwell tuning sai**: `repickMinDwellMs=2000` < `engagementLockMs=5000` → vô hiệu. `repickMaxDwellMs=15000` = p95 TTK manual → cắt nhầm mob trâu, waste damage ~5-10%.
2. **Detect mob chết kém ổn định**: logic `max(HP/MP window) - min < 1%` false-positive khi MP regen lên cao (max-min lớn dù mob đã chết) và phụ thuộc HP delta (build pure-physical / full HP cap không reliable).

**Manual TTK observed:** 8-15s (median ~11s, p95 ~15s).

## Requirements
- Min dwell ≥ engagementLock + jitter để tránh setting vô hiệu.
- Max dwell phải cover p95 TTK + detect latency + safety margin.
- Death detect: chỉ dùng MP, cửa sổ 2-3s, "MP tụt rồi hồi" vẫn tính active.
- Giấu tham số detect mới trong config.json (không expose UI).
- KISS — không over-engineer, không adaptive logic, không vision-based.

## Evaluated Approaches

### Numbers (Part 1)
| Option | min/max | Verdict |
|---|---|---|
| Aggressive 5500/8000 | Quá ngắn vs TTK 8-15s | ❌ Reject — bỏ mob ở median |
| Default cũ 2000/15000 | Min vô hiệu, max cắt p95 | ❌ Reject — status quo problem |
| **Recommended 6000/22000** | Min ≥ lock+jitter, max ≈ p95×1.5 | ✅ **Pick** |
| Safe 7000/30000 | Stuck escape chậm gấp đôi | ❌ Reject — không cần |

### Death Detect (Part 2)
| Option | Logic | Verdict |
|---|---|---|
| Status quo HP+MP max-min | Nhiễu vì regen tạo max-min lớn | ❌ Reject |
| MP-only max-min | Vẫn nhiễu vì regen | ❌ Reject |
| **MP downward-tick only** | Track lần tick âm cuối, no-down-in-window = dead | ✅ **Pick** |
| Vision-based mob HP OCR | Over-engineer, fragile | ❌ Reject (YAGNI) |

## Final Solution

### Config defaults
```
repickMinDwellMs = 6000    // engagementLock 5000 + jitter 500 + buffer 500
repickMaxDwellMs = 22000   // p95 TTK 15000 + detect window 3000 + safety 4000
deathConfirmMs   = 2500    // hidden in config.json
mpDropEpsilon    = 0.005   // hidden in config.json
```

### CombatActivityMonitor — new design
```cpp
class CombatActivityMonitor {
  double prevMp_ = -1;
  TimePoint lastMpDropAt_ = TimePoint::min();
public:
  int deathConfirmMs = 2500;
  double mpDropEpsilon = 0.005;

  void update(double mp, TimePoint now) {
    if (prevMp_ >= 0 && (mp - prevMp_) <= -mpDropEpsilon) {
      lastMpDropAt_ = now;
    }
    prevMp_ = mp;
  }
  void reset(TimePoint now) {
    prevMp_ = -1;
    lastMpDropAt_ = now;   // 2.5s grace after pick
  }
  bool mobLikelyDead(TimePoint now) const {
    return lastMpDropAt_ != TimePoint::min()
        && (now - lastMpDropAt_) >= ms(deathConfirmMs);
  }
};
```

### Rationale
- **6000/22000**: min đồng bộ lock; max = p95×1.5 cover TTK tail + detect latency 3s + 4s safety.
- **MP-downward-tick**: spec user "MP tụt rồi hồi vẫn tính tụt" → chỉ reset timer khi có tick âm, regen lên không xóa dấu. Drop HP tracking (build không reliable + giảm complexity).
- **Reset = now** (không phải `min()`): 2.5s ân hạn sau pick mob mới — tránh false-positive trước khi player kịp cast skill đầu tiên.
- **Bỏ HP tracking**: -50% bộ nhớ, -50% complexity, KISS.

## Implementation Considerations

### Files to touch
- `src/state/game-state.h` — defaults 6000/22000, thêm `deathConfirmMs`, `mpDropEpsilon`.
- `src/combat/combat-activity-monitor.h` — redesign class (single prevMp + lastMpDropAt).
- `src/combat/combat-activity-monitor.cpp` (nếu có) — rewrite.
- `src/combat/combat-fsm.cpp` — verify `activity_.mobLikelyDead()` call site (có thể cần truyền `now`).
- `src/config/config-loader.cpp` — load/save 2 field mới.
- `docs/ui-parameters-guide.md` + `dist/HUONG-DAN-CAU-HINH.md` — update defaults 6000/22000.
- `config.json`, `dist/config.json`, `dist-test/config.json` — bump defaults.

### Risks & Mitigation
| Risk | Impact | Mitigation |
|---|---|---|
| MP full cap nếu skill cost rất nhỏ | False positive death | Confirmed: user có auto-pot, MP vẫn dao động do skill cast — bỏ qua |
| MP cạn 0% | False positive | Confirmed: auto-pot threshold đảm bảo MP không cạn — bỏ qua |
| Pixel/OCR noise tạo tick âm giả | False negative (mob chết nhưng nghĩ còn sống) | `mpDropEpsilon=0.005` lọc noise; tăng 0.01 nếu cần |
| Player chưa cast skill đầu tiên | False positive ngay sau pick | `reset(now)` cho 2.5s grace |
| Cast skill cooldown > 2.5s | False positive giữa cast | Hiếm trong farm; nếu xảy ra → tăng `deathConfirmMs` lên 3000 |

### Backward compat
- Config loader đã dùng `if (j.contains(...))` → user cũ tự động nhận default mới.
- Không migration script cần thiết.

## Success Metrics
- **Kill throughput** tăng (mob HP cao không bị cắt ở 15s).
- **False positive rate** (đổi mob khi mob chưa chết) giảm rõ rệt — đo qua log/observation 1 buổi farm.
- **Stuck recovery**: mob immortal/bug vẫn được force-repick trong 22s.
- **Config UI clean**: không thêm slider mới.

## Next Steps
1. `/ck:plan` tạo implementation plan với phases:
   - Phase 1: Update defaults trong game-state.h + config files.
   - Phase 2: Redesign combat-activity-monitor.{h,cpp}.
   - Phase 3: Wire deathConfirmMs/mpDropEpsilon vào config-loader.
   - Phase 4: Verify FSM call site + compile.
   - Phase 5: Update docs (ui-parameters-guide, HUONG-DAN-CAU-HINH).
   - Phase 6: Manual soak test (1 buổi farm) → ghi nhận kill rate.

## Unresolved Questions
- Soak test có cần script đo throughput tự động không, hay chỉ observation? (đề xuất: observation, YAGNI).
- Có muốn log `lastMpDropAt` timestamp ra debug file để tune `deathConfirmMs` sau này không? (đề xuất: chưa cần, đợi data thực tế).
