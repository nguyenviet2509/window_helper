# Phase 3 — Action Dispatcher + Combat FSM + Mob Death Detect

**Est:** 2 ngày
**Priority:** P0 — critical logic
**Status:** pending
**Depends:** Phase 1 (vision), Phase 2 (input + gate)

## Mục Tiêu
- ActionDispatcher với 5 priority levels (P0–P4) + preempt logic
- Combat FSM: IDLE → BUFFING → ARMING → ATTACKING → re-buff
- Smart mob death detect (MP+HP sliding window 2s)

## Files
```
src/dispatch/
├── action-dispatcher.h/.cpp
├── action.h                            # InputCmd struct với priority
└── priority.h                          # P0..P4 enum
src/combat/
├── combat-fsm.h/.cpp
├── combat-activity-monitor.h/.cpp      # sliding window mob death
├── buff-sequencer.h/.cpp
├── attack-sweep.h/.cpp
└── pot-evaluator.h/.cpp                # P0-P2 pot trigger logic
src/state/
└── game-state.h                        # VisionState shared
```

LOC: ~600.

## Implementation

### 3.1 ActionDispatcher (Section 23)
```cpp
enum Priority { P0_HpEmergency, P1_MpSp, P2_Recall, P3_Combat, P4_Buff };

class ActionDispatcher {
public:
    void onVisionTick(const VisionState& v, TimePoint now);
private:
    std::optional<Action> evalP0_Hp(const VisionState& v);
    std::optional<Action> evalP1_MpSp(const VisionState& v);
    std::optional<Action> evalP2_Recall(const VisionState& v);
    std::optional<Action> evalP3_Combat(const VisionState& v);   // delegate to CombatFSM
    std::optional<Action> evalP4_Buff(const VisionState& v);     // delegate to BuffSequencer
    
    void execute(Action a);                // preempt + dispatch
    std::optional<Action> inflight_;
    InputScheduler& input_;
    OutputGate& gate_;
};
```

Evaluator chạy TỪ CAO TỚI THẤP priority mỗi vision tick. Stop ngay khi có action.

### 3.2 PotEvaluator (P0/P1/P2)
```cpp
class PotEvaluator {
public:
    std::optional<Action> evalHp(const VisionState& v, TimePoint now);    // P0
    std::optional<Action> evalMp(const VisionState& v, TimePoint now);    // P1
    std::optional<Action> evalSp(const VisionState& v, TimePoint now);    // P1
    std::optional<Action> evalRecall(const VisionState& v, TimePoint now); // P2
private:
    TimePoint lastHpPot_, lastMpPot_, lastSpPot_, lastRecall_;
    TimePoint hpBelowSince_;       // theo dõi HP < threshold lâu để trigger P2
    int hpConfirmCount_ = 0;       // 2-frame confirm
};
```

P0 HP logic (Section 23.4):
- `v.hpPct > threshold` → reset confirm counter
- `< threshold`: confirm++; nếu confirm ≥ 2 và now-lastHpPot ≥ cooldown(600ms) → emit Action{P0, key=cfg.hp.key, jitter=[0,8]}
- Track `hpBelowSince_` để P2 Recall trigger sau N giây

### 3.3 CombatActivityMonitor (Section 32)
```cpp
class CombatActivityMonitor {
    std::deque<float> mpSamples_, hpSamples_;
    int windowSize_ = 40;
public:
    void update(float hp, float mp);
    void reset();
    bool mobLikelyDead() const;
private:
    float maxMin(const std::deque<float>& d) const;
};
```

`mobLikelyDead`: cần đủ 40 sample (2s), MP drain < 1% VÀ HP drop < 1%.

