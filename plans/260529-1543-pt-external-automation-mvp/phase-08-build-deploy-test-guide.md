# Phase 8 — Build, Deploy & Test Simulation Guide

**Est:** 0.5 ngày (documentation only)
**Priority:** P1 — onboarding/user-facing docs
**Status:** pending
**Depends:** Phase 7 (tool đã ổn định)

## Mục Tiêu
Tạo tài liệu hướng dẫn end-to-end để:
1. Setup môi trường build từ source
2. Build `WindowHelper.exe` từ Visual Studio 2022
3. Deploy + chạy lần đầu (calibrate)
4. Test giả lập qua `PtMockGame.exe` + replay video — KHÔNG cần mở PT thật

## Files Sẽ Tạo

```
docs/
├── build-from-source.md           # setup VS2022 + vcpkg + build
├── deploy-and-run.md              # cài đặt portable + first-run flow
├── test-with-mock.md              # hướng dẫn test giả lập
├── test-with-replay-video.md      # hướng dẫn replay frame thật
├── troubleshooting.md             # common issues + fix
├── user-manual.md                 # cách dùng UI hằng ngày
└── architecture-overview.md       # tóm tắt kiến trúc cho dev
```

LOC: documentation only, không code.

## Implementation

### 8.1 `docs/build-from-source.md`

Nội dung:
1. **Yêu cầu hệ thống**
   - Windows 10 1809+ hoặc Windows 11
   - 8GB RAM, 10GB ổ đĩa
   - Internet (lần đầu để vcpkg pull dependencies)

2. **Cài Visual Studio 2022**
   - Tải VS 2022 Community (free) từ visualstudio.microsoft.com
   - Workload: *Desktop development with C++*
   - Components: MSVC v143, Windows 10/11 SDK (latest), CMake tools for Windows, C++/WinRT (nếu có)

3. **Cài vcpkg**
   ```powershell
   git clone https://github.com/microsoft/vcpkg C:\vcpkg
   C:\vcpkg\bootstrap-vcpkg.bat
   C:\vcpkg\vcpkg integrate install
   [Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\vcpkg", "User")
   ```
   Logout/login để env var có hiệu lực.

4. **Clone source**
   ```powershell
   git clone <repo-url> d:\Vietnt\Game\anonymous
   cd d:\Vietnt\Game\anonymous
   ```

5. **Mở project trong VS 2022**
   - *File → Open → Folder* → chọn `d:\Vietnt\Game\anonymous`
   - VS tự nhận `CMakeLists.txt`
   - Chờ VS configure CMake (5–15 phút lần đầu vì vcpkg pull OpenCV + ImGui + nlohmann_json)

6. **Verify CMakeSettings.json**
   - Config: `x64-Release`
   - Toolchain: `${env.VCPKG_ROOT}\scripts\buildsystems\vcpkg.cmake`

7. **Build**
   - *Build → Build All* (Ctrl+Shift+B)
   - Output:
     - `out/build/x64-Release/WindowHelper.exe` (~5–8 MB)
     - `out/build/x64-Release/PtMockGame.exe` (~1 MB)
     - `out/build/x64-Release/postmessage_probe.exe` (~100 KB)

8. **Troubleshoot build fails**
   - vcpkg pull fail → check internet, retry `vcpkg install opencv4 nlohmann-json imgui[dx11-binding,win32-binding]` manual
   - Missing Win SDK → install latest qua VS Installer
   - CMake configure fail → check VCPKG_ROOT env var đúng path

### 8.2 `docs/deploy-and-run.md`

Nội dung:
1. **Cấu trúc deploy portable**
   ```
   WindowHelper/
   ├── WindowHelper.exe
   ├── config.json                  (sẽ auto-tạo lần đầu)
   ├── assets/
   │   ├── replays/                 (optional, cho test mode)
   │   └── icon/                    (đã embed trong exe)
   └── logs/                        (auto-tạo)
   ```

