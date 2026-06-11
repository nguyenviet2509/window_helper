---
phase: 5
title: Integrate main.cpp Gate + Tray Status + UX Polish
status: pending
priority: medium
effort: 0.5d
depends_on: [phase-04]
---

# Phase 5 — Integration & Polish

## Context Links
- Brainstorm: [../reports/brainstorm-260611-1352-license-activation-system.md](../reports/brainstorm-260611-1352-license-activation-system.md)
- Main: [src/main.cpp](../../src/main.cpp)
- Tray: [src/ui/tray-icon.cpp](../../src/ui/tray-icon.cpp)

## Overview
Wire LicenseManager vào main.cpp BEFORE bất cứ heavy init nào (capture, vision, scheduler). Add tray menu showing license status. Polish UX (Copy Machine ID button, error messages tiếng Việt).

## Key Insights
- License gate phải chạy SAU khi ImGui+DX11 init (cần render dialog) nhưng TRƯỚC khi spawn capture/vision threads (tránh phí resource nếu user exit)
- Tray menu hiện trạng thái: "License: valid (exp: YYYY-MM-DD)" hoặc "offline grace 12h left"
- Copy button cho Machine ID rất quan trọng UX (user gửi qua chat/telegram)

## Requirements
**Functional:**
- Main flow: init window+ImGui → LicenseManager::Bootstrap → branch
- ENTER_MAIN: tiếp tục init pipeline như cũ
- SHOW_DIALOG: render-only loop với ActivationDialog, exit hoặc activate
- EXIT: PostQuitMessage
- Tray menu item "License Info" → popup dialog với details
- Toast khi periodic verify fail nhiều lần liên tiếp

**Non-functional:**
- Không phá vỡ flow hiện có
- Code main.cpp tăng <50 LOC, tách helper nếu cần

## Files to Modify
- `src/main.cpp` — insert license gate sau ImGui init
- `src/ui/tray-icon.cpp` — add "License Info" menu item
- `src/ui/main-window.cpp` — add license status indicator góc bottom-right (optional)

## Files to Create
- `src/ui/license-info-dialog.h` / `.cpp` — read-only dialog showing status (~80 LOC)

## Implementation Steps

### 5.1 Main.cpp wiring
```cpp
// after ImGui+DX11 init, before vision pipeline init:
LicenseManager license{license_client, hwid_collector, license_cache};
auto bootstrap = license.Bootstrap();

if (bootstrap == BootstrapResult::SHOW_DIALOG) {
    ActivationDialog dlg{license_client, hwid_collector};
    bool done = false;
    dlg.SetOnActivated([&]{ done = true; });
    dlg.SetOnExit([&]{ PostQuitMessage(0); });
    while (!done && PumpMessages()) {
        RenderImGuiFrame([&]{ dlg.Render(); });
    }
    if (!done) return 0;  // user clicked Exit
}
if (bootstrap == BootstrapResult::EXIT) return 0;

license.StartPeriodicVerify();
license.SetOnLicenseLost([&](auto reason){ /* set atomic flag for main loop toast */ });

// ... existing init: capture, vision, scheduler ...
```

### 5.2 Copy Machine ID button
- ImGui::SameLine + Button "Copy" → `OpenClipboard` + `SetClipboardData(CF_UNICODETEXT, ...)`
- Visual feedback: temporary status "Copied!"

### 5.3 Vietnamese error messages
- Map server error codes:
  - `INVALID_TOKEN` → "Mã không hợp lệ"
  - `MACHINE_MISMATCH` → "Mã đã được dùng cho máy khác. Liên hệ admin để reset."
  - `REVOKED` → "Mã đã bị thu hồi"
  - `EXPIRED` → "Mã đã hết hạn"
  - `NETWORK_ERROR` → "Không kết nối được server. Kiểm tra mạng."

### 5.4 Tray menu
- Add menu item before existing items: "License Info..."
- Click → show LicenseInfoDialog (modal showing token mask, machine_id_short, expires, last_verified, grace remaining)

### 5.5 Status indicator (optional, time-permitting)
- Bottom-right corner main window: small green dot "Licensed ✓" hoặc orange "Offline grace 8h"

## Todo
- [ ] main.cpp insert gate, manual run test happy path
- [ ] Exit button trong dialog → app quits clean
- [ ] Copy Machine ID button hoạt động
- [ ] Vietnamese error map
- [ ] Tray "License Info" menu + dialog
- [ ] Manual test full flow on clean machine (no cache):
  - First run → dialog → enter wrong token → error
  - Enter right token → main UI loads, cache saved
  - Restart → no dialog
  - Admin revoke → wait 6h periodic OR force restart → blocked
  - Disconnect network → restart within 48h → still works
  - Disconnect network → simulate >48h → blocked
- [ ] Package new build via package.ps1 → distribute test
- [ ] Update `dist/README.txt` with activation instructions for end users

## Success Criteria
- All manual test cases pass
- No regression in existing features
- UX cho user VN rõ ràng, không cần đọc tech doc
- README hướng dẫn user cách lấy Machine ID + xin activation code

## Risks
- Insert gate sai vị trí → có thể phải khởi tạo lại DX11 → test kỹ
- Tray menu render trên thread khác ImGui → cẩn thận thread-safety khi popup dialog

## Security
- Final review: không có code path nào skip license check
- Build release, test với debug build disabled symbol logging

## Next Steps
- Ship + monitor server events log để phát hiện share attempts
- Future: dashboard tab analytics (top revoked, suspicious activate patterns)
