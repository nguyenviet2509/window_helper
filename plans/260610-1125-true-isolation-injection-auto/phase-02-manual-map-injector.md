---
phase: 2
title: Manual map injector + PEB hide + PE strip
status: pending
priority: P0
effort: 2.5d
---

# Phase 2 — Manual map injector

## Context
Tránh `LoadLibrary` (XC3 hook LoadLibrary → detect). Manual map = đọc DLL từ file, allocate trong target process, parse + apply relocations + resolve imports manually, call DllMain qua remote thread/hijack.

Sau inject: unlink from PEB Ldr + zero PE headers trong remote memory → XC3 PEB walk không thấy module.

## Files
- `src/injection/manual-map.h` (~100 LOC)
- `src/injection/manual-map.cpp` (~500 LOC)
- `src/injection/peb-hide.h/.cpp` (~150 LOC)

## High-level steps

```cpp
class ManualMapInjector {
public:
    bool inject(DWORD pid, const std::wstring& dllPath);

private:
    bool readDll(const std::wstring& path, std::vector<uint8_t>& image);
    bool allocateRemote(HANDLE proc, size_t size, LPVOID& remoteBase);
    bool applyRelocations(uint8_t* localImage, LPVOID remoteBase);
    bool resolveImports(HANDLE proc, uint8_t* localImage, LPVOID remoteBase);
    bool writeRemote(HANDLE proc, LPVOID remoteBase, const uint8_t* localImage, size_t size);
    bool callDllMain(HANDLE proc, LPVOID remoteBase, LPVOID entryRva);
    bool stripPE(HANDLE proc, LPVOID remoteBase);
    bool unlinkPEB(HANDLE proc, LPVOID remoteBase);
};
```

## Steps detail

### 1. Read DLL file → buffer
- ReadFile DLL từ disk (DLL ship cạnh exe).
- Parse PE: validate MZ/PE signature, get OptionalHeader.

### 2. Allocate remote memory
- `VirtualAllocEx(proc, NULL, image.size(), MEM_COMMIT|RESERVE, PAGE_EXECUTE_READWRITE)`.
- Lý tưởng allocate ở ImageBase nếu rảnh; fallback random base + rebase.

### 3. Apply relocations
- Walk `.reloc` section. Mỗi entry: patch local image buffer với delta (remoteBase - ImageBase).

### 4. Resolve imports
- Walk import directory. Mỗi DLL (kernel32, user32, dinput8...):
  - `GetModuleHandleExW(name)` trong target (qua `GetModuleHandle` shellcode hoặc `GetProcAddress(LoadLibrary)` của caller, applied to target — workaround: use `GetModuleHandle` via remote call).
  - Resolve mỗi function: write address vào IAT của local buffer.

### 5. Write to remote
- `WriteProcessMemory(proc, remoteBase, localImage, size)`.

### 6. Execute DllMain
- 2 options:
  - **CreateRemoteThread** với DllMain entry: dễ nhưng tạo thread mới (XC3 có thể scan).
  - **Thread hijack**: pick existing thread trong target, suspend, set context.Eip = DllMain stub, resume. Stealthier.
- Defer thread hijack đến Phase 5; Phase 2 dùng CreateRemoteThread.

### 7. Strip PE headers
```cpp
bool ManualMapInjector::stripPE(HANDLE proc, LPVOID remoteBase) {
    size_t headerSize = 0x1000;  // typical
    std::vector<uint8_t> zeros(headerSize, 0);
    DWORD oldProtect;
    VirtualProtectEx(proc, remoteBase, headerSize, PAGE_READWRITE, &oldProtect);
    WriteProcessMemory(proc, remoteBase, zeros.data(), headerSize, nullptr);
    VirtualProtectEx(proc, remoteBase, headerSize, PAGE_EXECUTE_READ, &oldProtect);
    return true;
}
```

### 8. PEB unlink
- Vào DLL (sau DllMain): walk `NtCurrentTeb()->ProcessEnvironmentBlock->Ldr->InMemoryOrderModuleList`.
- Find entry matching `remoteBase`.
- Unlink LIST_ENTRY (set Flink/Blink prev/next bypass entry).
- Tương tự cho InLoadOrderModuleList + InInitializationOrderModuleList + HashLinks.

Note: PEB unlink phải chạy trong target process (từ DLL sau DllMain). Host injector không làm được.

## DLL injection point
- Inject `pt_input_proxy.dll` từ disk (ship cạnh `WindowHelperInjection.exe`).
- DLL name random: build CMake với output `proxy_{random}.dll`.
- Khi inject: copy DLL ra `%TEMP%` với name random khác → inject từ đó.

## Todo
- [ ] manual-map.h/.cpp.
- [ ] PE parser utility.
- [ ] Relocation walker.
- [ ] Import resolver (remote GetModuleHandle/GetProcAddress).
- [ ] WriteProcessMemory + CreateRemoteThread DllMain.
- [ ] PE header strip.
- [ ] PEB unlink (trong DLL side — phase 3 implement, header here).
- [ ] Test inject trên Notepad + dummy DLL → verify load + DllMain fire + module not in `Process Hacker` list.

## Success criteria
- Inject Notepad: DllMain fire, module KHÔNG visible trong Process Hacker module list.
- PE header zeroed trong remote (verify dump memory).
- No LoadLibrary call trong target process (verify via API Monitor).

## Risks
- Import resolve cần GetModuleHandle in remote — nếu target không có kernel32.GetModuleHandle cached → workaround: inject 1 tiny shellcode để remote-call GetModuleHandle.
- Memory pattern (executable buffer ngoài module list) là 1 detection vector. Mitigation Phase 5.
- Driver-level AC scan physical memory pages → user-mode bypass không stop được. Acknowledge limit.
