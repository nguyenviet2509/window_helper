---
phase: 6
title: Integration test + soak
status: pending
priority: P1
effort: 0.5d
---

# Phase 6 — Integration test + soak

## Context
Verify success criteria của plan trên PT thật.

## Test plan

### Smoke (5 phút)
- [ ] Mở 1 PT → behaviour cũ (backward compat).
- [ ] Mở 2 PT → cả 2 vision tick, FG slot count tăng đều.
- [ ] Toggle AUTO W0 only → W1 không bắn input.
- [ ] F8 → cả 2 toggle.
- [ ] 2 tab UI hiển thị; switch tab không lag.

### Calibration + Pin tests (Phase 7)
- [ ] Move PT window sang vị trí khác → HP/MP/SP đọc vẫn đúng (verify position-independence).
- [ ] Calibration panel: click drag set HP rect → overlay updated → save → restart → load đúng.
- [ ] Click inventory slot trong calibration → refill click trúng slot mới.
- [ ] Capture anchor → bật pinOnAutoOn → toggle AUTO ON → window snap về (x,y).
- [ ] Drag window khi AUTO ON → audit log warn rect change trong vòng 5s.

### Profile tests
- [ ] Lần đầu chạy: `profiles/profile-Default.json` được tạo từ `config.json`.
- [ ] Tạo profile "MainChar" trong tab W0 → file `profile-MainChar.json` xuất hiện.
- [ ] Đổi HP threshold trong tab W0 → 300ms sau file `profile-MainChar.json` cập nhật + CombatFsm áp threshold mới (verify qua pot trigger frame).
- [ ] Đổi profile binding W0 → load đúng cfg; AUTO state giữ nguyên.
- [ ] Restart app → `lastAssignment.json` load đúng profile per W0/W1.
- [ ] Rename profile in-use → file đổi tên, tab cập nhật, assignment update.
- [ ] Delete profile in-use → tab rebind về Default, không crash.

### Stress 30 phút
- [ ] HP/MP/SP của cả 2 PT auto pot đúng threshold, không miss > 2s.
- [ ] Combat attack cả 2 cửa sổ chạy đều (count > 100 mỗi window).
- [ ] P0 emergency (giả lập HP<20% W1 khi W0 đang attack) → preempt < 150ms.
- [ ] Arbiter `fgFailures` < 1% slot count.

### Soak 4 giờ
- [ ] Không deadlock (UI vẫn responsive).
- [ ] Không leak HWND/handle (Process Explorer check).
- [ ] CPU < 20%, RAM stable.
- [ ] Log không spam error.

## Metrics to capture
- Slot count W0 vs W1 (fairness).
- Avg slot duration (target ≥100ms).
- FG fail rate.
- Cmd drop count (gatedDrops).
- Combat actions/min mỗi window.

## Files to add (optional)
- `tools/multi-window-soak-checklist.md` — checklist record.

## Todo
- [ ] Smoke pass.
- [ ] Stress 30m pass.
- [ ] Soak 4h pass.
- [ ] Record metrics → update plan status → completed.

## Success criteria
Tất cả ✓ ở 3 phase test trên.

## Rollback
Nếu fail nghiêm trọng (FG fail > 10%, deadlock):
- Disable multi-window qua config flag `combat.multiWindowEnabled = false` (default false trong release đầu).
- Fall back N=1 path.

## Open
- Cần thêm telemetry export (CSV) để phân tích offline không? Đợi feedback sau soak.
