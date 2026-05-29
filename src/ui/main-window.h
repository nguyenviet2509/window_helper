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

class MainWindow {
public:
    MainWindow(ConfigBus& bus, ConfigLoader& loader, std::string configPath);
    ~MainWindow();

    bool init(HINSTANCE hInst, int nShow);
    int  runLoop();                              // returns WM_QUIT wParam
    void requestClose();

    // Callbacks fired from UI thread (e.g. AUTO toggle).
    void setOnCombatToggle(std::function<void(bool)> cb) { onCombatToggle_ = std::move(cb); }

    void attachTray(TrayIcon* t)        { tray_ = t; }
    void attachHotkey(HotkeyManager* h) { hotkey_ = h; }
    void setOnExit(std::function<void()> cb) { onExit_ = std::move(cb); }
    void setOnSessionLockChange(std::function<void(bool)> cb) { onSessionLock_ = std::move(cb); }

    // Toggle the combat flag from any thread (UI marshals via PostMessage).
    void toggleCombatRequested();

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

    std::function<void(bool)> onCombatToggle_;
    std::function<void()> onExit_;
    std::function<void(bool)> onSessionLock_;
public:
    void notifySessionLock(bool locked) { if (onSessionLock_) onSessionLock_(locked); }
private:

    TrayIcon* tray_ = nullptr;
    HotkeyManager* hotkey_ = nullptr;

public:
    struct DxState;
private:
    DxState* dx_ = nullptr;
};
