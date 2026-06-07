---
phase: 03
title: "Extend VisionState + VisionPipeline với targetLocked / mobHpPct"
status: pending
priority: P1
effort: 2h
blockers: [02]
---

## Context
- `VisionState` ([roi.h](src/vision/roi.h)) chỉ có hp/mp/sp. Cần thêm `targetLocked` + `mobHpPct`.
- `VisionPipeline::runLoop` ([vision-pipeline.cpp](src/vision/vision-pipeline.cpp)) hiện compute 3 bar. Thêm step 4: detect target frame.

## Goal
Pipeline publish `VisionState` với target frame state đồng bộ frame data.

## Files
- Modify: `src/vision/roi.h` (extend struct)
- Modify: `src/vision/vision-pipeline.h` (add ctor param + member)
- Modify: `src/vision/vision-pipeline.cpp` (detect + populate state)
- Modify: `src/main.cpp` (build TargetFrameDetector config, pass to pipeline)

## Implementation Steps

### 1. Extend `VisionState` ([roi.h](src/vision/roi.h))
```cpp
struct VisionState {
    double hpPct = 0;
    double mpPct = 0;
    double spPct = 0;
    bool valid = false;
    uint64_t seq = 0;
    // NEW (Phase 1):
    bool targetLocked = false;
    double mobHpPct = 0.0;
    // NEW (Phase 2 — added empty, populated later):
    // std::vector<MobDetection> mobs;  // (defer to Phase 10)
};
```

### 2. Extend `VisionPipeline`
```cpp
class VisionPipeline {
public:
    VisionPipeline(IFrameSource& src,
                   BarConfig hp, BarConfig mp, BarConfig sp,
                   BarConfig mobHp);   // NEW arg

private:
    BarConfig mobHpCfg_;
    TargetFrameDetector targetDet_;
    EmaFilter mobHpEma_{0.5};
    // Optional: small hysteresis counter cho targetLocked transitions
    int lockStableFrames_ = 0;
};
```

### 3. `runLoop` extension
Sau khi compute hp/mp/sp:
```cpp
TargetFrameResult tgt = targetDet_.detect(f.bgra);
// Hysteresis 2 frames để tránh flicker:
if (tgt.locked) lockStableFrames_ = std::min(lockStableFrames_ + 1, 10);
else            lockStableFrames_ = std::max(lockStableFrames_ - 1, 0);
s.targetLocked = lockStableFrames_ >= 2;
s.mobHpPct = s.targetLocked ? mobHpEma_.update(tgt.mobHpPct) : 0.0;
if (!s.targetLocked) mobHpEma_.reset();
```

### 4. Hysteresis rationale
- 1-frame flicker khi UI render → tránh `targetLocked` bật/tắt rapidly.
- Threshold 2 frames @ 20Hz = 100ms grace.
- Khi unlock: reset EMA để lần lock sau bắt đầu sạch.

### 5. `main.cpp` wiring
- Build `BarConfig mobHpCfg{ region: targetFrameROI, hues: mobHpHues }` từ AppConfig.
- Pass vào `VisionPipeline` constructor.
- Read ROI + hue từ `config.json` (Phase 5 sẽ add field).

## Edge Cases
- `mobHp` ROI invalid (w=0/h=0) → BarDetector returns 0 → never lock → safe.
- Frame stale (`acquire` timeout) → state không update → consumers see last known. OK.

## Success Criteria
- Compile.
- Log line: vision tick có `targetLocked=Y` khi engaging mob, `=N` khi idle.
- Transition lock→unlock takes ≥2 frames (100ms).

## Risks
- Hysteresis 2 frames có thể quá ngắn nếu game render lag. Tunable trong Phase 6.
- EMA alpha 0.5 → mobHpPct lag ~2 frames. Acceptable.
