---
name: Multi-Window Auto (N=2 Priston Tale)
slug: multi-window-auto
created: 2026-06-10
status: superseded
supersededBy: ../260610-0928-multi-window-coexist-auto/
type: implementation
estimatedDays: 7-8
totalLoc: ~1400
brainstorm: ../reports/brainstorm-260610-0831-multi-window-auto.md
amendments:
  - reports/brainstorm-260610-0845-tabs-and-per-profile-config.md
  - reports/brainstorm-260610-0859-calibration-and-window-pin.md
  - reports/brainstorm-260610-0908-cursor-sharing-strategy.md
blockedBy: []
blocks: []
relatedPlans:
  - 260531-0857-virtual-desktop-mode  # cursor isolation orthogonal — combinable later
---

# Multi-Window Auto — Plan

Auto đồng thời 2 cửa sổ PT trên cùng 1 PC bằng time-sliced ForegroundArbiter. Tận dụng kiến trúc hiện tại; capture (WGC) chạy background song song; chỉ foreground/SendInput phải tuần tự hoá.

**Amendment 2026-06-10 (08:45):** UI tabs per window + per-profile config (full AppConfig). Xem `reports/brainstorm-260610-0845-tabs-and-per-profile-config.md`.

**Amendment 2026-06-10 (08:59):** Live calibration UI + window pin per profile + audit log. Bar/refill coords move từ hardcoded → AppConfig per profile. Xem `reports/brainstorm-260610-0859-calibration-and-window-pin.md`.

**Amendment 2026-06-10 (09:08):** Cursor sharing strategy — N≥2 auto disable Bezier mouse path + teleport click + park cursor sau slot. Xem `reports/brainstorm-260610-0908-cursor-sharing-strategy.md`.

## Stack
C++17, MSVC v143 (giữ nguyên). Không thêm dependency.

## Architecture summary

```
N pipelines (capture+vision+combat+dispatcher) chạy song song
        │
        ▼
1 ForegroundArbiter (priority queue, slot ~100ms, FG switch)
        │
        ▼
1 SendInputBackend (target set động per slot)
```

- 1 process, 1 UI, **per-window profile config** (full AppConfig riêng từng tab; bar region shared, assume cùng resolution).
- Capture không cần foreground → vision cả 2 luôn fresh.
- ForegroundArbiter = component mới; giữ slot ≥100ms, preempt cho P0 emergency.
- `InputScheduler` đổi: mỗi cmd mang `HWND owner`; trước flush gọi `arbiter.acquireSlot(owner)`.
- `SendInputBackend` không cần đổi (đã có `setTarget` động).

## Phases

| # | Phase | File | Effort | Status |
|---|-------|------|--------|--------|
| 1 | Window discovery + PerWindowContext skeleton | [phase-01](phase-01-window-discovery-and-context.md) | 0.5d | pending |
| 2 | ForegroundArbiter component | [phase-02](phase-02-foreground-arbiter.md) | 1d | pending |
| 3 | InputScheduler ownerHwnd refactor | [phase-03](phase-03-input-scheduler-owner.md) | 0.5d | pending |
| 4 | main.cpp wiring N=2 + per-window cfg/bus | [phase-04](phase-04-main-wiring.md) | 0.5d | pending |
| 4b | ProfileManager + assignment persist | [phase-04b](phase-04b-profile-manager.md) | 0.5d | pending |
| 5 | MainWindow tabs + per-tab config editor | [phase-05](phase-05-ui-tabs.md) | 2d | pending |
| 7 | Live calibration UI + window pin + audit log | [phase-07](phase-07-calibration-and-window-pin.md) | 2d | pending |
| 6 | Integration test + 30min/4h soak | [phase-06](phase-06-test-soak.md) | 0.5d | pending |

## Key dependencies
- Phase 2 độc lập.
- Phase 3 cần Phase 2.
- Phase 4 cần Phase 1-3.
- Phase 4b cần Phase 4 (per-window cfg in place).
- Phase 5 cần Phase 4 + 4b.
- Phase 7 cần Phase 4b (AppConfig field mới) + Phase 5 (UI host cho calibration panel).
- Phase 6 sau cùng (test bao gồm phase 7).

## Success criteria
- 2 PT auto pot + attack đồng thời, 0 miss combat trong stress 30 phút.
- P0 emergency preempt < 150ms.
- 4h soak: không race, không deadlock, không leak HWND.
- UI 2 cột realtime, toggle độc lập per window.

## Out of scope
- N ≥ 3 windows.
- Per-window config override (file).
- Driver-level input / AC evasion.
