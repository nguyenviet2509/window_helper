# Brainstorm — Có cần xoay camera khi tool chạy?

**Date:** 2026-05-31 09:22
**Status:** Decided — KHÔNG xoay, không wander.

## Câu hỏi user

"Hiện tại khi trong game có cần xoay camera để khi tool chạy click chuột theo range dễ tìm mob hơn không? Hay chỉ cần giữ nguyên màn hình không quay?"

## Quyết định

**Không xoay camera. Giữ màn hình nguyên trạng.**

## Lý do

1. PT camera fixed-angle (4 góc Insert/Delete) — xoay = đổi viewport, KHÔNG đổi world position của player. Mob spawn theo world coords.
2. `AttackSweep::pickAttackPosition()` đã sample uniform 360° quanh player center. Annulus đã cover tất cả hướng screen-space.
3. Shift+right-click target theo world-proximity, không cần thấy mob trên screen.
4. Thêm key Insert/Delete = thêm input event, thêm timing logic, tăng bề mặt anti-detect — lợi ích thực ≈ 0.

## Alternative đã cân nhắc

**Wander/relocate**: khi activity_ idle quá lâu, tự click left point xa để character đi bộ qua farming spot mới.
→ User quyết định **không cần** (spot hiện tại đủ mob).

## Action

Không có code change. Chỉ là quyết định strategy.

Nếu sau này spot hết mob thường xuyên → revisit wander feature.

## Open questions

Không.
