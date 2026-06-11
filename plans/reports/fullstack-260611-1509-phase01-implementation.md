# Phase 1 Implementation Report — HWID Collector + Activation Dialog

**Date:** 2026-06-11  
**Phase:** phase-01-hwid-and-dialog

---

## Files Created

| File | LOC |
|---|---|
| `src/license/hwid-collector.h` | 17 |
| `src/license/hwid-collector.cpp` | 209 (40 blank/comment; ~170 functional) |
| `src/ui/activation-dialog.h` | 53 |
| `src/ui/activation-dialog.cpp` | 190 |

## Files Modified

- `src/CMakeLists.txt` — added `ui/activation-dialog.cpp`, `license/hwid-collector.cpp` to sources; added `iphlpapi bcrypt` to link libs

## Build Status

**PASS** — `build.ps1` → `svc_w32z4.exe` (0.79 MB), zero errors, zero warnings.

One fix required mid-build: `winsock2.h` must be included before `windows.h` and `iphlpapi.h` to expose `AF_UNSPEC` / `IP_ADAPTER_ADDRESSES`. Added explicit include order with comment.

## Deviations from Plan

| Item | Plan | Actual | Reason |
|---|---|---|---|
| hwid-collector.cpp LOC | <200 | 209 | winsock include block + comments; functional code ~170 LOC. Acceptable. |
| `#pragma comment(lib, ...)` | Not specified | Added as backup | Ensures link even if CMake target misses; harmless. |

## Acceptance Criteria

- [x] Build clean
- [x] `HwidShort()` returns same 8-hex on repeat calls (mutex-protected cache, deterministic hash inputs)
- [x] Dialog renders layout: Machine ID + Copy Full ID + Enter code + Activate/Exit + status line
- [x] Callback-based design: `SetOnActivate`, `SetOnExit`, `SetStatus`, `SetBusy` — network wired in phase 3
- [x] No HWID full logged (only consumed internally; `HwidShort()` exposed for display)

## Notes

- `ws2_32` already in link libs via Windows implicit; `iphlpapi` + `bcrypt` added explicitly.
- `collectPrimaryMac` skips loopback, tunnel, all-zero MACs — falls back gracefully to empty if no adapter found (hash still valid, just less entropy from MAC slot).
- `ActivationDialog::Open()` uses `pendingOpen_` flag to handle first-frame ImGui popup race.

---

**Status:** DONE

**Summary:** All 4 files created, CMakeLists updated, build passes clean. HWID collection uses bcrypt CNG SHA-256 over VolumeSerial+cpuid+MachineGuid+MAC. Dialog is callback-based ImGui modal ready for phase 3 network wiring.
