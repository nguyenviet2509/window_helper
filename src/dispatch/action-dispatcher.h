#pragma once
// ActionDispatcher: single entry point per vision tick.
// Priority order: P0 HP -> P1 MP/SP -> P2 Recall -> P3 Combat -> P4 Buff.
// P0..P2 handled here via PotEvaluator. P3/P4 handled by CombatFsm.
#include <chrono>
#include <functional>

#include "../state/game-state.h"
#include "../vision/roi.h"
#include "../input/input-scheduler.h"
#include "../combat/pot-evaluator.h"
#include "../combat/combat-fsm.h"
#include "priority.h"

class ActionDispatcher {
public:
    ActionDispatcher(InputScheduler& sched, CombatFsm& combat, const AppConfig& cfg);

    void onVisionTick(const VisionState& v);

    void updateConfig(const AppConfig& cfg);

    // Optional debug logger (signature: void(const char* tag, int priority, WORD vk)).
    void setLogger(std::function<void(const char*, int, WORD)> log) { log_ = std::move(log); }

private:
    bool schedulePot(const PotAction& a);

    InputScheduler& sched_;
    CombatFsm& combat_;
    PotEvaluator pots_;
    AppConfig cfg_;
    std::function<void(const char*, int, WORD)> log_;
};
