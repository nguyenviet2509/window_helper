#pragma once
// Dear ImGui + Win32 + D3D11 main window. Owns AppConfig editing; publishes
// changes to ConfigBus with 500ms debounce; auto-saves to disk.
#include <windows.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include "../state/game-state.h"
#include "../config/config-bus.h"
#include "../config/config-loader.h"
#include "tray-icon.h"
#include "hotkey-manager.h"
#include "calibration-window.h"

class MainWindow {
public:
    MainWindow(ConfigBus& bus, ConfigLoader& loader, std::string configPath);
    ~MainWindow();

    bool init(HINSTANCE hInst, int nShow);
    int  runLoop();                              // returns WM_QUIT wParam
    void requestClose();

    // Render a single ImGui frame calling only the provided overlay (e.g. activation
    // dialog). Used by the license gate loop before the main pipeline is started.
    void renderActivationFrame(std::function<void()> overlay);

    // Callbacks fired from UI thread (e.g. AUTO toggle).
    void setOnCombatToggle(std::function<void(bool)> cb) { onCombatToggle_ = std::move(cb); }
    void setOnBuffToggle(std::function<void(bool)> cb) { onBuffToggle_ = std::move(cb); }
    // Fires after debounced flush (config saved). Use to propagate live edits to runtime systems.
    void setOnConfigChanged(std::function<void(const AppConfig&)> cb) { onConfigChanged_ = std::move(cb); }

    void attachTray(TrayIcon* t)        { tray_ = t; }
    void attachHotkey(HotkeyManager* h) { hotkey_ = h; }
    void notifyVisionState(double hp, double mp, double sp) {
        calibration_.setLatestVision(hp, mp, sp);
    }
    void setTarget(HWND target) { target_ = target; }
    void setOnExit(std::function<void()> cb) { onExit_ = std::move(cb); }
    void setOnSessionLockChange(std::function<void(bool)> cb) { onSessionLock_ = std::move(cb); }
    // Called each frame after drawSettingsPanel() — use for overlay popups (license info, toasts).
    void setOnFrameOverlay(std::function<void()> cb) { onFrameOverlay_ = std::move(cb); }

    // Toggle the combat flag from any thread (UI marshals via PostMessage).
    void toggleCombatRequested();
    void toggleBuffRequested();

    HWND hwnd() const { return hwnd_; }
    TrayIcon* tray() const { return tray_; }
    HotkeyManager* hotkey() const { return hotkey_; }

private:
    void renderFrame();
    void drawSettingsPanel();
    void markDirty();
    void flushIfDue();

    HWND hwnd_ = nullptr;
    bool initialized_ = false;

    ConfigBus& bus_;
    ConfigLoader& loader_;
    std::string configPath_;
    AppConfig draft_;

    bool dirty_ = false;
    std::chrono::steady_clock::time_point lastChangeAt_{};
    int debounceMs_ = 500;

    // Auto-fit chiều cao cửa sổ tới content cho đến khi user tự resize manual.
    // Mục đích: hiển thị hết settings + nút Calibrate mà không cần scroll khi mở.
    // userResized_ set true khi WM_SIZE đến với h khác autoClientHLastApplied_.
    float lastContentBottomY_ = 0.0f;
    int   autoClientHLastApplied_ = 0;
    bool  userResized_ = false;

    std::function<void(bool)> onCombatToggle_;
    std::function<void(bool)> onBuffToggle_;
    std::function<void(const AppConfig&)> onConfigChanged_;
    std::function<void()> onExit_;
    std::function<void(bool)> onSessionLock_;
    std::function<void()> onFrameOverlay_;
public:
    void notifySessionLock(bool locked) { if (onSessionLock_) onSessionLock_(locked); }
    void onResize(unsigned w, unsigned h);
private:
    void applyPendingResize();

    TrayIcon* tray_ = nullptr;
    HotkeyManager* hotkey_ = nullptr;
    HWND target_ = nullptr;
    CalibrationWindow calibration_;

public:
    struct DxState;
private:
    DxState* dx_ = nullptr;
};
