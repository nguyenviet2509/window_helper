# Phase 02 — Redesign CombatActivityMonitor

## Status: pending
## Priority: high
## Effort: 30 min

## Overview
Bỏ hoàn toàn HP tracking + sliding-window max-min logic. Thay bằng MP downward-tick: track `prevMp_` + `lastMpDropAt_` timestamp. Mob dead khi không có tick âm trong `deathConfirmMs` (default 2500ms).

## Rationale
Spec user: "MP tụt rồi hồi lại vẫn tính là đang attack". Logic max-min cũ nhiễu vì MP regen lên cao tạo max-min lớn dù mob đã chết. Logic mới chỉ quan tâm **thời điểm tick âm cuối** — regen lên không xóa dấu.

## File: `src/combat/combat-activity-monitor.h`

**Rewrite hoàn toàn:**
```cpp
#pragma once
// MP-only mob-death detect: track last "MP went down" timestamp. If no
// downward MP tick within deathConfirmMs window, current target likely dead.
// "MP tụt rồi hồi" vẫn tính active vì timer chỉ reset khi có tick âm mới.
#include <chrono>

class CombatActivityMonitor {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    void update(double mp, TimePoint now);
    void reset(TimePoint now);
    bool mobLikelyDead(TimePoint now) const;

    // Tunable (load from config.json).
    int deathConfirmMs = 2500;     // no-MP-drop window to declare dead
    double mpDropEpsilon = 0.005;  // 0.5% threshold for "MP went down" tick

private:
    double prevMp_ = -1.0;                          // sentinel "no sample yet"
    TimePoint lastMpDropAt_ = TimePoint::min();     // last "MP went down" event
};
```

## File: `src/combat/combat-activity-monitor.cpp`

**Rewrite hoàn toàn:**
```cpp
#include "combat-activity-monitor.h"

void CombatActivityMonitor::update(double mp, TimePoint now) {
    if (prevMp_ >= 0.0) {
        double delta = mp - prevMp_;
        if (delta <= -mpDropEpsilon) {
            lastMpDropAt_ = now;   // mark "still attacking"
        }
    }
    prevMp_ = mp;
}

void CombatActivityMonitor::reset(TimePoint now) {
    prevMp_ = -1.0;
    lastMpDropAt_ = now;   // 2.5s grace after pick mob mới
}

bool CombatActivityMonitor::mobLikelyDead(TimePoint now) const {
    if (lastMpDropAt_ == TimePoint::min()) return false;
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - lastMpDropAt_).count();
    return elapsed >= deathConfirmMs;
}
```

## Implementation Steps
1. Backup current `combat-activity-monitor.{h,cpp}` (git tracks → skip).
2. Rewrite `.h` per snippet trên.
3. Rewrite `.cpp` per snippet trên.

## Todo
- [ ] Rewrite combat-activity-monitor.h
- [ ] Rewrite combat-activity-monitor.cpp

## Success Criteria
- File compile được sau Phase 03 (call site update).
- Không còn `<deque>`, `<algorithm>`, `hp_`, `mp_`, `windowSize`, `maxMinusMin`.

## Risks
- API thay đổi: `update(hp,mp)` → `update(mp,now)`, `reset()` → `reset(now)`, `mobLikelyDead()` → `mobLikelyDead(now)`. Phase 03 phải update call site đồng bộ.

## Next
Phase 03 — Wire FSM + config loader.
