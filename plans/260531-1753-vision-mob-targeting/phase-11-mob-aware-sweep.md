---
phase: 11
title: "Mob-aware AttackSweep — click chính xác lên mob khi có detections"
status: pending
priority: P2
effort: 1d
blockers: [10]
---

## Context
Có mob list từ Phase 10. Thay vì sweep stratified mù, chọn nearest mob → click trên bbox center (với jitter).

## Goal
`AttackSweep::pickAttackPosition` ưu tiên mob list; fallback stratified random.

## Files
- Modify: `src/combat/attack-sweep.h` (overload signature consume mobs)
- Modify: `src/combat/combat-fsm.cpp` (pass `v.mobs` vào pick call)
- Modify: `src/state/game-state.h` (`CombatConfig::mobAwareEnabled = true`, `mobConfThreshold = 0.5`)

## Implementation Steps

### 1. AttackSweep API extend
```cpp
class AttackSweep {
public:
    // Existing: stratified random fallback
    std::pair<int,int> pickAttackPosition(HWND game);

    // NEW: mob-aware
    std::pair<int,int> pickAttackPosition(
        HWND game,
        const std::vector<MobDetection>& mobs,
        double confThreshold);
};
```

### 2. Logic
```cpp
auto pickAttackPosition(HWND game, const std::vector<MobDetection>& mobs, double conf) {
    // Filter by confidence
    std::vector<const MobDetection*> good;
    for (const auto& m : mobs) if (m.conf >= conf) good.push_back(&m);
    if (good.empty()) return pickAttackPosition(game);  // fallback stratified

    // Compute client center
    RECT r{}; GetClientRect(game, &r);
    int cx = (r.right+r.left)/2, cy = (r.bottom+r.top)/2;

    // Pick nearest mob (by L2 distance from cx,cy)
    auto* best = *std::min_element(good.begin(), good.end(),
        [cx,cy](const MobDetection* a, const MobDetection* b) {
            auto d = [cx,cy](const MobDetection* m){
                int mx = m->x + m->w/2, my = m->y + m->h/2;
                return (mx-cx)*(mx-cx) + (my-cy)*(my-cy);
            };
            return d(a) < d(b);
        });

    // Click target: bbox center + Gaussian jitter (anti-detect)
    int tx = best->x + best->w/2;
    int ty = best->y + best->h/2;
    // Jitter scale: ~20% bbox size (small, vẫn trong bbox)
    std::normal_distribution<double> jx(0, best->w * 0.15);
    std::normal_distribution<double> jy(0, best->h * 0.15);
    tx += static_cast<int>(jx(rng_));
    ty += static_cast<int>(jy(rng_));
    // Clamp trong bbox để không click outside
    tx = std::clamp(tx, best->x, best->x + best->w);
    ty = std::clamp(ty, best->y, best->y + best->h);
    return {tx, ty};
}
```

### 3. Anti-target-repeat
- Avoid clicking same mob right after unlock (mob có thể bị engaged rồi died, list stale).
- Simple: track `lastClickedMobBbox`; nếu nearest mob trùng bbox lớn → skip & pick next nearest.
- Or rely on `targetLocked` feedback (Phase 4) — click stale mob → not lock → next click pick from updated mob list.

### 4. FSM wiring
```cpp
// combat-fsm.cpp stepAttacking
auto [x, y] = cfg_.mobAwareEnabled
    ? sweep_.pickAttackPosition(target_, v.mobs, cfg_.mobConfThreshold)
    : sweep_.pickAttackPosition(target_);
```

### 5. Config
```cpp
struct CombatConfig {
    // ... existing
    bool mobAwareEnabled = true;
    double mobConfThreshold = 0.5;
};
```

UI toggle + slider.

## Edge Cases
- `mobs.empty()` → fallback random sweep (continue coverage 360°).
- All mobs below confidence → fallback random.
- Mob bbox 0×0 (model bug) → skip, pick next.
- Multi-mob cluster → nearest priority OK; future: priority HP-low / target-distance.

## Success Criteria
- Khi có mob in screen: click coord trong bbox 95%+ time.
- Khi không có mob detection: fallback stratified random (Phase 1 behavior preserved).
- Per-click jitter visible (anti-detect).

## Risks
- Stale mob list (mob đã chết) → wasted click. Mitigated by `targetLocked` feedback + lower interval.
- Player/NPC misclassified as mob → attack nhầm. Mitigation Phase 8: include player/npc class trong training để filter rõ.
