---
phase: 0
title: POC — probe PT input API + minimal inject feasibility
status: pending
priority: P0
effort: 1.5d
blocking: true
---

# Phase 0 — POC input API probe

## Context
Plan cả 20d phụ thuộc vào 2 giả định chưa verify:
1. PT.exe poll input qua DirectInput8 (hoặc API nào khác).
2. Có thể inject DLL vào PT.exe và XC3 không detect ngay lập tức.

**User-confirmed inputs (2026-06-10):**
- Game name: **Priston Tale** — binary likely `PristonTale.exe` / `priston.exe` / `Priston Tale.exe`; verify trong POC1.
- XC3: **active** trong PT version user chơi → Phase 5 hardening MANDATORY.
- Input API: **unknown** → POC1 auto-detect (xem section bên dưới).

Phase 0 verify cả 2 → fail = abort, fallback plan 0928.

## POC 1: API probe (0.75d)

### Method A — Manual check (15 phút, làm trước)
User-runnable, không cần code:

1. **Tìm PT.exe**:
   - Mở PT bình thường → Task Manager → Details tab → tìm process tên chứa "priston" → ghi nhớ **exact filename** (vd `PristonTale.exe`).
   - Right-click → "Open file location" → ghi đường dẫn cài.

2. **DLL load check** (xác định API dùng):
   - Tải [Process Explorer](https://learn.microsoft.com/sysinternals/downloads/process-explorer) (free, Microsoft).
   - Mở Process Explorer → View → Lower Pane View → **DLLs**.
   - Click vào process PT trong list trên → pane dưới hiện DLL loaded.
   - **Tìm các DLL sau, đánh dấu có/không:**
     | DLL | Có | Nghĩa là PT dùng |
     |---|---|---|
     | `dinput8.dll` | ☐ | DirectInput8 (ưu tiên hook) |
     | `dinput.dll` | ☐ | DirectInput7 legacy |
     | `user32.dll` | ☐ luôn có | `GetAsyncKeyState`, `GetCursorPos` (always cần hook user32) |
     | `xinput*.dll` | ☐ | Gamepad (không cần care) |

3. **XC3 confirm**:
   - Process Explorer search (Ctrl+F): "xc3" hoặc "wellbia" → liệt kê module/process liên quan.
   - Ghi version XC3 (right-click XC3 module → Properties → File version).

4. **Screenshot** kết quả + ghi `phase-00-poc/manual-probe-notes.md`.

### Method B — Auto-detect runtime (DLL build later)
Khi Phase 1-3 ready, DLL có thể **tự detect** API tại runtime thay vì hardcode:

```cpp
// pt_input_proxy.dll DllMain side
void detectAndInstallHooks() {
    HMODULE dinput8 = GetModuleHandleW(L"dinput8.dll");
    if (dinput8) {
        installDInput8Hooks();
        LOG("DirectInput8 hooks installed");
    }
    HMODULE dinput7 = GetModuleHandleW(L"dinput.dll");
    if (dinput7) {
        installDInput7Hooks();
        LOG("DirectInput7 (legacy) hooks installed");
    }
    // user32 always hooked (GetCursorPos / GetAsyncKeyState)
    installUser32Hooks();
    LOG("user32 hooks installed");

    // Future: if neither DInput loaded, poll RawInput functions
    // RegisterRawInputDevices → hook GetRawInputData
}
```

→ Plan dùng combo: Method A để **verify trước commit phase tiếp theo** (tiết kiệm dev time), Method B để DLL adapt nếu PT đổi API trong tương lai.

**Accept**: identify ≥ 1 DLL trong list (gần như chắc chắn dinput8 hoặc user32 minimum).

**Output**: `phase-00-poc/manual-probe-notes.md` + ảnh chụp Process Explorer.

## POC 2: minimal inject Notepad (0.75d)
**Mục tiêu**: prove hook engine + IPC work trên target an toàn trước khi đụng PT.

Method:
- Build minimal `probe_dll.dll`:
  - Hook `MessageBoxW` → log args qua named pipe.
- Build minimal injector (LoadLibrary-based, KHÔNG manual map — đơn giản phase này):
  - `OpenProcess(notepad.exe)` + `CreateRemoteThread(LoadLibraryW)`.
- Run Notepad → File → Save (popup MessageBox) → verify hook fire + log received.

**Accept**: hook fire 10/10 trials. Injector success rate 100% trên Notepad.

**Output**: `tools/poc-injection/` folder + `phase-00-poc/inject-report.md`.

## POC 3: XC3 baseline — user already confirmed ACTIVE

User confirm XC3 active. Phase 5 hardening MANDATORY (không skip).

Vẫn cần document version để Phase 5 calibrate:
- Process Explorer scan XC3 module Properties → ghi version + file path.
- Note: XC3 install thường ở `%ProgramFiles%\Priston Tale\XC3` hoặc tương tự.
- Output: `phase-00-poc/xc3-version.md` (1 dòng + version).

## Gate decision

| POC1 | POC2 | POC3 | Decision |
|---|---|---|---|
| PASS | PASS | XC3 standard | Tiếp Phase 1 |
| PASS | PASS | XC3 mới/cứng | Tiếp nhưng tăng effort Phase 5 lên 3-4d |
| PASS | FAIL | — | Abort — hook engine không feasible |
| FAIL | — | — | Abort — không identify được API PT dùng → fallback plan 0928 |

## Files to create
- `tools/poc-injection/CMakeLists.txt`
- `tools/poc-injection/probe-dll.cpp`
- `tools/poc-injection/probe-injector.cpp`
- `plans/260610-1125-.../phase-00-poc/api-probe-report.md`
- `plans/260610-1125-.../phase-00-poc/inject-report.md`

## Todo

**User-side (do trước, ~15 phút, không cần code):**
- [ ] Process Explorer download + run
- [ ] Identify PT.exe exact filename
- [ ] Check dinput8.dll / dinput.dll / user32.dll loaded
- [ ] XC3 module version note
- [ ] Screenshot + commit `phase-00-poc/manual-probe-notes.md`

**Dev-side:**
- [ ] Update `WindowLifecycleManagerPid` filter với exact exe name user found
- [ ] Build minimal probe-dll + probe-injector cho Notepad
- [ ] Run inject Notepad 10 trials, record success rate
- [ ] Implement auto-detect logic trong DLL (Method B)
- [ ] Write `phase-00-poc/inject-report.md`
- [ ] Gate decision

## Success criteria
- API set PT dùng được identify chính xác.
- Notepad inject success 10/10.
- XC3 status documented.

## Risks
- PT.exe có anti-static-analysis (packer/obfuscation) → import list không thấy. Workaround: runtime probe (gắn debugger sau khi PT load 30s, dump IAT).
- POC2 fail = hook engine fundamental issue → re-evaluate library choice (MinHook vs Detours vs Polyhook).
