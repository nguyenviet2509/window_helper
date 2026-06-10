# Phase 05 — Manual Soak Test

## Status: pending
## Priority: medium
## Effort: 1 buổi farm (~30-60 min)

## Overview
Verify behavior thực tế: TTK tail không bị cắt, stuck escape vẫn hoạt động, detect mob chết không false-positive quá nhiều.

## Test Plan

### Setup
- Build Release.
- Config: defaults mới (6000/22000/2500/0.005).
- Map: nơi user thường farm (TTK 8-15s đã observed).
- Combat enabled, ít nhất 30 phút quan sát.

### Observations
| Aspect | Expected | Red flag |
|---|---|---|
| Mob HP cao (~15s TTK) | Bot đánh đến chết, không bỏ giữa chừng | Bot đổi mob khi mob còn HP > 20% |
| Mob HP thấp (~8s TTK) | Bot đánh xong → detect chết → repick sau ~2.5s | Bot kẹt lâu sau khi mob chết (>5s) |
| Mob stuck/immortal (rare) | Force-repick ở 22s | Kẹt vĩnh viễn, không thoát |
| Detect false-positive | Hiếm — chỉ khi user pause cast skill | Đổi mob giữa combat thường xuyên |

### Sanity checks
- Log số kill/phút trước vs sau (rough estimate, không cần script).
- Đếm số lần đổi mob "sai" (mob chưa chết đã đổi) trong 30 phút.

### Rollback criteria
Nếu detect false-positive >10% (đổi mob nhầm >3 lần trong 30 phút) → revert sang status quo, tune lại `deathConfirmMs` lên 3000 hoặc `mpDropEpsilon` xuống 0.003.

## Todo
- [ ] Build Release với code mới
- [ ] Run 30-60 phút farm
- [ ] Ghi nhận: kill rate, số lần đổi nhầm, số lần stuck
- [ ] Quyết định: keep / tune / rollback

## Success Criteria
- Kill rate ≥ status quo.
- False-positive đổi mob ≤ 1 lần / 10 phút.
- Stuck escape hoạt động đúng.

## Unresolved
- Tune `deathConfirmMs` nếu cần (sau khi có data thực tế).
- Có thêm log debug `lastMpDropAt` không? — Quyết định sau soak test.

## End of Plan
Plan complete sau khi soak test pass → archive.
