---
phase: 8
title: Live calibration UI + window pin + audit log
status: pending
priority: P2
effort: 1.5d
---

# Phase 8 — Calibration & window pin

## Context
Reuse Phase 7 plan cũ (`260610-0831/phase-07-calibration-and-window-pin.md`). Logic không đổi vì plan mới superset; chỉ cần wire vào dynamic tabs.

## Scope (reuse từ plan cũ)
- Live calibration UI: bấm vào điểm trên capture preview để set HP/MP/SP/refill coords per profile.
- Window pin: lock window position+size cho profile (auto SetWindowPos khi detect mismatch).
- Audit log: log mọi config change + window detect event vào `audit.log`.

## Adaptations vs plan cũ
- Calibration panel nằm trong tab của window cụ thể (per ctx).
- Pin/unpin: button "Pin position" — store rect vào profile + assignment.
- Audit log thêm event types: `window-added`, `window-removed`, `pause-state-changed`, `profile-bound`.

## Files
Reuse files plan cũ:
- `src/ui/calibration-panel.h/.cpp`
- `src/core/window-pin.h/.cpp`
- `src/core/audit-log.h/.cpp`

## Todo
- [ ] Port `calibration-panel` từ plan cũ, gắn vào tab content (Phase 7).
- [ ] `window-pin` enforcer: tick thread check rect mỗi 1s; nếu mismatch + pin enabled → SetWindowPos.
- [ ] Audit log: append-only file, rotate khi > 10MB.
- [ ] Wire log events từ lifecycle/monitor/profile-manager.

## Success criteria
- Click trên preview → coord cập nhật vào AppConfig + hiển thị overlay.
- Pin window → minimize/restore không bị lệch.
- Audit log có đủ event trace cho debug.

## Risks
- WindowPin xung đột với user nếu user move window cố ý → tạm disable pin khi user active (PauseGate signal).
- Audit log spam khi user active liên tục → rate-limit pause-state log (1 event / 1s).
