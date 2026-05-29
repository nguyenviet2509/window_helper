#pragma once
// System tray icon with a small popup menu. Owner window handles WM_TRAYICON
// (must call onMessage). Menu items wire to user-supplied callbacks.
#include <windows.h>
#include <shellapi.h>
#include <functional>

constexpr UINT WM_TRAYICON = WM_APP + 1;

class TrayIcon {
public:
    void install(HWND owner, HICON icon, const wchar_t* tooltip);
    void uninstall();
    void onMessage(WPARAM wp, LPARAM lp);

    void setOnToggleAuto(std::function<void()> cb)  { onToggleAuto_ = std::move(cb); }
    void setOnShowWindow(std::function<void()> cb)  { onShowWindow_ = std::move(cb); }
    void setOnExit(std::function<void()> cb)        { onExit_ = std::move(cb); }

private:
    NOTIFYICONDATAW nid_{};
    HWND owner_ = nullptr;
    std::function<void()> onToggleAuto_, onShowWindow_, onExit_;

    void showContextMenu();
};
