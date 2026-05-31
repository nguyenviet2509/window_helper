#include "combat-fsm.h"
#include "pot-refill-scheduler.h"

#include <cstdlib>

using ms = std::chrono::milliseconds;
using sec = std::chrono::seconds;

CombatFsm::CombatFsm(InputScheduler& s, HWND target, const CombatConfig& cfg)
    : sched_(s), target_(target), cfg_(cfg),
      buffs_(cfg.buffs),
      sweep_(cfg.attackRadiusMin, cfg.attackRadiusMax) {}

void CombatFsm::enable(bool on) {
    enabled_ = on;
    if (!on) { state_ = CombatState::Idle; return; }
    auto now = std::chrono::steady_clock::now();
    if (buffs_.empty()) enterArming(now);
    else enterBuffing(now);
}

void CombatFsm::updateConfig(const CombatConfig& cfg) {
    cfg_ = cfg;
    buffs_.updateSlots(cfg.buffs);
    sweep_.setRange(cfg.attackRadiusMin, cfg.attackRadiusMax);
}

void CombatFsm::enterBuffing(std::chrono::steady_clock::time_point now) {
    state_ = CombatState::Buffing;
    buffs_.reset();
    nextStepAt_ = now;
    cycleStart_ = now;
    buffsDeliveredThisRound_ = 0;
}

void CombatFsm::enterArming(std::chrono::steady_clock::time_point now) {
    state_ = CombatState::Arming;
    InputCmd c;
    c.priority = P3_Combat;
    c.fireAt = now;
    WORD vk = cfg_.mainAttackKey;
    int holdMs = 30;
    c.action = [vk, holdMs](IInputBackend& b) { b.sendKeyTap(vk, holdMs); };
    sched_.schedule(std::move(c));
    enterAttacking(now + ms(120));
}

void CombatFsm::enterAttacking(std::chrono::steady_clock::time_point now) {
    state_ = CombatState::Attacking;
    activity_.reset();
    lastPickAt_ = now;
    lastAttackAt_ = now;
}

void CombatFsm::stepBuffing(std::chrono::steady_clock::time_point now) {
    if (now < nextStepAt_) return;

    auto slot = buffs_.nextBuff();
    if (!slot) { enterArming(now); return; }

    HWND target = target_;
    WORD vk = slot->key;
    int hold = 30;

    InputCmd keyCmd;
    keyCmd.priority = P4_Buff;
    keyCmd.fireAt = now;
    keyCmd.action = [vk, hold](IInputBackend& b) { b.sendKeyTap(vk, hold); };
    sched_.schedule(std::move(keyCmd));

    if (slot->rightClickAfter) {
        RECT r{}; GetClientRect(target, &r);
        int cx = (r.right + r.left) / 2;
        int cy = (r.bottom + r.top) / 2;
        InputCmd clickCmd;
        clickCmd.priority = P4_Buff;
        clickCmd.fireAt = now + ms(slot->castDelayMs / 2);
        clickCmd.action = [cx, cy](IInputBackend& b) { b.sendRightClick(cx, cy); };
        sched_.schedule(std::move(clickCmd));
    }

    nextStepAt_ = now + ms(slot->castDelayMs);
    ++buffsDeliveredThisRound_;

    int enabledCount = 0;
    for (const auto& b : cfg_.buffs) if (b.enabled) ++enabledCount;
    if (buffsDeliveredThisRound_ >= enabledCount) {
        // Round complete -> arm after last buff finishes.
        state_ = CombatState::Arming;
        nextStepAt_ = now + ms(slot->castDelayMs + 200);
    }
}

void CombatFsm::stepAttacking(const VisionState& v, std::chrono::steady_clock::time_point now) {
    activity_.update(v.hpPct, v.mpPct);

    // Re-buff cycle expiry.
    if ((now - cycleStart_) >= sec(cfg_.cycleDurationSec)) {
        enterBuffing(now);
        return;
    }

    // MP gate: skip if low MP.
    if (cfg_.waitMpGate && v.mpPct < cfg_.waitMpGateThreshold) return;

    auto dwell = now - lastPickAt_;
    bool minDwellOk = dwell >= ms(cfg_.repickMinDwellMs);
    bool forceRepick = dwell >= ms(cfg_.repickMaxDwellMs);
    bool mobDead = minDwellOk && activity_.mobLikelyDead();
    bool firstClick = lastPickAt_.time_since_epoch().count() == 0;

    // Engagement lock: sau shift+right-click, game tự auto-chain attack mob đó.
    // Im lặng cho đến khi mob chết, hết lock window, hoặc chạm maxDwell.
    bool inLock = now < engagementUntil_;
    if (inLock && !mobDead && !forceRepick && !firstClick) return;

    // Hard floor giữa 2 repick click liên tiếp (anti-burst).
    if (!firstClick && (now - lastAttackAt_) < ms(cfg_.attackCooldownMs)) return;

    auto [x, y] = sweep_.pickAttackPosition(target_);
    InputCmd c;
    c.priority = P3_Combat;
    c.fireAt = now;
    c.action = [x, y](IInputBackend& b) { b.sendShiftRightClick(x, y); };
    sched_.schedule(std::move(c));

    lastPickAt_ = now;
    lastAttackAt_ = now;
    activity_.reset();

    int jitter = cfg_.engagementLockJitterMs > 0
        ? (std::rand() % (cfg_.engagementLockJitterMs + 1))
        : 0;
    engagementUntil_ = now + ms(cfg_.engagementLockMs + jitter);
}

void CombatFsm::tick(const VisionState& v, std::chrono::steady_clock::time_point now) {
    if (!enabled_) return;
    if (refill_ && refill_->busy()) return;   // pause combat khi refill đang chạy

    switch (state_) {
    case CombatState::Idle:
        enterBuffing(now);
        break;
    case CombatState::Buffing:
        stepBuffing(now);
        break;
    case CombatState::Arming:
        if (now >= nextStepAt_) enterArming(now);
        break;
    case CombatState::Attacking:
        stepAttacking(v, now);
        break;
    }
}
