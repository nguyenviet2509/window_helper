---
phase: 1
title: DLL hook engine (MinHook wrapper, randomized trampoline)
status: pending
priority: P0
effort: 2d
---

# Phase 1 — Hook engine

## Context
Wrapper trên MinHook để hook arbitrary function với:
- Per-hook trampoline byte randomization (anti pattern scan)
- Hook re-apply nếu XC3 unhook
- Thread-safe enable/disable

## Files
- `src/injection/hook-engine.h` (~80 LOC)
- `src/injection/hook-engine.cpp` (~250 LOC)
- vcpkg: thêm `minhook` vào manifest.

## API

```cpp
namespace inj {

class HookEngine {
public:
    static HookEngine& instance();

    // Install hook. Returns handle for later remove/refresh.
    template<typename Fn>
    bool install(const char* tag, void* target, Fn detour, Fn* original);

    bool remove(const char* tag);
    void removeAll();

    // Self-monitor: spawn thread check hook integrity mỗi 2s,
    // re-install nếu detect unhook (XC3 patch back).
    void startWatchdog();
    void stopWatchdog();

private:
    struct HookEntry {
        std::string tag;
        void* target;
        void* detour;
        void* trampoline;
        std::vector<uint8_t> origBytes;  // first 16 bytes original
        bool active;
    };
    std::mutex mu_;
    std::map<std::string, HookEntry> hooks_;
    std::atomic<bool> watchdogRunning_{false};
    std::thread watchdogTh_;
};

} // namespace inj
```

## Trampoline randomization
MinHook tự generate trampoline standard. Để né pattern scan:
- Sau `MH_CreateHook` thành công, get trampoline addr → patch leading nops với random byte order (vẫn equivalent semantic).
- Padding bytes giữa hooks → fill random.

→ Trampoline mỗi process inject sẽ khác nhau byte pattern.

## Watchdog
```cpp
void HookEngine::watchdogLoop() {
    while (watchdogRunning_) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& [tag, h] : hooks_) {
            if (!h.active) continue;
            // Read first 5 bytes của target. Nếu không phải JMP của mình → restore.
            uint8_t curBytes[5];
            memcpy(curBytes, h.target, 5);
            if (!isOurJmp(curBytes, h.trampoline)) {
                LOG_WARN("Hook %s unhooked, restoring", tag.c_str());
                MH_DisableHook(h.target);
                MH_EnableHook(h.target);
            }
        }
    }
}
```

## Usage example
```cpp
typedef HRESULT (WINAPI *GetDeviceState_t)(IDirectInputDevice8W*, DWORD, LPVOID);
GetDeviceState_t origGetDeviceState = nullptr;

HRESULT WINAPI MyGetDeviceState(IDirectInputDevice8W* dev, DWORD sz, LPVOID buf) {
    HRESULT r = origGetDeviceState(dev, sz, buf);
    // Fabricate buf based on FabricatedState từ shared mem.
    // ...
    return r;
}

void installHooks() {
    void* target = getDirectInputGetDeviceStateAddr();
    inj::HookEngine::instance().install("DI8::GetDeviceState",
        target, &MyGetDeviceState, &origGetDeviceState);
}
```

## Todo
- [ ] Add minhook to vcpkg.json.
- [ ] hook-engine.h/.cpp.
- [ ] Trampoline randomize routine.
- [ ] Watchdog thread.
- [ ] Unit test trên LoadLibraryW (hook + verify call).

## Success criteria
- Hook MessageBoxW trong test exe: 100% fire.
- Watchdog re-install sau manual VirtualProtect+memcpy unhook (simulate XC3).
- 10 hook đồng thời không conflict.

## Risks
- MinHook không thread-safe per-target khi đang re-hook. Watchdog dùng disable+enable thay vì uninstall.
- Trampoline addr fixed range trong MinHook → có thể detect bằng range scan. Defer mitigation Phase 5.
