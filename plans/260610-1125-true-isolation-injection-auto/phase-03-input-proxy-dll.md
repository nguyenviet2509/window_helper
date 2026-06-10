---
phase: 3
title: pt_input_proxy.dll — state + DirectInput8 hooks
status: pending
priority: P0
effort: 2d
---

# Phase 3 — pt_input_proxy.dll

## Context
DLL chạy trong PT.exe. Hook input APIs, fabricate state từ shared mem read.

## Files
- `src/injection/dll/dllmain.cpp` (~80 LOC)
- `src/injection/dll/hooks-dinput.cpp` (~250 LOC)
- `src/injection/dll/hooks-user32.cpp` (~150 LOC)
- `src/injection/dll/fabricated-state.h` (~50 LOC)
- `src/injection/dll/CMakeLists.txt`

## Build target
```cmake
add_library(pt_input_proxy SHARED
    src/injection/dll/dllmain.cpp
    src/injection/dll/hooks-dinput.cpp
    src/injection/dll/hooks-user32.cpp
    src/injection/hook-engine.cpp  # shared with host
    src/injection/peb-hide.cpp
    src/ipc/ipc-server.cpp
)
target_link_libraries(pt_input_proxy PRIVATE minhook user32 kernel32)
# Output: proxy_{random}.dll
```

## FabricatedState (matches IPC layout)

```cpp
struct FabricatedState {
    // Mouse
    LONG  mouseX, mouseY;      // absolute screen coords
    DWORD mouseButtons;        // bit 0..4 = L/R/M/X1/X2 down

    // Keyboard
    uint8_t keys[256];         // VK_* → 0x80 if pressed

    // Meta
    uint64_t tickWritten;      // GetTickCount64 from host
    uint32_t version;          // protocol version
    uint32_t flags;            // bit 0 = enabled, bit 1 = drag mode

    // Padding
    uint8_t pad[64];
};
static_assert(sizeof(FabricatedState) <= 4096);
```

## DllMain

```cpp
BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        // 1. PEB unlink self
        peb_hide(hMod);
        // 2. Init IPC (open shared mem by name based on PID)
        ipc::startServer();
        // 3. Install hooks
        installHooks();
        // 4. Start hook watchdog
        inj::HookEngine::instance().startWatchdog();
    }
    return TRUE;
}
```

## Hooks: DirectInput8

### Detect target functions
- Wait until PT loads `dinput8.dll` → poll `GetModuleHandleW(L"dinput8.dll")` mỗi 100ms từ DllMain thread.
- Hook `DirectInput8Create` để intercept device creation → khi PT tạo mouse/keyboard device → grab vtable → hook `GetDeviceState` / `GetDeviceData` của device đó.

```cpp
HRESULT WINAPI MyDirectInput8Create(HINSTANCE inst, DWORD ver, REFIID riid,
                                     LPVOID* ppv, LPUNKNOWN punkOuter) {
    HRESULT r = origDirectInput8Create(inst, ver, riid, ppv, punkOuter);
    if (SUCCEEDED(r) && *ppv) {
        wrapDirectInput8(*ppv);  // hook CreateDevice on this instance
    }
    return r;
}
```

### Hook IDirectInputDevice8::GetDeviceState
```cpp
HRESULT STDMETHODCALLTYPE MyGetDeviceState(
    IDirectInputDevice8W* This, DWORD cbData, LPVOID lpvData) {
    HRESULT r = origGetDeviceState(This, cbData, lpvData);
    if (SUCCEEDED(r)) {
        DeviceType t = getDeviceType(This);  // mouse/keyboard
        auto& state = g_fabricated;          // shared mem mapped
        if (state.flags & 1) {  // enabled
            if (t == DeviceType::Mouse && cbData == sizeof(DIMOUSESTATE2)) {
                fabricateMouse((DIMOUSESTATE2*)lpvData, state);
            } else if (t == DeviceType::Keyboard && cbData == 256) {
                fabricateKeyboard((uint8_t*)lpvData, state);
            }
        }
    }
    return r;
}

void fabricateMouse(DIMOUSESTATE2* out, const FabricatedState& s) {
    // Cumulative delta — PT typically reads relative; we set X/Y to delta
    static LONG lastX = s.mouseX, lastY = s.mouseY;
    out->lX = s.mouseX - lastX;
    out->lY = s.mouseY - lastY;
    lastX = s.mouseX; lastY = s.mouseY;
    out->rgbButtons[0] = (s.mouseButtons & 1) ? 0x80 : 0;
    out->rgbButtons[1] = (s.mouseButtons & 2) ? 0x80 : 0;
    out->rgbButtons[2] = (s.mouseButtons & 4) ? 0x80 : 0;
}

void fabricateKeyboard(uint8_t* out256, const FabricatedState& s) {
    // Overlay fabricated keys ON TOP of real state? No — replace.
    // But: user keystrokes for PT KHÔNG bao giờ reach (real cursor/input not on PT).
    // → an toàn replace toàn bộ.
    memcpy(out256, s.keys, 256);
}
```

