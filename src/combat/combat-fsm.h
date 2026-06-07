#pragma once
// Combat finite-state machine.
// States: IDLE -> BUFFING -> ARMING -> ATTACKING -> (cycle expires) -> BUFFING ...
// Schedules concrete InputCmd actions on the provided InputScheduler.
// PAUSED is implicit: when OutputGate denies input, scheduler drops cmds.
#include <chrono>
#include <optional>
#include <windows.h>

#include <vector>

#include "../state/game-state.h"
#include "../dispatch/priority.h"
#include "../input/input-scheduler.h"
#include "attack-sweep.h"
#include "combat-activity-monitor.h"
#include "../vision/roi.h"

enum class CombatState { Idle, Buffing, Arming, Attacking };

class PotRefillScheduler;   // fwd decl: skip combat khi refill đang chạy

class CombatFsm {
public:
    CombatFsm(InputScheduler& sched, HWND target, const CombatConfig& cfg);

    void enable(bool on);
    bool enabled() const { return enabled_; }

    void tick(const VisionState& v, std::chrono::steady_clock::time_point now);

    void updateConfig(const CombatConfig& cfg);
    void setRefillScheduler(const PotRefillScheduler* r) { refill_ = r; }

    CombatState state() const { return state_; }

private:
    void enterBuffing(std::chrono::steady_clock::time_point now);
    void enterArming(std::chrono::steady_clock::time_point now);
    void enterAttacking(std::chrono::steady_clock::time_point now);

    void stepBuffing(std::chrono::steady_clock::time_point now);
    void stepAttacking(const VisionState& v, std::chrono::steady_clock::time_point now);

    // Trả index slot enabled đầu tiên đang due (now - lastCastAt_[i] >= interval).
    // -1 nếu không slot nào due.
    int findDueSlot(std::chrono::steady_clock::time_point now) const;
    void syncLastCastSize();   // resize lastCastAt_ khớp cfg_.buffs.size().

    InputScheduler& sched_;
    HWND target_;
    CombatConfig cfg_;

    bool enabled_ = false;
    CombatState state_ = CombatState::Idle;

    AttackSweep sweep_;
    CombatActivityMonitor activity_;

    std::chrono::steady_clock::time_point nextStepAt_{};
    std::chrono::steady_clock::time_point lastPickAt_{};
    std::chrono::steady_clock::time_point lastAttackAt_{};
    std::chrono::steady_clock::time_point engagementUntil_{};  // im lặng cho đến điểm này
    // Per-slot last cast timestamp; epoch zero = chưa cast lần nào → due ngay.
    std::vector<std::chrono::steady_clock::time_point> lastCastAt_;
    const PotRefillScheduler* refill_ = nullptr;
};
