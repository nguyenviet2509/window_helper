# Phase 00 — Desktop Probe (HARD GATE)

## Context
- Plan: [plan.md](plan.md)
- Brainstorm: [../reports/brainstorm-260531-0846-virtual-desktop-anti-hijack.md](../reports/brainstorm-260531-0846-virtual-desktop-anti-hijack.md)
- Tham chiếu code hiện tại: [src/capture/wgc-capture.cpp](../../src/capture/wgc-capture.cpp), [src/input/send-input-backend.cpp](../../src/input/send-input-backend.cpp)

## Overview
- Priority: P0 (blocker cho toàn plan)
- Status: pending
- Mục tiêu: verify 3 assumption killer trước khi đầu tư refactor lớn.

## Key Insights
- Windows Desktop object có input queue + cursor riêng (cùng cơ chế UAC/Winlogon).
- `SetThreadDesktop` per-thread; thread không được có HWND khi đổi desktop.
- `PrintWindow` với flag `PW_RENDERFULLCONTENT` (0x2) bắt buộc cho DX windows.

## Requirements
- Functional:
  - F1: Tạo BotDesk, spawn process vào BotDesk.
  - F2: Worker thread attach BotDesk, gửi `SendInput` → verify cursor target.
  - F3: Capture pixel của window trên BotDesk từ Default desktop.
- Non-functional:
  - Probe binary đơn giản, <300 LOC, không link với main project.
  - Log đầy đủ ra console + file để paste vào docs.

## Architecture
```
desktop-probe.exe (standalone tool)
├── main() — on Default desktop
│   ├── CreateDesktopW("BotDesk_probe", DESKTOP_ALL_ACCESS)
│   ├── CreateProcessW(notepad/PT, lpDesktop="WinSta0\\BotDesk_probe")
│   ├── Spawn worker thread → SetThreadDesktop(BotDesk_probe)
│   │   └── SendInput sequence (mouse move + click + key)
│   ├── PrintWindow capture loop (5 frame, save PNG)
│   └── Log GetCursorPos của Default desktop (verify không nhảy)
└── Cleanup: CloseDesktop khi process die
```

## Related Code Files
- Create: `tools/desktop-probe/probe.cpp`
- Create: `tools/desktop-probe/CMakeLists.txt`
- Update: `tools/CMakeLists.txt` add_subdirectory
- Output: `docs/desktop-probe-results.md` (kết quả đo)

## Implementation Steps
1. Tạo dir `tools/desktop-probe/` + CMakeLists target `desktop-probe`.
2. `probe.cpp`:
   - `CreateDesktopW("BotDesk_probe", nullptr, nullptr, 0, DESKTOP_CREATEWINDOW|DESKTOP_HOOKCONTROL|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_SWITCHDESKTOP|GENERIC_READ|GENERIC_WRITE, nullptr)`.
   - Test A — Notepad target:
     - `CreateProcessW("notepad.exe", ..., STARTUPINFO{lpDesktop="WinSta0\\BotDesk_probe"}, ...)`.
     - Sleep 1s, `EnumDesktopWindows(BotDesk_probe, ...)` tìm Notepad HWND.
     - Worker thread: `SetThreadDesktop(BotDesk_probe)` → `SendInput` gõ "test123".
     - Main thread (Default): `GetCursorPos` trước/sau, in delta. **Pass nếu delta == 0.**
     - Verify Notepad nhận text qua `GetWindowTextW`.
   - Test B — PT target (chỉ chạy nếu user opt-in qua cmdline flag, dùng offline / private server):
     - CreateProcess PT với lpDesktop=BotDesk_probe.
     - Sleep 8s cho game load.
     - `EnumDesktopWindows` tìm PT HWND.
     - `PrintWindow(hwnd, hdc, PW_RENDERFULLCONTENT)` 5 lần, lưu PNG.
     - Log: kích thước window, có thay đổi pixel giữa các frame không (verify game vẫn render).
     - Worker SendInput shift+right-click → check screen capture có animation không.
3. Output report `docs/desktop-probe-results.md`:
   - Test A pass/fail.
   - Test B pass/fail + sample screenshots.
   - Cursor delta measurements.
   - Quyết định GO/NO-GO cho plan.

## Todo
- [ ] Tạo tools/desktop-probe/ skeleton + CMakeLists
- [ ] Implement Test A (Notepad cross-desktop SendInput)
- [ ] Implement Test B (PT capture + input) — opt-in
- [ ] Build + run trên máy thật
- [ ] Viết docs/desktop-probe-results.md
- [ ] Quyết định GO/NO-GO

## Success Criteria
- Test A: Notepad trên BotDesk nhận text "test123"; cursor Default delta == 0.
- Test B (nếu chạy): PrintWindow lưu được screenshot khác null; pixel có thay đổi giữa các frame (game render).
- Document đầy đủ trong `docs/desktop-probe-results.md`.

## Risk Assessment
- Risk: `CreateDesktopW` fail do quyền → mitigation: chạy non-Admin trước, fallback Admin nếu cần.
- Risk: `SetThreadDesktop` fail vì thread đã có window → đảm bảo worker thread mới, không tạo HWND.
- Risk: PT crash khi spawn vào non-default desktop → log lỗi, abandon Test B nếu fail.

## Security Considerations
- Desktop ACL: dùng `DESKTOP_ALL_ACCESS` cho process owner only.
- Không expose desktop handle ra ngoài process.

## Next Steps
- Pass → Phase 01 + 02 parallel.
- Fail → abandon plan, fallback Coexist Mode (separate brainstorm needed).
