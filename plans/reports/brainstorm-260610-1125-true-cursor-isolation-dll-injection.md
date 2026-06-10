---
type: brainstorm-report
date: 2026-06-10
slug: true-cursor-isolation-dll-injection
status: approved
relatedPlans:
  - ../260610-0928-multi-window-coexist-auto/   # safe variant (giữ làm fallback)
newPlanSlug: true-isolation-injection-auto
---

# Brainstorm: True cursor isolation via DLL injection + XC3 bypass

## Problem
User yêu cầu "tuyệt đối không chiếm chuột" — cursor user KHÔNG bao giờ bị app touch. Trên Windows, đây là bất khả thi với SendInput user-mode (1 system cursor). Chỉ giải pháp khả thi (không VM): inject DLL vào process PT, fabricate input state ở layer game đọc → system cursor không đổi.

PT dùng Xingcode3 (kernel AC) → injection cần bypass.

## Approaches đã đánh giá lại

| # | Approach | Verdict |
|---|---|---|
| A | Keyboard-only (no mouse SendInput) | **Loại** — PT bắt buộc drag refill + click target |
| B | DLL injection + DirectInput hook + XC3 bypass | **Chosen** |
| C | VM/Hyper-V | Loại đợt này (heavy setup, plan riêng 260531-0857) |
| D | Plan 260610-0928 (pause+restore) | Loại — vẫn flicker cursor khi idle |

## Architecture

```
Host App (WindowHelperInjection.exe) — main control
  ├─ UI tabs (per PT window) — reuse từ plan 260610-0928
  ├─ Vision (WGC capture per HWND) — reuse
  ├─ Combat FSM / Refill — reuse logic, đổi output backend
  ├─ Injector (manual map DLL → PT processes)
  └─ IpcClient (shared memory + event signal per process)

PT.exe (process 1..N)
  └─ Injected: pt_input_proxy.dll
        ├─ Hooks: DirectInput8::GetDeviceState (mouse/keyboard)
        │         DirectInput8::GetDeviceData
        │         GetCursorPos / GetAsyncKeyState / GetKeyState
        ├─ Fabricated state (mouse pos, button mask, key bitmap)
        ├─ IpcServer (shared memory listener)
        ├─ AC bypass: manual map, PE strip, PEB unlink, anti-debug
        └─ Self-monitor: detect unhook by XC3 → re-hook
```

## Key insight: **state injection, không event injection**

PT poll input mỗi frame qua `GetDeviceState(buffer)`. DLL hook trả buffer giả → PT "thấy" mouse ở (x,y), button pressed.

→ System cursor THẬT không thay đổi. User dùng máy bình thường.

## Components mới

### 1. `pt_input_proxy.dll` (injected target)
- Hook table:
  - `IDirectInputDevice8::GetDeviceState`
  - `IDirectInputDevice8::GetDeviceData`
  - `GetCursorPos`
  - `GetAsyncKeyState`
  - `GetKeyState`
  - `GetKeyboardState`
- State store:
  ```cpp
  struct FabricatedState {
      POINT mousePos;
      uint8_t mouseButtons;    // L/M/R/X1/X2
      uint8_t keyBitmap[256];  // VK_* → pressed
      uint64_t tickWritten;    // detect stale
  };
  ```
- IPC: shared memory `PtInputProxy_{PID}` + named event `PtInputProxy_Update_{PID}` → host write state, DLL read on next hook call.

### 2. Injector (host-side)
- **Manual map** (no LoadLibrary):
  - Allocate memory in PT.exe via `VirtualAllocEx`
  - Copy DLL image, fix relocations, resolve imports manually
  - Strip MZ/PE header trong target memory
  - Call DllMain via `CreateRemoteThread` hoặc thread hijack
- Inject point: sau khi PT process stable (5s sau detect), trước khi user login (tránh XC3 init scan ban đầu)
- Stealth name: random module name; KHÔNG list trong PEB Ldr.

### 3. Host IPC client
- Per PT process: 1 shared mem mapping + event handle
- API:
  ```cpp
  class InjectionInputBackend : public IInputBackend {
      bool sendMouseClick(int x, int y, MouseButton btn) override;
      bool sendKeyPress(WORD vk) override;
      void setTarget(HWND hwnd) override;  // resolves to PID → mapping
  };
  ```
- Implementation: write FabricatedState → SetEvent → DLL hook reads on next PT poll.
- **No ForegroundArbiter cần** — input state per-process, không cần PT là foreground.

