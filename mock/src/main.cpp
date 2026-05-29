// PtMockGame.exe — Phase-1 mock target.
// 800x600 window, title "Priston Tale" so WindowHelper FindWindow matches.
// Renders 3 vertical bars (HP red, SP yellow, MP blue) at the Phase-1 ROIs,
// driven by 3 horizontal trackbars (one per stat).

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

namespace {

constexpr int kHpId = 1001;
constexpr int kSpId = 1002;
constexpr int kMpId = 1003;

// Bar layout (matches WindowHelper default ROIs).
struct BarSpec { int x, y, w, h; COLORREF color; };
constexpr BarSpec kHp = {355, 475, 12, 105, RGB(220,  30,  30)};
constexpr BarSpec kSp = {383, 475, 12, 105, RGB(220, 180,  40)};
constexpr BarSpec kMp = {411, 475, 12, 105, RGB( 50, 100, 220)};

int g_hpPct = 100, g_spPct = 100, g_mpPct = 100;
HWND g_hpBar = nullptr, g_spBar = nullptr, g_mpBar = nullptr;

void DrawBar(HDC dc, const BarSpec& s, int pct) {
    // Background (dark stone)
    RECT bg = { s.x, s.y, s.x + s.w, s.y + s.h };
    HBRUSH bgBr = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(dc, &bg, bgBr);
    DeleteObject(bgBr);

    // Fill from bottom up.
    int fillH = (s.h * pct) / 100;
    if (fillH > 0) {
        RECT fr = { s.x, s.y + s.h - fillH, s.x + s.w, s.y + s.h };
        HBRUSH fb = CreateSolidBrush(s.color);
        FillRect(dc, &fr, fb);
        DeleteObject(fb);
    }

    // 1px border.
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    HGDIOBJ old = SelectObject(dc, pen);
    HGDIOBJ oldBr = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, s.x - 1, s.y - 1, s.x + s.w + 1, s.y + s.h + 1);
    SelectObject(dc, old);
    SelectObject(dc, oldBr);
    DeleteObject(pen);
}

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_KEYDOWN: {
        char buf[64];
        wsprintfA(buf, "[mock] WM_KEYDOWN vk=0x%02X\n", (unsigned)wp);
        OutputDebugStringA(buf);
        return 0;
    }
    case WM_RBUTTONDOWN: {
        char buf[96];
        wsprintfA(buf, "[mock] WM_RBUTTONDOWN x=%d y=%d wp=0x%04X\n",
                  GET_X_LPARAM(lp), GET_Y_LPARAM(lp), (unsigned)wp);
        OutputDebugStringA(buf);
        return 0;
    }
    case WM_CREATE: {
        // Trackbars: HP/SP/MP each labeled. Vertical stack near top.
        auto mk = [&](int id, int y, const wchar_t* label) {
            CreateWindowW(L"STATIC", label, WS_CHILD | WS_VISIBLE,
                          10, y, 30, 20, h, nullptr, nullptr, nullptr);
            HWND tb = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                          WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
                          50, y, 300, 24, h, (HMENU)(INT_PTR)id, nullptr, nullptr);
            SendMessageW(tb, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
            SendMessageW(tb, TBM_SETPOS, TRUE, 100);
            return tb;
        };
        g_hpBar = mk(kHpId, 10, L"HP");
        g_spBar = mk(kSpId, 40, L"SP");
        g_mpBar = mk(kMpId, 70, L"MP");
        return 0;
    }
    case WM_HSCROLL: {
        HWND src = (HWND)lp;
        int pos = (int)SendMessageW(src, TBM_GETPOS, 0, 0);
        if (src == g_hpBar) g_hpPct = pos;
        else if (src == g_spBar) g_spPct = pos;
        else if (src == g_mpBar) g_mpPct = pos;
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(h, &rc);
        HBRUSH br = CreateSolidBrush(RGB(15, 15, 25));
        FillRect((HDC)wp, &rc, br);
        DeleteObject(br);
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        DrawBar(dc, kHp, g_hpPct);
        DrawBar(dc, kSp, g_spPct);
        DrawBar(dc, kMp, g_mpPct);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nShow) {
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"PtMockGameClass";
    RegisterClassW(&wc);

    // Client area 800x600.
    RECT r = { 0, 0, 800, 600 };
    DWORD style = WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    AdjustWindowRect(&r, style, FALSE);

    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Priston Tale", style,
                              CW_USEDEFAULT, CW_USEDEFAULT,
                              r.right - r.left, r.bottom - r.top,
                              nullptr, nullptr, hInst, nullptr);
    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return 0;
}