### 3.4 CombatFsm (Section 29, 31)
```cpp
enum class CombatState { IDLE, BUFFING, ARMING, ATTACKING, PAUSED };

class CombatFsm {
public:
    void tick(const VisionState& v, TimePoint now);
    std::optional<Action> nextAction();    // for ActionDispatcher.evalP3
private:
    void stepBuffing(TimePoint now);
    void enterArming(TimePoint now);
    void stepAttacking(TimePoint now);
    CombatState state_ = IDLE, resumeState_;
    int currentBuffIdx_ = 0;
    TimePoint nextStep_, cycleStart_, lastTargetPick_;
    CombatActivityMonitor activity_;
    BuffSequencer& buffs_;
    AttackSweep& attack_;
};
```

BUFFING flow:
- For each enabled buff in order: emit Action{P3, key=buffKey, then sendRightClick (KHÔNG SHIFT)}, wait `cast_delay_ms`
- Hết queue → ARMING

ARMING: emit Action{P3, key=cfg.combat.main_attack_key (F1)}, set cycleStart_, → ATTACKING.

ATTACKING:
- Cycle timeout (cycle_duration_sec=300) → BUFFING
- `wait_mp_gate`: nếu enabled và MP < threshold, idle
- Min dwell 2s sau pick → check `activity_.mobLikelyDead()` hoặc max_dwell 15s → repick
- Repick: `pickAttackPosition()` qua `AttackSweep` → moveCursor + sendShiftRightClick

### 3.5 AttackSweep (Mode A stationary)
```cpp
class AttackSweep {
public:
    cv::Point pickAttackPosition(HWND game) const;
private:
    int rMin_, rMax_;     // từ config
};
```

Annulus around game center (player position):
```cpp
RECT r; GetClientRect(game, &r);
cv::Point center{(r.right+r.left)/2, (r.bottom+r.top)/2};
double angle = uniform(0, 2*M_PI);
double radius = uniform(rMin_, rMax_);
return {center.x + int(cos(angle)*radius), center.y + int(sin(angle)*radius)};
```

### 3.6 BuffSequencer
```cpp
struct BuffSlot { bool enabled; std::string key; int castDelayMs; };
class BuffSequencer {
public:
    void reset();
    std::optional<BuffSlot> nextBuff();    // skip disabled
private:
    std::vector<BuffSlot> slots_;
    size_t idx_ = 0;
};
```

### 3.7 PAUSED Resume
FSM check `OutputGate.allowInput()` mỗi tick. Nếu false → save state vào `resumeState_`, set PAUSED. Khi gate mở → restore.

## Integration Points
- VisionPipeline (Phase 1) push `VisionState` mỗi 50ms
- ActionDispatcher.onVisionTick consume
- Dispatcher gọi InputScheduler.schedule (Phase 2)
- InputScheduler check OutputGate trước mỗi send

## Test (qua PtMockGame)
- Hồi HP: slider HP xuống 50% → mock log key `1` xuất hiện
- HP emergency: slider 30% → key `1` ngay với jitter tối thiểu (log timestamp tick-vs-send delta < 100ms)
- Buff cycle: bật AUTO → log F2→right→F3→right→F4→right→F5→right→F1 trong ~2.5s
- Attack sweep: bật mock "mob alive sim" → log mouse SHIFT+right-click ở positions trong annulus
- Mob death detect: tắt mob alive sim → 2s sau log repick mới
- Recall: slider HP 12% giữ > 3s → log F12
- PAUSED: minimize mock → log "input dropped" stream

## Acceptance
- [ ] P0 HP preempt cancel P3/P4 đúng
- [ ] Buff sequence chạy đúng thứ tự F2→F3→F4→F5→F1
- [ ] SHIFT+right-click khác right-click (verify backend specific message)
- [ ] Mob death detect repick < 2.5s sau mob "die" (mock)
- [ ] Max dwell fallback 15s hoạt động
- [ ] Re-buff sau 300s đúng

## Risks
- Race condition giữa vision tick thread và dispatcher → all state read-only trong eval, write trong execute
- Activity monitor false positive khi MP regen nhanh → tăng threshold lên 1.5% nếu cần

## Next
Phase 4 wrap UI quanh logic này.
