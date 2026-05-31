# Phase 03 — Game Launcher (CreateProcess lpDesktop)

## Context
- Plan: [plan.md](plan.md)
- Tham chiếu: [src/ui/main-window.cpp](../../src/ui/main-window.cpp), [src/desktop/desktop-manager.cpp](../../src/desktop/desktop-manager.cpp) (Phase 02)

## Overview
- Priority: P1
- Status: pending
- Mục tiêu: từ UI helper trên Default, spawn PT vào BotDesk, lấy HWND truyền cho worker.

## Key Insights
- `CreateProcessW` với `STARTUPINFOW.lpDesktop = L"WinSta0\\BotDesk"` đẩy process con vào desktop chỉ định.
- Tìm HWND PT trên desktop: `EnumDesktopWindows(hdesk, ...)` với callback filter title/class.
- Nếu PT đã chạy sẵn (user start manual) → discover bằng `EnumDesktopWindows` không cần spawn lại.

## Requirements
- F1: UI button "Launch PT in Virtual Desktop" + textbox path tới PristonTale.exe.
- F2: Spawn process, đợi window xuất hiện (timeout 30s).
- F3: Discover existing PT trên BotDesk khi helper restart.
- F4: Lưu PT path vào config để lần sau không phải nhập.

## Architecture
```
UI (Default) ─ "Launch" ─→ GameLauncher::launch(path, hdesk)
                              ├── CreateProcessW(path, ..., lpDesktop)
                              ├── Poll EnumDesktopWindows(hdesk) tìm "Priston Tale*"
                              ├── Return HWND or timeout
                              └── shared.gameHwnd.store(hwnd)
Worker thread ─ reads shared.gameHwnd ─→ setTarget on backend + capture
```

## Related Code Files
- Create: `src/launcher/game-launcher.h`
- Create: `src/launcher/game-launcher.cpp`
- Update: `src/state/shared-state.h` — thêm `std::atomic<HWND> gameHwnd`
- Update: `src/ui/main-window.cpp` — UI button + textbox
- Update: `src/config/config-loader.cpp` — field `gameExePath`, `virtualDesktopName`
- Update: `src/worker/worker-thread.cpp` — chờ `gameHwnd != nullptr` trước khi loop

## Implementation Steps
1. `game-launcher.{h,cpp}`:
   ```cpp
   class GameLauncher {
   public:
       struct LaunchResult { bool ok; HWND hwnd; DWORD pid; std::string error; };
       LaunchResult launch(const std::wstring& exePath, HDESK desk, const std::wstring& desktopName, int timeoutMs);
       LaunchResult discover(HDESK desk, const std::wstring& titlePattern);
   };
   ```
2. `launch`:
   - `STARTUPINFOW si{}; si.lpDesktop = const_cast<LPWSTR>(fullPath.c_str())`.
   - `CreateProcessW(exePath, nullptr, ..., &si, &pi)`.
   - Poll loop max `timeoutMs`:
     - `EnumDesktopWindows(desk, cb, &state)` với cb match title "Priston Tale*" AND pid == pi.dwProcessId.
     - Sleep 200ms giữa các poll.
   - Return HWND khi tìm thấy.
3. `discover`:
   - Tương tự nhưng không spawn, chỉ enum + match title.
   - Dùng khi helper restart và PT đã đang chạy.
4. UI changes:
   - Trong `MainWindow::render()`:
     - Section "Virtual Desktop Mode" (collapsible).
     - Textbox: PT exe path (load từ config).
     - Button: "Create BotDesk + Launch PT".
     - Status: "BotDesk: created/not", "PT HWND: 0x..../not found".
5. Worker startup:
   - Loop chờ `shared.gameHwnd.load() != nullptr` (sleep 100ms).
   - Khi có → init capture + backend, vào main loop.

## Todo
- [ ] GameLauncher class
- [ ] EnumDesktopWindows helper với pid filter
- [ ] UI button + textbox + status
- [ ] Config field gameExePath
- [ ] Worker chờ gameHwnd
- [ ] Smoke test: spawn notepad vào BotDesk → tìm HWND
- [ ] Real test: spawn PT vào BotDesk → tìm HWND PT

## Success Criteria
- Click button → BotDesk tạo + PT chạy trong đó + HWND trả về <30s.
- Helper restart → discover PT đang chạy không cần spawn lại.
- Config persist exe path.

## Risk Assessment
- Risk: PT cần CWD đúng để load asset → set `lpCurrentDirectory` = thư mục chứa exe.
- Risk: PT có launcher 2-stage (Launcher.exe → Client.exe), HWND của Client mới quan trọng → enum theo title pattern, không theo pid (Client là pid khác).
- Risk: BotDesk không có Explorer → một số game crash khi không có shell process. Mitigation: nếu fail, document workaround.

## Security Considerations
- Path traversal: validate exe path nằm trong filesystem hợp lệ trước khi CreateProcess.

## Next Steps
- Phase 04: hotkey switch + UI polish để user "peek" game.
