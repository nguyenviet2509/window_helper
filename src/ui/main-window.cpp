// Dear ImGui + DX11 + Win32 main window. Modeled on ImGui's
// example_win32_directx11/main.cpp, trimmed to what we need.

#include "main-window.h"
#include <d3d11.h>
#include <tchar.h>
#include <wtsapi32.h>
#include <wrl/client.h>
#include <algorithm>
#include <climits>
#include <cstdlib>
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
    // Force-flush bỏ qua debounce: tránh mất sửa đổi cuối nếu user đóng app
    // trong cửa sổ debounce 500ms của flushIfDue().
    if (dirty_) {
        loader_.save(configPath_, draft_);
        dirty_ = false;
    }
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

    // Load preset list cho dropdown ngoài (chọn preset không cần mở calibration).
    calibration_.refreshPresets();

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
    lastSavedItemId_ = lastEditedItemId_;
    lastSavedAt_ = now;
    if (onConfigChanged_) onConfigChanged_(*snap);
}

bool MainWindow::editField(bool changed) {
    if (changed) {
        lastEditedItemId_ = (unsigned int)ImGui::GetItemID();
        markDirty();
    }
    drawSavedHint();
    return changed;
}

void MainWindow::drawSavedHint() {
    if (lastSavedItemId_ == 0) return;
    if ((unsigned int)ImGui::GetItemID() != lastSavedItemId_) return;
    auto age = std::chrono::steady_clock::now() - lastSavedAt_;
    if (age >= std::chrono::milliseconds(savedHintMs_)) return;
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.4f, 1.0f), "Đã lưu");
}

