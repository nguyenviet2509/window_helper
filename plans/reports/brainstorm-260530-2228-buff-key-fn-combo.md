# Brainstorm — Buff key picker: VK code → F1–F12 combo

## Problem
UI Buff hiện hiển thị mã VK thô (113, 114, 115, 116) trong field "Mã phím (VK)". End-user phải biết VK code mới chỉnh được. Cần đổi sang chọn F1–F12 trực quan.

## Context
- File: [src/ui/main-window.cpp:237-253](src/ui/main-window.cpp#L237-L253)
- Widget hiện tại: `ImGui::DragInt("Mã phím (VK)", &vk, 1, 0x70, 0x7B)` — đã clamp F1 (0x70=112) đến F12 (0x7B=123).
- Config storage: `b.key` là `WORD` lưu VK code — giữ nguyên format JSON.

## Approaches evaluated

| | A. Combo F1–F12 | B. Slider + custom label | C. InputText parse "F1" |
|---|---|---|---|
| KISS | ✅ | ⚠️ | ❌ |
| Invalid input | impossible | possible | possible (typo) |
| LOC change | ~5 | ~10 | ~25 |
| UX | click chọn | kéo | gõ |

## Decision: A — Combo dropdown

Lý do: range đã clamp F1–F12 sẵn, combo loại bỏ hoàn toàn invalid input, code ngắn nhất, không đổi schema config.

## Implementation sketch

Tại [main-window.cpp:244-246](src/ui/main-window.cpp#L244-L246):
```cpp
static const char* kFnNames = "F1\0F2\0F3\0F4\0F5\0F6\0F7\0F8\0F9\0F10\0F11\0F12\0";
int idx = std::clamp((int)b.key - 0x70, 0, 11);
if (ImGui::Combo("Mã phím", &idx, kFnNames)) {
    b.key = static_cast<WORD>(0x70 + idx);
    any = true;
}
```

## Risks
- Config cũ có VK ngoài F1–F12: clamp idx sẽ ép về F1, có thể "im lặng" thay đổi behavior. → Mitigation: range cũ đã clamp 0x70–0x7B, không có giá trị ngoài range trong config thực tế.

## Success criteria
- Combo hiển thị "F1".."F12" thay vì số.
- Chọn F2 → `b.key == 0x71` lưu vào config.json.
- Compile clean (CMake build).

## Next steps
- Apply patch trực tiếp (single-file, ~5 LOC, không cần /ck:plan).

## Unresolved
- Tương lai mở rộng ngoài F-keys? Hiện scope: KHÔNG.
