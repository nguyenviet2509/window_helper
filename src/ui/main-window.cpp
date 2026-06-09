// Dear ImGui + DX11 + Win32 main window. Modeled on ImGui's
// example_win32_directx11/main.cpp, trimmed to what we need.

#include "main-window.h"
#include <d3d11.h>
#include <tchar.h>
#include <wtsapi32.h>
#include <wrl/client.h>
#include <climits>
#include <cstring>
#include <vector>
#include <functional>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

using Microsoft::WRL::ComPtr;

// Forward-declared by imgui_impl_win32.h:
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

struct MainWindow::DxState {
    ComPtr<ID3D11Device>         device;
    ComPtr<ID3D11DeviceContext>  ctx;
    ComPtr<IDXGISwapChain>       swap;
    ComPtr<ID3D11RenderTargetView> rtv;
    UINT resizeW = 0, resizeH = 0;
    bool pending = false;
};

namespace {
MainWindow* g_self = nullptr;

// Tên cửa sổ giả lập app phổ biến để né Xingcode3 text scan.
// Class name giữ nguyên "WindowHelperUI" để single-instance check vẫn hoạt động.
const wchar_t* pickFakeTitle() {
    static const wchar_t* kPool[] = {
        L"Notepad",
        L"Document - WordPad",
        L"Untitled - Paint",
        L"Calculator",
        L"Snipping Tool",
    };
    constexpr size_t kCount = sizeof(kPool) / sizeof(kPool[0]);
    LARGE_INTEGER pc{};
    QueryPerformanceCounter(&pc);
    return kPool[static_cast<size_t>(pc.QuadPart) % kCount];
}

LRESULT CALLBACK MainWndProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(h, m, wp, lp)) return 1;
    if (m == WM_TRAYICON && g_self && g_self->tray()) {
        g_self->tray()->onMessage(wp, lp);
        return 0;
    }
    if (m == WM_HOTKEY && g_self && g_self->hotkey()) {
        g_self->hotkey()->onMessage(wp);
        return 0;
    }
    if (m == WM_WTSSESSION_CHANGE && g_self) {
        if (wp == WTS_SESSION_LOCK)        g_self->notifySessionLock(true);
        else if (wp == WTS_SESSION_UNLOCK) g_self->notifySessionLock(false);
        return 0;
    }
    switch (m) {
    case WM_SIZE:
        // Hoãn resize swap chain tới đầu frame kế (tránh resize giữa lúc đang
        // render). SIZE_MINIMIZED không quan tâm — chỉ skip vẽ trong runLoop.
        if (g_self && wp != SIZE_MINIMIZED) {
            g_self->onResize(LOWORD(lp), HIWORD(lp));
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wp & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_CLOSE:
        ShowWindow(h, SW_HIDE);                   // close = minimize-to-tray
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

bool CreateDeviceD3D(HWND h, MainWindow::DxState& s) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = h;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
    D3D_FEATURE_LEVEL fl;
    D3D_FEATURE_LEVEL want[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        want, _countof(want), D3D11_SDK_VERSION,
        &sd, &s.swap, &s.device, &fl, &s.ctx);
    if (FAILED(hr)) return false;

    ComPtr<ID3D11Texture2D> back;
    s.swap->GetBuffer(0, IID_PPV_ARGS(&back));
    s.device->CreateRenderTargetView(back.Get(), nullptr, &s.rtv);
    return true;
}
}  // namespace

MainWindow::MainWindow(ConfigBus& bus, ConfigLoader& loader, std::string path)
    : bus_(bus), loader_(loader), configPath_(std::move(path)),
      dx_(new DxState()) {
    if (auto snap = bus_.snapshot()) draft_ = *snap;
    g_self = this;
}

MainWindow::~MainWindow() {
    if (initialized_) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    delete dx_;
    g_self = nullptr;
}

