---
phase: 02
title: "TargetFrameDetector — reuse BarDetector cho mob HP bar"
status: pending
priority: P1
effort: 3h
---

## Context
- Mob HP bar render dưới portrait trong target frame UI (góc trên-phải).
- ROI populated ↔ đang lock target. ROI trống ↔ no target.
- Tận dụng `BarDetector` ([bar-detector.h](src/vision/bar-detector.h)) — không viết detector mới from scratch.

## Goal
Module detect: (a) đang lock target hay không; (b) mob HP %.

## Files
- New: `src/vision/target-frame-detector.h`
- New: `src/vision/target-frame-detector.cpp`
- Modify: `src/CMakeLists.txt` (add 2 file)

## Implementation Steps

### 1. Header `target-frame-detector.h`
```cpp
#pragma once
#include <opencv2/core.hpp>
#include "roi.h"
#include "bar-detector.h"

struct TargetFrameResult {
    bool locked = false;
    double mobHpPct = 0.0;   // valid khi locked
};

class TargetFrameDetector {
public:
    TargetFrameDetector(BarRegion hpBarRoi, std::vector<HueRange> hpHues,
                        double presenceThreshold = 0.05);

    TargetFrameResult detect(const cv::Mat& bgra) const;

    void setRoi(const BarRegion& roi) { hpBarRoi_ = roi; }
    void setHues(std::vector<HueRange> h) { hues_ = std::move(h); }
    void setPresenceThreshold(double t) { presenceThreshold_ = t; }

private:
    BarRegion hpBarRoi_;
    std::vector<HueRange> hues_;
    double presenceThreshold_;
    BarDetector det_;
};
```

### 2. Implementation `target-frame-detector.cpp`
```cpp
TargetFrameResult TargetFrameDetector::detect(const cv::Mat& bgra) const {
    TargetFrameResult r;
    double fill = det_.computeFillRatio(bgra, hpBarRoi_, hues_);
    // fill = 0 khi ROI không có hue match → no target.
    // fill > threshold → có HP bar visible → locked.
    if (fill > presenceThreshold_) {
        r.locked = true;
        r.mobHpPct = fill;
    }
    return r;
}
```

### 3. Detection logic
- `BarDetector::computeFillRatio` trả 0..1 dựa trên hue mask + waterline.
- Khi target frame trống → no pixel match hue → fill ≈ 0.
- Khi target lock → HP bar visible → fill = HP% mob.
- Threshold default 0.05 = mob HP <5% vẫn coi locked (mob gần chết). Edge case: mob HP = 0% (vừa chết) → frame still drawn? Verify trong Phase 6.

### 4. CMakeLists
Add 2 file vào `add_executable` (theo pattern hiện tại của bar-detector).

## Edge Cases
- ROI miscalibrated (rỗng / wrong area) → luôn fill 0 → never lock → fallback `repickMaxDwellMs` (Phase 4).
- Mob HP rất thấp (<5%) → có thể bị coi unlocked dù vẫn engaging. Tune threshold xuống 0.02 nếu cần.
- Frame transition (mob switch) → 1 frame có thể flicker. EMA smoothing handled at VisionPipeline level (Phase 3).

## Success Criteria
- Compile sạch.
- Manual test với screenshot mẫu: ROI có portrait → `locked=true`; ROI trống → `locked=false`.
- mobHpPct giảm dần khi player attack mob.

## Risks
- Hue range HP bar mob có thể khác HP player (mob bar màu khác). Cần calibrate trong Phase 5.
- Nếu game render portrait nhưng HP bar full opaque cùng màu → fill có thể stuck ở 1.0. OK.
