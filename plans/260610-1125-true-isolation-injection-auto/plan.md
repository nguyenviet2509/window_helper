---
name: True Isolation Injection Auto (DLL inject + XC3 bypass)
slug: true-isolation-injection-auto
created: 2026-06-10
status: pending
type: implementation
estimatedDays: 19-21
totalLoc: ~3500
brainstorm: ../reports/brainstorm-260610-1125-true-cursor-isolation-dll-injection.md
relatedPlans:
  - ../260610-0928-multi-window-coexist-auto/   # safe variant (co-exist, không supersede)
blockedBy: []
blocks: []
riskLevel: HIGH
---

# True Isolation Injection Auto — Plan

**HIGH-RISK variant** của multi-window auto. Inject `pt_input_proxy.dll` vào process PT, hook DirectInput8 + GetCursorPos/GetAsyncKeyState, fabricate input state ở layer game đọc. **System cursor user TUYỆT ĐỐI không bị touch** — đáp ứng yêu cầu "không chiếm chuột tuyệt đối".

⚠️ **Critical risks (user accepted):**
- PT dùng Xingcode3 (kernel AC) — có thể HW ban + account ban
- **PHẢI test trên alt account + alt machine** — KHÔNG test main
- Maintenance ongoing (XC3 update arms race ~1d/month)
- 19-21d initial effort + 2-3d arms race buffer

## Co-exist với plans khác

| Plan | Variant | Status |
|---|---|---|
| 260610-0831-multi-window-auto | Dedicated farm N=2 | superseded |
| 260610-0928-multi-window-coexist-auto | Safe co-exist (pause+restore, flicker) | active — ship as safe variant |
| **260610-1125 (this)** | **High-risk true isolation (inject)** | **active — ship as advanced variant** |

→ 3 exe variants in build: `WindowHelper` (classic), `WindowHelperCoexist` (safe), `WindowHelperInjection` (this).

## Architecture

```
Host App: WindowHelperInjection.exe
  ├─ UI (reuse từ plan 260610-0928, strip pause/cursor controls)
  ├─ WindowLifecycleManager (poll PT processes — PID-based, không HWND-based)
  ├─ PerProcessContext[N] (per PT.exe PID)
  │     ├─ WgcCapture (per HWND của process)
  │     ├─ Vision / Combat FSM / Refill
  │     └─ InjectionInputBackend → IpcClient
  └─ Injector (manual map DLL → PT processes)

PT.exe (1..N processes)
  └─ pt_input_proxy.dll (injected, hidden)
        ├─ Hooks: DirectInput8::GetDeviceState/Data
        │         GetCursorPos / GetAsyncKeyState / GetKeyState / GetKeyboardState
        ├─ FabricatedState (shared mem read)
        ├─ IpcServer (named event listener)
        └─ XC3 bypass: manual map, PEB unlink, PE strip, anti-debug, self-integrity
```

## Stack
C++17, MSVC v143. Thêm dependency: **MinHook** (https://github.com/TsudaKageyu/minhook) — qua vcpkg `minhook`. KHÔNG dùng Detours (license + pattern signature).

## Phases

| # | Phase | File | Effort | Status |
|---|-------|------|--------|--------|
| 0 | **POC**: probe PT input API + minimal inject Notepad | [phase-00](phase-00-poc-input-api-probe.md) | 1.5d | pending |
| 1 | DLL hook engine (MinHook wrapper, randomized trampoline) | [phase-01](phase-01-hook-engine.md) | 2d | pending |
| 2 | Manual map injector + PEB hide + PE strip | [phase-02](phase-02-manual-map-injector.md) | 2.5d | pending |
| 3 | `pt_input_proxy.dll` core: state + DirectInput8 hooks | [phase-03](phase-03-input-proxy-dll.md) | 2d | pending |
| 4 | IPC: shared mem + event protocol | [phase-04](phase-04-ipc-shared-mem.md) | 0.75d | pending |
| 5 | XC3 bypass hardening | [phase-05](phase-05-xc3-bypass.md) | 2d | pending |
| 6 | Host integration: InjectionInputBackend + lifecycle | [phase-06](phase-06-host-integration.md) | 1.5d | pending |
| 7 | UI reuse from coexist plan | [phase-07](phase-07-ui-reuse.md) | 1d | pending |
| 8 | Vision/Combat/Refill wire to new backend | [phase-08](phase-08-vision-combat-wire.md) | 1d | pending |
| 9 | Testing trên PT (alt account + machine) | [phase-09](phase-09-pt-testing.md) | 3d | pending |
| 10 | XC3 arms-race buffer | [phase-10](phase-10-arms-race-buffer.md) | 2-3d | pending |

**Total: 19-21d**

## Key dependencies
- Phase 0 BLOCKING — fail = abort plan, fallback dùng plan 0928.
- Phase 1, 2 độc lập (chạy parallel).
- Phase 3 cần Phase 1 (hook engine).
- Phase 4 độc lập.
- Phase 5 cần Phase 1, 2, 3 (bypass áp dụng lên hook + inject).
- Phase 6 cần Phase 2, 3, 4 (inject + IPC + DLL).
- Phase 7 cần Phase 6.
- Phase 8 cần Phase 6.
- Phase 9 sau cùng (cần Phase 5-8).
- Phase 10 reactive (sau Phase 9 nếu detect).

## Build separation
- `src/CMakeLists.txt`: thêm target `WindowHelperInjection` + SHARED lib `pt_input_proxy`.
- Common sources: capture/vision/combat/refill/config/logger (link cả 3 exe).
- Injection-specific: `injection/*.cpp`, `ipc/*.cpp` — chỉ link WindowHelperInjection.
- DLL target: `pt_input_proxy.dll` (output cùng folder bin/).
- Output naming: `svc_xxxxx_inj.exe` + `proxy_xxxxx.dll` (random anti-AC).

## Success criteria
- 3 PT auto đồng thời 30 phút: system cursor di chuyển 0 pixel (verify bằng cursor tracker).
- User dùng máy bình thường (browser, gaming, typing) song song.
- 4h soak: 0 PT crash, 0 XC3 detect signal.
- Inject success rate > 95% trên 50 PT process start.
- Hook overhead < 5% FPS impact PT.

## Out of scope
- Kernel driver hook (ring0) — defer; user-mode bypass trước.
- Memory write to PT.exe (chỉ hook input read).
- Network/packet cheat.
- Auto-unhook XC3 detection response — defer phase sau.

## Unresolved questions
- PT chính xác poll input qua API nào? Phase 0 verify.
- XC3 version PT hiện tại? Phase 0 probe.
- Có cần kernel driver phase sau? Tùy Phase 9 testing.
- Maintenance owner: ai update bypass khi XC3 patch?
