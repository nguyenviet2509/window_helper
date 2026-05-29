#include "send-input-backend.h"
#include <thread>
#include <chrono>

void SendInputBackend::clientToScreenAbs(int xClient, int yClient, LONG& absX, LONG& absY) const {
    POINT p{ xClient, yClient };
    if (target_) ClientToScreen(target_, &p);
    int sx = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int sy = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int ox = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int oy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    if (sx <= 0) sx = 1;
    if (sy <= 0) sy = 1;
    absX = static_cast<LONG>(((p.x - ox) * 65535LL) / sx);
    absY = static_cast<LONG>(((p.y - oy) * 65535LL) / sy);
}

void SendInputBackend::pressKey(WORD vk, bool down) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.wScan = LOWORD(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    in.ki.dwFlags = (down ? 0 : KEYEVENTF_KEYUP);
    SendInput(1, &in, sizeof(in));
}

void SendInputBackend::mouseButton(bool right, bool down) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = right
        ? (down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP)
        : (down ? MOUSEEVENTF_LEFTDOWN  : MOUSEEVENTF_LEFTUP);
    SendInput(1, &in, sizeof(in));
}

void SendInputBackend::sendKeyDown(WORD vk) { pressKey(vk, true); }
void SendInputBackend::sendKeyUp(WORD vk)   { pressKey(vk, false); }

void SendInputBackend::sendKeyTap(WORD vk, int holdMs) {
    pressKey(vk, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(holdMs > 0 ? holdMs : 25));
    pressKey(vk, false);
}

void SendInputBackend::sendRightClick(int x, int y) {
    LONG ax, ay;
    clientToScreenAbs(x, y, ax, ay);
    INPUT mv{};
    mv.type = INPUT_MOUSE;
    mv.mi.dx = ax;
    mv.mi.dy = ay;
    mv.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK;
    SendInput(1, &mv, sizeof(mv));
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
    mouseButton(true, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    mouseButton(true, false);
}

void SendInputBackend::sendShiftRightClick(int x, int y) {
    pressKey(VK_LSHIFT, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
    sendRightClick(x, y);
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
    pressKey(VK_LSHIFT, false);
}
