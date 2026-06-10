#include "combat-fsm.h"
#include "pot-refill-scheduler.h"

#include <algorithm>
#include <cstdlib>

using ms = std::chrono::milliseconds;
using sec = std::chrono::seconds;

CombatFsm::CombatFsm(InputScheduler& s, HWND target, const CombatConfig& cfg)
    : sched_(s), target_(target), cfg_(cfg),
      sweep_(cfg.attackRadiusMin, cfg.attackRadiusMax) {
    syncLastCastSize();
}

void CombatFsm::syncLastCastSize() {
    // Preserve existing timestamps; new slots default = epoch zero (due ngay).
    lastCastAt_.resize(cfg_.buffs.size(), std::chrono::steady_clock::time_point{});
}

bool buffsAnyEnabled(const std::vector<BuffSlotCfg>& v) {
    for (const auto& b : v) if (b.enabled) return true;
    return false;
}

void CombatFsm::enable(bool on) {
    enabled_ = on;
    if (!on) { state_ = CombatState::Idle; return; }
    auto now = std::chrono::steady_clock::now();
    // F8 chỉ kích hoạt đánh quái — KHÔNG cast buff lúc khởi động.
    // Buff chạy độc lập qua master switch F9 (cfg_.buffEnabled) + chu kỳ rebuff
    // trong stepAttacking. Nếu F9 OFF, findDueSlot luôn trả -1 → không buff.
    enterArming(now);
}

void CombatFsm::updateConfig(const CombatConfig& cfg) {
    cfg_ = cfg;
    syncLastCastSize();
    sweep_.setRange(cfg.attackRadiusMin, cfg.attackRadiusMax);
}

void CombatFsm::enterBuffing(std::chrono::steady_clock::time_point now) {
    state_ = CombatState::Buffing;
    nextStepAt_ = now;
}

int CombatFsm::findDueSlot(std::chrono::steady_clock::time_point now) const {
    // Master gate F9: tắt → không buff.
    if (!cfg_.buffEnabled) return -1;
    for (size_t i = 0; i < cfg_.buffs.size() && i < lastCastAt_.size(); ++i) {
        const auto& b = cfg_.buffs[i];
        if (!b.enabled) continue;
        if ((now - lastCastAt_[i]) >= sec(b.rebuffIntervalSec)) {
            return static_cast<int>(i);
        }
    }
    return -1;
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

    int idx = findDueSlot(now);
    if (idx < 0) { enterArming(now); return; }

    const BuffSlotCfg& slot = cfg_.buffs[idx];
    HWND target = target_;
    WORD vk = slot.key;
    int hold = 30;

    // 1. F key tap now.
    InputCmd keyCmd;
    keyCmd.priority = P4_Buff;
    keyCmd.fireAt = now;
    keyCmd.action = [vk, hold](IInputBackend& b) { b.sendKeyTap(vk, hold); };
    sched_.schedule(std::move(keyCmd));

    // 2. Right-click confirm self-target at safe spot (% client rect, clamped).
    if (slot.rightClickAfter) {
        RECT r{}; GetClientRect(target, &r);
        int w = r.right - r.left;
        int h = r.bottom - r.top;
        double px = std::clamp(cfg_.buffSafeSpotXPct, 0.05, 0.95);
        double py = std::clamp(cfg_.buffSafeSpotYPct, 0.05, 0.95);
        int cx = r.left + static_cast<int>(w * px);
        int cy = r.top  + static_cast<int>(h * py);
        InputCmd clickCmd;
        clickCmd.priority = P4_Buff;
        clickCmd.fireAt = now + ms(slot.rightClickDelayMs);
        clickCmd.action = [cx, cy](IInputBackend& b) { b.sendRightClick(cx, cy); };
        sched_.schedule(std::move(clickCmd));
    }

    // Mark slot này đã cast; tính cooldown từ đây.
    lastCastAt_[idx] = now;

    // 3. Wait full animation + safety gap trước khi xét slot due kế.
    int totalMs = slot.animationMs + slot.postBuffGapMs;
    nextStepAt_ = now + ms(totalMs);
}

void CombatFsm::stepAttacking(const VisionState& v, std::chrono::steady_clock::time_point now) {
    activity_.update(v.hpPct, v.mpPct);

    // Re-buff khi bất kỳ slot enabled nào đã quá rebuffIntervalSec từ lần cast trước.
    // Hủy engagement lock để sau khi buff xong, FSM repick mob ngay (không phải đợi
    // hết phần còn lại của lock window mới shift+right click).
    if (findDueSlot(now) >= 0) {
        engagementUntil_ = now;
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