bool MainWindow::init(HINSTANCE hInst, int nShow) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"WindowHelperUI";
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(0, wc.lpszClassName, pickFakeTitle(),
                            WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, 520, 600,
                            nullptr, nullptr, hInst, nullptr);
    if (!hwnd_) return false;
    if (!CreateDeviceD3D(hwnd_, *dx_)) return false;

    ShowWindow(hwnd_, nShow);
    UpdateWindow(hwnd_);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsLight();

    // Load Windows font with Vietnamese glyph ranges (Segoe UI fallback Tahoma).
    {
        wchar_t winDir[MAX_PATH] = {};
        GetWindowsDirectoryW(winDir, MAX_PATH);
        wchar_t fontPath[MAX_PATH] = {};
        swprintf_s(fontPath, L"%s\\Fonts\\segoeui.ttf", winDir);
        char fontPathUtf8[MAX_PATH] = {};
        WideCharToMultiByte(CP_UTF8, 0, fontPath, -1, fontPathUtf8, MAX_PATH, nullptr, nullptr);
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 2;
        io.Fonts->AddFontFromFileTTF(fontPathUtf8, 16.0f, &cfg,
                                      io.Fonts->GetGlyphRangesVietnamese());
    }

    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(dx_->device.Get(), dx_->ctx.Get());

    initialized_ = true;
    return true;
}

void MainWindow::markDirty() {
    dirty_ = true;
    lastChangeAt_ = std::chrono::steady_clock::now();
}

void MainWindow::flushIfDue() {
    if (!dirty_) return;
    auto now = std::chrono::steady_clock::now();
    if ((now - lastChangeAt_) < std::chrono::milliseconds(debounceMs_)) return;
    auto snap = std::make_shared<const AppConfig>(draft_);
    bus_.publish(snap);
    loader_.save(configPath_, draft_);
    dirty_ = false;
    if (onConfigChanged_) onConfigChanged_(*snap);
}

