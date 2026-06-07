---
phase: 07
title: "Data collection tool — frame dumper với context metadata"
status: pending
priority: P2
effort: 1d
blockers: [06]
---

## Context
Phase 2 cần dataset frames chứa mob đa dạng (loại, map, lighting, pose). Tool dump frames trong combat session.

## Goal
Capture mode toggle-able trong app: lưu frames + metadata (timestamp, targetLocked, mob name nếu có) sang folder cho labeling sau.

## Files
- New: `src/tools/frame-dumper.h/cpp`
- Modify: `src/vision/vision-pipeline.cpp` (optional sink callback when dump enabled)
- Modify: `src/ui/main-window.cpp` (toggle + folder picker)
- Modify: `src/state/game-state.h` (`DumpConfig`)

## Implementation Steps

### 1. `FrameDumper`
```cpp
class FrameDumper {
public:
    struct Config {
        std::string outDir;
        int intervalMs = 500;       // 2Hz
        bool onlyWhenLocked = false; // capture cả unlocked để có negative samples
        size_t maxFrames = 5000;    // safety cap
    };
    void enable(bool on);
    void onFrame(const cv::Mat& bgra, const VisionState& s, uint64_t seq);
private:
    Config cfg_;
    std::atomic<bool> enabled_{false};
    std::chrono::steady_clock::time_point nextAt_{};
    size_t count_ = 0;
    std::ofstream csv_;  // metadata
};
```

### 2. Output structure
```
dataset/
  20260601-1430/
    frame_0001.png
    frame_0002.png
    ...
    metadata.csv  # frame_id,timestamp,seq,target_locked,mob_hp_pct,hp_pct,mp_pct
```

### 3. Capture trigger
- `VisionPipeline::runLoop` sau mỗi frame compute → if dumper enabled & interval elapsed → invoke `dumper_->onFrame()`.
- Add `setDumper(FrameDumper*)` to pipeline (optional sink).
- PNG encode dùng `cv::imwrite` (OpenCV có sẵn).

### 4. UI
- Tab "Data Capture":
  - Enable toggle
  - Output folder picker
  - Interval slider (200-2000ms)
  - Filter: "only when target locked" checkbox
  - Live counter "frames captured: N"

### 5. Storage budget
- 1024x768 BGRA PNG ≈ 500KB-1MB.
- 5000 frames ≈ 2.5-5GB. Cap reasonable.
- User clear dataset folder thủ công.

## Edge Cases
- Disk full → log error, disable dumper, không crash.
- Folder không tồn tại → tạo (`std::filesystem::create_directories`).
- Concurrent access metadata.csv → flush mỗi line (no buffer).

## Success Criteria
- Toggle UI → folder filled với PNG + CSV.
- 30 min capture → ≥1000 frames diverse mob/map.
- Metadata CSV parseable.

## Risks
- Performance impact pipeline khi encode PNG (~20-50ms/frame). Mitigation: dump trên thread riêng (queue + worker).
- File handle leak. Cleanup `FrameDumper::~`.
