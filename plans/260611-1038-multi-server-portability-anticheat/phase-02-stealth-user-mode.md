# Phase 2 — Stealth user-mode

## Context
- Plan: [plan.md](plan.md)
- Related: [260530-2118-anti-detect-tier1](../260530-2118-anti-detect-tier1/plan.md) — overlap nhẹ, tier1 focus XC3 mouse path. Phase này focus identity hiding.

## Overview
- **Priority**: P1 (gate bằng test thực tế trên server đích)
- **Status**: pending
- Vá lỗ hổng identity user-mode: window class random, exe rename, backend selectable, hide UI khỏi taskbar/Alt-Tab.

## Key insights
- Class name `WindowHelperUI` ([src/main.cpp:39](../../src/main.cpp#L39)) là signature dễ scan nhất.
- `IInputBackend` đã abstract → backend selector ở UI/config, không refactor lõi.
- PostMessage backend đã có sẵn nhưng note PT từ chối ([src/main.cpp:124](../../src/main.cpp#L124)) — vẫn worth retry vì server khác có thể nhận.

## Requirements

**Functional:**
- Window class: configurable string, default random per-build (GUID-style).
- Exe rename: `build.ps1` accept `-ExeName` param → output `{name}.exe`.
- Backend selector: config field `input.backend = "auto" | "sendinput" | "postmessage"`. Auto = thử PostMessage trước, fallback SendInput nếu input không tới được game.
- Hide UI: option `ui.hideFromTaskbar = true` → no taskbar entry, no Alt-Tab. Toggle tray-only mode.
- Single-instance mutex: hash với exe name để nhiều build chạy được song song.

**Non-functional:**
- Không break existing UX khi tắt stealth (default off cho dev).

## Architecture
```
build.ps1 -ExeName WinHelp_v2  →  WinHelp_v2.exe (class name random embedded as resource)

config.json
└── stealth (NEW)
    ├── windowClassName     // "" = use built-in default; "auto" = random per-run
    ├── hideFromTaskbar     // bool
    └── input.backend       // "auto"|"sendinput"|"postmessage"

src/
├── main.cpp                // read stealth config, register class with random name
├── core/
│   └── stealth-config.{h,cpp}   // NEW — random name gen, mutex hashing
└── input/
    └── backend-selector.{h,cpp} // NEW — auto-detect best backend
```

## Related files

**Modify:**
- [build.ps1](../../build.ps1) — accept `-ExeName`, patch CMake target rename.
- [src/main.cpp](../../src/main.cpp) — replace `kMainWindowClass` hardcode; mutex name include exe hash; hideFromTaskbar logic.
- [src/state/game-state.h](../../src/state/game-state.h) — add `StealthConfig`.
- [src/config/config-loader.cpp](../../src/config/config-loader.cpp) — parse stealth section.
- [src/ui/main-window.cpp](../../src/ui/main-window.cpp) — apply `WS_EX_TOOLWINDOW` when hide.

**Create:**
- `src/core/stealth-config.h` / `.cpp` (~60 LOC) — random class name, exe hash util.
- `src/input/backend-selector.h` / `.cpp` (~80 LOC) — auto-detect: try PostMessage, validate by sending no-op + checking response window state, fallback SendInput.

## Implementation steps

1. **build.ps1 `-ExeName` param** — default `WindowHelper.exe`; pass to CMake `-DTARGET_NAME=...`.
2. **CMakeLists** — `set_target_properties(... OUTPUT_NAME ${TARGET_NAME})`.
3. **`stealth-config`** — generate random class name (GUID); hash exe path for mutex.
4. **main.cpp wire** — read `cfg.stealth.windowClassName`; if "auto" → random per-run; else use config value.
5. **Hide from taskbar** — `WS_EX_TOOLWINDOW` extended style; remove `WS_EX_APPWINDOW`. Tray icon (đã có) làm fallback UI access.
6. **`backend-selector`** — at startup, if `backend=auto`: try PostMessage send harmless key (e.g. NumLock) twice, check if game window proc reachable (`IsWindow` + `GetWindowThreadProcessId`). If response normal → keep PostMessage; else swap SendInput.
7. **UI toggle** — Settings tab: 3 radio buttons backend + checkbox hideFromTaskbar.
8. **Test** — build 2 exe khác name, chạy song song confirm mutex không clash. Test PostMessage→SendInput fallback path.

## Todo
- [ ] build.ps1 `-ExeName` param
- [ ] CMake OUTPUT_NAME from var
- [ ] `stealth-config` module
- [ ] Random/configurable window class
- [ ] Mutex name hash with exe path
- [ ] `WS_EX_TOOLWINDOW` hide-from-taskbar
- [ ] `backend-selector` auto fallback
- [ ] UI Settings tab toggles
- [ ] Test parallel multi-exe
- [ ] **Field test** trên server đích → đo TTL trước/sau

## Success criteria
- Build với `-ExeName Foo` → exe name = `Foo.exe`, window class = random GUID.
- Hide enabled → không xuất hiện Alt-Tab, không taskbar; tray icon vẫn click được.
- Backend `auto` chọn đúng backend khả dụng cho cả mock PT + real PT.
- Tool sống ≥ 1h trên server target có anti-cheat nhẹ (no immediate detect).

## Risks
| Risk | Mitigation |
|---|---|
| GG scan ngoài class name (PE signature, mem) | Phase 2 chỉ là first line. Phase 3 Interception nếu Phase 2 die |
| Auto-fallback sai (PostMessage "có vẻ work" nhưng game ignore silently) | Validation phải check 1 key có visible effect; nếu không chắc → manual select |
| User confuse khi exe đổi tên (config path lookup) | Config resolved cạnh exe (đã có ở main.cpp:96-105) — OK |

## Security
- Random class name là OBFUSCATION không phải security. **Không** giấu được người có khả năng inspect process.
- Document rõ "không bypass kernel-level anti-cheat".

## Next steps
→ Field test server đích. Nếu vẫn die → Phase 3 Interception.