## Hooks: user32 (cursor + key state)

```cpp
BOOL WINAPI MyGetCursorPos(LPPOINT pt) {
    if (pt && (g_fabricated.flags & 1)) {
        pt->x = g_fabricated.mouseX;
        pt->y = g_fabricated.mouseY;
        return TRUE;
    }
    return origGetCursorPos(pt);
}

SHORT WINAPI MyGetAsyncKeyState(int vk) {
    if (g_fabricated.flags & 1 && vk >= 0 && vk < 256) {
        return (g_fabricated.keys[vk] & 0x80) ? (SHORT)0x8000 : 0;
    }
    return origGetAsyncKeyState(vk);
}

SHORT WINAPI MyGetKeyState(int vk) { /* similar */ }
BOOL WINAPI MyGetKeyboardState(PBYTE state) {
    BOOL r = origGetKeyboardState(state);
    if (r && (g_fabricated.flags & 1)) {
        memcpy(state, g_fabricated.keys, 256);
    }
    return r;
}
```

## Hook install order
```cpp
void installHooks() {
    auto& he = inj::HookEngine::instance();
    he.install("user32.GetCursorPos",     GetProcAddress("user32", "GetCursorPos"),
               MyGetCursorPos, &origGetCursorPos);
    he.install("user32.GetAsyncKeyState", ..., MyGetAsyncKeyState, &origGetAsyncKeyState);
    he.install("user32.GetKeyState",      ..., MyGetKeyState,      &origGetKeyState);
    he.install("user32.GetKeyboardState", ..., MyGetKeyboardState, &origGetKeyboardState);

    // dinput8 hooked after PT loads it
    spawnDInputWaiter();
}

void spawnDInputWaiter() {
    std::thread([]{
        while (true) {
            HMODULE h = GetModuleHandleW(L"dinput8.dll");
            if (h) {
                auto p = (DirectInput8Create_t)GetProcAddress(h, "DirectInput8Create");
                if (p) {
                    inj::HookEngine::instance().install(
                        "dinput8.DI8Create", p, MyDirectInput8Create, &origDirectInput8Create);
                    return;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }).detach();
}
```

## Todo
- [ ] dllmain + init sequence
- [ ] fabricated-state.h
- [ ] DirectInput8Create hook + device wrapper
- [ ] GetDeviceState / GetDeviceData hooks
- [ ] user32 hooks (GetCursorPos / GetAsyncKeyState / GetKeyState / GetKeyboardState)
- [ ] CMakeLists target pt_input_proxy
- [ ] Test trên test exe (poll DirectInput → verify fabricated state)

## Success criteria
- Test exe poll `GetDeviceState(mouse)` → trả về fabricated lX/lY, button mask.
- Test exe `GetCursorPos()` → fabricated coords.
- DLL load trong PT (sau Phase 2 inject) không crash PT.

## Risks
- PT có thể cache device pointer rồi không call CreateDevice lại → spawnDInputWaiter mất chance hook. Mitigation: hook IDirectInput8::CreateDevice trên instance returned từ DI8Create (cover all device creations).
- PT dùng DInput7 hoặc DInput legacy → check trong Phase 0; nếu yes, thêm hook tương ứng.
- DIMOUSESTATE vs DIMOUSESTATE2 (DX 7 vs 8) — handle cả 2 size.