void MainWindow::drawSettingsPanel() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    auto size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Settings", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    // Trạng thái cửa sổ đích (auto-attach lúc khởi động; muốn re-attach phải restart).
    if (target_) {
        wchar_t title[128] = {};
        GetWindowTextW(target_, title, 127);
        char utf8[256] = {};
        WideCharToMultiByte(CP_UTF8, 0, title, -1, utf8, sizeof(utf8), nullptr, nullptr);
        ImGui::TextColored(ImVec4(0.1f, 0.6f, 0.1f, 1.0f),
                           "Đã gắn: %s [HWND=0x%p]", utf8, (void*)target_);

        // Live cursor coords để calibrate vị trí pot slot trong kho.
        POINT cs{};
        if (GetCursorPos(&cs)) {
            POINT cc = cs;
            ScreenToClient(target_, &cc);
            RECT cr{};
            GetClientRect(target_, &cr);
            ImGui::TextDisabled(
                "Cursor: screen=(%ld,%ld) client=(%ld,%ld) clientSize=%ldx%ld",
                cs.x, cs.y, cc.x, cc.y, cr.right - cr.left, cr.bottom - cr.top);
        }
    } else {
        ImGui::TextColored(ImVec4(0.8f, 0.1f, 0.1f, 1.0f), "Chưa tìm thấy cửa sổ game");
    }
    ImGui::Separator();

    bool combatOn = draft_.combat.enabled;
    if (ImGui::Checkbox("AUTO (Đánh quái)", &combatOn)) {
        draft_.combat.enabled = combatOn;
        if (onCombatToggle_) onCombatToggle_(combatOn);
        markDirty();
    }

    // Ngưỡng nhập theo phần trăm (0-100). Lưu trữ vẫn là tỉ lệ 0.0-1.0.
    auto percentInput = [](const char* label, double* d, float maxPct) -> bool {
        float pct = static_cast<float>(*d) * 100.0f;
        if (ImGui::DragFloat(label, &pct, 1.0f, 0.0f, maxPct, "%.1f%%")) {
            if (pct < 0.0f) pct = 0.0f;
            if (pct > maxPct) pct = maxPct;
            *d = pct / 100.0f;
            return true;
        }
        return false;
    };

    if (ImGui::CollapsingHeader("Hồi phục", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& p = draft_.pot;
        bool any = false;
        any |= percentInput("Ngưỡng hồi HP (%)", &p.hpThreshold, 100.0f);
        any |= percentInput("Ngưỡng hồi MP (%)", &p.mpThreshold, 100.0f);
        any |= percentInput("Ngưỡng hồi SP (%)", &p.spThreshold, 100.0f);
        // Recall (F12 bùa hồi thành) tạm ẩn UI; logic vẫn chạy theo giá trị
        // trong config.json. Bỏ comment 2 dòng dưới khi cần expose lại.
        // any |= percentInput("Ngưỡng recall HP (%)", &p.hpRecallThreshold, 50.0f);
        // any |= ImGui::DragInt("Recall ổn định (ms)", &p.hpRecallStableMs, 100, 500, 10000);
        // Cooldown bình + Số frame xác nhận: logic vẫn chạy theo config.json.
        // Bỏ comment 2 dòng dưới khi cần expose lại.
        // any |= ImGui::DragInt("Cooldown bình (ms)", &p.cooldownMs, 50, 200, 3000);
        // any |= ImGui::DragInt("Số frame xác nhận", &p.confirmFrames, 1, 1, 5);
        if (any) markDirty();
    }

    if (ImGui::CollapsingHeader("Đánh quái", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& c = draft_.combat;
        bool any = false;
        any |= ImGui::DragInt("Chờ tối thiểu khi đổi mục tiêu (ms)", &c.repickMinDwellMs, 100, 500, 10000);
        any |= ImGui::DragInt("Chờ tối đa khi đổi mục tiêu (ms)", &c.repickMaxDwellMs, 100, 1000, 60000);
        any |= ImGui::DragInt("Khoá đánh sau shift+phải (ms)", &c.engagementLockMs, 100, 1000, 15000);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(
            "Sau khi shift+chuột phải, im lặng X ms để game tự đánh mob;\nthoát sớm nếu phát hiện mob chết.");
        any |= ImGui::DragInt("Dao động khoá đánh (ms)", &c.engagementLockJitterMs, 50, 0, 2000);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(
            "Random hoá độ dài khoá ±jitter để né pattern detect.");
        any |= ImGui::DragInt("Bán kính đánh (min)", &c.attackRadiusMin, 5, 20, 400);
        any |= ImGui::DragInt("Bán kính đánh (max)", &c.attackRadiusMax, 5, 20, 400);
        any |= ImGui::Checkbox("Chờ đủ MP mới buff", &c.waitMpGate);
        any |= percentInput("Ngưỡng MP để buff (%)", &c.waitMpGateThreshold, 100.0f);

        ImGui::Separator();
        ImGui::TextUnformatted("Safe spot chuột phải khi buff");
        {
            float sx = (float)(c.buffSafeSpotXPct * 100.0);
            float sy = (float)(c.buffSafeSpotYPct * 100.0);
            if (ImGui::SliderFloat("Safe spot X (%)", &sx, 5.0f, 95.0f, "%.0f%%")) {
                c.buffSafeSpotXPct = sx / 100.0;
                any = true;
            }
            if (ImGui::SliderFloat("Safe spot Y (%)", &sy, 5.0f, 95.0f, "%.0f%%")) {
                c.buffSafeSpotYPct = sy / 100.0;
                any = true;
            }
            ImGui::TextDisabled("Tọa độ chuột phải để confirm self-target.\nPick chỗ TRỐNG: không mob, không UI element.\nNếu trúng mob, skill sẽ biến thành đánh thường.");
        }
        if (any) markDirty();
    }

    if (ImGui::CollapsingHeader("Nạp pot từ kho", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& r = draft_.refill;
        bool any = false;
        any |= ImGui::Checkbox("Bật nạp pot tự động", &r.enabled);

        ImGui::Separator();
        ImGui::TextUnformatted("Thời gian nạp lại (giây) — 0 = tắt");
        any |= ImGui::DragInt("HP mỗi N giây", &r.hp.intervalSec, 5, 0, 7200);
        any |= ImGui::DragInt("SP mỗi N giây", &r.sp.intervalSec, 5, 0, 7200);
        any |= ImGui::DragInt("MP mỗi N giây", &r.mp.intervalSec, 5, 0, 7200);

        ImGui::Separator();
        ImGui::TextDisabled("Tọa độ pot trong kho được hardcode (đo từ PT, cố định).");

        ImGui::Separator();
        ImGui::TextUnformatted("Tinh chỉnh");
        any |= ImGui::DragInt("Chờ kho mở (ms)", &r.inventoryOpenDelayMs, 10, 100, 3000);
        any |= ImGui::DragInt("Chờ kho đóng (ms)", &r.inventoryCloseDelayMs, 10, 50, 3000);
        any |= ImGui::DragInt("Chờ chuột di chuyển (ms)", &r.mouseMoveDelayMs, 10, 50, 2000);
        any |= ImGui::DragInt("Chờ sau Shift+N (ms)", &r.postHotkeyDelayMs, 10, 50, 2000);
        any |= ImGui::DragInt("Timeout toàn refill (ms)", &r.refillTimeoutMs, 500, 2000, 60000);
        {
            float thr = (float)(r.hpCriticalAbortThreshold * 100.0);
            if (ImGui::DragFloat("Ngưỡng abort khi HP thấp (%)", &thr, 1.0f, 5.0f, 90.0f, "%.0f%%")) {
                r.hpCriticalAbortThreshold = thr / 100.0;
                any = true;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Nếu HP tụt dưới ngưỡng này khi đang nạp pot, hủy nạp → đóng kho → uống pot HP ingame.");
        }
        any |= ImGui::DragInt("Tạm dừng sau khi hủy (ms)", &r.abortBackoffMs, 1000, 5000, 300000);

        if (any) markDirty();
    }

    if (ImGui::CollapsingHeader("Buff")) {
        for (size_t i = 0; i < draft_.combat.buffs.size(); ++i) {
            auto& b = draft_.combat.buffs[i];
            ImGui::PushID((int)i);
            bool any = false;
            char label[48]; snprintf(label, sizeof(label), "Bật buff %zu", i);
            any |= ImGui::Checkbox(label, &b.enabled);
            // F1=0x70 ... F12=0x7B; combo index 0..11 maps to VK code.
            static const char* kFnNames =
                "F1\0F2\0F3\0F4\0F5\0F6\0F7\0F8\0F9\0F10\0F11\0F12\0";
            int fnIdx = static_cast<int>(b.key) - 0x70;
            if (fnIdx < 0) fnIdx = 0;
            if (fnIdx > 11) fnIdx = 11;
            if (ImGui::Combo("Mã phím", &fnIdx, kFnNames)) {
                b.key = static_cast<WORD>(0x70 + fnIdx);
                any = true;
            }
            any |= ImGui::DragInt("Animation cast (ms)", &b.animationMs, 10, 200, 3000);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Thời gian animation cast skill. Phải >= animation thực để không bị cancel.");
            any |= ImGui::DragInt("Delay chuột phải (ms)", &b.rightClickDelayMs, 5, 0, 500);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Delay từ bấm F đến right-click. Chờ game đổi cursor self-target.");
            any |= ImGui::DragInt("Gap sang buff sau (ms)", &b.postBuffGapMs, 5, 0, 1000);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Khoảng đệm an toàn trước khi bấm buff kế tiếp.");
            any |= ImGui::DragInt("Chu kỳ rebuff (giây)", &b.rebuffIntervalSec, 5, 10, 7200);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Cast lại buff này sau N giây. Đặt theo duration thực của buff in-game.\n"
                "Quá thấp (< 60s) sẽ liên tục ngắt đánh quái.");
            if (b.enabled && b.rebuffIntervalSec < 60) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                    "Canh bao: chu ky < 60s se ngat danh lien tuc.");
            }
            any |= ImGui::Checkbox("Chuột phải sau buff", &b.rightClickAfter);
            ImGui::Separator();
            ImGui::PopID();
            if (any) markDirty();
        }

        // Cảnh báo nếu tổng thời gian 1 vòng buff vượt rebuff interval ngắn nhất.
        int totalBuffMs = 0;
        int minIntervalMs = INT_MAX;
        for (const auto& b : draft_.combat.buffs) {
            if (!b.enabled) continue;
            totalBuffMs += b.animationMs + b.postBuffGapMs;
            int ivMs = b.rebuffIntervalSec * 1000;
            if (ivMs < minIntervalMs) minIntervalMs = ivMs;
        }
        if (minIntervalMs != INT_MAX && totalBuffMs > minIntervalMs / 2) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                "Canh bao: Tong thoi gian buff (%d ms) > 50%% chu ky nho nhat (%d ms).\nCo the chong cheo voi rebuff.",
                totalBuffMs, minIntervalMs);
        }
    }

    ImGui::TextDisabled("Calibrate: sửa toạ độ ROI trong config.json (overlay kéo thả chưa làm).");

    if (ImGui::Button("Lưu ngay")) {
        loader_.save(configPath_, draft_);
        dirty_ = false;
    }
    ImGui::SameLine();
    ImGui::TextDisabled(dirty_ ? "(chưa lưu)" : "(đã lưu)");

    ImGui::End();
}

