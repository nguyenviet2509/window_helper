---
phase: 0
title: POC feasibility (WGC 3-session + arbiter + cursor restore + input filter)
status: pending
priority: P0
effort: 1d
blocking: true
---

# Phase 0 — POC feasibility

## Context
Plan có 4 giả định kỹ thuật chưa verify. Nếu sai → refactor đắt ở phase sau. Verify trước, gate plan execution.

## Files (throwaway, không vào exe production)
- `tools/poc-coexist/wgc-3-session.cpp`
- `tools/poc-coexist/arbiter-timing.cpp`
- `tools/poc-coexist/cursor-restore.cpp`
- `tools/poc-coexist/self-input-filter.cpp`
- `tools/poc-coexist/CMakeLists.txt`

## POC 1: WGC 3-session concurrent
**Question:** Windows cho phép 3 GraphicsCaptureSession đồng thời lên 3 HWND khác nhau không? FPS / GPU ổn không?

**Method:**
- Mở 3 mock window (PtMockGame x3 hoặc 3 Notepad).
- Init 3 WgcCapture instance → start() đồng thời.
- 5 phút loop: log FPS từng session + GPU usage (DXGI query).

**Accept:** 0 crash, mỗi session ≥ 30 FPS, GPU < 60%, không memory leak (handle count stable).

**Fallback nếu fail:** N=2 hard cap; ghi trong plan.

## POC 2: ForegroundArbiter slot timing
**Question:** SetForegroundWindow 70ms slot với 3 owner có throttle không? `fgFailures` rate?

**Method:**
- 3 thread acquire slot rotating; mỗi slot SetForegroundWindow(hwnd_i) → wait 50ms confirm → SendInput dummy keystroke → release.
- Run 5000 slots; đo: success rate, average switch latency, total fgFailures.

**Accept:** failure rate < 5%, p95 switch < 60ms, no Windows lock manifest (`LockSetForegroundWindow`).

**Fallback nếu fail:** slot 100ms; document N=3 throughput hit.

## POC 3: Cursor save/restore latency
**Question:** `GetCursorPos → SendInput click → SetCursorPos restore` có nằm trong 1 frame (~16ms) không?

**Method:**
- 1000 iteration: ghi `QueryPerformanceCounter` trước/sau sequence.
- Log p50/p95/p99 latency.

**Accept:** p95 < 20ms (1.2 frame). Visible flicker tối thiểu.

**Fallback nếu fail:** skip cursor restore feature; document trong UI "cursor moves to last click position".

## POC 4: GetLastInputInfo self-filter
**Question:** Phân biệt được input do app SendInput vs user input?

**Method:**
- Loop 60s:
  - Mỗi 200ms: `monitor.notifySelfInput()` + `SendInput` dummy.
  - Trong khi đó user manual gõ 100 keystroke random.
- Đếm: false positive (self coi là user) + false negative (user coi là self).

**Accept:** false negative < 5% (miss < 5 user inputs). False positive < 10% (acceptable — chỉ tăng pause spurious).

**Fallback nếu fail:** manual pause F9 là primary mechanism; auto-detect mark experimental.

## Todo
- [ ] Tạo tools/poc-coexist subdir + CMakeLists.
- [ ] 4 POC source files.
- [ ] Run mỗi POC, log kết quả.
- [ ] Viết `tools/poc-coexist/REPORT.md` với metrics.
- [ ] Gate decision: 4/4 PASS → tiếp Phase 1. Else → amendment plan với fallback.

## Success criteria
- Tất cả 4 POC PASS theo accept criteria.
- Report ghi rõ số đo cho future reference.

## Risks
- POC 1 (WGC 3-session) là biggest unknown — nếu Windows limit → phải N=2 hard cap, viewport feature gate.
- POC time có thể vượt 1d nếu Windows behave bất thường → escalate user trước khi mở rộng scope.
