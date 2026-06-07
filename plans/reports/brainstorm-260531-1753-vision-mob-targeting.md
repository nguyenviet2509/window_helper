# Brainstorm: Vision-Based Mob Targeting (PT)

**Date:** 2026-05-31 17:53
**Scope:** Giải pain "click trượt mob" trong `CombatFsm` + `AttackSweep`.
**Decision:** Two-phase plan — Phase 1 target-frame feedback (MVP), Phase 2 ML mob detection (optimization).

## Pain Point

Game PT không có snap-to-mob. `sendShiftRightClick(x,y)` click chính xác pixel → nếu trượt mob → mất `engagementLockMs` (~3s) im lặng vì timer-based lock.

Current AttackSweep ([attack-sweep.h](src/combat/attack-sweep.h)) click mù → tỷ lệ miss cao → hiệu suất combat thấp.

## Key Visual Insight (PT)

Mob trong field **KHÔNG có HP bar / nameplate overhead**. Nhưng game render **target portrait frame ở góc trên-phải** với hành vi:

- Hiện khi shift+right-click trúng mob
- Mất khi mob chết
- Mất khi mob ra ngoài tầm / bỏ target
- Chứa **HP bar mob ngay dưới portrait**

→ Đây là **deterministic UI signal** cho "có đang lock target không" + "mob HP".

## Phase 1: Target-Frame Feedback Loop (MVP)

### Concept

Thay timer-based `engagementUntil_` bằng vision-based `targetLocked`. Bot click nhanh hơn nhiều khi miss vì biết miss ngay (không phải đợi timer giả định).

```
Click attack → 150-300ms grace → check target frame ROI
  ├─ Populated → targetLocked=true → im lặng cho đến khi clear
  └─ Empty    → targetLocked=false → click sector tiếp theo NGAY
```

### Architecture Changes

**1. Detector mới: `TargetFrameDetector`** (file: `src/vision/target-frame-detector.h/.cpp`)
- Input: full frame BGRA + ROI cho portrait box
- Output: `{ bool present; double mobHpPct; }`
- Implementation options (ưu tiên đơn giản):
  - **(A) Pixel variance**: portrait có variance cao, ROI trống thấp → threshold
  - **(B) Border detection**: frame có border màu cố định → match
  - **(C) HP bar reuse**: dùng `BarDetector` có sẵn cho mob HP bar dưới portrait — nếu detect được fill > 0 → present. Bonus: lấy luôn mobHpPct.
- **Khuyến nghị: (C)** — tái dụng module, ít code mới, lấy luôn HP.

**2. Extend `VisionState`** ([roi.h](src/vision/roi.h)):
```cpp
struct VisionState {
    double hpPct = 0, mpPct = 0, spPct = 0;
    bool valid = false;
    uint64_t seq = 0;
    // NEW:
    bool targetLocked = false;
    double mobHpPct = 0.0;   // valid khi targetLocked
};
```

**3. Extend `VisionPipeline`**:
- Add `BarConfig mobHpCfg_` (constructor param)
- `runLoop()` thêm detect target frame → publish `targetLocked`, `mobHpPct`

