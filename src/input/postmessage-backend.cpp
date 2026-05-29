#include "postmessage-backend.h"
#include <thread>
#include <chrono>

LPARAM PostMessageBackend::keyLParam(WORD vk, bool up) {
    WORD scan = LOWORD(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    DWORD lp = 1u;                                  // repeat count
    lp |= (DWORD)scan << 16;                        // scan code in bits 16..23
    if (up) lp |= (1u << 30) | (1u << 31);          // KF_REPEAT (previous state) | KF_UP
    return static_cast<LPARAM>(lp);
}

void PostMessageBackend::sendKeyDown(WORD vk) {
    if (!target_) return;
    PostMessageW(target_, WM_KEYDOWN, vk, keyLParam(vk, false));
}

void PostMessageBackend::sendKeyUp(WORD vk) {
    if (!target_) return;
    PostMessageW(target_, WM_KEYUP, vk, keyLParam(vk, true));
}

void PostMessageBackend::sendKeyTap(WORD vk, int holdMs) {
    if (!target_) return;
    sendKeyDown(vk);
    std::this_thread::sleep_for(std::chrono::milliseconds(holdMs > 0 ? holdMs : 25));
    sendKeyUp(vk);
}

void PostMessageBackend::sendRightClick(int x, int y) {
    if (!target_) return;
    LPARAM lp = MAKELPARAM(x, y);
    PostMessageW(target_, WM_MOUSEMOVE, 0, lp);
    PostMessageW(target_, WM_RBUTTONDOWN, MK_RBUTTON, lp);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    PostMessageW(target_, WM_RBUTTONUP, 0, lp);
}

void PostMessageBackend::sendShiftRightClick(int x, int y) {
    if (!target_) return;
    LPARAM lp = MAKELPARAM(x, y);
    PostMessageW(target_, WM_KEYDOWN, VK_LSHIFT, keyLParam(VK_LSHIFT, false));
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
    PostMessageW(target_, WM_MOUSEMOVE, MK_SHIFT, lp);
    PostMessageW(target_, WM_RBUTTONDOWN, MK_RBUTTON | MK_SHIFT, lp);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    PostMessageW(target_, WM_RBUTTONUP, MK_SHIFT, lp);
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
    PostMessageW(target_, WM_KEYUP, VK_LSHIFT, keyLParam(VK_LSHIFT, true));
}
