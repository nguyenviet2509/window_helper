// WindowHelper — Phase 1-5 integration.
// Pipeline: WgcCapture -> VisionPipeline -> ActionDispatcher (Pot + CombatFsm)
//           -> InputScheduler -> Backend -> game window.
// UI: Dear ImGui + DX11 settings panel, tray icon, F8 global hotkey.
// License gate (Phase 4-5): Bootstrap -> dialog or ENTER_MAIN -> periodic verify.

#include <windows.h>
#include <wtsapi32.h>
#include <filesystem>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <string>
#include <cwctype>

#include "capture/wgc-capture.h"
#include "vision/vision-pipeline.h"
#include "input/postmessage-backend.h"
#include "input/send-input-backend.h"
#include "input/input-scheduler.h"
#include "core/humanizer.h"
#include "core/output-gate.h"
#include "core/capture-health-fsm.h"
#include "core/logger.h"
#include "combat/combat-fsm.h"
#include "combat/pot-refill-scheduler.h"
#include "dispatch/action-dispatcher.h"
#include "config/config-loader.h"
#include "config/config-bus.h"
#include "ui/main-window.h"
#include "ui/tray-icon.h"
#include "ui/hotkey-manager.h"
#include "ui/activation-dialog.h"
#include "ui/license-info-dialog.h"
#include "state/game-state.h"
#include "license/license-manager.h"
#include "license/license-types.h"
#include "imgui.h"

