// PostMessage Probe — Phase 0 gate test.
// Determines whether the target Priston Tale client accepts background
// WM_KEYDOWN / WM_RBUTTONDOWN messages. Decides input backend for Phase 2.
//
// Usage:
//   1. Launch PT (offline / private server, NOT official with anti-cheat).
//   2. Run PostMessageProbe.exe.
//   3. Alt-tab to Notepad within 3s.
//   4. Observe PT for: F2 skill animation, right-click at (400,300),
//      SHIFT+right-click stationary attack.
//   5. Record results in docs/input-backend-decision.md.

#include <windows.h>
#include <cstdio>
#include <cwchar>

static HWND FindGameWindow() {
    // Try common title variants.
    const wchar_t* candidates[] = {
        L"Priston Tale",
        L"PristonTale",
        L"Priston Tale Client",
    };
    for (const auto* t : candidates) {
        HWND h = FindWindowW(nullptr, t);
        if (h) return h;
    }
    return nullptr;
}

int wmain() {
    HWND h = FindGameWindow();
    if (!h) {
        std::fwprintf(stderr, L"[ERR] Game window not found (tried 'Priston Tale' variants).\n");
        std::fwprintf(stderr, L"      Make sure the game client is running.\n");
        return 1;
    }

    wchar_t title[256] = {};
    GetWindowTextW(h, title, 255);
    std::fwprintf(stdout, L"[OK] Found HWND=0x%p title='%s'\n", (void*)h, title);
    std::fwprintf(stdout, L"     Alt-tab to Notepad now. Probe sends in 3s...\n");
    std::fflush(stdout);
    Sleep(3000);

    // Test 1: F2 key (background) — checks WM_KEYDOWN/UP acceptance.
    std::fwprintf(stdout, L"[T1] F2 keydown/up...\n");
    PostMessageW(h, WM_KEYDOWN, VK_F2, 0);
    Sleep(50);
    PostMessageW(h, WM_KEYUP, VK_F2, 0);
    Sleep(700);

    // Test 2: right-click at (400, 300) — checks WM_RBUTTONDOWN/UP.
    std::fwprintf(stdout, L"[T2] Right-click at (400,300)...\n");
    LPARAM lp = MAKELPARAM(400, 300);
    PostMessageW(h, WM_RBUTTONDOWN, MK_RBUTTON, lp);
    Sleep(20);
    PostMessageW(h, WM_RBUTTONUP, 0, lp);
    Sleep(700);

    // Test 3: SHIFT + right-click — stationary attack.
    std::fwprintf(stdout, L"[T3] SHIFT + right-click at (400,300)...\n");
    PostMessageW(h, WM_KEYDOWN, VK_LSHIFT, 0);
    Sleep(10);
    PostMessageW(h, WM_RBUTTONDOWN, MK_RBUTTON | MK_SHIFT, lp);
    Sleep(20);
    PostMessageW(h, WM_RBUTTONUP, MK_SHIFT, lp);
    Sleep(10);
    PostMessageW(h, WM_KEYUP, VK_LSHIFT, 0);
    Sleep(700);

    // Test 4: WM_MOUSEMOVE sequence rồi right-click — verify path-then-click pattern.
    std::fwprintf(stdout, L"[T4] Mouse-move trail (5 points) then right-click...\n");
    const POINT trail[] = { {200,200}, {250,220}, {300,240}, {350,270}, {400,300} };
    for (const auto& p : trail) {
        PostMessageW(h, WM_MOUSEMOVE, 0, MAKELPARAM(p.x, p.y));
        Sleep(20);
    }
    PostMessageW(h, WM_RBUTTONDOWN, MK_RBUTTON, lp);
    Sleep(20);
    PostMessageW(h, WM_RBUTTONUP, 0, lp);

    std::fwprintf(stdout, L"[DONE] Observe game. Record results:\n");
    std::fwprintf(stdout, L"       - T1 (F2 skill visible?) -> keys OK?\n");
    std::fwprintf(stdout, L"       - T2 (right-click took effect?) -> mouse OK?\n");
    std::fwprintf(stdout, L"       - T3 (attack-in-place triggered?) -> SHIFT mod OK?\n");
    std::fwprintf(stdout, L"       - T4 (cursor moved + clicked?) -> WM_MOUSEMOVE OK?\n");
    return 0;
}
