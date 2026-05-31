# Phase 04 — UI Expose Sliders (Required)

**Priority:** P2
**Status:** pending — required per user request 2026-05-31 09:10
**Effort:** 15m
**Depends on:** Phase 01

User cần chủ động tinh chỉnh thời gian khóa giao chiến từ UI, không phải sửa config.json thủ công.

## Overview

Expose `engagementLockMs` and `engagementLockJitterMs` as ImGui sliders in main window combat config section.

## Key Insights

- Existing peers exposed in `src/ui/main-window.cpp` around lines 244-245:
  ```cpp
  any |= ImGui::DragInt("Chờ tối thiểu khi đổi mục tiêu (ms)", &c.repickMinDwellMs, 100, 500, 10000);
  any |= ImGui::DragInt("Chờ tối đa khi đổi mục tiêu (ms)", &c.repickMaxDwellMs, 100, 1000, 60000);
  ```
- Follow same pattern + Vietnamese label convention.

## Requirements

- Two new `DragInt` controls in combat config section, adjacent to repick dwell sliders.
- Range: lock 1000–30000ms (step 100), jitter 0–5000ms (step 50).
- Edits trigger `any = true` for config save.

## Related Files

Modify:
- `src/ui/main-window.cpp`

## Implementation Steps

1. After the `repickMaxDwellMs` `DragInt` (~line 245), add:
   ```cpp
   any |= ImGui::DragInt("Khoá đánh sau shift+phải (ms)", &c.engagementLockMs, 100, 1000, 15000);
   if (ImGui::IsItemHovered()) ImGui::SetTooltip(
       "Sau khi shift+chuột phải, im lặng X ms để game tự đánh mob;\nthoát sớm nếu phát hiện mob chết.");
   any |= ImGui::DragInt("Dao động khoá đánh (ms)", &c.engagementLockJitterMs, 50, 0, 2000);
   if (ImGui::IsItemHovered()) ImGui::SetTooltip(
       "Random hoá độ dài khoá ±jitter để né pattern detect.");
   ```
2. Compile + spot-check UI renders + values persist after restart.

## Todo

- [ ] Add 2 DragInt rows với tooltip
- [ ] Compile clean
- [ ] Verify values persist to `config.json`
- [ ] Verify FSM apply ngay sau khi user kéo slider (qua ConfigBus → updateConfig)

## Success Criteria

- UI shows 2 new sliders; edits saved; FSM picks them up via `updateConfig()`.

## Risks

- Trivial. Mitigation: copy-paste from existing pattern.
