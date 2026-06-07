---
phase: 04
title: "CombatFsm — vision-based engagement lock thay timer"
status: pending
priority: P1
effort: 3h
blockers: [03]
supersedes: "260531-0900-engagement-lock (timer-based)"
---

## Context
- Hiện tại `stepAttacking` ([combat-fsm.cpp:114](src/combat/combat-fsm.cpp#L114)) dùng `engagementUntil_` timer.
- Pain: timer = đoán mò. Trượt mob vẫn mất ~3s im lặng.
- Solution: dùng `v.targetLocked` (deterministic) thay timer.

## Goal
Click trượt → biết ngay (sau hysteresis ~150-200ms) → click sector tiếp. Click trúng → im lặng đến khi mob chết / unlock.

## Files
- Modify: `src/combat/combat-fsm.h` (bỏ field timer, optional fallback flag)
- Modify: `src/combat/combat-fsm.cpp` (rewrite `stepAttacking`)
- Keep: `engagementLockMs/JitterMs` config fields làm **fallback** khi vision không khả dụng

## Implementation Steps

### 1. `stepAttacking` mới
```cpp
void CombatFsm::stepAttacking(const VisionState& v, std::chrono::steady_clock::time_point now) {
    activity_.update(v.hpPct, v.mpPct);

    // Re-buff cycle expiry.
    if ((now - cycleStart_) >= sec(cfg_.cycleDurationSec)) {
        enterBuffing(now);
        return;
    }

    // MP gate.
    if (cfg_.waitMpGate && v.mpPct < cfg_.waitMpGateThreshold) return;

    auto dwell = now - lastPickAt_;
    bool forceRepick = dwell >= ms(cfg_.repickMaxDwellMs);
    bool firstClick = lastPickAt_.time_since_epoch().count() == 0;

    // === VISION-BASED ENGAGEMENT LOCK ===
    // Nếu pipeline vision hợp lệ → dùng targetLocked. Else fallback timer.
    bool inLock;
    if (v.valid) {
        inLock = v.targetLocked;
    } else {
        // Fallback: timer cũ
        inLock = now < engagementUntil_;
    }

    if (inLock && !forceRepick && !firstClick) return;

    // Anti-burst floor giữa 2 click.
    if (!firstClick && (now - lastAttackAt_) < ms(cfg_.attackCooldownMs)) return;

    // Grace period sau click cho UI render target frame.
    if (!firstClick && (now - lastAttackAt_) < ms(cfg_.targetLockGracePeriodMs)) return;

    auto [x, y] = sweep_.pickAttackPosition(target_);
    InputCmd c;
    c.priority = P3_Combat;
    c.fireAt = now;
    c.action = [x, y](IInputBackend& b) { b.sendShiftRightClick(x, y); };
    sched_.schedule(std::move(c));

    lastPickAt_ = now;
    lastAttackAt_ = now;
    activity_.reset();

    // Fallback timer set (chỉ active khi vision invalid).
    int jitter = cfg_.engagementLockJitterMs > 0
        ? (std::rand() % (cfg_.engagementLockJitterMs + 1)) : 0;
    engagementUntil_ = now + ms(cfg_.engagementLockMs + jitter);
}
```

### 2. Config thêm field
- `int targetLockGracePeriodMs = 250;` — đợi UI sau click trước khi check lock (anti false-miss).
- Tunable Phase 6.

### 3. Behavior matrix

| `v.valid` | `v.targetLocked` | Action |
|---|---|---|
| true | true | Stay silent (until unlock or maxDwell) |
| true | false | Click next sector (after cooldown + grace) |
| false | — | Fallback timer-based engagementLockMs |

### 4. Logging
- Log mỗi transition `targetLocked` true↔false trong FSM với current dwell.
- Log mỗi click với reason: `unlock_detected` / `force_repick` / `first_click`.

## Edge Cases
- Mob chết ngay sau click → unlock detect → click ngay → OK.
- Mob HP rất thấp trigger threshold → may unlock false. Tune `presenceThreshold` (Phase 2 default 0.05) hoặc grace.
- `targetLockGracePeriodMs` quá ngắn → click trước khi UI render → false miss → click thừa. Default 250ms safe.
- `targetLockGracePeriodMs` quá dài → miss real misses. Trade-off tune Phase 6.

## Success Criteria
- Compile.
- Click trúng mob (manual) → bot im lặng ≥ thời gian mob alive.
- Click trượt (point empty area) → bot click tiếp sau ~250-400ms (cooldown + grace).
- Soak 5 min không stuck silent / không spam.

## Risks
- `repickMaxDwellMs` fallback essential cho safety khi vision sai. Keep default 8s.
- Engagement-lock plan (260531-0900) bị supersede phần lock logic; UI slider engagementLockMs vẫn giữ làm fallback config.
