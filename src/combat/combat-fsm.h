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

enum class CombatState { Idle, Buffing, Arming, Attacking, Spamming };

class PotRefillScheduler;   // fwd decl: skip combat khi refill đang chạy

class CombatFsm {
public:
    CombatFsm(InputScheduler& sched, HWND target, const CombatConfig& cfg);

    void enable(bool on);
    bool enabled() const { return enabled_; }
    // Master gate buff (F9 hotkey). Tách biệt với enable() — F8 chỉ đánh, F9 chỉ buff.
    void setBuffEnabled(bool on) { cfg_.buffEnabled = on; }
    bool buffEnabled() const { return cfg_.buffEnabled; }

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
    void stepSpamming(std::chrono::steady_clock::time_point now);
    void enterSpamming(std::chrono::steady_clock::time_point now);

    // Cache safe-spot tọa độ từ buffSafeSpot{X,Y}Pct * client rect. Gọi khi enable spam hoặc pct đổi.
    void cacheSpamSpot();
    // Random uniform [5000, 10000] ms — interval giữa các left-click humanizer.
    int rollHumanIntervalMs() const;
    // Random [-10, +10] px jitter quanh safe spot.
    int jitter10() const;

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

    // Spam mode state.
    int spamX_ = 0;
    int spamY_ = 0;
    std::chrono::steady_clock::time_point lastSpamAt_{};
    bool pendingF1AfterBuff_ = false;
    // Humanizer: schedule left-click random 5-10s.
    std::chrono::steady_clock::time_point nextHumanClickAt_{};
    std::chrono::steady_clock::time_point lastHumanClickAt_{};
};
