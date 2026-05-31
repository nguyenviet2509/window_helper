# Phase 04 — Switch Hotkey + UI Polish

## Context
- Plan: [plan.md](plan.md)
- Tham chiếu: [src/ui/main-window.cpp](../../src/ui/main-window.cpp)

## Overview
- Priority: P2
- Status: pending
- Mục tiêu: hotkey global để "peek" BotDesk vài giây rồi tự switch về Default; UI hiển thị thumbnail PT periodic.

## Key Insights
- `SwitchDesktop(hdesk)` mang desktop ra foreground (hiển thị trên màn hình vật lý).
- Hotkey global: `RegisterHotKey` trên main thread UI.
- Auto switch back: spawn timer thread, sau X giây gọi `SwitchDesktop(default)`.
- Thumbnail PT: PrintWindow target trên BotDesk, downscale, hiển thị qua ImGui texture.

## Requirements
- F1: Global hotkey configurable (default `Ctrl+Alt+G`) — toggle peek.
- F2: Peek duration configurable (default 5s).
- F3: UI thumbnail PT 320x240 refresh @ 2Hz.
- F4: Status indicator: đang ở Default / BotDesk.

## Architecture
```
UI thread:
├── RegisterHotKey(WM_HOTKEY id=1, MOD_CONTROL|MOD_ALT, 'G')
├── WndProc nhận WM_HOTKEY → switch logic
│   ├── current = Default → SwitchDesktop(BotDesk) + start peek timer
│   └── current = BotDesk → SwitchDesktop(Default) (manual return)
└── PeekTimer thread: sleep N seconds → PostMessage(WM_USER_RETURN) → UI thread switch back

Thumbnail update:
└── UI thread @2Hz: PrintWindow(ptHwnd) → resize → upload to ImGui texture
    (chạy trên Default desktop, capture window trên BotDesk — verify Phase 00)
```

## Related Code Files
- Create: `src/ui/desktop-switch-controller.h`
- Create: `src/ui/desktop-switch-controller.cpp`
- Update: `src/ui/main-window.cpp` — hotkey register, thumbnail panel
- Update: `src/config/config-loader.cpp` — `hotkey.peek`, `peek.durationMs`, `thumbnail.enabled`

## Implementation Steps
1. `desktop-switch-controller.{h,cpp}`:
   ```cpp
   class DesktopSwitchController {
   public:
       void setBotDesk(HDESK desk);
       void registerHotkey(HWND owner, UINT mod, UINT vk, int id);
       void onHotkey();                  // called from WndProc
       void schedulePeekReturn(int ms);  // async return to Default
   private:
       HDESK botDesk_ = nullptr;
       HDESK default_ = nullptr;
       std::atomic<bool> onBotDesk_{false};
       std::thread peekTh_;
   };
   ```
2. WndProc của ImGui main window xử lý `WM_HOTKEY`:
   - id=1 → controller.onHotkey() → SwitchDesktop + spawn peek thread.
3. Thumbnail:
   - Static buffer cv::Mat 320x240.
   - Timer 500ms: `PrintWindow(ptHwnd)` → `cv::resize` → upload texture.
   - Render trong UI panel "Game Preview".
4. Config:
   - JSON: `"virtualDesktop": { "hotkey": "Ctrl+Alt+G", "peekDurationMs": 5000, "thumbnailEnabled": true }`.

## Todo
- [ ] DesktopSwitchController
- [ ] RegisterHotKey + WndProc dispatch
- [ ] Peek timer thread (auto return)
- [ ] Thumbnail capture + texture upload
- [ ] Config fields
- [ ] UX test: hotkey ergonomics, peek duration phù hợp

## Success Criteria
- Press hotkey → màn hình switch sang BotDesk (thấy game) → 5s sau tự về Default.
- Thumbnail update mượt, không flicker.
- Hotkey không conflict app khác.

## Risk Assessment
- Risk: `SwitchDesktop` cần `DESKTOP_SWITCHDESKTOP` access — đã include trong desktop creation.
- Risk: Default desktop cần handle ref → `OpenInputDesktop` lưu lại khi start.
- Risk: Peek thread bị join lock khi shutdown → dùng atomic flag + detached thread.

## Security Considerations
- Hotkey conflict: cho phép user customize qua config.

## Next Steps
- Phase 05: soak test toàn bộ trên throwaway account.
