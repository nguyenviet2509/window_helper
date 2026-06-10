---
phase: 4
title: IPC — shared memory + event protocol
status: pending
priority: P1
effort: 0.75d
---

# Phase 4 — IPC

## Context
Host write FabricatedState → DLL hook đọc khi PT poll input. Channel: shared memory (low latency) + named event (signal change, optional).

Per-process: 1 shared mem mapping named `PtProxy_{PID}`.

## Files
- `src/ipc/ipc-types.h` (~40 LOC) — shared layout
- `src/ipc/ipc-client.h/.cpp` (~150 LOC) — host side
- `src/ipc/ipc-server.h/.cpp` (~120 LOC) — DLL side

## Protocol

### Naming
- Shared mem: `Local\PtProxy_{PID}_{rand}`
- Random salt: host và DLL phải agree. Pass via env var khi inject: host set `PTPROXY_SALT={hex}` env in target process before inject.

### Layout
```cpp
struct IpcLayout {
    uint32_t magic;       // 'PTPX' = 0x58505450
    uint32_t version;
    uint32_t sizeBytes;
    FabricatedState state;
};
```

### Sync
- Atomic write: host writes state với `tickWritten` increment last (memory barrier).
- DLL hook reads atomically: read `tickWritten` → if changed since last → reload state.
- KHÔNG dùng mutex (perf hit in PT main loop). Race acceptable: PT đọc 1-frame-old state OK.

### Host API
```cpp
class IpcClient {
public:
    bool connect(DWORD pid, const std::string& salt);
    bool writeState(const FabricatedState& s);
    void disconnect();
private:
    HANDLE mapping_ = nullptr;
    IpcLayout* view_ = nullptr;
    DWORD pid_;
};
```

### DLL API
```cpp
namespace ipc {
    bool startServer();   // reads PTPROXY_SALT env, opens mapping
    FabricatedState* state();  // pointer to live state
    void stopServer();
}
```

## Performance
- Read mỗi GetDeviceState call (PT poll ~60-200 Hz). Memory read 4KB cache-hot → ~50ns. Negligible.
- Host write rate: ~10-50 Hz (mỗi action). Negligible.

## Todo
- [ ] ipc-types.h shared layout
- [ ] IpcClient (host): CreateFileMapping + MapViewOfFile
- [ ] ipc::startServer (DLL): read env + OpenFileMapping + MapViewOfFile
- [ ] Atomic write helper (tickWritten last)
- [ ] Unit test: 2 process (host + test client) share mem, write/read state.

## Success criteria
- 1000 writes/reads back-to-back: 0 corrupted state.
- Latency host write → DLL visible < 100µs.

## Risks
- Shared mem named object sometimes blocked by AC (rare cho XC3). Fallback: anonymous handle duplicate via `DuplicateHandle` from host → write handle vào target process memory.
