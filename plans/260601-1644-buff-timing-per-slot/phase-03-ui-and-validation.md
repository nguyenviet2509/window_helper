# Phase 03: UI Fields + Validation

**Priority:** Medium | **Status:** pending | **Blocked by:** Phase 02

## Context
- File: [src/ui/main-window.cpp](../../src/ui/main-window.cpp) — buff slot UI block quanh line 308.

## Goal
Thêm 3 DragInt per slot, hiển thị warning nếu tổng thời gian buff > 50% `cycleDurationSec`.

## Changes

`src/ui/main-window.cpp` — trong loop `for (size_t i = 0; i < draft_.combat.buffs.size(); ++i)`:

```cpp
for (size_t i = 0; i < draft_.combat.buffs.size(); ++i) {
    auto& b = draft_.combat.buffs[i];
    ImGui::PushID((int)i);
    any |= ImGui::Checkbox("Enable", &b.enabled);
    any |= ImGui::InputScalar("VK key", ImGuiDataType_U16, &b.key);
    any |= ImGui::DragInt("Cast anim (ms)", &b.animationMs, 10, 200, 3000);
    any |= ImGui::DragInt("Right-click delay (ms)", &b.rightClickDelayMs, 5, 0, 500);
    any |= ImGui::DragInt("Gap sang buff sau (ms)", &b.postBuffGapMs, 5, 0, 1000);
    any |= ImGui::Checkbox("Right-click after", &b.rightClickAfter);
    ImGui::PopID();
    ImGui::Separator();
}

// Validation warning: tổng thời gian 1 vòng buff không nên > 50% cycle
int totalBuffMs = 0;
for (const auto& b : draft_.combat.buffs) {
    if (b.enabled) totalBuffMs += b.animationMs + b.postBuffGapMs;
}
int cycleMs = draft_.combat.cycleDurationSec * 1000;
if (cycleMs > 0 && totalBuffMs > cycleMs / 2) {
    ImGui::TextColored(ImVec4(1,0.5f,0,1),
        "Canh bao: Tong thoi gian buff (%d ms) > 50%% cycle (%d ms). Co the chong cheo voi rebuff.",
        totalBuffMs, cycleMs);
}
```

**Lưu ý:**
- Match đúng các control hiện có cho slot (giữ checkbox enable, key input, rightClickAfter checkbox — chỉ thay `castDelayMs` thành 3 control mới).
- Warning chỉ hiển thị, KHÔNG block save.

## Todo
- [ ] Đọc lại block buff UI hiện tại (line ~305-320) để giữ structure
- [ ] Thay control `castDelayMs` cũ bằng 3 DragInt mới
- [ ] Thêm validation warning block sau loop
- [ ] Build, mở app, verify UI hiển thị đúng, kéo thanh trượt → giá trị save vào draft
- [ ] Manual test full flow: chỉnh animationMs nhỏ → save → reload → giá trị giữ nguyên

## Success Criteria
- UI hiển thị 3 DragInt per slot, tooltip rõ ràng.
- Warning hiển thị khi config nguy hiểm.
- Save/reload round-trip giữ đúng giá trị.

## Risks
- Có thể quên xóa control cũ → UI hiển thị cả 2. Đọc kỹ block hiện tại trước khi edit.
- Range DragInt quá hẹp → user không chỉnh được. Default range an toàn: anim 200-3000, delay 0-500, gap 0-1000.

## Final Manual Test (5 cycle)
Sau khi cả 3 phase xong:
1. Load app, enable combat, set animationMs khác nhau per slot (vd 600/800/1000/1200).
2. Quan sát 5 cycle rebuff (5×300s = 25 phút hoặc giảm cycleDurationSec xuống 60s để test nhanh).
3. Verify: mỗi cycle có đủ 4 buff icon xuất hiện trên nhân vật, không miss.
4. Verify log: thời gian giữa các key F khớp `animationMs + postBuffGapMs`.
