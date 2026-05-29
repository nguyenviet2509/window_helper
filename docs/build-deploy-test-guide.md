# WindowHelper — Build, Deploy & Test Guide

## 1. Yêu cầu

| Item | Min |
|------|-----|
| OS | Windows 10 1809+ / Windows 11 |
| Visual Studio | 2022 + workload **Desktop development with C++** (MSVC v143, Windows 10/11 SDK, CMake tools, C++/WinRT) |
| vcpkg | latest (with `VCPKG_ROOT` env var set) |
| Disk | 10 GB |
| Note | Run game offline / private server. Probe (Phase 0) requires `WM_KEY*` background acceptance — official PT with anti-cheat is OUT OF SCOPE. |

## 2. Setup môi trường

```powershell
# vcpkg
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\vcpkg", "User")
# logout/login để env var lan tới session mới
```

Mở **Developer PowerShell for VS 2022** rồi vào source root.

## 3. Build

### 3.1 Build mọi exe

```powershell
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Output:
- `build\bin\Release\WindowHelper.exe` — tool chính
- `build\bin\Release\PtMockGame.exe` — mock target (Phase 1 test)
- `build\bin\Release\PostMessageProbe.exe` — Phase 0 probe

Lần build đầu vcpkg sẽ pull `opencv4`, `nlohmann-json`, `imgui[dx11-binding,win32-binding]` — mất 10–30 phút.

### 3.2 Build target lẻ

```powershell
cmake --build build --config Release --target WindowHelper
cmake --build build --config Release --target postmessage_probe
cmake --build build --config Release --target PtMockGame
```

## 4. Phase 0 — Probe (BẮT BUỘC trước khi tin tưởng PostMessage)

```powershell
# 1. Mở Priston Tale (offline / private server)
.\build\bin\Release\PostMessageProbe.exe
# 2. Khi probe in "Alt-tab to Notepad", alt-tab ngay
# 3. Quan sát game:
#    - F2 cast skill?     -> keys OK
#    - Right-click (400,300) đi attack hoặc move? -> mouse OK
#    - SHIFT+right-click attack-in-place? -> SHIFT modifier OK
```

Điền kết quả vào [docs/input-backend-decision.md](input-backend-decision.md).
- Cả 3 PASS → giữ `PostMessageBackend` (mặc định)
- T2/T3 FAIL → đổi backend sang `SendInputBackend` trong [src/main.cpp](../src/main.cpp) (đổi một dòng `PostMessageBackend backend;` thành `SendInputBackend backend;`)

## 5. Test với mock (KHÔNG cần PT thật)

```powershell
# Terminal 1
.\build\bin\Release\PtMockGame.exe
# Terminal 2
.\build\bin\Release\WindowHelper.exe
```

UI Settings panel mở lên. AUTO mặc định ON. Quy trình test:

| Scenario | Steps | Pass |
|----------|-------|------|
| HP pot | Kéo trackbar HP < 55% trong PtMockGame | DebugView log `[dispatch] HP-pot` + `[mock] WM_KEYDOWN vk=0x70` |
| MP pot | Kéo MP < 40% | `[dispatch] MP-pot` + `[mock] WM_KEYDOWN vk=0x71` |
| Recall | HP < 18% giữ ≥ 3s | `[dispatch] Recall` + `vk=0x7B` |
| Combat buff | AUTO=ON → quan sát F2/F3/F4/F5 + right-click trong ~2.5s | mock log dãy 4 buff keys + 4 right-clicks |
| Combat attack | Sau buff → F1 (arming) → SHIFT+right-click sweep | mock log `vk=0x70` (F1) rồi liên tục `WM_RBUTTONDOWN ... wp=...` với MK_SHIFT |
| Mob death | Tắt thay đổi HP/MP của mock trong 2s | repick mới trong DebugView |
| Gate minimize | Minimize PtMockGame | logger `[dispatch]` ngừng, không có `[mock] WM_KEYDOWN` |
| Gate session lock | Win+L → unlock | logger `Session locked` + `Session unlocked`; input dừng trong khoảng lock |
| Hotkey F8 | Minimize WindowHelper, bấm F8 toàn cục | log `AUTO toggled -> OFF/ON` |
| Tray menu | Click chuột phải tray icon → Toggle / Show / Exit | menu trả lời tương ứng |
| Single instance | Run WindowHelper lần 2 | window cũ bật lên, instance 2 thoát ngay |

DebugView (sysinternals) hoặc đọc `logs/WindowHelper.log` (rolling 5 MB × 5).

## 6. Replay video (Phase 6 — chưa implement)

`FileReplaySource` được spec ở Phase 5 nhưng deferred. Roadmap:
1. Thêm `src/capture/file-replay-source.cpp` dùng `cv::VideoCapture` đọc MP4
2. Pace 20 Hz (`Sleep(50)` giữa frames), loop khi hết
3. Switch trong `main.cpp`: nếu config có `advanced.capture.backend == "replay"` → instantiate `FileReplaySource` thay cho `WgcCapture`

## 7. Deploy portable

Copy thư mục output sang USB / máy đích:
```
WindowHelper/
├── WindowHelper.exe
├── config.json          # tự sinh ở lần chạy đầu
├── logs/                # tự tạo
└── (DLLs runtime nếu vcpkg dynamic link — copy `*.dll` từ build\bin\Release)
```

Không cần Admin. Không cài driver. Không cần internet runtime.

## 8. Troubleshooting

| Triệu chứng | Nguyên nhân thường gặp | Fix |
|-------------|----------------------|-----|
| `cmake configure` lỗi `Could NOT find OpenCV` | `VCPKG_ROOT` chưa set hoặc terminal chưa reload env | Đóng terminal, mở lại sau khi set env var |
| Build lỗi `imgui_impl_dx11.h` not found | vcpkg port `imgui` chưa enable feature `dx11-binding` | `vcpkg install imgui[dx11-binding,win32-binding]:x64-windows` hoặc xóa `vcpkg_installed/` + build lại |
| `WGC capture failed` | Windows < 1809 hoặc GPU driver outdated | Update Windows + driver, hoặc fallback DXGI Duplication (không có trong MVP) |
| Không thấy `[mock] WM_KEYDOWN` khi HP < 30 | Mock chưa foreground / vision ROI sai | Verify mock window title đúng "Priston Tale"; check `[vision]` log có HP value đúng không |
| Build cảnh báo nhiều về WinRT | Warnings W4 + WinRT headers — vô hại | Có thể giảm xuống `/W3` trong root CMakeLists nếu khó chịu |
| Tray icon biến mất sau crash | Win Explorer chưa refresh | Hover qua tray bar hoặc restart Explorer |

## 9. Architecture cheat-sheet

```
                       +-----------------+
                       |  Game window    |
                       |  (Priston Tale  |
                       |   / PtMockGame) |
                       +--------+--------+
                                |
                  WGC capture   |   PostMessage / SendInput
                                v
