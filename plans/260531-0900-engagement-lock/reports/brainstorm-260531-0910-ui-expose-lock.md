# Brainstorm Addendum — UI Expose Engagement Lock

**Date:** 2026-05-31 09:10
**Parent plan:** `plans/260531-0900-engagement-lock/`

## Trigger

User: "tôi muốn 5s đó tôi setup được trên layout cho chủ động."

→ Promote P04 from optional → required. Expose cả `engagementLockMs` và `engagementLockJitterMs` lên UI ImGui, có tooltip giải thích.

## Decision

**Approach A — 2 DragInt + tooltip**, vị trí ngay sau `repickMaxDwellMs` trong section "Đánh quái" của `main-window.cpp` (~line 245).

### Snippet

```cpp
any |= ImGui::DragInt("Khoá đánh sau shift+phải (ms)", &c.engagementLockMs, 100, 1000, 15000);
if (ImGui::IsItemHovered()) ImGui::SetTooltip(
    "Sau khi shift+chuột phải, im lặng X ms để game tự đánh mob;\nthoát sớm nếu phát hiện mob chết.");
any |= ImGui::DragInt("Dao động khoá đánh (ms)", &c.engagementLockJitterMs, 50, 0, 2000);
if (ImGui::IsItemHovered()) ImGui::SetTooltip(
    "Random hoá độ dài khoá ±jitter để né pattern detect.");
```

### Range rationale

- Lock 1000–15000ms: 1s tối thiểu để game kịp lock mob; 15s đủ lâu cho mob trâu.
- Jitter 0–2000ms: 0 = tắt jitter (debug); 2s đủ làm gãy pattern detect chu kỳ cố định.

## Alternatives rejected

- **B — Chỉ lock, ẩn jitter:** mất quyền tinh chỉnh anti-detect; YAGNI sai chỗ vì user đã muốn chủ động.
- **C — Jitter dạng %:** thêm conversion code (% ↔ ms), không match pattern existing → DRY-violation cosmetic.

## Plan updates

- `plan.md`: P04 đổi từ "(Optional)" thành required; dependency thêm P1 → P4 và P2+P4 → P3.
- `phase-04-ui-expose.md`: status + steps cập nhật tooltip; todo thêm verify hot-apply qua ConfigBus.

## Open questions

- ConfigBus có propagate `updateConfig` đến FSM ngay khi user kéo slider hay chỉ khi save? → verify trong P3.
