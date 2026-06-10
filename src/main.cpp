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
#include "state/game-state.h"

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
    // Production: chỉ ghi Warn/Error để giữ log gọn khi chạy dài.
    // Đổi sang LogLevel::Info hoặc Debug khi cần điều tra.
    Logger::instance().setMinLevel(LogLevel::Warn);
    LOG_INFO("WindowHelper starting");

    // Config — resolve cạnh .exe để tránh phụ thuộc CWD lúc launch
    // (shortcut Desktop vs double-click khác CWD sẽ đọc/ghi nhầm file).
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

    // Input + gate + scheduler. PT tu choi PostMessage -> dung SendInput foreground.
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
        sched.stop(); Logger::instance().close();
        return 2;
    }
    // WGC frame = 1010x789. HP do tu Paint screenshot.
    auto hp = MakeBar(403, 656, 22, 121, { {0,10}, {170,180} });
    auto sp = MakeBar(383, 675, 11, 102, { {40, 80} });  // top-left (383,675), bottom-right (394,777)
    auto mp = MakeBar(586, 655, 21, 121, { {100,130} });  // top-left (586,655), bottom-right (607,776)
    VisionPipeline pipe(cap, hp, mp, sp);
    // Log gia tri detector moi 1s (tam thoi de debug calibration).
    std::atomic<uint64_t> lastLogMs{0};
    pipe.setCallback([&](const VisionState& s) {
        health.notifyFrameArrived(s.seq);
        dispatcher.onVisionTick(s);
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
    win.setOnBuffToggle([&](bool on) {
        combat.setBuffEnabled(on);
        Logger::instance().logf(LogLevel::Info, "BUFF toggled -> %s", on ? "ON" : "OFF");
    });
    // Hot-reload các config sub-section sau khi UI flush (debounced).
    win.setOnConfigChanged([&](const AppConfig& c) {
        dispatcher.updateConfig(c);
        refill.enable(c.refill.enabled);
        backend.setMousePathEnabled(c.combat.enableMousePath);
        Logger::instance().logf(LogLevel::Info,
            "[config] hot-reload refill.enabled=%d intervalSec hp=%d sp=%d mp=%d",
            (int)c.refill.enabled,
            c.refill.hp.intervalSec, c.refill.sp.intervalSec, c.refill.mp.intervalSec);
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
    win.setTarget(target);

    // Global hotkey F8 -> toggle AUTO.
    HotkeyManager hk;
    hk.registerKey(win.hwnd(), 1, 0, VK_F8, [&] { win.toggleCombatRequested(); });
    hk.registerKey(win.hwnd(), 2, 0, VK_F9, [&] { win.toggleBuffRequested(); });
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
