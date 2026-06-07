---
phase: 06
title: "Phase 1 compile + manual soak + tune"
status: pending
priority: P1
effort: 4h
blockers: [01,02,03,04,05]
---

## Context
End of Phase 1. Verify hệ thống end-to-end trước khi mở Phase 2.

## Files
- No code changes (chỉ tune config).
- Log to: `build/bin/Release/logs/WindowHelper.log`.
- Create: `plans/260531-1753-vision-mob-targeting/reports/phase1-soak-results.md`

## Steps

### 1. Build
- CMake configure + build Release.
- Fix compile errors nếu có.

### 2. ROI calibration
- Launch app + game.
- Calibrate mob HP bar ROI trong UI tab Mob HP.
- Verify live preview: spawn mob → click → ROI fills.

### 3. Manual functional test (15 min)
| Scenario | Expected |
|---|---|
| Click trúng mob | `targetLocked=true` log; bot im lặng |
| Mob chết | `targetLocked` flip false; bot click ngay sau ~300ms |
| Click trượt (giả lập: di chuyển bot khỏi mob area) | `targetLocked=false`; bot click sector tiếp ngay |
| Mob ra tầm | unlock detect → repick |
| Force unlock (maxDwell 8s) | bot force repick dù vẫn locked |

### 4. Soak test ≥1h
- Combat enable, log everything.
- Metrics cần extract từ log:
  - Click count per minute (rate)
  - Lock acquisition rate (% click → locked within grace)
  - Lock duration distribution (histogram)
  - False unlock count (lock→unlock→lock <500ms)
  - Crash count = 0

### 5. Tune knobs
- Nếu false-miss cao → tăng `targetLockGracePeriodMs` (300-400ms)
- Nếu lock flicker → tăng hysteresis frames (Phase 3, từ 2 → 3)
- Nếu coverage cluster 1 hướng → tăng `attackSectorCount` (12)

### 6. Compare vs baseline
- Run với vision DISABLED (set ROI w=0) → bot dùng timer fallback.
- Đo click rate + mob kill rate.
- Vision should improve mob kill rate ≥1.5x baseline.

### 7. Soak report
Doc kết quả vào `reports/phase1-soak-results.md`:
- Click rate before/after
- Mob kill rate before/after
- Crash/anomaly observations
- Recommended tune values cho `config.json` default

## Success Criteria
- Build sạch.
- 1h soak no crash.
- Mob kill rate ≥1.5x baseline (timer-only).
- All tune knobs có recommendation.

## Risks
- Game-specific behavior phát hiện → có thể cần revisit Phase 4 logic.
- Anti-cheat trigger với click rate cao hơn → backoff (tăng cooldown).

## Decision Point
- ✅ Pass → Phase 2 unlock.
- ❌ Fail → fix root cause Phase 2-5 trước khi tiếp tục.