2. **Copy ra folder portable**
   - Copy `WindowHelper.exe` từ `out/build/x64-Release/` ra folder mới (vd `D:\Tools\WindowHelper\`)
   - KHÔNG cần installer

3. **Yêu cầu Windows**
   - VC++ Redistributable 2022 x64 (hầu hết Win10/11 có sẵn). Nếu thiếu: `vc_redist.x64.exe` từ Microsoft.

4. **First run**
   - Double-click `WindowHelper.exe`
   - Lần đầu sẽ tạo `config.json` mặc định
   - SmartScreen có thể cảnh báo "Windows protected your PC" → bấm "More info" → "Run anyway" (chưa ký số)
   - UI mở ra, system tray icon xuất hiện

5. **Calibrate đầu tiên (BẮT BUỘC)**
   - Mở Priston Tale 800×600 windowed mode (`PristonTale.exe -window`)
   - Trong WindowHelper UI, bấm "Attach"
   - Mở section "Calibrate", bấm "Calibrate HP"
   - Overlay xuất hiện → drag rectangle quanh HP bar trong game → confirm
   - Lặp lại cho MP, SP
   - Verify preview waterline detect đúng

6. **Bật AUTO**
   - Set thresholds HP/MP/SP, phím pot, key buff/attack
   - Bấm AUTO master toggle (hoặc F8 hotkey)
   - Tool bắt đầu auto-pot + buff cycle + attack sweep

7. **Auto-start cùng Windows (optional)**
   - Nhấn `Win+R` → `shell:startup` → copy shortcut `WindowHelper.exe` vào

8. **Uninstall**
   - Xóa folder. Xong. Không đụng registry.

### 8.3 `docs/test-with-mock.md`

Nội dung:
1. **Mục đích**
   - Test toàn bộ tool mà KHÔNG cần PT
   - Verify pipeline detect + input + FSM
   - An toàn 100% không lộ tài khoản

2. **Chuẩn bị**
   - Build cả `WindowHelper.exe` + `PtMockGame.exe` (xem 8.1)
   - Copy cả 2 exe ra cùng folder test

3. **Quy trình test**
   - Chạy `PtMockGame.exe` (window 800×600 hiện 3 bar HP/SP/MP + 3 slider)
   - Chạy `WindowHelper.exe`
   - Trong UI, bấm "Attach" → tool tự tìm window `"Priston Tale"` (mock dùng title này)
   - Calibrate HP/SP/MP qua UI như production
   - Bật AUTO

4. **Test scenarios (11 TC)**

   | TC | Action | Expected |
   |---|---|---|
   | TC-01 Hồi HP | Kéo slider HP xuống 50% | Mock log "KEY 1" < 200ms |
   | TC-02 HP Emergency | Slider HP 35% (dưới emergency 40%) | KEY 1 ngay với jitter < 8ms |
   | TC-03 Buff Cycle | Bật AUTO, 4 buff enabled | Log F2→RBUTTON→F3→RBUTTON→F4→RBUTTON→F5→RBUTTON→F1 |
   | TC-04 Attack Sweep | Sau ARMING | SHIFT+RBUTTON tại positions trong annulus quanh center |
   | TC-05 Mob Death | Toggle "Mob alive" OFF | Sau 2s tool repick (click mới) |
   | TC-06 Recall | HP 12% giữ 4s | KEY F12 gửi |
   | TC-07 Foreground gate | Alt-tab sang Notepad | Input dropped log |
   | TC-08 Minimize | Minimize mock | Capture pause + state UNSAFE |
   | TC-09 Session lock | Win+L lock | Tool pause hoàn toàn |
   | TC-10 Re-buff | cycle_duration=30s, đợi | Sau 30s quay lại BUFFING |
   | TC-11 Humanizer | AUTO 30 phút | Có break 3–8s xen kẽ |

5. **Mock UI controls**
   - 3 slider HP/MP/SP (0–100%)
   - Checkbox "Damage tick" (-5% HP mỗi 2s)
   - Checkbox "MP drain on F1" (-3% MP khi nhận F1)
   - Checkbox "Mob alive" (toggle: alive=HP/MP biến động; dead=stable)
   - Log panel xem input từ tool

6. **Debug khi TC fail**
   - Xem `logs/WindowHelper.log` level Debug
   - Check `Capture FPS` ở status bar > 15
   - Check `Gate: ●` (xanh = healthy)

### 8.4 `docs/test-with-replay-video.md`

Nội dung:
1. **Mục đích**
   - Test detector trên frame **thật** từ PT
   - Verify HSV hue ranges + calibrate coords đúng
   - Reproduce bug deterministic

2. **Record video gốc**
   - Mở PT offline (KHÔNG dùng official, không cần XC3)
   - Mở OBS Studio (free) → Settings → Output: format MP4, encoder x264, quality 23, FPS 30
   - Add Window Capture source → chọn Priston Tale
   - Record 5–10 phút farming bình thường (gồm: combat, low HP, low MP, town, loading screen)
   - Save vào `assets/replays/pt-farm-001.mp4`

3. **Config tool đọc replay**
   - Mở `config.json`, thêm:
     ```json
     "advanced": {
       "capture": {
         "backend": "replay",
         "replay_source": "assets/replays/pt-farm-001.mp4"
       }
     }
     ```
   - Save, restart tool

4. **Chạy verify detector**
   - Tool sẽ replay video thay vì capture live
   - Mở Vision Preview trong UI
   - Verify HP/MP/SP bar reading match visual bar trong video (±3%)
   - Nếu sai → re-calibrate region coords + tune hue ranges trong `advanced.detection_hue.*`

5. **Test edge cases**
   - Tìm segment particle dày → verify EMA không trigger false pot
   - Tìm segment loading screen → verify state UNSAFE
   - Tìm segment damage burst → verify P0 reaction time

6. **Iterate**
   - Tune config → rerun → check log
   - Khi accuracy MAE < 3% → ready for live PT test

### 8.5 `docs/troubleshooting.md`

Common issues:

| Issue | Cause | Fix |
|---|---|---|
| SmartScreen block | Unsigned exe | "More info" → "Run anyway" |
| "Game window not found" | PT chưa mở hoặc title khác | Mở PT trước, kiểm tra title regex trong `advanced.window.title_regex` |
| Capture FPS = 0 | WGC fail / Win < 1809 | Update Windows hoặc dùng replay mode |
| Detect HP sai (% lệch lớn) | Calibrate sai | Re-calibrate, kéo rect đúng vùng bar |
| Pot không gửi dù HP thấp | Foreground gate đóng | Click vào game để foreground; hoặc Phase 0 chọn PostMessage |
| Tool gõ phím vào Notepad | Backend = SendInput, alt-tab | Đợi quay lại game; hoặc dùng PostMessage backend |
| Char chạy lung tung | SHIFT+right-click không work trên server | Verify server hỗ trợ; giảm sweep radius |
| Buff không cast | Buff cooldown chưa hết | Tăng `cycle_duration_sec` lớn hơn buff cooldown game |
| RAM tăng dần | Memory leak | Update lên build mới, report bug |

### 8.6 `docs/user-manual.md`

Hướng dẫn UI hằng ngày cho end user:
- Cách attach window
- Cách calibrate
- Cách set thresholds
- Cách bật/tắt AUTO
- Hotkey F8
- System tray menu
- Đọc status bar
- Pause / resume

### 8.7 `docs/architecture-overview.md`

Tóm tắt cho dev kế thừa code:
- Sơ đồ module (capture → vision → dispatcher → input)
- Threading model (capture thread / vision thread / dispatcher / UI)
- Priority system P0–P4
- Combat FSM states
- Config bus pattern
- Output gate logic
- Link tới brainstorm report Section gốc

## Acceptance
- [ ] Tất cả 7 docs file tạo trong `docs/`
- [ ] `build-from-source.md` test thực tế bằng cách clone fresh + follow step-by-step → build success
- [ ] `deploy-and-run.md` test thực tế: copy exe ra folder mới + chạy → tool start được
- [ ] `test-with-mock.md`: follow guide → chạy mock + tool → ít nhất 5 TC pass
- [ ] `test-with-replay-video.md`: record OBS 2 phút + follow guide → tool replay được
- [ ] `troubleshooting.md` cover ít nhất 10 issue thường gặp
- [ ] Docs viết tiếng Việt, đơn giản, có screenshot nếu cần

## Risks
- Documentation drift: phase sau update code mà quên update docs → mỗi PR phải update docs liên quan
- Screenshot outdated khi UI đổi → giữ minimal screenshot, prefer text instructions

## Next
- Plan complete. User có thể bắt đầu Phase 0.
- Sau v1 release: feedback từ user → docs improvements
