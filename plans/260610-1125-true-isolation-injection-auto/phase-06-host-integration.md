---
phase: 6
title: Host integration — InjectionInputBackend + PID-based lifecycle
status: pending
priority: P1
effort: 1.5d
---

# Phase 6 — Host integration

## Context
Wire injector + IPC + lifecycle manager. Đổi từ HWND-centric sang PID-centric (vì DLL state per process).

## Files
- `src/input/injection-input-backend.h/.cpp` (~200 LOC) — implements IInputBackend
- `src/state/per-process-context.h` (~70 LOC)
- `src/state/window-lifecycle-manager-pid.h/.cpp` (~250 LOC) — PID-based variant
- `src/main-injection.cpp` (~200 LOC)

## InjectionInputBackend

```cpp
class InjectionInputBackend : public IInputBackend {
public:
    InjectionInputBackend();
    bool requiresForeground() const override { return false; }  // KEY DIFF

    void setTarget(HWND hwnd) override;   // resolves HWND → PID → IpcClient

    bool sendKeyDown(WORD vk) override;
    bool sendKeyUp(WORD vk) override;
    bool sendKeyTap(WORD vk) override;
    bool sendMouseMove(int x, int y) override;
    bool sendMouseDown(MouseButton b) override;
    bool sendMouseUp(MouseButton b) override;
    bool sendMouseClick(int x, int y, MouseButton b) override;
    bool sendMouseDrag(int x1, int y1, int x2, int y2, MouseButton b) override;

private:
    DWORD currentPid_ = 0;
    std::map<DWORD, std::unique_ptr<IpcClient>> clients_;
    std::map<HWND, DWORD> hwndToPid_;
    FabricatedState localState_{};   // built up via sendKey/sendMouse calls
    void flush();                     // write localState_ to current client
};
```

State machine: each `sendKeyDown(VK_X)` → set `localState_.keys[VK_X] = 0x80` → flush. `sendKeyUp` → clear. PT poll → reads new state.

Mouse click sequence:
- `sendMouseDown(L)`: set bit 0, set mouseX/Y current, increment tickWritten, flush.
- (wait 30-80ms human-like)
- `sendMouseUp(L)`: clear bit 0, flush.

PT đọc giữa 2 flush → thấy mouse down at (x,y) → register click.

## PerProcessContext

```cpp
struct PerProcessContext {
    DWORD pid = 0;
    HWND mainHwnd = nullptr;       // primary window of process
    std::wstring procPath;
    int index = 0;
    std::string profileName = "Default";
    AppConfig cfg;
    ConfigBus bus;

    std::unique_ptr<WgcCapture> capture;
    std::unique_ptr<VisionPipeline> vision;
    std::unique_ptr<OutputGate> gate;
    std::unique_ptr<CaptureHealthFsm> health;
    std::unique_ptr<CombatFsm> combat;
    std::unique_ptr<PotRefillScheduler> refill;
    std::unique_ptr<ActionDispatcher> dispatcher;
    std::unique_ptr<InputScheduler> sched;
    std::unique_ptr<InjectionInputBackend> backend;   // 1 backend per process

    bool injected = false;
};
```

## WindowLifecycleManager (PID variant)

```cpp
class WindowLifecycleManagerPid {
public:
    using Initializer = std::function<bool(PerProcessContext&)>;
    using Teardown    = std::function<void(PerProcessContext&)>;
    using OnAdded     = std::function<void(PerProcessContext*)>;
    using OnRemoved   = std::function<void(DWORD, int)>;

    // Same API as HWND variant, but iterates ToolHelp32 / EnumProcesses,
    // filters by exe name "PristonTale.exe" or similar.

    void enumerate(std::vector<DWORD>& outPids);
    // ... similar lifecycle as Phase 1 plan 0928
};
```

Process detection:
- `CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS)` → walk processes.
- Filter exe name match "priston" (case-insensitive).
- For each PID: `EnumWindows` → find top-level visible HWND with matching PID via `GetWindowThreadProcessId`.

## main-injection.cpp wiring

```cpp
// Skeleton — like main-coexist but PID-based + Injector
ManualMapInjector injector;
auto initCtx = [&](PerProcessContext& ctx) -> bool {
    // 1. Inject DLL
    std::wstring dllPath = exeDir() / randomizeDllName();  // copy original to TEMP with random name
    if (!injector.inject(ctx.pid, dllPath)) {
        LOG_ERROR("Inject fail PID %lu", ctx.pid);
        return false;
    }
    ctx.injected = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));  // let DLL init

    // 2. Backend
    ctx.backend = std::make_unique<InjectionInputBackend>();
    ctx.backend->setTarget(ctx.mainHwnd);  // resolves to PID

    // 3. Standard pipeline (vision/combat/refill — like plan 0928 but uses backend.injection)
    ctx.gate = std::make_unique<OutputGate>();
    ctx.gate->setTarget(ctx.mainHwnd);
    ctx.gate->setRequireForeground(false);  // KEY DIFF

    Humanizer human;
    ctx.sched = std::make_unique<InputScheduler>(*ctx.backend, human, *ctx.gate,
                                                  /* no arbiter */, /* no pause */, ctx.mainHwnd);
    ctx.sched->start();

    ctx.combat = std::make_unique<CombatFsm>(*ctx.sched, ctx.mainHwnd, ctx.cfg.combat);
    ctx.refill = std::make_unique<PotRefillScheduler>(*ctx.sched, *ctx.gate, ctx.mainHwnd, ctx.cfg.refill);
    ctx.dispatcher = std::make_unique<ActionDispatcher>(*ctx.sched, *ctx.combat, ctx.cfg);
    ctx.dispatcher->setRefillScheduler(ctx.refill.get());

    ctx.capture = std::make_unique<WgcCapture>();
    ctx.capture->start(ctx.mainHwnd);
    ctx.vision = std::make_unique<VisionPipeline>(*ctx.capture, hp, mp, sp);
    ctx.vision->setCallback(/* same */);
    ctx.vision->start();
    return true;
};
// ... teardown reverse (vision stop, capture stop, sched stop, backend disconnect)
```

## InputScheduler change (backward compat)
- Plan 0928 đã thiết kế optional Arbiter + PauseGate. Plan này pass `nullptr` cho cả 2 → scheduler skip foreground acquire + pause check → direct flush via backend.
- `requiresForeground() == false` → InputScheduler không cần Arbiter slot.

## Todo
- [ ] injection-input-backend.h/.cpp
- [ ] per-process-context.h
- [ ] window-lifecycle-manager-pid.h/.cpp
- [ ] main-injection.cpp
- [ ] InputScheduler accept nullptr arbiter/pause (verify backward compat from plan 0928)

## Success criteria
- 1 PT process: inject + capture + 1 fabricated click → PT register click (verify combat skill fires).
- Cursor system KHÔNG di chuyển 1 pixel (verify với GetCursorPos external).
- 3 PT process song song: 3 inject, 3 IPC, 3 contexts independent.

## Risks
- HWND ↔ PID mapping race: PT có thể có multiple top-level windows; pick first visible.
- DLL inject mất 100-500ms; lifecycle init phải account → bump init timeout 2s.
- Inject fail (XC3 active scan during inject) → retry với delay + alternate technique. Phase 5 hardening reduce probability.
