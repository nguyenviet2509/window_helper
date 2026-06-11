# Phase 3 — Interception driver backend

## Context
- Plan: [plan.md](plan.md)
- Reference: https://github.com/oblitum/Interception (signed kernel keyboard/mouse driver)

## Overview
- **Priority**: P2 (only if Phase 2 die trên server đích)
- **Status**: DEFERRED — spec giữ làm reference, KHÔNG implement cho tới khi có evidence cần. YAGNI gate: Phase 2 ship + thực sự bị detect/block input trên server target.
- Thêm `InterceptionBackend` implement `IInputBackend`, send input qua kernel driver bypass user-mode hook của GameGuard.

## Key insights
- Interception là driver kernel-mode signed → input đi qua HID layer thật, GG user-mode hook không thấy.
- Driver phải cài 1 lần (UAC). Tool runtime chỉ load `interception.dll` (user-mode wrapper).
- `IInputBackend` đã abstract → chỉ thêm 1 file backend mới, plug vào `backend-selector` từ Phase 2.

## Requirements

**Functional:**
- `InterceptionBackend` đầy đủ method của `IInputBackend`: keyTap, keyDown/Up, left/right/shift+right click, mouseMove.
- Detect driver installed → enable backend; else fallback SendInput + UI message hướng dẫn cài.
- UI button "Cài Interception driver" → mở installer URL hoặc chạy installer kèm.
- Foreground requirement: Interception cần game window focused → reuse `requiresForeground()` = true.

**Non-functional:**
- Không link cứng `interception.dll` lúc build (delay-load) → exe vẫn chạy được khi user chưa cài.
- Logging riêng để debug khi input không tới game.

## Architecture
```
src/input/
├── i-input-backend.h               // existing — không đổi
├── interception-backend.h          // NEW (~30 LOC)
├── interception-backend.cpp        // NEW (~250 LOC)
└── backend-selector.cpp            // MODIFY — add Interception to candidates

third_party/
└── interception/                   // NEW — vendored headers (interception.h) + .lib
    ├── interception.h
    └── interception.lib

vendor/                             // dist-time download của interception.dll
```

Backend candidate order khi `backend=auto`:
1. Interception (nếu driver installed + dll loaded)
2. PostMessage (sanity check responsive)
3. SendInput (fallback cuối)

## Related files

**Modify:**
- [CMakeLists.txt](../../CMakeLists.txt) — add Interception lib path, delay-load flag (`/DELAYLOAD:interception.dll`).
- [src/input/backend-selector.cpp](../../src/input/backend-selector.cpp) (từ Phase 2) — chèn Interception priority.
- [src/state/game-state.h](../../src/state/game-state.h) — backend enum thêm `Interception`.
- [src/ui/main-window.cpp](../../src/ui/main-window.cpp) — radio button "Interception" + install button.
- [package.ps1](../../package.ps1) — bundle `interception.dll` vào dist.

**Create:**
- `src/input/interception-backend.h` / `.cpp` — wrapper convert virtual key → scan code → Interception stroke.
- `third_party/interception/README.md` — version pinned, license note (LGPL).
- `tools/install-interception.ps1` — wrapper chạy `install-interception.exe /install` với UAC elevation.

## Implementation steps

1. **Vendor Interception** — tải release signed, commit `interception.h` + `.lib` vào `third_party/interception/`.
2. **CMake wire** — link delay-load; copy `interception.dll` vào output dir post-build.
3. **`InterceptionBackend` skeleton** — implement `IInputBackend`:
   - ctor: `interception_create_context()`, set filter cho keyboard+mouse.
   - VK → scan code conversion (`MapVirtualKeyW(vk, MAPVK_VK_TO_VSC)`).
   - `sendKeyTap`: `interception_send` 2 strokes (down + up) với sleep `holdMs`.
   - `sendRightClick / sendShiftRightClick / sendLeftClick`: move cursor (`SetCursorPos` to client→screen), gửi mouse stroke. Note Interception KHÔNG đi qua `SetCursorPos` được, phải gửi `INTERCEPTION_MOUSE_MOVE` relative hoặc `MOVE_ABSOLUTE`.
   - `sendMouseMove`: absolute coords via `INTERCEPTION_MOUSE_MOVE_ABSOLUTE`.
4. **Driver detection** — `LoadLibraryW(L"interception.dll")` + `interception_create_context()`. Nếu null → backend unavailable.
5. **Backend-selector update** — thêm Interception vào candidate list.
6. **UI install button** — Settings tab: "Cài Interception driver" → run `tools/install-interception.ps1` elevated, kèm warning "tool sẽ restart".
7. **Test** — bật Interception, attack mob trên mock; so sánh input log với SendInput baseline.
8. **Field test** — server target có GG, đo TTL.

## Todo
- [ ] Vendor Interception headers + lib + dll
- [ ] CMake delay-load setup
- [ ] `InterceptionBackend` keyboard methods
- [ ] `InterceptionBackend` mouse methods (move + click)
- [ ] VK → scan code mapping
- [ ] Driver detect at startup
- [ ] `backend-selector` add Interception priority
- [ ] UI install button + status
- [ ] Package dll vào dist
- [ ] Field test GG server

## Success criteria
- Tool chạy được khi driver chưa cài (delay-load OK, fallback SendInput).
- Driver cài + enabled → mouse cursor di chuyển + click đến đúng tọa độ in-game.
- Shift+right-click attack trigger được engagement lock đúng.
- Tool sống ≥ 1h trên server GG nhẹ-trung mà không detect.

## Risks
| Risk | Mitigation |
|---|---|
| Interception signature đã public, GG mới có thể detect | Phase 4 HW HID làm fallback cuối |
| User UAC từ chối cài driver | Clear error + fallback SendInput tự động |
| `interception.dll` xung đột với app khác cùng dùng driver (rare) | Document; release context khi exit |
| Mouse move absolute coord scale sai trên multi-monitor | Test multi-mon; doc giới hạn |
| License LGPL → linking concern | Delay-load + dll redistribute OK theo LGPL; document |

## Security
- Driver kernel-mode = full system privilege. Reuse signed driver chính chủ, không tự build.
- Document rõ cho user: cài driver = đồng ý chạy code kernel của bên thứ ba.

## Next steps
→ Nếu Phase 3 vẫn die trên GG mạnh → Phase 4 HW HID.
