---
phase: 12
title: "Phase 2 soak + tune + ship decision"
status: pending
priority: P2
effort: 1d
blockers: [11]
---

## Context
End of Phase 2. Validate mob-aware targeting cải thiện ≥30% hit rate vs Phase 1.

## Files
- No code (tune only).
- Create: `plans/260531-1753-vision-mob-targeting/reports/phase2-soak-results.md`

## Steps

### 1. Build + smoke
- CMake build với onnxruntime + model file.
- Verify `onnxruntime.dll` next to exe.
- Smoke test: launch, vision pipeline produce mob list (log size per second).

### 2. Manual functional test
| Scenario | Expected |
|---|---|
| 1 mob in screen | Click within bbox center (jitter) → targetLocked instant |
| Multi-mob | Click nearest mob first |
| Mob ngoài view | No detections → fallback stratified sweep |
| Mob hidden (occlusion) | May miss; fallback stratified |
| Model file missing | Bot vẫn chạy với Phase 1 logic (stratified + targetLocked) |

### 3. Soak ≥2h
Metrics:
- Click count / total
- Click landed in detected mob bbox: %
- targetLocked acquired within grace: %
- Mob kill rate per minute
- Inference latency p50/p95
- Crash count = 0

### 4. Compare phases
| Metric | Baseline (timer) | Phase 1 | Phase 2 |
|---|---|---|---|
| Click rate (cpm) | base | ? | ? |
| Lock acquisition % | ? | ? | ? |
| Mob kills / hour | base | ≥1.5× | ≥2× |

### 5. Tune
- `mobConfThreshold`: tăng nếu false positives nhiều (NPC/decoration); giảm nếu miss mob.
- Mob detector interval (Phase 10 N frames): 5Hz default; thử 10Hz nếu CPU dư.
- IoU NMS threshold: default 0.45.

### 6. Decision Point
| Result | Action |
|---|---|
| ✅ ≥2× baseline kill rate | Ship Phase 2 production |
| ⚠️ 1.5-2× | Iterate dataset (Phase 8 retrain), repeat |
| ❌ <1.5× | Disable mob-aware default; keep as opt-in flag; revisit dataset/model |

### 7. Production rollout
- Default `mobAwareEnabled = true` chỉ khi model file exists.
- Ship `mob-detector.onnx` trong release zip.
- Update `docs/build-deploy-test-guide.md` với onnxruntime prerequisite.
- Update `dist/HUONG-DAN-CAU-HINH.md` user guide.

### 8. Report
`reports/phase2-soak-results.md`:
- Metrics table
- Comparison vs Phase 1
- Tune recommendations
- Known issues / future work (multi-map model, attention to player avoidance, etc.)

## Success Criteria
- Soak 2h no crash.
- Mob kill rate ≥2× baseline timer-only.
- Inference p95 <100ms.
- Production config defaults safe.

## Risks
- Anti-cheat may detect higher hit rate → throttle: respect existing `attackCooldownMs`.
- Player avoidance failure (attack PC nhầm) → critical. Phase 8 phải có label `player` rõ.

## Open Questions
- Có cần GPU inference cho late-game (nhiều mob cùng lúc)? Default CPU đủ.
- Retrain cadence khi user farm map mới? → 1-2 lần / năm tuỳ user feedback.
