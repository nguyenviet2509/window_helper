---
phase: 10
title: XC3 arms-race iteration buffer
status: pending
priority: P1
effort: 2-3d (reactive)
---

# Phase 10 — Arms race buffer

## Context
Buffer effort cho khả năng XC3 detect → cần bypass thêm. Reactive phase — chỉ work nếu Phase 9 / production phát sinh detect.

## Common iterations đã anticipated

### Iter 1: Kernel-mode AC scan
Triggered if: Phase 9 detect via PsSetLoadImageNotifyRoutine hoặc kernel module scan.

Mitigation options:
- **Manual map LẠI từ memory** (read DLL from disk → encrypt → decrypt in target → map) — đã có nền tảng Phase 2.
- **No file on disk**: DLL embed trong host exe (resource), decrypt in memory, write to target via WriteProcessMemory only (no LoadLibrary, no file in TEMP).
- **Kernel driver hook** (last resort, ~5-7d effort thêm): hook ring0 to lie to XC3 callbacks. Cần signed driver cert HOẶC test signing mode boot.

### Iter 2: Hook pattern detection
Triggered if: XC3 detects hook trampolines pattern.

Mitigation:
- **Code cave hooks** (Phase 5 deferred): scan PE for caves, place trampoline in cave (blend with code).
- **Indirect hook**: hook không trên target function directly, mà trên upstream caller via stack trace.
- **Vectored exception handler** abuse: register VEH, mark target page no-execute, on access fault → exec our code → restore.

### Iter 3: Anti-debug stronger
Triggered if: XC3 detect via newer techniques (vd VEH list scan, TLS callback debug check).

Mitigation:
- Enumerate VEH list, remove any non-ours.
- TLS callback override.
- Hardware breakpoint detection via Dr0-Dr7 read in context.

### Iter 4: Network signal
Triggered if: XC3 server-side detect (heuristic from gameplay anomaly + send report).

Mitigation:
- Slow auto cadence (mimic human reaction time variability).
- Add randomized "human errors" (occasionally skip skill rotation).
- Defer if not detected.

## Files (created as needed)
- `src/injection/anti-detect/kernel-driver/` — defer
- `src/injection/anti-detect/code-cave-hook.cpp` — Iter 2
- `src/injection/anti-detect/veh-cleanup.cpp` — Iter 3
- `src/core/humanizer-advanced.cpp` — Iter 4

## Decision matrix

| Phase 9 result | Action |
|---|---|
| All green, no detect | Skip Phase 10. Ship. |
| 1-2 minor detect | Iter 1 or Iter 2, 1d each |
| Major detect (kernel-level) | Iter 1 kernel route, 5-7d → re-evaluate go/no-go |
| Catastrophic (HW ban alt machine) | STOP. Re-evaluate VM mode or abandon. |

## Todo (reactive — fill as Phase 9 results come in)
- [ ] Wait Phase 9 results
- [ ] Identify which iter needed
- [ ] Implement chosen iter
- [ ] Re-test Phase 9 critical scenarios

## Success criteria
- 0 XC3 detect after iter applied, 4h soak retest pass.

## Risks
- Iter cost mỗi cái 1-7d. Total Phase 10 có thể blow up nếu nhiều iter cần thiết.
- Kernel driver route requires user enable Test Signing → security risk for user system.
- Maintenance compound: mỗi iter là 1 vector cần re-verify mỗi PT/XC3 update.

## Long-term maintenance
- Ước tính ~1d/month cho XC3 updates (after initial stabilize).
- Bug tracker: keep "XC3 incidents" list with detect technique observed.
- Ship updates qua signed auto-updater? Defer — start manual.
