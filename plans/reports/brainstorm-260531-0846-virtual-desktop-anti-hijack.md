# Brainstorm: Virtual Desktop Mode — Auto không chiếm chuột

**Date:** 2026-05-31 08:46 (Asia/Saigon)
**Branch:** master
**Status:** Design approved by user, awaiting plan decision

## Problem Statement

Tool hiện tại dùng `SendInputBackend` (foreground required) → mỗi tick attack-sweep kéo cursor thật về tâm game + click. Hệ quả:
- User không thể vừa farm vừa lướt web/làm việc cùng máy.
- Alt-Tab sang app khác → game mất foreground → tool fire nhưng game ignore (đa số) HOẶC click landed vào app phía trên (rủi ro).

Context bổ sung từ discovery:
- Account **chính**, server **official + XignCode3** anti-cheat.
- Hardware: Win10/11 Pro, ≥16GB RAM.
- Có throwaway account để soak test.
- PT chạy bình thường khi Alt-Tab (không pause khi unfocused).
- User chịu 1-2 tuần refactor + 1 tuần soak test.

## Evaluated Approaches

| # | Approach | Chiếm chuột? | Inject? | XGC risk | Verdict |
|---|---|---|---|---|---|
| 1 | DLL injection + hook PT internals | ❌ | ✅ | 🔴 Cao | Reject — XGC kernel scan |
| 2 | Hyper-V GPU-PV (hardened) | ❌ | ❌ | 🟡 TB | Reject cho main acc — VM detect lâu đời |
| 3 | Sandboxie | ❌ | ❌ | 🔴 Cao | Reject — có signature |
| 4 | PostMessage/SendMessage background | ❌ | ❌ | 🟢 Thấp | Probe cũ đã fail — PT/XGC filter WndProc |
| 5 | SendInput status quo + auto-pause foreground | ✅ | ❌ | 🟢 Thấp | Fallback nếu approach chính fail |
| 6 | **Separate Desktop (`CreateDesktop`) — "Virtual Desktop Mode"** | ❌ | ❌ | 🟡 TB (chưa rõ, educated guess thấp) | **CHỌN** |
| 7 | Máy thứ 2 vật lý | ❌ | ❌ | 🟢 0 | Chuẩn nhất nhưng cần hardware riêng |

## Final Solution — Virtual Desktop Mode

### Kiến trúc

```
window_helper.exe (1 process, 2 threads)
├── [UI Thread]      desktop = WinSta0\Default
│   ├── ImGui main window, tray, hotkey listener
│   └── Real-time HP/MP gauge + log mirror từ worker
├── [Worker Thread]  desktop = WinSta0\BotDesk (SetThreadDesktop)
│   ├── Vision capture (PrintWindow + PW_RENDERFULLCONTENT)
│   ├── Input scheduler (SendInput → BotDesk queue → cursor #2)
│   └── Combat FSM, pot evaluator
└── IPC: shared atomic state + lock-free SPSC queue

Game process:
└── PristonTale.exe — CreateProcess(STARTUPINFO.lpDesktop = "WinSta0\\BotDesk")
```

### Nguyên lý

- Windows Desktop object có input queue + cursor + clipboard độc lập (cùng cơ chế Winlogon/UAC dùng).
- `SetThreadDesktop(BotDesk)` ở worker → mọi `SendInput` từ thread đó chỉ ảnh hưởng cursor BotDesk.
- User cursor #1 trên Default tuyệt đối không bị động.

### User flow

1. Mở helper trên Default desktop (như hiện tại).
2. Click "Launch PT in Virtual Desktop" → spawn PT vào BotDesk.
3. Worker thread attach BotDesk, vision + combat chạy.
4. UI mirror trạng thái về Default.
5. Hotkey toggle `SwitchDesktop(BotDesk)` ~5s để peek game, auto switch lại.

## Implementation Plan (high-level)

| Phase | Mô tả | Effort |
|---|---|---|
| **0 — Spike/probe** ⚠️ blocker | `tools/desktop-probe.exe`: test cross-desktop SendInput, PrintWindow capture, PT render trên inactive desktop | 1-2 ngày |
| 1 — PrintWindow capture | Thêm `print-window-frame-source.cpp` song song WGC; config flag | 2 ngày |
| 2 — Desktop manager + worker split | `src/desktop/desktop-manager.cpp`; refactor main loop thành 2 thread | 2 ngày |
| 3 — Game launcher | UI button + CreateProcess với lpDesktop; discover existing PT instance | 1 ngày |
| 4 — Switch hotkey + polish | Global hotkey, peek timer, UX | 1 ngày |
| 5 — Soak test throwaway acc | ≥1 tuần farm 24/7, monitor XGC reaction | 1 tuần (passive) |

Code delta ước tính: ~400 LOC + refactor split-thread.

## Risk Register

| Risk | Severity | Mitigation |
|---|---|---|
| XGC detect non-default desktop | 🟡 | Soak test throwaway 1 tuần trước khi turn on main |
| PrintWindow chậm hơn WGC | 🟢 | 30ms vs 5ms, 20Hz cần 50ms/frame → vẫn fit |
| PT pause khi BotDesk inactive | 🟢 (đã confirm chạy ok Alt-Tab) | Backup: gửi WM_ACTIVATEAPP giả qua PostMessage in-queue |
| SetThreadDesktop fail nếu thread có HWND | 🟢 | Worker thread không tạo window — design đảm bảo |
| BotDesk leak khi crash | 🟢 | OS auto-cleanup khi process die |
| User confused vì không thấy game | 🟢 | UI gauge + periodic screenshot preview |

## Success Criteria

- Phase 0 probe pass: cursor Default không nhảy khi worker SendInput; PrintWindow đọc được pixel PT trên BotDesk.
- Phase 5 soak: throwaway account farm ≥7 ngày liên tục không bị flag.
- End-to-end: user lướt web bình thường trên Default, tool farm trên BotDesk.

## Next Steps

- Quyết định: tạo plan formal qua `/ck:plan` để chia phase với todo chi tiết?
- Nếu approve plan → Phase 0 spike sẽ là deliverable đầu tiên.

## Unresolved Questions

1. XGC version cụ thể trên server bạn chơi — có data community nào về reaction với non-default desktop chưa?
2. WGC có thực sự không capture được cross-desktop, hay chỉ giới hạn trên 1 số version Windows? Cần đo trong Phase 0; nếu WGC vẫn work thì skip Phase 1.
3. UI design: hiện game qua periodic screenshot preview trong helper UI có cần thiết không, hay chỉ hotkey peek là đủ?