### 4. XC3 bypass core
Techniques (ordered by importance):
1. **Manual map** (bypass LoadLibrary detection)
2. **PEB module hide** — unlink LDR_DATA_TABLE_ENTRY sau inject
3. **PE header wipe** — zero MZ/PE/section headers trong remote memory
4. **Hook trampoline randomize** — không dùng fixed pattern Detours; rotate stub bytes
5. **Anti-VEH/SEH scan** — wrap hooks trong try/except
6. **Self-integrity** — DLL CRC own .text; nếu XC3 patch → re-restore
7. **Anti-debug** — IsDebuggerPresent, PEB.BeingDebugged, NtQueryInformationProcess(DebugPort)
8. **Optional kernel driver** (DEFER, phase sau): hook ở ring0 để skip user-mode scan. Effort cao + driver sign cert phức tạp.

### 5. Components plan 260610-0928 vẫn dùng
- WindowLifecycleManager (poll PT.exe processes thay vì HWND — adjusted)
- PerWindowContext (per PID)
- Profile manager
- UI tabs (dynamic)
- Vision/Combat/Refill logic (đổi backend interface)
- Calibration UI

### 6. Components plan 260610-0928 KHÔNG cần
- ForegroundArbiter (no FG switch needed)
- PauseGate / UserActivityMonitor (no conflict với user)
- Cursor save/restore (cursor user không bị touch)
- Phase 0 POC liên quan arbiter/cursor restore (chuyển sang POC injection)

## Phase outline (high-level)

| # | Phase | Effort |
|---|---|---|
| 0 | **POC**: probe PT input API + test minimal DLL inject (Notepad first) | 1.5d |
| 1 | DLL hook engine (MinHook-based, randomized trampoline) | 2d |
| 2 | Manual map injector + PEB hide + PE strip | 2.5d |
| 3 | `pt_input_proxy.dll` core: state struct + DirectInput8 hooks | 2d |
| 4 | IPC: shared mem + event protocol | 0.75d |
| 5 | XC3 bypass hardening (anti-debug, integrity, anti-scan) | 2d |
| 6 | Host integration: InjectionInputBackend + WindowLifecycleManager adjust to PID-based | 1.5d |
| 7 | Reuse UI from 260610-0928 (strip pause/cursor controls) | 1d |
| 8 | Vision/Combat/Refill wire to new backend | 1d |
| 9 | Testing on PT thật (priority: alt account + machine) | 3d |
| 10 | XC3 arms-race iteration (expect 1-2 round) | 2-3d |

**Total: 19-21d** (20-25d ước tính ban đầu là realistic).

## Build separation
- 3 exe variants:
  - `WindowHelper` (classic, single PT) — giữ
  - `WindowHelperCoexist` (plan 260610-0928, pause+restore) — giữ làm safe variant
  - `WindowHelperInjection` (plan mới) — high-risk variant
- Plus `pt_input_proxy.dll` (target binary).
- Share common: vision/combat/refill/config/logger.
- CMake: thêm target `pt_input_proxy_dll` (SHARED lib).

## Risks (brutal, ranked)

| Risk | Severity | Mitigation |
|---|---|---|
| XC3 detect → HW ban + account ban | **Critical** | Test trên alt account/machine; không test main; backup HWID spoof |
| PT update phá hook | High | Modular hook design; CI smoke test sau mỗi PT patch |
| DLL crash PT process | High | Try/except wrap; fail-safe unhook on exception |
| Reverse engineer PT input API sai → hook không fire | High | Phase 0 POC verify trước commit |
| XC3 update detect mới | Recurring | Plan ongoing maintenance ~1d/month |
| Legal / TOS violation | Critical (user accept) | User aware; document trong README |
| Performance: hook overhead trong PT main loop | Medium | Benchmark; target <5% FPS impact |
| Multiple PT inject race | Medium | Per-process state isolated; test N=3 |

## Success criteria
- 3 PT auto đồng thời, system cursor user KHÔNG di chuyển 1 pixel suốt 30 phút stress.
- User dùng máy hoàn toàn bình thường trong khi auto chạy (verified: web browsing, gaming, typing).
- 4h soak: 0 PT crash, 0 XC3 detect signal.
- Inject success rate > 95% trên 50 PT process start.
- Hook overhead < 5% FPS PT.

## Out of scope đợt 1
- Kernel driver hook (ring0) — defer nếu user-mode bypass đủ
- Network/packet-level cheat
- Memory write to PT.exe (chỉ hook input read)
- Auto-unhook khi XC3 update detect — defer phase sau

## Unresolved questions
- PT chính xác dùng API nào? DirectInput8? RawInput? Cần Phase 0 verify.
- XC3 version PT đang chạy? Cần probe.
- Có chấp nhận kernel driver phase sau (nếu user-mode không đủ)? Cần signed cert hoặc test mode boot.
- Maintenance plan: ai update bypass khi XC3 patch?

## Build separation cuối cùng
- Plan 260610-0928 (coexist pause+restore) ship như safe variant
- Plan mới này = high-risk variant
- User chọn variant theo risk tolerance
- 2 plans co-exist (không supersede)
