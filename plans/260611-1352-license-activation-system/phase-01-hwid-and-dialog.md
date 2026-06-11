---
phase: 1
title: HWID Collector + ImGui Activation Dialog Skeleton
status: completed
priority: high
effort: 0.5d
---

# Phase 1 — HWID Collector + Activation Dialog Skeleton

## Context Links
- Brainstorm: [../reports/brainstorm-260611-1352-license-activation-system.md](../reports/brainstorm-260611-1352-license-activation-system.md)
- Main entry: [src/main.cpp](../../src/main.cpp)
- Existing UI: [src/ui/main-window.cpp](../../src/ui/main-window.cpp)

## Overview
Build HWID fingerprint module + ImGui modal dialog matching mockup. No network yet — dialog reads HWID and accepts code input, calls injected callback.

## Key Insights
- Mockup: Machine ID (8 hex), input field, Activate / Exit buttons
- HWID phải stable across reboot, khó fake bằng VM clone
- ImGui modal phải block trước khi main window hiện

## Requirements
**Functional:**
- Generate stable HWID = SHA-256(VolumeSerial(C:) || cpuid_eax1 || MachineGuid || primary_MAC)
- Expose 32-hex full + 8-hex short
- ImGui modal: hiện Machine ID short, input field activation code, 2 buttons
- Callback `OnActivate(token, hwid_full)` để layer khác xử lý

**Non-functional:**
- File <200 LOC mỗi file
- No global state — pass dependencies via ctor

## Files to Create
- `src/license/hwid-collector.h` — interface `std::string HwidFull()`, `std::string HwidShort()`
- `src/license/hwid-collector.cpp` — Win32 GetVolumeInformationW, __cpuid, RegGetValueW, GetAdaptersAddresses
- `src/ui/activation-dialog.h` — `class ActivationDialog { void Render(); void SetCallback(...); }`
- `src/ui/activation-dialog.cpp` — ImGui::OpenPopup + BeginPopupModal

## Files to Modify
- `src/CMakeLists.txt` — add new sources
- `src/main.cpp` — wire stub call (real integration in phase 5)

## Implementation Steps
1. Add `src/license/` dir, register in CMake
2. `hwid-collector.cpp`:
   - `GetVolumeInformationW(L"C:\\", ..., &serial, ...)`
   - `__cpuid(info, 1)` → take EAX+EBX
   - `RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", L"MachineGuid", RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, ...)`
   - `GetAdaptersAddresses` → first non-loopback physical MAC
   - Concat → SHA-256 (use bcrypt CNG)
   - Return hex string
3. `activation-dialog.cpp`:
   - `ImGui::OpenPopupOnItemClick` / forced open via flag
   - `ImGui::BeginPopupModal("Activation Required", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)`
   - Layout: text "Machine ID: %s" / "Please send this ID to the author to get activation code." / "Enter code:" / InputText / [Activate] [Exit]
   - Status line at bottom for error messages (set via SetStatus)
4. Build verify: `./build.ps1` hoặc cmake build, no errors

## Todo
- [x] Tạo `src/license/hwid-collector.{h,cpp}`
- [x] Tạo `src/ui/activation-dialog.{h,cpp}`
- [x] Update `src/CMakeLists.txt`
- [x] Test HWID stable qua 2 lần chạy
- [x] Test modal render đúng layout giống mockup
- [x] Compile clean

## Success Criteria
- 2 lần chạy `HwidShort()` cùng máy → cùng kết quả 8 hex
- Modal hiện đúng layout, input nhận text, click Exit → callback exit
- Build pass

## Risks
- `GetAdaptersAddresses` chậm (50-200ms) → cache kết quả sau lần đầu
- MachineGuid registry yêu cầu 64-bit view → dùng `RRF_SUBKEY_WOW6464KEY`

## Security
- Không log HWID full ra log file (chỉ short)
- HWID không phải PII nhưng vẫn cần ý thức

## Completion Notes
**Date:** 2026-06-11
**Implementation Report:** [fullstack-260611-1509-phase01-implementation.md](../reports/fullstack-260611-1509-phase01-implementation.md)
**Test Report:** [tester-260611-1523-phase1-2-validation.md](../reports/tester-260611-1523-phase1-2-validation.md)
**Code Review:** [code-review-260611-1509-phase1-2.md](../reports/code-review-260611-1509-phase1-2.md) — APPROVE_WITH_CHANGES 7.5/10
**Deviations:** None. All checkboxes completed as planned. Build clean, stable HWID generation verified, modal UI renders correctly per mockup.

## Next Steps
→ Phase 3 (WinHTTP + crypto)