void MainWindow::onResize(unsigned w, unsigned h) {
    if (!dx_ || !dx_->device) return;
    dx_->resizeW = w;
    dx_->resizeH = h;
    dx_->pending = true;
}

void MainWindow::applyPendingResize() {
    if (!dx_->pending) return;
    if (dx_->resizeW == 0 || dx_->resizeH == 0) { dx_->pending = false; return; }
    dx_->rtv.Reset();
    HRESULT hr = dx_->swap->ResizeBuffers(0, dx_->resizeW, dx_->resizeH,
                                          DXGI_FORMAT_UNKNOWN, 0);
    if (SUCCEEDED(hr)) {
        ComPtr<ID3D11Texture2D> back;
        dx_->swap->GetBuffer(0, IID_PPV_ARGS(&back));
        dx_->device->CreateRenderTargetView(back.Get(), nullptr, &dx_->rtv);
    }
    dx_->pending = false;
}

void MainWindow::renderFrame() {
    applyPendingResize();
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    drawSettingsPanel();
    ImGui::Render();

    const float clear[4] = { 0.92f, 0.92f, 0.94f, 1.0f };
    dx_->ctx->OMSetRenderTargets(1, dx_->rtv.GetAddressOf(), nullptr);
    dx_->ctx->ClearRenderTargetView(dx_->rtv.Get(), clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    dx_->swap->Present(1, 0);
}

void MainWindow::requestClose() {
    if (hwnd_) PostMessageW(hwnd_, WM_CLOSE, 0, 0);
}

void MainWindow::toggleCombatRequested() {
    draft_.combat.enabled = !draft_.combat.enabled;
    if (onCombatToggle_) onCombatToggle_(draft_.combat.enabled);
    markDirty();
}

int MainWindow::runLoop() {
    MSG msg;
    bool running = true;
    while (running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;
        if (IsWindowVisible(hwnd_)) renderFrame();
        else Sleep(50);                          // throttle when minimized to tray
        flushIfDue();
    }
    return (int)msg.wParam;
}
