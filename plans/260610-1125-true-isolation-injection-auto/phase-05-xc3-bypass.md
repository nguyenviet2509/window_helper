---
phase: 5
title: XC3 bypass hardening
status: pending
priority: P0
effort: 2d
---

# Phase 5 — XC3 bypass hardening

## Context
Phase 1-4 cung cấp injection cơ bản. Phase 5 = harden chống XC3 detection vectors cụ thể. **Phase này có thể tiếp tục arms-race trong Phase 10.**

XC3 detection vectors (đã biết):
1. LoadLibrary hook → detect injected DLL (mitigated Phase 2 manual map)
2. PEB module walk → mitigated Phase 2 PEB unlink
3. PE header scan → mitigated Phase 2 strip
4. IAT/EAT hook scan → mitigated Phase 1 randomize + này
5. Code section CRC integrity → này
6. Debugger detection (PEB.BeingDebugged, NtQueryInfoProcess) → này
7. Thread enumeration (CreateRemoteThread spawns new thread) → này (thread hijack)
8. Module enumeration via kernel callback (PsSetLoadImageNotifyRoutine) → very hard; user-mode KHÔNG bypass được; chấp nhận risk hoặc kernel driver phase sau
9. Hook trampoline pattern scan → mitigated Phase 1 randomize
10. NtQueryVirtualMemory scan for executable non-image pages → này

## Files
- `src/injection/anti-detect/anti-debug.h/.cpp` (~120 LOC)
- `src/injection/anti-detect/integrity-check.h/.cpp` (~100 LOC)
- `src/injection/anti-detect/thread-hijack.h/.cpp` (~150 LOC)
- `src/injection/anti-detect/memory-cloak.h/.cpp` (~80 LOC)

## Hardening implementations

### 1. Thread hijack (replace CreateRemoteThread)
Pick existing thread trong PT.exe, suspend, set context.Eip = shellcode that calls DllMain, resume. Sau khi DllMain xong, restore context, resume original execution.

```cpp
bool ThreadHijacker::injectViaHijack(HANDLE proc, LPVOID dllMainStub) {
    HANDLE thread = pickIdleThread(proc);  // lowest CPU thread
    SuspendThread(thread);
    CONTEXT ctx; ctx.ContextFlags = CONTEXT_FULL;
    GetThreadContext(thread, &ctx);
    LPVOID savedRip = (LPVOID)ctx.Rip;
    // Build shellcode: call dllMainStub, then jmp savedRip
    writeShellcode(proc, ...);
    ctx.Rip = (DWORD64)shellcodeAddr;
    SetThreadContext(thread, &ctx);
    ResumeThread(thread);
    return true;
}
```

### 2. Anti-debug
```cpp
bool AntiDebug::isDebuggerPresent() {
    if (IsDebuggerPresent()) return true;
    BOOL remoteDbg = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &remoteDbg);
    if (remoteDbg) return true;

    // PEB.BeingDebugged
    PPEB peb = (PPEB)__readgsqword(0x60);
    if (peb->BeingDebugged) return true;
    if (peb->NtGlobalFlag & 0x70) return true;  // FLG_HEAP_*

    // NtQueryInformationProcess ProcessDebugPort
    HANDLE port = NULL;
    NtQueryInformationProcess(GetCurrentProcess(), 7 /*ProcessDebugPort*/, &port, sizeof(port), NULL);
    if (port) return true;

    return false;
}

void AntiDebug::evade() {
    PPEB peb = (PPEB)__readgsqword(0x60);
    peb->BeingDebugged = 0;
    peb->NtGlobalFlag &= ~0x70;
}
```

DLL trong DllMain: gọi `evade()`. Watchdog thread: check `isDebuggerPresent()` mỗi 5s → if true → exit silently (DLL gracefully unloads).

### 3. Self-integrity check
```cpp
class IntegrityCheck {
public:
    void snapshot(HMODULE self);   // CRC .text section
    bool verify();                  // re-CRC, compare
    void startWatcher();            // thread check mỗi 3s
private:
    uint32_t origCrc_;
    LPVOID textBase_;
    SIZE_T textSize_;
};
```

Nếu XC3 patch `.text` (xoá hook hoặc inject probe) → CRC mismatch → DLL re-restore từ saved copy hoặc unload self.

### 4. Memory cloak
```cpp
void MemoryCloak::cloak(LPVOID base, SIZE_T size) {
    // Mark page as PAGE_NOACCESS most of time
    // Khi hook fire: VirtualProtect → EXECUTE_READ → execute → restore NOACCESS
    // Quá expensive cho hot path. Alternative: register VEH (vectored exception) để on-demand uncloak.
    // DEFER nếu basic bypass đủ.
}
```

DEFER: memory cloak chỉ implement nếu Phase 9 testing show XC3 detect executable non-image pages.

### 5. Hook trampoline obfuscation
Phase 1 đã randomize byte. Phase 5 thêm:
- Trampoline allocate trong **existing executable region** của PT.exe (vd code cave trong padding section) thay vì VirtualAlloc → blend với original code.
- Tricky implementation: scan PE sections for `.text` cave (sequence of NOPs/INT3 ≥ 100 bytes) → write trampoline ở đó.
- Defer to last priority.

## Todo
- [ ] anti-debug.h/.cpp (evade + check)
- [ ] integrity-check.h/.cpp (CRC watcher)
- [ ] thread-hijack.h/.cpp (replace CreateRemoteThread)
- [ ] DLL DllMain wire: gọi evade + start watchers
- [ ] Memory cloak (defer)
- [ ] Hook in code cave (defer)
- [ ] Test: inject PT trong DEBUG mode (debugger attached) → DLL phải evade hoặc unload graceful

## Success criteria
- Inject thành công khi PT đang chạy bình thường.
- Debugger attached → DLL unload (no crash).
- Self-integrity detect VirtualProtect+memcpy patch → restore.
- 30 phút stress: 0 XC3 detect log trong PT memory (verify bằng PT-side debug nếu có).

## Risks
- Thread hijack có thể deadlock nếu hijacked thread đang giữ lock (vd loader lock). Pick idle thread không có lock — heuristic dựa GetThreadTimes.
- Self-integrity check thread = 1 new thread visible — XC3 scan thread list có thể catch. Mitigation: chạy check trong DLL via `SetTimer` (UI thread) thay vì new thread. PT có UI thread.
- Anti-debug evade phải gọi MỖI khi XC3 scan; one-time evade trong DllMain không đủ. Chạy trong watchdog.
