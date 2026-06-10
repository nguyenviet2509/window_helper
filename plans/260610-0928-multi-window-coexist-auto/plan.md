---
name: Multi-Window Co-Exist Auto (N=2-3 PT, không phiền user)
slug: multi-window-coexist-auto
created: 2026-06-10
status: pending
type: implementation
estimatedDays: 7-9
totalLoc: ~1600
brainstorm: ../reports/brainstorm-260610-0928-multi-window-coexist-auto.md
amendments:
  - ../reports/brainstorm-260610-1044-coexist-plan-review-and-upgrades.md
supersedes: ../260610-0831-multi-window-auto/
blockedBy: []
blocks: []
---

# Multi-Window Co-Exist Auto — Plan

Auto đồng thời 2-3 cửa sổ PT trên cùng 1 PC, **user dùng máy song song được** (auto-pause khi user active). Detect động: PT mở/đóng giữa chừng → tab tự thêm/biến mất.

**Supersedes** plan `260610-0831-multi-window-auto` (dedicated farm, N=2 static, cursor park). Plan này = superset: N=3 dynamic + hot-plug + auto-pause + per-profile tabs + calibration.

**Amendment 2026-06-10 (10:44):** Plan review identified "không chiếm chuột" CHƯA tuyệt đối — user idle passive vẫn thấy cursor nhảy. Upgrades duyệt: (1) build separation 2 exe targets — bản cũ giữ nguyên; (2) cursor save/restore trong arbiter slot; (3) deep-idle 30s threshold cho mouse-required action; (4) Phase 0 POC sớm verify WGC 3-session + arbiter timing. Xem `../reports/brainstorm-260610-1044-coexist-plan-review-and-upgrades.md`.

## Stack
C++17, MSVC v143. Không thêm dependency. Tất cả Windows API native.

## Architecture

```
WindowLifecycleManager (background thread)
  ├─ poll EnumWindows 2.5s, diff vs current contexts
  ├─ spawn → init PerWindowContext + emit "windowAdded" event
  ├─ teardown → cancel arbiter pending + free + emit "windowRemoved"
  └─ hard cap N=3 (skip + warn nếu phát hiện thêm)
        │
        ▼
PerWindowContext[N]  (dynamic, N ∈ {0..3})
  ├─ Capture (WGC, background OK)
  ├─ Vision / Combat / Refill
  └─ InputScheduler (owner=HWND)
        │
        ▼
PauseGate ← UserActivityMonitor (GetLastInputInfo poll 200ms)
  │ if userActive → block flush; finish current transaction; reject new
  ▼
ForegroundArbiter (slot 70-80ms, round-robin N variable, P0 preempt)
  ▼
SendInputBackend (setTarget per slot)
```

## Phases

| # | Phase | File | Effort | Status |
|---|-------|------|--------|--------|
| **0** | **POC feasibility (WGC 3-session + arbiter slot + cursor restore + self-input filter)** | [phase-00](phase-00-poc-feasibility.md) | 1d | pending |
| 1 | Window lifecycle manager (continuous discovery + hot-plug) | [phase-01](phase-01-window-lifecycle.md) | 1d | pending |
| 2 | ForegroundArbiter (N variable, skip paused, **cursor save/restore**) | [phase-02](phase-02-foreground-arbiter.md) | 1.25d | pending |
| 3 | InputScheduler ownerHwnd + PauseGate + **requiresMouse flag** | [phase-03](phase-03-input-scheduler-pause.md) | 0.6d | pending |
| 4 | UserActivityMonitor + PauseGate (**3 thresholds: 3s/5s/30s deep-idle**) | [phase-04](phase-04-user-activity-pause.md) | 0.6d | pending |
| 5 | **main-coexist.cpp** wiring (new file, không touch main.cpp cũ) | [phase-05](phase-05-main-wiring.md) | 0.5d | pending |
| 6 | ProfileManager + assignment persist (match HWND mới theo title+rect) | [phase-06](phase-06-profile-manager.md) | 0.75d | pending |
| 7 | **main-window-coexist.cpp** dynamic tabs + per-tab config + cursor/deep-idle settings | [phase-07](phase-07-ui-dynamic-tabs.md) | 2.25d | pending |
| 8 | Live calibration UI + window pin + audit log | [phase-08](phase-08-calibration.md) | 1.5d | pending |
| 9 | Integration test + **deep-idle scenarios + classic exe smoke** | [phase-09](phase-09-test-soak.md) | 0.85d | pending |

## Key dependencies
- **Phase 0 blocking** — fail → revisit design trước khi build.
- Phase 1 độc lập (skeleton lifecycle manager).
- Phase 2 độc lập.
- Phase 3 cần Phase 2 + Phase 4 (PauseGate interface).
- Phase 4 độc lập (UserActivityMonitor + PauseGate API).
- Phase 5 cần Phase 1-4.
- Phase 6 cần Phase 5 (per-window cfg slot).
- Phase 7 cần Phase 5 + 6.
- Phase 8 cần Phase 6 + 7.
- Phase 9 sau cùng (gồm classic exe smoke regression).

## Build separation
- 2 exe targets trong `src/CMakeLists.txt`: `WindowHelper` (cũ) + `WindowHelperCoexist` (mới).
- Share common sources; component mới chỉ link vào exe mới.
- Shared component mở rộng → optional/no-op default (backward compat).
- main split: `main.cpp` (cũ) + `main-coexist.cpp` (mới).
- UI split: `ui/main-window.cpp` (cũ) + `ui/main-window-coexist.cpp` (mới).
- Output: `svc_xxxxx.exe` (cũ) + `svc_yyyyy_v2.exe` (mới).

## Success criteria
- 3 PT auto đồng thời, 0 miss combat khi user idle 30 phút.
- User input → toàn bộ auto pause trong < 250ms.
- Idle 3s → auto resume.
- Mở/đóng PT runtime → tab add/remove ≤ 3s, không crash.
- 4h soak chuyển user-active/idle 100 lần → no HWND leak, no deadlock arbiter.
- Profile reassign khi PT đóng+mở lại: match đúng > 90% case.

## Out of scope
- True cursor isolation (VM/driver-level).
- N ≥ 4 windows.
- Per-window resolution support (bar region shared).
- AC evasion driver-level.

## Unresolved questions
- Slot size cho N=3: 70 vs 80ms — POC đo trong Phase 9.
- Pause threshold adaptive (vd user gõ liên tục → tăng) hay fix 3s — chờ user test.
- Profile match key: title+rect đủ unique, hay cần thêm PID/start-time.
