#include "tray-icon.h"

namespace { enum { kMenuLicenseInfo = 1, kMenuToggleAuto, kMenuShow, kMenuExit }; }

void TrayIcon::install(HWND owner, HICON icon, const wchar_t* tooltip) {
    owner_ = owner;
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = owner;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAYICON;
    nid_.hIcon = icon ? icon : LoadIconW(nullptr, IDI_APPLICATION);
    lstrcpynW(nid_.szTip, tooltip ? tooltip : L"WindowHelper",
              sizeof(nid_.szTip) / sizeof(wchar_t));
    Shell_NotifyIconW(NIM_ADD, &nid_);
}

void TrayIcon::uninstall() {
    if (nid_.hWnd) Shell_NotifyIconW(NIM_DELETE, &nid_);
    nid_.hWnd = nullptr;
}

void TrayIcon::onMessage(WPARAM /*wp*/, LPARAM lp) {
    UINT ev = LOWORD(lp);
    if (ev == WM_RBUTTONUP) showContextMenu();
    else if (ev == WM_LBUTTONDBLCLK) { if (onShowWindow_) onShowWindow_(); }
}

void TrayIcon::showContextMenu() {
    HMENU m = CreatePopupMenu();
#ifdef WH_REQUIRE_LICENSE
    AppendMenuW(m, MF_STRING,    kMenuLicenseInfo, L"License Info...");
    AppendMenuW(m, MF_SEPARATOR, 0,                nullptr);
#endif
    AppendMenuW(m, MF_STRING,    kMenuToggleAuto,  L"Bật/Tắt AUTO");
    AppendMenuW(m, MF_STRING,    kMenuShow,        L"Hiện cửa sổ");
    AppendMenuW(m, MF_SEPARATOR, 0,                nullptr);
    AppendMenuW(m, MF_STRING,    kMenuExit,        L"Thoát");

    POINT p; GetCursorPos(&p);
    SetForegroundWindow(owner_);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, owner_, nullptr);
    DestroyMenu(m);

    switch (cmd) {
    case kMenuLicenseInfo: if (onLicenseInfo_) onLicenseInfo_(); break;
    case kMenuToggleAuto:  if (onToggleAuto_)  onToggleAuto_();  break;
    case kMenuShow:        if (onShowWindow_)  onShowWindow_();  break;
    case kMenuExit:        if (onExit_)        onExit_();        break;
    default: break;
    }
}
