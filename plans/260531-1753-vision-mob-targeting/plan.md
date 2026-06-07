---
title: "Vision-based Mob Targeting (Phase 1 + Phase 2)"
description: "Phase 1: target-frame feedback + stratified sweep. Phase 2: YOLOv8 + ONNX Runtime mob detection."
status: pending
priority: P1
effort: P1=2d, P2=~1.5w
branch: master
tags: [combat, vision, anti-detect, ml]
created: 2026-05-31
blockedBy: []
blocks: []
supersedes:
  - 260531-0900-engagement-lock  # Phase 1.4 thay timer engagementLockMs bằng vision targetLocked
relatedPlans:
  - 260530-2118-anti-detect-tier1  # tận dụng Gaussian/Bezier đã làm
---

# Plan — Vision-based Mob Targeting

## Context
- Brainstorm reports:
  - [brainstorm-260531-1753-vision-mob-targeting.md](../reports/brainstorm-260531-1753-vision-mob-targeting.md)
  - [brainstorm-260531-1743-attack-sweep-coverage.md](../reports/brainstorm-260531-1743-attack-sweep-coverage.md)
- Pain: PT không snap-to-mob. Click trượt → mất `engagementLockMs` (~3s) im lặng.
- Insight: mob không có overhead bar, nhưng target portrait frame (top-right) chứa mob HP bar khi locked → deterministic signal cho "đang engage".
- Decision: split 2 phase. Phase 1 (MVP target-frame feedback, ~2d). Phase 2 (ML mob detection, ~1.5w).

## Principles
- KISS/YAGNI/DRY. Không mock. Reuse `BarDetector` cho Phase 1.
- File <200 LOC. Modularize theo concern.
- Ship Phase 1 + soak ≥1 tuần trước khi kích hoạt Phase 2.

## Phases — Phase 1: Target-Frame Feedback + Stratified Sweep

| # | Phase | File | Status | Effort | Blockers |
|---|-------|------|--------|--------|----------|
| 01 | Stratified angle sampling | [phase-01-stratified-sweep.md](phase-01-stratified-sweep.md) | pending | 2h | — |
| 02 | TargetFrameDetector (reuse BarDetector) | [phase-02-target-frame-detector.md](phase-02-target-frame-detector.md) | pending | 3h | — |
| 03 | VisionState + Pipeline extend | [phase-03-vision-state-pipeline.md](phase-03-vision-state-pipeline.md) | pending | 2h | 02 |
| 04 | FSM vision-based engagement lock | [phase-04-fsm-vision-engagement.md](phase-04-fsm-vision-engagement.md) | pending | 3h | 03 |
| 05 | Config + UI calibration | [phase-05-config-ui-calibration.md](phase-05-config-ui-calibration.md) | pending | 3h | 02,04 |
| 06 | Phase-1 compile + manual soak + tune | [phase-06-phase1-soak-tune.md](phase-06-phase1-soak-tune.md) | pending | 4h | 01-05 |

## Phases — Phase 2: ML Mob Detection (sau Phase 1)

| # | Phase | File | Status | Effort | Blockers |
|---|-------|------|--------|--------|----------|
| 07 | Data collection tool (frame dumper) | [phase-07-data-collection-tool.md](phase-07-data-collection-tool.md) | pending | 1d | 06 |
| 08 | Labeling + YOLOv8n training | [phase-08-labeling-training.md](phase-08-labeling-training.md) | pending | 1.5d | 07 |
| 09 | ONNX Runtime C++ integration | [phase-09-onnx-runtime-integration.md](phase-09-onnx-runtime-integration.md) | pending | 2d | 06 |
| 10 | MobDetector module + VisionState extend | [phase-10-mob-detector-module.md](phase-10-mob-detector-module.md) | pending | 1.5d | 08,09 |
| 11 | Mob-aware AttackSweep | [phase-11-mob-aware-sweep.md](phase-11-mob-aware-sweep.md) | pending | 1d | 10 |
| 12 | Phase-2 soak + tune + ship | [phase-12-phase2-soak-tune.md](phase-12-phase2-soak-tune.md) | pending | 1d | 11 |

## Success Criteria
- **Phase 1:**
  - Click miss cost giảm từ ~3s xuống ~300ms (engagement lock vision deterministic)
  - Coverage 360° đảm bảo sau N click (stratified)
  - Soak ≥1h: no crash, no false-lock spam
- **Phase 2:**
  - Mob hit rate per click ≥70% khi mobs in screen
  - Inference latency <100ms (5-10Hz pipeline)
  - Fallback stratified random khi detector confidence low

## Risks
- Target frame ROI miscalibrate → false `targetLocked` → bot stuck silent. Mitigation: `repickMaxDwellMs` force unlock; calibrate UI step.
- Phase 2 dataset size không đủ → low recall. Mitigation: data augmentation; iterative labeling.
- ONNX runtime dep tăng binary size. Mitigation: dynamic load; ship model file separate.

## Dependencies
- OpenCV (đã có)
- ONNX Runtime C++ (Phase 2 only)
- LabelImg / Roboflow (Phase 2 data tooling)
