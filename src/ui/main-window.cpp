// Dear ImGui + DX11 + Win32 main window. Modeled on ImGui's
// example_win32_directx11/main.cpp, trimmed to what we need.

#include "main-window.h"
#include <d3d11.h>
#include <tchar.h>
#include <wtsapi32.h>
#include <wrl/client.h>
#include <cstring>
#include <vector>
#include <functional>

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"

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

    hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"WindowHelper",
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
}

void MainWindow::drawSettingsPanel() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    auto size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Settings", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    bool combatOn = draft_.combat.enabled;
    if (ImGui::Checkbox("AUTO (Combat)", &combatOn)) {
        draft_.combat.enabled = combatOn;
        if (onCombatToggle_) onCombatToggle_(combatOn);
        markDirty();
    }

    if (ImGui::CollapsingHeader("Recovery", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& p = draft_.pot;
        bool any = false;
        any |= ImGui::SliderFloat("HP threshold", reinterpret_cast<float*>(&p.hpThreshold),
                                  0.0f, 1.0f, "%.2f");
        any |= ImGui::SliderFloat("MP threshold", reinterpret_cast<float*>(&p.mpThreshold),
                                  0.0f, 1.0f, "%.2f");
        any |= ImGui::SliderFloat("SP threshold", reinterpret_cast<float*>(&p.spThreshold),
                                  0.0f, 1.0f, "%.2f");
        any |= ImGui::SliderFloat("Recall HP%", reinterpret_cast<float*>(&p.hpRecallThreshold),
                                  0.0f, 0.5f, "%.2f");
        any |= ImGui::DragInt("Recall stable (ms)", &p.hpRecallStableMs, 100, 500, 10000);
        any |= ImGui::DragInt("Pot cooldown (ms)", &p.cooldownMs, 50, 200, 3000);
        any |= ImGui::DragInt("Confirm frames", &p.confirmFrames, 1, 1, 5);
        if (any) markDirty();
    }

    if (ImGui::CollapsingHeader("Combat", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& c = draft_.combat;
        bool any = false;
        any |= ImGui::DragInt("Cycle (sec)", &c.cycleDurationSec, 1, 30, 1800);
        any |= ImGui::DragInt("Repick min dwell (ms)", &c.repickMinDwellMs, 100, 500, 10000);
        any |= ImGui::DragInt("Repick max dwell (ms)", &c.repickMaxDwellMs, 100, 1000, 60000);
        any |= ImGui::DragInt("Sweep r min", &c.attackRadiusMin, 5, 20, 400);
        any |= ImGui::DragInt("Sweep r max", &c.attackRadiusMax, 5, 20, 400);
        any |= ImGui::Checkbox("Wait MP gate", &c.waitMpGate);
        any |= ImGui::SliderFloat("MP gate threshold",
                                  reinterpret_cast<float*>(&c.waitMpGateThreshold),
                                  0.0f, 1.0f, "%.2f");
        if (any) markDirty();
    }

    if (ImGui::CollapsingHeader("Buffs")) {
        for (size_t i = 0; i < draft_.combat.buffs.size(); ++i) {
            auto& b = draft_.combat.buffs[i];
            ImGui::PushID((int)i);
            bool any = false;
            char label[32]; snprintf(label, sizeof(label), "Buff %zu enabled", i);
            any |= ImGui::Checkbox(label, &b.enabled);
            int vk = b.key;
            any |= ImGui::DragInt("VK code", &vk, 1, 0x70, 0x7B);
            b.key = static_cast<WORD>(vk);
            any |= ImGui::DragInt("Cast delay (ms)", &b.castDelayMs, 50, 100, 5000);
            any |= ImGui::Checkbox("Right-click after", &b.rightClickAfter);
            ImGui::Separator();
            ImGui::PopID();
            if (any) markDirty();
        }
    }

    ImGui::TextDisabled("Calibrate: edit ROIs in config.json (full-screen overlay deferred).");

    if (ImGui::Button("Save now")) {
        loader_.save(configPath_, draft_);
        dirty_ = false;
    }
    ImGui::SameLine();
    ImGui::TextDisabled(dirty_ ? "(unsaved)" : "(saved)");

    ImGui::End();
}

void MainWindow::renderFrame() {
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
