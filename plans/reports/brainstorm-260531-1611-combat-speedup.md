# Combat Speedup Tuning — Brainstorm Report

**Date:** 2026-05-31 16:11
**Topic:** Tăng tốc tìm/đánh mob mà giữ an toàn anti-detect

## Problem
- Mob find/attack chậm. User báo cả 3 triệu chứng:
  1. Click cách nhau 5s+
  2. Click trượt mob (vô tác dụng)
  3. Mất thời gian phát hiện mob chết

## Root Cause
Combat hiện tại (`combat-fsm.cpp:94-135`) **không có visual mob detection** — chỉ random 1 điểm vành khuyên r=60-180px quanh player center → shift+right-click → game tự auto-target. Bottlenecks:

| Bottleneck | Giá trị cũ | Hậu quả |
|---|---|---|
| `engagementLockMs` | 5000ms | Im lặng cứng 5s sau mỗi click, kể cả mob chết sớm |
| `repickMinDwellMs` | 2000ms | Tối thiểu 2s mới được repick |
| `windowSize` activity | 40 (2s) | 2s detect mob chết |
| Radius rộng (60-180) | — | Khả năng vành khuyên rỗng mob cao |

Best case 5s/mob, worst 15s.

## Approaches Considered

**A. Tune param (chosen, KISS):** zero code change risk, instant effect, vẫn giữ jitter+humanizer+Bezier.
**B. Early-exit lock khi mob HP bar disappear:** medium effort, cần probe UI target HP bar.
**C. Visual mob detection:** giải triệt để (cả click trượt) nhưng >1 ngày work, nhiều mob variant.

User chọn **A**, risk thấp.

## Final Tuning (applied)

| Param | Old | New | Reason |
|---|---|---|---|
| `engagementLockMs` | 5000 | **2500** | Half delay, vẫn đủ chờ mob chết |
| `engagementLockJitterMs` | 500 | **800** | Variance rộng hơn → giống người |
| `repickMinDwellMs` | 2000 | **1000** | Detect chết sớm hơn |
| `repickMaxDwellMs` | 15000 | **8000** | Force repick nhanh khi treo |
| `attackRadiusMin` | 60 | **50** | Vành khuyên gần hơn |
| `attackRadiusMax` | 180 | **140** | Density mob cao hơn |
| `windowSize` (activity) | 40 (2s) | **20 (1s)** | Death detect nhanh gấp đôi |

## Files Changed
- `src/state/game-state.h` — CombatConfig defaults
- `src/combat/combat-activity-monitor.h` — windowSize

## Expected Impact
- Gap giữa 2 attack: 5s+ → **~1.5-3s** tùy mob density
- Click trượt: GIẢM (radius hẹp) — không HẾT (cần option C)
- Anti-detect: vẫn an toàn (humanizer + Bezier + jitter 800ms còn nguyên)

## Risks
- `windowSize=20` (1s) có thể false-positive death khi mob có stun/freeze pha ngắn → bot bỏ mob còn HP. Theo dõi vài session, tăng 25-30 nếu có hiện tượng.
- Defaults chỉ áp dụng khi `config.json` chưa có — user cần delete config.json hoặc chỉnh UI để dùng giá trị mới.

## Next Steps
1. Rebuild + chạy thử 1-2 session farm để confirm cảm giác tốc độ.
2. Nếu vẫn chậm: cân nhắc option B (early-exit lock) — cần tìm vị trí target HP bar trong UI PT.
3. Nếu vẫn click trượt nhiều: option C visual mob detect — đề xuất scope riêng.

## Unresolved
- Có nên thêm UI slider cho `windowSize` activity? Hiện đang hardcode.
- PT có hiển thị target HP bar khi shift+right-click không? Cần verify để cân nhắc option B.
