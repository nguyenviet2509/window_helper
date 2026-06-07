# Brainstorm: Attack Sweep Angle Coverage

**Date:** 2026-05-31 17:43
**Scope:** `src/combat/attack-sweep.h` — angle picking strategy cho SHIFT+right-click.
**Decision:** Stratified random với shuffled sectors. Bác đề xuất gốc (CW/CCW sweep).

## Problem Statement

User báo: click attack hay trượt mob, muốn đổi từ random angle sang sweep vòng tròn (CW/CCW xen kẽ) để tăng tốc và độ chính xác.

Anti-detect requirement: CAO (yêu cầu cứng).
Game targeting: shift+right-click click chính xác pixel, **không có snap-to-mob**.

## Current Implementation

`AttackSweep::pickAttackPosition` ([attack-sweep.h:19](src/combat/attack-sweep.h#L19)):
- Angle: uniform random [0, 2π]
- Radius: Gaussian quanh mean của [rMin, rMax], σ = range/6
- Mục tiêu thiết kế gốc: né ML detector

FSM dùng kết quả → `sendShiftRightClick(x, y)` → set `engagementUntil_` = now + `engagementLockMs`. Trong lock → FSM im lặng.

## Approaches Evaluated

### A. Đề xuất gốc — Sweep CW/CCW xen kẽ
- `angle += step` mỗi click, đảo dấu sau mỗi vòng đủ 2π
- ✅ Coverage 360° đảm bảo
- ❌ **Angular autocorrelation ~1.0** — exactly pattern ML detector flag
- ❌ Mâu thuẫn với anti-detect priority CAO
- ❌ Không fix click trượt (vẫn click mù)

### B. Stratified random với shuffled sectors — **CHỌN**
- Chia 360° thành N sector (default 8, 45°/sector)
- Sectors visit theo permutation shuffled; reshuffle khi visit hết
- Angle = `sector_base + uniform(0, sectorSize)`; radius Gaussian giữ nguyên
- ✅ Coverage 360° sau N click
- ✅ Autocorrelation thấp (jitter trong sector + shuffle thứ tự)
- ✅ Code đơn giản
- ❌ Không fix click trượt triệt để

### C. Vision-based mob targeting
- Detect mob position từ frame → click đúng
- ✅ Giải triệt để click trượt
- ❌ Effort lớn, ngoài scope
- → Roadmap riêng

## Recommended Solution: B

### Thay đổi `AttackSweep`

State mới:
```cpp
int numSectors_;                       // config, default 8
std::vector<int> sectorOrder_;         // shuffled permutation [0..N-1]
size_t sectorIdx_ = 0;
std::uniform_real_distribution<double> sectorJitter_;  // [0, 2π/N]
```

`pickAttackPosition`:
1. Nếu `sectorIdx_ == 0` → reshuffle `sectorOrder_` (Fisher-Yates với `rng_`)
2. `sector = sectorOrder_[sectorIdx_]`; `sectorIdx_ = (sectorIdx_ + 1) % numSectors_`
3. `angle = sector * (2π / numSectors_) + sectorJitter_(rng_)`
4. `radius = sampleRadius()` (giữ nguyên)
5. Return `(cx + cos(angle)*radius, cy + sin(angle)*radius)`

### Config

- Thêm `attackSectorCount` vào `CombatConfig` + `config.json` (default 8, range 4–16, validate)
- Truyền vào constructor `AttackSweep`
- `updateConfig` propagate khi user đổi

### Edge cases

- `numSectors_ < 2` → fallback về random uniform
- Reshuffle dùng `std::shuffle(sectorOrder_, rng_)`
- Setrange không reset sectorIdx_ (giữ tiến độ vòng quét)

## Caveats / Unresolved

- **Click trượt vẫn xảy ra.** Stratified chỉ cải thiện coverage, không phải hit rate per click. Mỗi miss vẫn tốn `engagementLockMs` im lặng.
- **Tuning N:** N=8 chỉ là default suy đoán. Cần observe in-game xem N=6 hay N=12 cho hit rate tốt hơn. Nên log số click trước khi mob chết để tune.
- **Radius coverage:** Gaussian tập trung quanh mean. Nếu mob hay xuất hiện ở viền (gần rMax) → click cluster ở giữa miss nhiều. Có thể cần variant `sampleRadius` uniform hoặc 2-mode distribution sau.
- **Vision-based targeting** là hướng giải triệt để click trượt — cần brainstorm riêng nếu user muốn theo đuổi.

## Next Steps

- [ ] Implement thay đổi `AttackSweep` (B)
- [ ] Thêm config `attackSectorCount` + UI binding
- [ ] Compile + manual test soak (đo hit rate)
- [ ] Tune `numSectors_` qua observation

## Open Questions

- Có muốn log click-miss telemetry để tune N không?
- `attackSectorCount` có expose ra UI runtime hay chỉ trong config.json?