void MainWindow::drawSettingsPanel() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    auto size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Settings", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    // Thu gọn ô nhập (DragInt/DragFloat/InputText) còn ~110px để label tiếng Việt dài
    // có chỗ hiển thị đầy đủ. ImGui default cho widget chiếm phần lớn dòng.
    ImGui::PushItemWidth(110.0f);

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

    // Nút Hiệu chỉnh đặt riêng dòng đầu để dễ thấy. Đổi server PT khác → vào đây
    // để chỉnh lại vùng quét HP/MP/SP + lấy mẫu màu thanh.
    ImVec4 calColor(0.15f, 0.35f, 0.70f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, calColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.45f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.10f, 0.30f, 0.65f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    if (ImGui::Button("Hiệu chỉnh nhận diện HP/MP/SP (cho server khác)", ImVec2(-1, 32))) {
        if (calibration_.isOpen()) calibration_.close();
        else calibration_.open();
    }
    ImGui::PopStyleColor(4);
    // Hiển thị tên preset đang dùng (rỗng = chưa nạp preset nào, dùng config gốc).
    const std::string& cur = calibration_.currentPresetName();
    if (!cur.empty()) {
        ImGui::TextColored(ImVec4(0.1f, 0.6f, 0.9f, 1.0f),
                           "Đang dùng cấu hình: %s", cur.c_str());
    } else {
        ImGui::TextDisabled("Chưa nạp cấu hình preset (dùng config gốc)");
    }
    // Quick dropdown chọn preset ngay tại main panel (khỏi mở calibration).
    const auto& presets = calibration_.presets();
    const char* curLabel = cur.empty() ? "(chọn preset)" : cur.c_str();
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::BeginCombo("Chọn nhanh preset", curLabel)) {
        for (const auto& name : presets) {
            bool sel = (name == cur);
            if (ImGui::Selectable(name.c_str(), sel)) {
                if (calibration_.loadPresetByName(name, draft_.vision)) {
                    markDirty();
                }
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Tải lại")) calibration_.refreshPresets();
    ImGui::Separator();

    bool combatOn = draft_.combat.enabled;
    if (editField(ImGui::Checkbox("AUTO (F8 — Đánh quái)", &combatOn))) {
        draft_.combat.enabled = combatOn;
        if (onCombatToggle_) onCombatToggle_(combatOn);
    }
    bool buffOn = draft_.combat.buffEnabled;
    if (editField(ImGui::Checkbox("Bật Buff (F9)", &buffOn))) {
        draft_.combat.buffEnabled = buffOn;
        if (onBuffToggle_) onBuffToggle_(buffOn);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
        "Master gate cho toàn bộ buff. Tắt → bot không cast bất kỳ buff nào,\n"
        "kể cả các slot bật ở mục Buff dưới.");

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
        editField(percentInput("Ngưỡng hồi HP (%)", &p.hpThreshold, 100.0f));
        editField(percentInput("Ngưỡng hồi MP (%)", &p.mpThreshold, 100.0f));
        editField(percentInput("Ngưỡng hồi SP (%)", &p.spThreshold, 100.0f));
    }

    if (ImGui::CollapsingHeader("Đánh quái", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& c = draft_.combat;

        // Spam skill mode: bỏ qua mob targeting, chỉ right-click lặp tại safe spot.
        editField(ImGui::Checkbox("Spam skill (bỏ qua mob, right-click safe spot)", &c.spamSkillEnabled));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(
            "Bật → chuột tự về safe spot, right-click theo interval bên dưới.\n"
            "Sau buff cycle, click kế tiếp = F1 tap + right-click.\n"
            "Random 5-10s fire 1 left-click humanizer ±10px để fake người thật.\n"
            "Pot/refill/buff vẫn chạy bình thường.");
        if (c.spamSkillEnabled) {
            int iv = c.spamSkillIntervalMs;
            if (editField(ImGui::DragInt("Spam interval (ms)", &iv, 50, 100, 10000))) {
                c.spamSkillIntervalMs = std::clamp(iv, 100, 10000);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Khoảng cách giữa 2 lần right-click.\n"
                "MẸO: set ≈ cooldown skill ingame (đo bằng đồng hồ).\n"
                "Set thấp hơn CD → click thừa khi skill chưa ready (wasted).\n"
                "Set cao hơn CD → bỏ lỡ chu kỳ → DPS giảm.");
        }
        ImGui::Separator();

        // Mob-targeting sliders — chỉ hiển thị khi KHÔNG ở spam mode (không liên quan).
        if (!c.spamSkillEnabled) {
            editField(ImGui::DragInt("Chờ tối thiểu khi đổi mục tiêu (ms)", &c.repickMinDwellMs, 100, 500, 10000));
            editField(ImGui::DragInt("Chờ tối đa khi đổi mục tiêu (ms)", &c.repickMaxDwellMs, 100, 1000, 60000));
            editField(ImGui::DragInt("Khoá đánh sau shift+phải (ms)", &c.engagementLockMs, 100, 1000, 15000));
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Sau khi shift+chuột phải, im lặng X ms để game tự đánh mob;\nthoát sớm nếu phát hiện mob chết.");
            editField(ImGui::DragInt("Dao động khoá đánh (ms)", &c.engagementLockJitterMs, 50, 0, 2000));
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Random hoá độ dài khoá ±jitter để né pattern detect.");
            editField(ImGui::DragInt("Bán kính đánh (min)", &c.attackRadiusMin, 5, 20, 400));
            editField(ImGui::DragInt("Bán kính đánh (max)", &c.attackRadiusMax, 5, 20, 400));
        }
    }

    if (ImGui::CollapsingHeader("Nạp pot từ kho", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& r = draft_.refill;
        editField(ImGui::Checkbox("Bật nạp pot tự động", &r.enabled));

        ImGui::Separator();
        ImGui::TextUnformatted("Thời gian nạp lại (giây) — 0 = tắt");
        editField(ImGui::DragInt("HP mỗi N giây", &r.hp.intervalSec, 5, 0, 7200));
        editField(ImGui::DragInt("SP mỗi N giây", &r.sp.intervalSec, 5, 0, 7200));
        editField(ImGui::DragInt("MP mỗi N giây", &r.mp.intervalSec, 5, 0, 7200));


        ImGui::Separator();
        // Tinh chỉnh: default collapsed — chỉ mở khi user cần đụng tới timing nâng cao.
        if (ImGui::TreeNodeEx("Tinh chỉnh nạp pot##refill", ImGuiTreeNodeFlags_SpanAvailWidth)) {
            editField(ImGui::DragInt("Chờ kho mở (ms)", &r.inventoryOpenDelayMs, 10, 100, 3000));
            editField(ImGui::DragInt("Chờ kho đóng (ms)", &r.inventoryCloseDelayMs, 10, 50, 3000));
            editField(ImGui::DragInt("Chờ chuột di chuyển (ms)", &r.mouseMoveDelayMs, 10, 50, 2000));
            editField(ImGui::DragInt("Chờ sau Shift+N (ms)", &r.postHotkeyDelayMs, 10, 50, 2000));
            editField(ImGui::DragInt("Timeout toàn refill (ms)", &r.refillTimeoutMs, 500, 2000, 60000));
            {
                float thr = (float)(r.hpCriticalAbortThreshold * 100.0);
                if (editField(ImGui::DragFloat("Ngưỡng abort khi HP thấp (%)", &thr, 1.0f, 5.0f, 90.0f, "%.0f%%"))) {
                    r.hpCriticalAbortThreshold = thr / 100.0;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                    "Nếu HP tụt dưới ngưỡng này khi đang nạp pot, hủy nạp → đóng kho → uống pot HP ingame.");
            }
            editField(ImGui::DragInt("Tạm dừng sau khi hủy (ms)", &r.abortBackoffMs, 1000, 5000, 300000));
            ImGui::TreePop();
        }
    }

    if (ImGui::CollapsingHeader("Buff")) {
        for (size_t i = 0; i < draft_.combat.buffs.size(); ++i) {
            auto& b = draft_.combat.buffs[i];
            ImGui::PushID((int)i);
            char label[48]; snprintf(label, sizeof(label), "Bật buff %zu", i);
            editField(ImGui::Checkbox(label, &b.enabled));
            // F1=0x70 ... F12=0x7B; combo index 0..11 maps to VK code.
            static const char* kFnNames =
                "F1\0F2\0F3\0F4\0F5\0F6\0F7\0F8\0F9\0F10\0F11\0F12\0";
            int fnIdx = static_cast<int>(b.key) - 0x70;
            if (fnIdx < 0) fnIdx = 0;
            if (fnIdx > 11) fnIdx = 11;
            if (editField(ImGui::Combo("Mã phím", &fnIdx, kFnNames))) {
                b.key = static_cast<WORD>(0x70 + fnIdx);
            }
            editField(ImGui::DragInt("Animation cast (ms)", &b.animationMs, 10, 200, 3000));
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Thời gian animation cast skill. Phải >= animation thực để không bị cancel.");
            editField(ImGui::DragInt("Delay chuột phải (ms)", &b.rightClickDelayMs, 5, 0, 500));
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Delay từ bấm F đến right-click. Chờ game đổi cursor self-target.");
            editField(ImGui::DragInt("Gap sang buff sau (ms)", &b.postBuffGapMs, 5, 0, 1000));
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Khoảng đệm an toàn trước khi bấm buff kế tiếp.");
            editField(ImGui::DragInt("Chu kỳ rebuff (giây)", &b.rebuffIntervalSec, 5, 10, 7200));
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Cast lại buff này sau N giây. Đặt theo duration thực của buff in-game.\n"
                "Quá thấp (< 60s) sẽ liên tục ngắt đánh quái.");
            if (b.enabled && b.rebuffIntervalSec < 60) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                    "Canh bao: chu ky < 60s se ngat danh lien tuc.");
            }
            editField(ImGui::Checkbox("Chuột phải sau buff", &b.rightClickAfter));
            ImGui::Separator();
            ImGui::PopID();
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

    // Capture content bottom Y trước khi End() để main loop tự-resize chiều cao
    // window OS cho khớp content. ImGui::GetCursorPosY() = window-local coord.
    lastContentBottomY_ = ImGui::GetCursorPosY();

    ImGui::PopItemWidth();
    ImGui::End();
}

void MainWindow::onResize(unsigned w, unsigned h) {
    if (!dx_ || !dx_->device) return;
    dx_->resizeW = w;
    dx_->resizeH = h;
    dx_->pending = true;
    // Phân biệt program-resize vs user-resize. Khi auto-fit kích hoạt
    // SetWindowPos, WM_SIZE bắn lại với h = autoClientHLastApplied_ → skip flag.
    // User drag border → h khác → set flag, dừng auto-fit sau đó.
    if (autoClientHLastApplied_ > 0 &&
        std::abs(static_cast<int>(h) - autoClientHLastApplied_) > 4) {
        userResized_ = true;
    }
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
    // Calibration window là cửa sổ riêng — không neo hint per-field ở main panel.
    calibration_.draw(draft_.vision, [this]() { lastEditedItemId_ = 0; markDirty(); });
    // Frame overlay: license info dialog, toast notifications, etc.
    if (onFrameOverlay_) onFrameOverlay_();
    ImGui::Render();

    // Auto-fit chiều cao OS window theo content main panel (calibration là
    // floating window riêng, không tính vào).
    if (!userResized_ && lastContentBottomY_ > 0.0f && hwnd_) {
        int desiredClientH = static_cast<int>(lastContentBottomY_) + 20;
        // Sau khi trần được thiết lập (lần auto-fit đầu): cap chiều cao tại đó.
        // Mở Buff/Tinh chỉnh sẽ vượt trần → ImGui hiện scrollbar trong panel.
        if (autoFitCeilingH_ > 0 && desiredClientH > autoFitCeilingH_) {
            desiredClientH = autoFitCeilingH_;
        }
        RECT cr{};
        GetClientRect(hwnd_, &cr);
        int curClientH = cr.bottom - cr.top;
        // Threshold 4 px để tránh oscillation từ rounding/scrollbar appearance.
        if (std::abs(desiredClientH - curClientH) > 4) {
            RECT wr{};
            GetWindowRect(hwnd_, &wr);
            int ncH = (wr.bottom - wr.top) - curClientH;
            int newH = desiredClientH + ncH;
            // Ghi BEFORE SetWindowPos để onResize không nhầm là user-resized.
            autoClientHLastApplied_ = desiredClientH;
            if (autoFitCeilingH_ == 0) autoFitCeilingH_ = desiredClientH;
            SetWindowPos(hwnd_, nullptr, 0, 0, wr.right - wr.left, newH,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    const float clear[4] = { 0.92f, 0.92f, 0.94f, 1.0f };
    dx_->ctx->OMSetRenderTargets(1, dx_->rtv.GetAddressOf(), nullptr);
    dx_->ctx->ClearRenderTargetView(dx_->rtv.Get(), clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    dx_->swap->Present(1, 0);
}

void MainWindow::requestClose() {
    if (hwnd_) PostMessageW(hwnd_, WM_CLOSE, 0, 0);
}

void MainWindow::renderActivationFrame(std::function<void()> overlay) {
    applyPendingResize();
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    if (overlay) overlay();
    ImGui::Render();
    const float clear[4] = { 0.12f, 0.12f, 0.14f, 1.0f };
    dx_->ctx->OMSetRenderTargets(1, dx_->rtv.GetAddressOf(), nullptr);
    dx_->ctx->ClearRenderTargetView(dx_->rtv.Get(), clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    dx_->swap->Present(1, 0);
}

void MainWindow::toggleCombatRequested() {
    draft_.combat.enabled = !draft_.combat.enabled;
    if (onCombatToggle_) onCombatToggle_(draft_.combat.enabled);
    // Hotkey path: clear field tracking để hint không neo nhầm vào control cũ.
    lastEditedItemId_ = 0;
    markDirty();
}

void MainWindow::toggleBuffRequested() {
    draft_.combat.buffEnabled = !draft_.combat.buffEnabled;
    if (onBuffToggle_) onBuffToggle_(draft_.combat.buffEnabled);
    lastEditedItemId_ = 0;
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