**4. Modify `CombatFsm::stepAttacking`** ([combat-fsm.cpp:94](src/combat/combat-fsm.cpp#L94)):
- Replace timer `engagementUntil_` với check `v.targetLocked`:
  ```cpp
  // OLD: bool inLock = now < engagementUntil_;
  bool inLock = v.targetLocked;  // vision-based
  ```
- Vẫn giữ `attackCooldownMs` (anti-burst) — không bỏ
- Vẫn giữ `repickMaxDwellMs` cho safety (forced repick nếu targetLocked stuck)
- Optional cleanup: bỏ field `engagementUntil_`, `engagementLockMs`, `engagementLockJitterMs` (hoặc giữ làm fallback nếu vision invalid)

**5. AttackSweep stratified random** (từ brainstorm trước, [brainstorm-260531-1743-attack-sweep-coverage.md](plans/reports/brainstorm-260531-1743-attack-sweep-coverage.md)):
- Stratified angle sampling với shuffled sectors
- Tích hợp song song với target-frame feedback

**6. Config additions:**
- `targetFrameROI: { x, y, w, h }` (calibrate UI)
- `mobHpBarROI: { x, y, w, h }` + hue ranges
- `targetLockGracePeriodMs` (default 200ms — đợi UI render sau click)
- Optional: keep `engagementLockMs` as fallback when vision unavailable

### Failure Modes & Mitigations

| Failure | Mitigation |
|---|---|
| Target frame ROI miscalibrated | UI calibration step như HP/MP/SP bars |
| Bot click trúng nhưng UI delay > grace period → false miss | Grace period configurable; dùng `attackCooldownMs` cũng floor lại click rate |
| Target frame stuck (mob bug) | `repickMaxDwellMs` force unlock |
| Vision pipeline crash / frame stale | Fallback về timer-based engagementLockMs |

### Effort Estimate

- Detector + ROI calibration: 0.5d
- VisionState + Pipeline integration: 0.3d
- FSM refactor: 0.3d
- UI config exposure: 0.2d
- Manual test + tune: 0.5d
- **Total: 1.5-2 days**

## Phase 2: ML Mob Detection (Optimization)

Phase 1 giảm cost mỗi miss xuống ~200ms, nhưng miss rate vẫn cao (sweep mù). Phase 2 giảm miss rate.

### Approach: YOLOv8-nano + ONNX Runtime

**Data collection:**
- Script capture frame mỗi N giây trong soak combat
- Manual label với LabelImg / Roboflow (bounding box quanh mob)
- Target: 200-500 labeled frames per map type
- Augmentation: HSV jitter, scale, flip

**Training:**
- YOLOv8n (smallest variant, ~3-6M params, ~10MB)
- Train trên Colab GPU (free)
- Export ONNX format

**Runtime integration:**
- Dep: `onnxruntime` (C++ static lib, ~10-15MB)
- New module: `src/vision/mob-detector.h/.cpp`
  - Load .onnx model
  - Inference 5-10Hz (không cần 20Hz)
  - Output: `std::vector<MobDetection>{x, y, w, h, conf}`
- Extend `VisionState` với `std::vector<MobDetection> mobs`
- Modify `AttackSweep::pickAttackPosition` → nếu `!mobs.empty()` chọn nearest mob; else fallback stratified random

### Concerns Phase 2

- **Training data quality** > model size. PT mob variety lớn → cần dataset đa dạng.
- **Latency budget**: YOLOv8n CPU inference ~30-80ms/frame trên modern CPU. Chạy 5Hz là OK.
- **Player & NPCs**: cần label phân biệt mob vs player/NPC để khỏi attack nhầm.
- **Model versioning**: shipping model file riêng, không bake vào binary.

### Effort Estimate

- Data collection tool: 1d
- Labeling 500 frames: 0.5d
- Training + tuning: 1d
- ONNX C++ integration: 2d
- Mob filter (vs player/NPC): 1d
- Soak test + tune: 1d
- **Total: ~1.5 weeks**

## Final Recommendation

**Ship Phase 1 trước.** Lý do:
- Solve 80% pain với 10% effort
- Validate target frame approach hoạt động ổn định trên PT
- Build infrastructure (VisionState extensions, FSM refactor) cần thiết cho Phase 2
- Observe metric: với Phase 1, miss rate còn ~bao nhiêu → quyết Phase 2 có justified không

**Phase 2 khi nào:**
- Sau khi Phase 1 ship + soak ≥ 1 tuần
- Nếu miss rate vẫn > 30% / nếu user muốn tối ưu thêm
- Skip Phase 2 nếu Phase 1 đủ tốt

## Implementation Order

1. **Phase 1**:
   - 1.1 `AttackSweep` stratified sectors (brainstorm-260531-1743)
   - 1.2 `TargetFrameDetector` reuse `BarDetector` cho mob HP bar
   - 1.3 `VisionState` + `VisionPipeline` extend
   - 1.4 `CombatFsm` vision-based engagement lock
   - 1.5 Config + UI calibration
   - 1.6 Manual test + tune
2. **Phase 2** (later):
   - 2.1 Data collection tool
   - 2.2 Labeling + training
   - 2.3 ONNX runtime integration
   - 2.4 `MobDetector` module
   - 2.5 `AttackSweep` mob-aware mode

## Open Questions

- Target portrait frame ROI tọa độ chính xác? (cần user calibrate)
- HP bar mob trong frame có dùng cùng hue range với HP player không? (likely khác — cần verify)
- `attackCooldownMs` còn cần thiết sau khi engagement vision-based? (giữ làm anti-burst floor)
- Có muốn log target-lock telemetry (lock duration, miss rate per sector) để tune sau không?
- Phase 2 fallback: nếu mob detector confidence thấp toàn bộ → fallback stratified random (auto) hay tắt mob mode?
