---
type: brainstorm-report
date: 2026-06-10
slug: multi-window-auto
status: approved
---

# Brainstorm: Auto 2 cửa sổ Priston Tale trên cùng 1 PC

## Problem
Hiện tại auto chỉ chạy 1 cửa sổ PT. Cần mở rộng để auto đồng thời 2 cửa sổ PT trên cùng 1 PC.

## Constraints
- `SendInputBackend` yêu cầu target foreground (PostMessage đã verify không work với PT).
- Windows chỉ cho 1 foreground tại 1 thời điểm.
- PC dành riêng farm — user không thao tác, chấp nhận foreground bị switch.
- WGC capture chạy background được — vision không bị giới hạn.

## Approaches đã đánh giá

| # | Approach | Verdict |
|---|---|---|
| 1 | Time-sliced single instance + ForegroundArbiter | **Chosen** |
| 2 | 2 instance độc lập + IPC mutex foreground | Loại — 2 UI/config rối, IPC race-prone |
| 3 | PT #2 chạy VM/Sandboxie | Loại — over-engineer, GPU/license issue |

## Chosen: Time-sliced single instance

### Architecture
- 1 process quản N cửa sổ (focus N=2 trước, kiến trúc support thêm).
- Per-window pipeline: `WgcCapture + VisionPipeline + CombatFsm + PotRefillScheduler + ActionDispatcher + OutputGate`.
- **ForegroundArbiter chung**: queue request foreground, round-robin slot ~100ms, priority preempt cho P0 emergency pot.
- 1 `InputScheduler` + 1 `SendInputBackend` shared — backend.setTarget(HWND) dynamic theo slot.
- 1 UI hiển thị 2 cột status, toggle AUTO/BUFF độc lập.
- Config dùng chung cho cả 2 (HP/MP/SP region giả định 2 PT cùng resolution).

### Components to add/modify
- `main.cpp`: `FindAllTargets()` thay `FindTarget()`.
- New: `PerWindowContext` struct gom pipeline.
- New: `dispatch/foreground-arbiter.{h,cpp}` — queue + round-robin + preempt.
- `InputScheduler`: action gắn `HWND ownerHwnd`, request arbiter trước khi flush.
- `SendInputBackend`: setTarget động (đã có).
- `MainWindow`: 2-column status panel, per-window toggle.
- `HotkeyManager`: F8 toggle tất cả; option F7/F8 per-window giai đoạn sau.
- `state/game-state.h`: index theo windowId.

### Foreground Arbiter algorithm
- Queue `Request{HWND, priority, sendInputFn}`.
- Slot ~100ms: SetForegroundWindow → wait confirm (≤50ms) → backend.setTarget → drain pending cho cùng HWND trong slot → release → window kia.
- P0 emergency (HP<20%) preempt slot đang chạy.
- Workaround Windows foreground lock: AttachThreadInput hoặc ALT prelude trick.

### Trade-offs
- Throughput input giảm ~25-30% per window do switch overhead (~50ms/switch, slot 100ms).
- Combat reaction trễ thêm 50-200ms — chấp nhận được với PT (action density thấp).
- Switch cap ≤10Hz để tránh AC nghi ngờ.

### Risks & mitigation
- SetForegroundWindow fail → AttachThreadInput workaround.
- Game steal focus → arbiter monitor WM_ACTIVATE, re-acquire.
- AC detect → giữ slot ≥100ms, không spam switch.
- 2 PT resolution khác nhau → giai đoạn 2: per-window region override.

### Success criteria
- 2 PT auto pot + attack đồng thời, 0 miss combat trong stress 30 phút.
- P0 preempt latency < 150ms.
- 4h chạy không race/deadlock.
- UI realtime 2 cột, toggle độc lập.

### Out of scope (Phase 1)
- N ≥ 3 cửa sổ.
- Per-window config override (file).
- Anti-cheat evasion / driver-level input.

## Next
Invoke `/ck:plan` để tạo phase plan chi tiết.

## Unresolved questions
- Slot size tối ưu (100ms là estimate; cần đo thực tế trên PT).
- AttachThreadInput có cần thiết hay SetForegroundWindow đơn thuần đủ — POC ngắn trong phase 1.
- Phase sau có cần per-window config override không (chờ feedback sau khi N=2 stable).
