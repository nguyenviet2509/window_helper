# Phase 5 — Logger + Tray + Hotkey + Naming/VersionInfo

**Est:** 1 ngày
**Priority:** P1
**Status:** pending
**Depends:** Phase 4 (UI integration)

## Mục Tiêu
- Logger (rolling file 5MB × 5)
- System tray icon (Shell_NotifyIcon) với menu Start/Stop/Show/Exit
- Hotkey toàn cục F8 START/STOP qua `RegisterHotKey`
- `WindowHelper.exe` rename + VersionInfo + icon
- Replay frame source

## Files
```
src/core/
├── logger.h/.cpp
└── log-rotator.h/.cpp
src/ui/
├── tray-icon.h/.cpp
└── hotkey-manager.h/.cpp
src/capture/
└── file-replay-source.h/.cpp
src/resources/
├── version.rc
└── app.ico
```

LOC: ~500.

## Implementation

### 5.1 Logger
```cpp
enum class LogLevel { Trace, Debug, Info, Warn, Error };

class Logger {
public:
    void log(LogLevel lvl, std::string_view msg);
    void flush();
private:
    std::mutex m_;
    std::ofstream file_;
    LogRotator rotator_;
    LogLevel minLevel_;
};

#define LOG_INFO(msg)  Logger::instance().log(LogLevel::Info, msg)
```

- Format: `[timestamp][thread][level][module] message`
- Async-ish: flush mỗi 1s, không mỗi line
- Rotator: file > 5MB → rename `.1` → `.2`... → giữ tối đa 5 file

### 5.2 TrayIcon
```cpp
class TrayIcon {
public:
    void install(HWND owner, HICON icon);
    void onMessage(WPARAM wp, LPARAM lp);   // WM_TRAYICON
    void uninstall();
private:
    NOTIFYICONDATA nid_{};
    void showContextMenu(HWND owner);
};
```

Menu: Start AUTO | Stop AUTO | Show window | --- | Exit.
Double-click → toggle show/hide window.

### 5.3 HotkeyManager
```cpp
class HotkeyManager {
public:
    bool registerHotkey(int id, UINT mod, UINT vk);  // RegisterHotKey
    void onHotkey(int id, std::function<void()> handler);
    void onMessage(WPARAM wp);                        // WM_HOTKEY
};
```

Register `id=1, vk=VK_F8, mod=0` → bind tới `toggleAuto()`.

### 5.4 FileReplaySource (Section 33.3)
```cpp
class FileReplaySource : public IFrameSource {
    cv::VideoCapture cap_;
    std::chrono::steady_clock::time_point lastFrame_;
public:
    bool start(const std::string& path);
    bool acquire(Frame& out, int timeoutMs) override;
};
```

Load MP4/PNG sequence. Pace 20Hz (50ms/frame). Loop khi hết.

Config switch:
```json
"advanced.capture": {
  "backend": "wgc",  // "wgc" | "replay"
  "replay_source": "assets/replays/pt-farm-001.mp4"
}
```

### 5.5 Naming + VersionInfo (Section 26)

CMake update:
```cmake
set_target_properties(pt_assistant PROPERTIES OUTPUT_NAME "WindowHelper")
target_sources(pt_assistant PRIVATE src/resources/version.rc)
set_target_properties(pt_assistant PROPERTIES WIN32_EXECUTABLE TRUE)
```

`version.rc`:
```rc
VS_VERSION_INFO VERSIONINFO
 FILEVERSION 1,0,0,0
 PRODUCTVERSION 1,0,0,0
 FILETYPE VFT_APP
BEGIN
 BLOCK "StringFileInfo"
 BEGIN
  BLOCK "040904B0"
  BEGIN
   VALUE "CompanyName",      "KIEU HIEN"
   VALUE "FileDescription",  "Window Helper Utility"
   VALUE "FileVersion",      "1.0.0.0"
   VALUE "InternalName",     "WindowHelper"
   VALUE "OriginalFilename", "WindowHelper.exe"
   VALUE "ProductName",      "Window Helper"
   VALUE "ProductVersion",   "1.0.0.0"
   VALUE "LegalCopyright",   "Copyright (C) 2026 KIEU HIEN"
  END
 END
 BLOCK "VarFileInfo"
 BEGIN VALUE "Translation", 0x409, 1200 END
END

IDI_APP_ICON ICON "app.ico"
```

### 5.6 Naming đồng bộ (Section 26.6)
- Window class: `"WindowHelperMainWnd"`
- Window title: `"Window Helper"`
- Mutex (single instance): `"Global\\{B6C9A2F1-3D5E-4F7A-8B1C-9E2D4F6A8C0B}_WindowHelper"`
- Log file: `logs/WindowHelper.log`
- Tray tooltip: `"Window Helper"`

### 5.7 Single Instance Check
```cpp
HANDLE mu = CreateMutex(NULL, TRUE, L"Global\\{...}_WindowHelper");
if (GetLastError() == ERROR_ALREADY_EXISTS) {
    // Bring existing window to front
    HWND prev = FindWindow(L"WindowHelperMainWnd", NULL);
    if (prev) { ShowWindow(prev, SW_RESTORE); SetForegroundWindow(prev); }
    return 0;
}
```

## Icon Asset
- Tạo PNG 256×256 đơn giản (logo abstract, không liên quan game)
- Convert: `magick convert app.png -define icon:auto-resize=256,128,64,48,32,16 app.ico`
- Save `src/resources/app.ico`

## Acceptance
- [ ] EXE output tên `WindowHelper.exe`
- [ ] Task Manager hiện "WindowHelper.exe" + Description "Window Helper Utility"
- [ ] Properties → Details: full VersionInfo
- [ ] Icon đẹp trên taskbar + explorer
- [ ] Tray icon xuất hiện khi run, menu hoạt động
- [ ] F8 toggle AUTO khi tool minimized
- [ ] Logger rotate khi > 5MB
- [ ] FileReplaySource đọc MP4 → vision pipeline process bình thường
- [ ] Single instance: chạy lần 2 → bring existing to front

## Risks
- Tray icon stay sau crash → register cleanup atexit
- F8 conflict với app khác sử dụng F8 global → cho user đổi trong config

## Next
Phase 6 integration test.
