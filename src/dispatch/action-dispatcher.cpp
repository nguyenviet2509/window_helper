#include "action-dispatcher.h"

ActionDispatcher::ActionDispatcher(InputScheduler& s, CombatFsm& c, const AppConfig& cfg)
    : sched_(s), combat_(c), pots_(cfg.pot), cfg_(cfg) {}

void ActionDispatcher::updateConfig(const AppConfig& cfg) {
    cfg_ = cfg;
    pots_.updateConfig(cfg.pot);
    combat_.updateConfig(cfg.combat);
}

bool ActionDispatcher::schedulePot(const PotAction& a) {
    InputCmd c;
    c.priority = a.priority;
    c.fireAt = std::chrono::steady_clock::now();
    WORD vk = a.vk;
    int hold = a.holdMs;
    c.action = [vk, hold](IInputBackend& b) { b.sendKeyTap(vk, hold); };
    sched_.schedule(std::move(c));
    if (log_) log_(a.tag, a.priority, a.vk);
    return true;
}

void ActionDispatcher::onVisionTick(const VisionState& v) {
    if (!v.valid) return;
    auto now = std::chrono::steady_clock::now();

    // P0 HP emergency.
    if (auto a = pots_.evalHp(v, now))     { schedulePot(*a); return; }
    // P1 MP / SP.
    if (auto a = pots_.evalMpSp(v, now))   { schedulePot(*a); return; }
    // P2 Recall.
    if (auto a = pots_.evalRecall(v, now)) { schedulePot(*a); return; }
    // P3 / P4 — combat FSM owns its own scheduling. Just advance it.
    combat_.tick(v, now);
}