namespace {
constexpr const wchar_t* kSingleInstanceMutex =
    L"Global\\{B6C9A2F1-3D5E-4F7A-8B1C-9E2D4F6A8C0B}_WindowHelper";
constexpr const wchar_t* kMainWindowClass = L"WindowHelperUI";

HWND FindTarget() {
    // Exact match cho mock + tên kinh điển trước (nhanh).
    const wchar_t* names[] = { L"Priston Tale", L"PtMockGame", L"PristonTale" };
    for (auto* n : names) if (HWND h = FindWindowW(nullptr, n)) return h;
    // Fallback: quét mọi top-level window, khớp chứa "priston" (case-insensitive).
    // Bắt được biến thể tiêu đề như "Priston Tale 2.8", "Priston Tale - [Server]"...
    struct Ctx { HWND found = nullptr; } ctx;
    EnumWindows([](HWND h, LPARAM lp) -> BOOL {
        if (!IsWindowVisible(h)) return TRUE;
        wchar_t title[256] = {};
        if (GetWindowTextW(h, title, 256) <= 0) return TRUE;
        std::wstring t(title);
        std::transform(t.begin(), t.end(), t.begin(), ::towlower);
        if (t.find(L"priston") != std::wstring::npos) {
            reinterpret_cast<Ctx*>(lp)->found = h;
            return FALSE; // stop enumeration
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));
    return ctx.found;
}

// Build runtime BarConfig (vision pipeline) từ VisionBarCfg (config).
BarConfig MakeBar(const VisionBarCfg& cfg) {
    BarConfig c;
    c.region = cfg.region;
    c.hues   = cfg.hues;
    return c;
}

bool BringOtherInstanceForward() {
    HWND prev = FindWindowW(kMainWindowClass, nullptr);
    if (!prev) return false;
    ShowWindow(prev, SW_RESTORE);
    SetForegroundWindow(prev);
    return true;
}
}  // namespace

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nShow) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Single instance.
    HANDLE mu = CreateMutexW(nullptr, TRUE, kSingleInstanceMutex);
    if (mu && GetLastError() == ERROR_ALREADY_EXISTS) {
        BringOtherInstanceForward();
        return 0;
    }

    // Logger.
    std::filesystem::create_directories("logs");
    Logger::instance().open("logs/WindowHelper.log");
    Logger::instance().setMinLevel(LogLevel::Warn);
    LOG_INFO("WindowHelper starting");

    // Config — resolve cạnh .exe để tránh phụ thuộc CWD lúc launch.
    auto exeDir = []() -> std::filesystem::path {
        wchar_t buf[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        return std::filesystem::path(buf).parent_path();
    };
    AppConfig cfg;
    ConfigLoader loader;
    const std::string configPath = (exeDir() / "config.json").string();
    if (loader.load(configPath, cfg)) {
        LOG_INFO("Loaded config.json");
        if (loader.visionMissing()) {
            LOG_WARN("config.json missing 'vision' section; filled defaults and saved.");
            loader.save(configPath, cfg);
        }
    } else {
        LOG_INFO("config.json missing; using defaults and writing one");
        loader.save(configPath, cfg);
    }
    ConfigBus bus;
    bus.publish(std::make_shared<const AppConfig>(cfg));

    // Target game window (mock during dev).
    HWND target = FindTarget();
    if (!target) {
        MessageBoxW(nullptr, L"Không tìm thấy cửa sổ game (mở PtMockGame hoặc Priston Tale trước).",
                    L"WindowHelper", MB_ICONERROR);
        Logger::instance().close();
        return 1;
    }

    // UI init first — required for the license gate dialog AND the main settings panel.
    // Capture/vision/input init happens AFTER the license gate below to avoid
    // wasting resources when the user exits without activating.
    MainWindow win(bus, loader, configPath);
    if (!win.init(hInst, nShow)) {
        MessageBoxW(nullptr, L"UI init failed.", L"WindowHelper", MB_ICONERROR);
        Logger::instance().close();
        return 3;
    }

    // ===== LICENSE GATE (Phase 4-5) =====
    // Compiled in only when WH_REQUIRE_LICENSE is defined (distribution builds).
    // Dev builds skip this block entirely — no dialog, no periodic verify.
#ifdef WH_REQUIRE_LICENSE
    License::LicenseManager licenseManager;
    License::BootstrapResult licenseBootstrap = licenseManager.Bootstrap();

    if (licenseBootstrap == License::BootstrapResult::EXIT) {
        Logger::instance().close();
        return 0;
    }

    if (licenseBootstrap == License::BootstrapResult::SHOW_DIALOG) {
        ActivationDialog dlg;
        bool activated   = false;
        bool exitClicked = false;
        dlg.SetOnActivated([&](License::CachedLicense lic) {
            licenseManager.AdoptFromDialog(lic);
            activated = true;
        });
        dlg.SetOnExit([&] { exitClicked = true; });
        dlg.Open();

        // Minimal render loop — activation dialog only, no settings panel.
        MSG dmsg{};
        while (!activated && !exitClicked) {
            while (PeekMessageW(&dmsg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&dmsg);
                DispatchMessageW(&dmsg);
                if (dmsg.message == WM_QUIT) {
                    Logger::instance().close();
                    return 0;
                }
            }
            win.renderActivationFrame([&] { dlg.Render(); });
        }
        if (exitClicked) {
            Logger::instance().close();
            return 0;
        }
    }

    licenseManager.StartPeriodicVerify();
#endif // WH_REQUIRE_LICENSE
    // ===== END LICENSE GATE =====

    // Input + gate + scheduler.
    SendInputBackend backend;
    backend.setTarget(target);
    backend.setMousePathEnabled(cfg.combat.enableMousePath);
    OutputGate gate;
    CaptureHealthFsm health;
    gate.setTarget(target);
    gate.setHealth(&health);
    gate.setRequireForeground(backend.requiresForeground());
    Humanizer human;
    InputScheduler sched(backend, human, gate);
    sched.start();

    // Combat + dispatcher + refill.
    CombatFsm combat(sched, target, cfg.combat);
    combat.enable(cfg.combat.enabled);
    PotRefillScheduler refill(sched, gate, target, cfg.refill);
    refill.enable(cfg.refill.enabled);
    ActionDispatcher dispatcher(sched, combat, cfg);
    dispatcher.setRefillScheduler(&refill);
    dispatcher.setLogger([](const char* tag, int prio, WORD vk) {
        Logger::instance().logf(LogLevel::Debug, "[dispatch] %s prio=P%d vk=0x%02X",
                                tag, prio, vk);
    });

    // Capture + vision pipeline.
    WgcCapture cap;
    if (!cap.start(target)) {
        MessageBoxW(nullptr, L"WGC capture failed.", L"WindowHelper", MB_ICONERROR);
#ifdef WH_REQUIRE_LICENSE
        licenseManager.Stop();
#endif
        sched.stop(); Logger::instance().close();
        return 2;
    }
    auto hp = MakeBar(cfg.vision.hp);
    auto sp = MakeBar(cfg.vision.sp);
    auto mp = MakeBar(cfg.vision.mp);
    VisionPipeline pipe(cap, hp, mp, sp);
    std::atomic<uint64_t> lastLogMs{0};
    // win already constructed above; set pointer after pipeline ready.
    MainWindow* uiPtr = nullptr;
    pipe.setCallback([&](const VisionState& s) {
        health.notifyFrameArrived(s.seq);
        dispatcher.onVisionTick(s);
        if (uiPtr) uiPtr->notifyVisionState(s.hpPct, s.mpPct, s.spPct);
        auto nowMs = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now().time_since_epoch()).count();
        if (nowMs - lastLogMs.load() >= 1000) {
            lastLogMs.store(nowMs);
            Logger::instance().logf(LogLevel::Debug,
                "[vision] HP=%.2f MP=%.2f SP=%.2f valid=%d seq=%llu",
                s.hpPct, s.mpPct, s.spPct, (int)s.valid, (unsigned long long)s.seq);
        }
    });
    pipe.start();

    // Health FSM tick thread.
    std::atomic<bool> ticking{true};
    std::thread tickTh([&] {
        while (ticking.load()) {
            health.tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    win.setOnCombatToggle([&](bool on) {
        combat.enable(on);
        Logger::instance().logf(LogLevel::Info, "AUTO toggled -> %s", on ? "ON" : "OFF");
    });
    win.setOnBuffToggle([&](bool on) {
        combat.setBuffEnabled(on);
        Logger::instance().logf(LogLevel::Info, "BUFF toggled -> %s", on ? "ON" : "OFF");
    });
    win.setOnConfigChanged([&](const AppConfig& c) {
        dispatcher.updateConfig(c);
        refill.enable(c.refill.enabled);
        backend.setMousePathEnabled(c.combat.enableMousePath);
        pipe.updateConfig(MakeBar(c.vision.hp), MakeBar(c.vision.mp), MakeBar(c.vision.sp));
        Logger::instance().logf(LogLevel::Info,
            "[config] hot-reload refill.enabled=%d intervalSec hp=%d sp=%d mp=%d vision=updated",
            (int)c.refill.enabled,
            c.refill.hp.intervalSec, c.refill.sp.intervalSec, c.refill.mp.intervalSec);
    });
    uiPtr = &win;
    win.setOnSessionLockChange([&](bool locked) {
        gate.setSessionLocked(locked);
        LOG_INFO(locked ? "Session locked — input blocked"
                        : "Session unlocked — input restored");
    });

    // Tray — "License Info..." item added by Phase 5.
    TrayIcon tray;
    HICON icon = LoadIconW(nullptr, IDI_APPLICATION);
    tray.install(win.hwnd(), icon, L"Window Helper");
    tray.setOnToggleAuto([&] { win.toggleCombatRequested(); });
    tray.setOnShowWindow([&] {
        ShowWindow(win.hwnd(), SW_SHOW); SetForegroundWindow(win.hwnd());
    });
    tray.setOnExit([&] { PostMessageW(win.hwnd(), WM_DESTROY, 0, 0); });

#ifdef WH_REQUIRE_LICENSE
    // License info dialog (stack-allocated; lives for full session lifetime).
    // Passes manager pointer so Render() calls Snapshot() for thread-safety (C1).
    LicenseInfoDialog licInfoDlg(&licenseManager);
    tray.setOnLicenseInfo([&] { licInfoDlg.Open(); });
#endif

    win.attachTray(&tray);
    win.setTarget(target);

    // Global hotkeys.
    HotkeyManager hk;
    hk.registerKey(win.hwnd(), 1, 0, VK_F8, [&] { win.toggleCombatRequested(); });
    hk.registerKey(win.hwnd(), 2, 0, VK_F9, [&] { win.toggleBuffRequested(); });
    win.attachHotkey(&hk);

    // Session lock notification.
    WTSRegisterSessionNotification(win.hwnd(), NOTIFY_FOR_THIS_SESSION);

#ifdef WH_REQUIRE_LICENSE
    // License-lost overlay state — rendered each frame via the frame overlay hook.
    bool  licLostActive    = false;
    float licLostCountdown = 30.0f;

    // Frame overlay: renders license info dialog + license-lost toast on top of
    // the settings panel without modifying drawSettingsPanel().
    win.setOnFrameOverlay([&] {
        // License info popup (tray-triggered).
        licInfoDlg.Render();

        // Check for mid-session revoke/expire (atomic, zero-cost when not lost).
        if (!licLostActive && licenseManager.LicenseLost()) {
            licLostActive    = true;
            licLostCountdown = 30.0f;
            LOG_INFO("License lost mid-session — 30s shutdown countdown started");
        }

        // Toast overlay shown until countdown reaches zero.
        if (licLostActive) {
            ImGuiIO& io = ImGui::GetIO();
            ImGui::SetNextWindowPos(
                ImVec2(io.DisplaySize.x * 0.5f, 10.0f),
                ImGuiCond_Always, ImVec2(0.5f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.88f);
            ImGui::Begin("##LicLost", nullptr,
                         ImGuiWindowFlags_NoDecoration |
                         ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_NoMove);
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
                               "%s", licenseManager.LicenseLostReason().c_str());
            ImGui::Text("Dong app sau %.0f giay...", licLostCountdown);
            ImGui::End();

            licLostCountdown -= io.DeltaTime;
            if (licLostCountdown <= 0.0f) {
                PostMessageW(win.hwnd(), WM_DESTROY, 0, 0);
            }
        }
    });
#endif // WH_REQUIRE_LICENSE

    LOG_INFO("WindowHelper ready");
    int rc = win.runLoop();

    // Cleanup in reverse init order.
    WTSUnRegisterSessionNotification(win.hwnd());
    hk.unregisterAll();
    tray.uninstall();
#ifdef WH_REQUIRE_LICENSE
    licenseManager.Stop();
#endif
    ticking.store(false);
    if (tickTh.joinable()) tickTh.join();
    pipe.stop();
    cap.stop();
    sched.stop();
    LOG_INFO("WindowHelper exiting");
    Logger::instance().close();
    if (mu) { ReleaseMutex(mu); CloseHandle(mu); }
    return rc;
}
