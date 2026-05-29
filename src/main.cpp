// WindowHelper — Phase 1-5 integration.
// Pipeline: WgcCapture -> VisionPipeline -> ActionDispatcher (Pot + CombatFsm)
//           -> InputScheduler -> Backend -> game window.
// UI: Dear ImGui + DX11 settings panel, tray icon, F8 global hotkey.

#include <windows.h>
#include <wtsapi32.h>
#include <filesystem>
#include <memory>
#include <thread>
#include <atomic>

#include "capture/wgc-capture.h"
#include "vision/vision-pipeline.h"
#include "input/postmessage-backend.h"
#include "input/input-scheduler.h"
#include "core/humanizer.h"
#include "core/output-gate.h"
#include "core/capture-health-fsm.h"
#include "core/logger.h"
#include "combat/combat-fsm.h"
#include "dispatch/action-dispatcher.h"
#include "config/config-loader.h"
#include "config/config-bus.h"
#include "ui/main-window.h"
#include "ui/tray-icon.h"
#include "ui/hotkey-manager.h"
#include "state/game-state.h"

namespace {
constexpr const wchar_t* kSingleInstanceMutex =
    L"Global\\{B6C9A2F1-3D5E-4F7A-8B1C-9E2D4F6A8C0B}_WindowHelper";
constexpr const wchar_t* kMainWindowClass = L"WindowHelperUI";

HWND FindTarget() {
    const wchar_t* names[] = { L"Priston Tale", L"PtMockGame", L"PristonTale" };
    for (auto* n : names) if (HWND h = FindWindowW(nullptr, n)) return h;
    return nullptr;
}

BarConfig MakeBar(int x, int y, int w, int h, std::vector<HueRange> hues) {
    BarConfig c; c.region.x = x; c.region.y = y; c.region.w = w; c.region.h = h;
    c.region.shape = "rect"; c.hues = std::move(hues);
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
    LOG_INFO("WindowHelper starting");

    // Config.
    AppConfig cfg;
    ConfigLoader loader;
    const std::string configPath = "config.json";
    if (loader.load(configPath, cfg)) {
        LOG_INFO("Loaded config.json");
    } else {
        LOG_INFO("config.json missing; using defaults and writing one");
        loader.save(configPath, cfg);
    }
    ConfigBus bus;
    bus.publish(std::make_shared<const AppConfig>(cfg));

    // Target game window (mock during dev).
    HWND target = FindTarget();
    if (!target) {
        MessageBoxW(nullptr, L"No target window found (run PtMockGame first).",
                    L"WindowHelper", MB_ICONERROR);
        Logger::instance().close();
        return 1;
    }

    // Input + gate + scheduler.
    PostMessageBackend backend;
    backend.setTarget(target);
    OutputGate gate;
    CaptureHealthFsm health;
    gate.setTarget(target);
    gate.setHealth(&health);
    gate.setRequireForeground(backend.requiresForeground());
    Humanizer human;
    InputScheduler sched(backend, human, gate);
    sched.start();

    // Combat + dispatcher.
    CombatFsm combat(sched, target, cfg.combat);
    combat.enable(cfg.combat.enabled);
    ActionDispatcher dispatcher(sched, combat, cfg);
    dispatcher.setLogger([](const char* tag, int prio, WORD vk) {
        Logger::instance().logf(LogLevel::Info, "[dispatch] %s prio=P%d vk=0x%02X",
                                tag, prio, vk);
    });

    // Capture + vision pipeline.
    WgcCapture cap;
    if (!cap.start(target)) {
        MessageBoxW(nullptr, L"WGC capture failed.", L"WindowHelper", MB_ICONERROR);
        sched.stop(); Logger::instance().close();
        return 2;
    }
    auto hp = MakeBar(355, 475, 12, 105, { {0,10}, {170,180} });
    auto sp = MakeBar(383, 475, 12, 105, { {20, 35} });
    auto mp = MakeBar(411, 475, 12, 105, { {100,130} });
    VisionPipeline pipe(cap, hp, mp, sp);
    pipe.setCallback([&](const VisionState& s) {
        health.notifyFrameArrived(s.seq);
        dispatcher.onVisionTick(s);
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

    // UI.
    MainWindow win(bus, loader, configPath);
    if (!win.init(hInst, nShow)) {
        MessageBoxW(nullptr, L"UI init failed.", L"WindowHelper", MB_ICONERROR);
        ticking.store(false); if (tickTh.joinable()) tickTh.join();
        pipe.stop(); cap.stop(); sched.stop(); Logger::instance().close();
        return 3;
    }

    win.setOnCombatToggle([&](bool on) {
        combat.enable(on);
        Logger::instance().logf(LogLevel::Info, "AUTO toggled -> %s", on ? "ON" : "OFF");
    });
    win.setOnSessionLockChange([&](bool locked) {
        gate.setSessionLocked(locked);
        LOG_INFO(locked ? "Session locked — input blocked"
                        : "Session unlocked — input restored");
    });

    // Tray.
    TrayIcon tray;
    HICON icon = LoadIconW(nullptr, IDI_APPLICATION);
    tray.install(win.hwnd(), icon, L"Window Helper");
    tray.setOnToggleAuto([&] { win.toggleCombatRequested(); });
    tray.setOnShowWindow([&] {
        ShowWindow(win.hwnd(), SW_SHOW); SetForegroundWindow(win.hwnd());
    });
    tray.setOnExit([&] { PostMessageW(win.hwnd(), WM_DESTROY, 0, 0); });
    win.attachTray(&tray);

    // Global hotkey F8 -> toggle AUTO.
    HotkeyManager hk;
    hk.registerKey(win.hwnd(), 1, 0, VK_F8, [&] { win.toggleCombatRequested(); });
    win.attachHotkey(&hk);

    // Session lock notification.
    WTSRegisterSessionNotification(win.hwnd(), NOTIFY_FOR_THIS_SESSION);

    LOG_INFO("WindowHelper ready");
    int rc = win.runLoop();

    // Cleanup.
    WTSUnRegisterSessionNotification(win.hwnd());
    hk.unregisterAll();
    tray.uninstall();
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