+-----------+    Frame    +-----------+   InputCmd   +-----------+
| WgcCapture| ----------->| Vision    | -----------> | InputSched|
+-----------+             | Pipeline  |              +-----+-----+
                          +-----+-----+                    |
                                |                          |
                                v                          v
                         +------+-------+          +-------+------+
                         | Action       |          | OutputGate   |
                         | Dispatcher   |          | + Humanizer  |
                         |  - PotEval   |          | + CaptureFsm |
                         |  - CombatFsm |          +--------------+
                         +--------------+
                                ^
                                | snapshot
                         +------+-------+
                         | ConfigBus    |<---- ImGui MainWindow (edit)
                         +--------------+
```

Modules:
- `capture/` — WGC frame source + interface
- `vision/` — HP/MP/SP bar detector (waterline algo)
- `core/` — humanizer, output-gate, capture-health-fsm, logger
- `input/` — PostMessage / SendInput backends + priority scheduler
- `combat/` — pot-evaluator, combat-activity-monitor, combat-fsm, buff-sequencer, attack-sweep
- `dispatch/` — action-dispatcher (P0..P4 priority ladder)
- `config/` — JSON loader + atomic snapshot bus
- `ui/` — ImGui+DX11 settings, tray, hotkey

## 10. Status snapshot (2026-05-29)

| Phase | Status | Notes |
|-------|--------|-------|
| 0 PostMessage probe | code-ready | needs user to run on real PT |
| 1 Capture + Vision + Mock | code-ready | full impl |
| 2 Input + Humanizer + Gate | code-ready | PostMessage primary, SendInput fallback |
| 3 Dispatcher + Combat FSM | code-ready | pot + buff + sweep |
| 4 Config + UI | code-ready | minimal ImGui panel; full-screen Calibrate overlay deferred |
| 5 Logger + Tray + Hotkey | code-ready | app.ico binary not committed; ICON line commented in version.rc |
| 6 Integration test | manual | mock-based scenarios in §5 |
| 7 Soak test | manual | run AUTO 8h, monitor `logs/` + Task Manager RSS |
| 8 This document | done | |

## 11. Known deferred items

- DXGI Duplication fallback (WGC only)
- Template-match mob detection (stationary sweep only)
- Multi-profile config (single profile only)
- Full-screen Calibrate overlay (edit `config.json` manually)
- FileReplaySource (Phase 6 testing helper)
- Code signing, randomised binary name
- ICO asset (placeholder default app icon)
