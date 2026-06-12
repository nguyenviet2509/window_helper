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

Repo có sẵn 3 preset trong `CMakePresets.json` (hiện đang giữ ở `CMakePresets.json.bak` — restore khi cần):

| Preset | Generator | Triplet | Output dir | Mục đích |
|--------|-----------|---------|------------|----------|
| `default` | VS 18 2026, x64 | `x64-windows` (dynamic) | `build/` | Dev nhanh, kèm DLL OpenCV |
| `ninja` | Ninja | `x64-windows` (dynamic) | `build-ninja/` | Bypass VS, dùng từ Developer PS |
| `portable` | VS 18 2026, x64 | `x64-windows-static` + `/MT` | `build-portable/` | Single-exe static, no DLL, no VC++ Redist |

Target chính: **`WindowHelper`** — exe output có tên ngẫu nhiên `svc_<random>.exe` (chống detect; tên sinh tại configure-time, giữ nguyên giữa các lần rebuild).

### 3.1 Build dev (dynamic, nhanh)

```powershell
# Restore preset (lần đầu)
Copy-Item CMakePresets.json.bak CMakePresets.json

# Configure + build
cmake --preset default
cmake --build build --config Release --target WindowHelper
```

Output: `build\bin\Release\svc_*.exe` + DLLs (`opencv_*.dll`, `libpng16.dll`, `libwebp*.dll`, …) — phải copy kèm khi share.

Rebuild nhanh (đã configure trước):
```powershell
cmake --build build --config Release --target WindowHelper
```

Lần build đầu vcpkg pull `opencv4`, `nlohmann-json`, `imgui[dx11-binding,win32-binding]` — mất 10–30 phút.

### 3.2 Build portable (single static exe)

```powershell
cmake --preset portable
cmake --build build-portable --config Release --target WindowHelper
```

Output: `build-portable\bin\Release\svc_*.exe` duy nhất (không DLL, không cần VC++ Redist). **Lần đầu rất lâu (~15–30 phút)** vì compile OpenCV static; lần sau nhanh.

### 3.3 Build + đóng gói zip share (one-shot)

```powershell
.\package.ps1                       # build portable + tạo dist\WindowHelper.zip (license gate ON)
.\package.ps1 -NoLicense            # build bỏ license gate (cho người thân) → zip suffix '-free'
.\package.ps1 -SkipBuild            # chỉ đóng gói lại từ build-portable hiện có
.\package.ps1 -Version 1.2.3        # gắn version vào tên zip
.\package.ps1 -OutDir D:\release    # đổi output dir
```

Script tự configure preset `portable` (nếu chưa) → build target `WindowHelper` → copy `svc_*.exe` + `config.json` + `HUONG-DAN-CAU-HINH.md` vào `dist\` → zip.

**License gate (`WH_REQUIRE_LICENSE`):**
- Mặc định portable preset bật `ON` ([CMakePresets.json](../CMakePresets.json)) — exe yêu cầu activation token mới chạy.
- `-NoLicense` override thành `OFF` lúc configure → exe build ra **không có dialog kích hoạt**, dùng được luôn. Dành cho build share người thân/nội bộ.
- Đổi mode (có ↔ không license) trên cùng `build-portable/`: CMake nhận diện flag đổi → recompile incremental các file dùng `WH_REQUIRE_LICENSE` (chủ yếu [src/main.cpp](../src/main.cpp), [src/ui/tray-icon.cpp](../src/ui/tray-icon.cpp)). Không cần xoá build dir.
- ⚠ Đừng dùng chung `-NoLicense -SkipBuild`: `-SkipBuild` không re-configure/re-build, sẽ đóng gói exe đã build ở mode trước đó.
- Zip output: bản license có tên `WindowHelper-<ts>.zip`, bản không-license có tên `WindowHelper-<ts>-free.zip` để phân biệt nhanh.

### 3.4 Build target lẻ (probe / mock)

```powershell
cmake --build build --config Release --target postmessage_probe
cmake --build build --config Release --target PtMockGame
```

Output: `build\bin\Release\PostMessageProbe.exe`, `build\bin\Release\PtMockGame.exe`.

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

**Cách khuyến nghị:** chạy `.\package.ps1` → lấy `dist\WindowHelper*.zip` giao user. Bên trong:
```
WindowHelper/
├── svc_<random>.exe     # exe duy nhất, static link
├── config.json          # default config (user có thể edit)
├── HUONG-DAN-CAU-HINH.md
└── logs/                # tự tạo ở lần chạy đầu
```

**Cách thủ công (dynamic build):** copy từ `build\bin\Release\`:
```
WindowHelper/
├── svc_<random>.exe
├── config.json
├── opencv_core4.dll, opencv_imgcodecs4.dll, opencv_imgproc4.dll
├── libpng16.dll, libwebp*.dll, libsharpyuv.dll, tiff.dll, jpeg62.dll, liblzma.dll, z.dll
└── logs/
```

Không cần Admin. Không cài driver. Không cần internet runtime. Bản `portable` không cần VC++ Redist; bản `default` cần Redist 2022 nếu máy đích chưa có.

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
