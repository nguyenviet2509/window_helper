---
phase: 01
title: "Stratified angle sampling với shuffled sectors"
status: pending
priority: P1
effort: 2h
---

## Context
- Brainstorm: [brainstorm-260531-1743-attack-sweep-coverage.md](../reports/brainstorm-260531-1743-attack-sweep-coverage.md)
- Current: random uniform angle [0, 2π] mỗi click → không đảm bảo coverage 360°.
- Anti-detect cao → KHÔNG dùng sweep CW/CCW (autocorrelation ~1).

## Goal
Đảm bảo coverage 360° sau N click trong khi giữ randomness chống ML detector.

## Algorithm
- Chia 360° thành `numSectors_` (default 8, configurable 4-16).
- Maintain shuffled permutation `sectorOrder_`. Visit theo thứ tự shuffled, reshuffle khi visit hết.
- Angle = `sector * (2π/N) + uniform(0, 2π/N)`.
- Radius: giữ Gaussian như cũ.

## Files
- Modify: `src/combat/attack-sweep.h`
- Modify: `src/state/game-state.h` (thêm `int attackSectorCount = 8` vào `CombatConfig`)
- Modify: `src/config/config-loader.cpp` (parse `attackSectorCount`)

## Implementation Steps

### 1. Extend `AttackSweep`
```cpp
class AttackSweep {
public:
    AttackSweep(int rMin, int rMax, int numSectors);
    std::pair<int,int> pickAttackPosition(HWND game);
    void setRange(int rMin, int rMax);
    void setSectorCount(int n);

private:
    void reshuffleSectors();
    int rMin_, rMax_;
    int numSectors_ = 8;
    std::vector<int> sectorOrder_;
    size_t sectorIdx_ = 0;
    std::uniform_real_distribution<double> sectorJitter_;  // [0, 2π/N]
    // ... existing rng_, radius_ Gaussian
};
```

### 2. `reshuffleSectors`
- Fill `sectorOrder_` với [0..N-1].
- `std::shuffle(sectorOrder_.begin(), sectorOrder_.end(), rng_)`.
- `sectorIdx_ = 0`.

### 3. `pickAttackPosition`
- If `sectorIdx_ == 0` → `reshuffleSectors()`.
- `sector = sectorOrder_[sectorIdx_]`.
- `sectorIdx_ = (sectorIdx_ + 1) % numSectors_`.
- `double base = sector * (2π / numSectors_);`
- `double angle = base + sectorJitter_(rng_);`
- `double rad = sampleRadius();` (unchanged)
- Return `(cx + cos(angle)*rad, cy + sin(angle)*rad)`.

### 4. `setSectorCount`
- Clamp [2, 32]. If changed → reset `sectorOrder_` + reshuffle. Update `sectorJitter_` range.

### 5. Config wiring
- `CombatConfig::attackSectorCount = 8`.
- `CombatFsm` constructor: `sweep_(cfg.attackRadiusMin, cfg.attackRadiusMax, cfg.attackSectorCount)`.
- `updateConfig`: call `sweep_.setSectorCount(cfg.attackSectorCount)`.

## Edge Cases
- `numSectors_ < 2` → fallback random uniform (skip sector logic).
- Constructor: gọi `reshuffleSectors()` trước khi pick đầu tiên.

## Success Criteria
- Compile sạch.
- Unit-visual: log 100 click angles → distribute đều khắp 8 sector.
- Autocorrelation lag-1 trên dãy angles ~0 (vẫn random trong sector).

## Risks
- Nếu N quá lớn (>16) jitter sector quá nhỏ → cluster. Default 8 OK.
