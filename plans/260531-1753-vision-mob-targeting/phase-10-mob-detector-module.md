---
phase: 10
title: "MobDetector module + VisionState mobs vector"
status: pending
priority: P2
effort: 1.5d
blockers: [08, 09]
---

## Context
Compose ONNX session + YOLO post-process thành 1 module producible `std::vector<MobDetection>`.

## Goal
`VisionPipeline` (at lower Hz) consume mob detections, populate `VisionState::mobs`.

## Files
- New: `src/vision/mob-detector.h/cpp`
- Modify: `src/vision/roi.h` (struct `MobDetection`; extend `VisionState`)
- Modify: `src/vision/vision-pipeline.h/cpp` (sub-loop or N-frame interval)

## Implementation Steps

### 1. `MobDetection` struct
```cpp
struct MobDetection {
    int x, y, w, h;          // bbox in source frame coords
    double conf;             // 0..1
    int classId;             // 0=mob, 1=player, 2=npc
};
```

### 2. Extend `VisionState`
```cpp
struct VisionState {
    // ... existing
    std::vector<MobDetection> mobs;     // chỉ class=mob, sorted by distance to player
    uint64_t mobsSeq = 0;               // separate seq vì mob detect ở Hz thấp hơn
};
```

### 3. `MobDetector`
```cpp
class MobDetector {
public:
    struct Config {
        std::string modelPath;
        double confThreshold = 0.5;
        double iouThreshold = 0.45;
        int maxDets = 50;
        // Filter: chỉ trả mob (drop player/npc)
        std::vector<int> includeClasses{0};
    };
    explicit MobDetector(Config cfg);
    bool available() const { return session_ != nullptr; }

    std::vector<MobDetection> detect(const cv::Mat& bgra);
private:
    Config cfg_;
    std::unique_ptr<OnnxSession> session_;
    std::vector<MobDetection> nms(std::vector<MobDetection>) const;
};
```

### 4. Detection pipeline
1. BGRA → BGR
2. `OnnxSession::run(bgr)` → raw tensor
3. Decode YOLOv8 output (84 = 4 box + 80 classes hoặc tuỳ): per anchor pick best class score.
4. Filter by `confThreshold`.
5. Filter by `includeClasses`.
6. NMS (IoU threshold) → final detections.
7. Sort by distance to player center (client center).

### 5. Pipeline integration
- `VisionPipeline` runLoop @20Hz cho bars; mob detect chậm hơn (5Hz, mỗi 4 frame).
- Add member: `int mobDetectFrameCounter_ = 0;`
- Every N frames → `mobs = mobDetector_.detect(f.bgra);` + `s.mobs = mobs;` + `s.mobsSeq++;`
- Other frames: keep last `s.mobs` (re-publish stale).

### 6. Graceful disable
- If `mobDetector_.available() == false` → skip detect, `s.mobs` luôn empty.
- AttackSweep (Phase 11) tự fallback random khi `s.mobs.empty()`.

## Edge Cases
- Inference exception (corrupt model) → catch, disable detector, log.
- 0 detections → empty vector OK.
- Stale detection (mob đã chết nhưng vẫn trong list) → AttackSweep tự verify bằng targetLocked sau click.

## Success Criteria
- Build + smoke test: feed 5 game screenshots → detect mob bbox đúng vị trí.
- Latency <100ms/frame.
- Pipeline 20Hz bar không bị chậm khi mob detect ON (vì 5Hz).

## Risks
- YOLOv8 output decode dễ sai. Tham khảo ultralytics inference example C++.
- Class ID order phải khớp `dataset.yaml` names.
