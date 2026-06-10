# Phase 04 — Compile + Update Docs

## Status: pending
## Priority: medium
## Effort: 15 min

## Overview
Build verify + đồng bộ docs với default mới.

## Build Verify
```powershell
# Adapt theo build system trong repo (cmake/msbuild). Ví dụ:
cmake --build build --config Release
```
Hoặc check `docs/build-deploy-test-guide.md` cho lệnh chính xác.

**Expect:** Build sạch, không warning mới về `CombatActivityMonitor`.

## Docs Update

### `docs/ui-parameters-guide.md`
Lines 40-41 (table mục "Đánh quái"):
```diff
- | **Chờ tối thiểu khi đổi mục tiêu (ms)** | 2000 | ... |
- | **Chờ tối đa khi đổi mục tiêu (ms)** | 15000 | ... |
+ | **Chờ tối thiểu khi đổi mục tiêu (ms)** | 6000 | ... |
+ | **Chờ tối đa khi đổi mục tiêu (ms)** | 22000 | ... |
```
Cập nhật ý nghĩa cột nếu cần (vd: mô tả tối đa = "Force pick mob khác sau N ms — escape hatch cho mob bug/immortal").

### `dist/HUONG-DAN-CAU-HINH.md`
Lines 40-41: same diff.

### (Optional) Thêm section nhỏ về detect logic
Nếu muốn user biết cơ chế MP-based detect, thêm note ngắn trong cùng doc:
> **Cơ chế detect mob chết:** Bot theo dõi MP của player. Nếu MP không tụt (cast skill) trong 2.5s → coi là mob đã chết. Tinh chỉnh: `deathConfirmMs` và `mpDropEpsilon` trong `config.json`.

(YAGNI: chỉ thêm nếu user thấy cần.)

## Implementation Steps
1. Build → verify pass.
2. Edit `docs/ui-parameters-guide.md`: bump 2 default values.
3. Edit `dist/HUONG-DAN-CAU-HINH.md`: same.

## Todo
- [ ] Build pass
- [ ] ui-parameters-guide.md updated
- [ ] HUONG-DAN-CAU-HINH.md updated

## Success Criteria
- Build pass.
- 2 docs file đồng bộ default mới (6000/22000).

## Next
Phase 05 — Soak test.
