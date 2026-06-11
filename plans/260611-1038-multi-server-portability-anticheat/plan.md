---
name: Multi-server portability + anti-cheat layered defense
slug: multi-server-portability-anticheat
created: 2026-06-11
updated: 2026-06-11
status: pending
type: implementation
estimatedDays: 3-5 (active) + 5-7 (deferred phases)
totalLoc: ~800 (active) + ~1400 (deferred)
brainstorm: ../reports/brainstorm-260611-1038-multi-server-portability-anticheat.md
relatedPlans:
  - ../260530-2118-anti-detect-tier1/         # overlap nhẹ Phase 2; tier1 focus XC3, plan này broader
blockedBy: []
blocks: []
riskLevel: MEDIUM
---

# Multi-server portability + anti-cheat layered defense

Cho phép WindowHelper dùng lại trên nhiều server PT private khác nhau, kèm layered defense chống anti-cheat (GameGuard).

## Context
- Brainstorm: `plans/reports/brainstorm-260611-1038-multi-server-portability-anticheat.md`
- Revision note (2026-06-11): server đích KHÔNG xác định loại AC. Tool external (không inject, không read mem, không hook) → threat tier thấp. Reassess → giảm scope active xuống Phase 1+2; Phase 3+4 deferred theo YAGNI.
- Blocker chính: vision ROI + hue ranges hard-coded ở [src/main.cpp:157-159](../../src/main.cpp#L157-L159), assume frame 1010x789.
- Anti-cheat risk độc lập với portability → tách phase, build incrementally.

## Principles
- KISS/YAGNI/DRY, file <200 LOC.
- Capture giữ `WgcCapture` nguyên — KHÔNG đụng.
- `IInputBackend` interface đã abstract → phase 3/4 chỉ thêm backend mới.
- Test thực tế trên server đích trước khi sang phase tiếp theo (YAGNI gate).

## Phases

### Active
| # | Phase | Status | Effort | Risk | Gate |
|---|-------|--------|--------|------|------|
| 1 | [Vision config + Calibration UI](phase-01-vision-config-calibration.md) | pending | 2-3d | LOW | Calibrate server mới < 5 phút |
| 2 | [Stealth user-mode](phase-02-stealth-user-mode.md) | pending | 1-2d | LOW | Tool sống ≥ 1h trên server bất kỳ AC nhẹ |

### Deferred (only activate when needed)
| # | Phase | Status | Trigger to activate |
|---|-------|--------|---------------------|
| 3 | [Interception driver backend](phase-03-interception-backend.md) | deferred | Phase 2 đã ship + bị detect/block input trên server đích thực tế |
| 4 | [Hardware HID backend](phase-04-hardware-hid-backend.md) | deferred | Phase 3 đã ship + vẫn bị detect |

## Build order
1. **Phase 1 bắt buộc** — independent of AC, useful cho mọi server.
2. **Phase 2 next** — cost thấp, cover phần lớn AC user-mode scan.
3. **Phase 3, 4 spec giữ** nhưng KHÔNG implement until có evidence cần. YAGNI.

## Threat model reassessment
Tool external, không inject/hook/read-mem → threat tier thấp:
- Hầu hết AC (EAC, BattlEye, GameGuard, Xigncode3, HackShield) target DLL inject, mem hook, packet manip → tool không vi phạm.
- Risk thực tế còn lại: window/process name scan (Phase 2 cover), behavioral server-side (Humanizer đã có baseline).
- Hardware HID + driver backend chỉ cần khi gặp AC scan input source — hiếm trên PT private.

## Key files affected
- [src/main.cpp](../../src/main.cpp) — remove hardcoded ROI, switch backend selector.
- [src/config/config-loader.cpp](../../src/config/config-loader.cpp), [src/state/game-state.h](../../src/state/game-state.h) — thêm `VisionConfig` + `StealthConfig`.
- [src/ui/main-window.cpp](../../src/ui/main-window.cpp) — calibration tab + backend selector UI.
- [src/input/](../../src/input/) — thêm `interception-backend.{h,cpp}`, `serial-hid-backend.{h,cpp}`.
- [build.ps1](../../build.ps1) — exe name customization.
- `config.json` schema — version bump + vision/stealth/backend sections.

## Risks
- GG update detect Interception driver signature → mitigate bằng Phase 4 fallback.
- User UAC từ chối cài driver → fallback SendInput, error message rõ.
- Ban acc khi test → warning UI + document throwaway acc workflow.

## Unresolved
- Server đích cụ thể chưa xác định → ảnh hưởng Phase 2-3 test plan.
- Phase 4 HW model (CH9329 vs Pico) chưa quyết — chọn khi tới phase.
