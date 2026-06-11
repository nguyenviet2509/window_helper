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
    if (cfg_.spamSkillEnabled) {
        enterSpamming(now);
        return;
    }
    enterArming(now);
}

void CombatFsm::updateConfig(const CombatConfig& cfg) {
    bool wasSpam = cfg_.spamSkillEnabled;
    cfg_ = cfg;
    syncLastCastSize();
    sweep_.setRange(cfg.attackRadiusMin, cfg.attackRadiusMax);
    // Spam toggle on giữa session khi đang chạy: chuyển sang Spamming + re-cache spot.
    if (enabled_ && cfg_.spamSkillEnabled && !wasSpam) {
        enterSpamming(std::chrono::steady_clock::now());
    } else if (enabled_ && cfg_.spamSkillEnabled) {
        // Pct safe spot có thể đổi — re-cache.
        cacheSpamSpot();
    } else if (enabled_ && wasSpam && !cfg_.spamSkillEnabled) {
        // Tắt spam giữa session: quay về Arming → Attacking.
        enterArming(std::chrono::steady_clock::now());
    }
}

void CombatFsm::cacheSpamSpot() {
    RECT r{};
    if (target_) GetClientRect(target_, &r);
    int w = r.right - r.left;
    int h = r.bottom - r.top;
    double px = std::clamp(cfg_.buffSafeSpotXPct, 0.05, 0.95);
    double py = std::clamp(cfg_.buffSafeSpotYPct, 0.05, 0.95);
    spamX_ = r.left + static_cast<int>(w * px);
    spamY_ = r.top  + static_cast<int>(h * py);
}

int CombatFsm::rollHumanIntervalMs() const {
    return 5000 + (std::rand() % 5001);   // [5000, 10000]
}

int CombatFsm::jitter10() const {
    return (std::rand() % 21) - 10;       // [-10, +10]
}

void CombatFsm::enterSpamming(std::chrono::steady_clock::time_point now) {
    cacheSpamSpot();
    int sx = spamX_, sy = spamY_;
    InputCmd m;
    m.priority = P3_Combat;
    m.fireAt = now;
    m.action = [sx, sy](IInputBackend& b) { b.sendMouseMove(sx, sy); };
    sched_.schedule(std::move(m));
    pendingF1AfterBuff_ = true;
    lastSpamAt_ = now;
    nextHumanClickAt_ = now + ms(rollHumanIntervalMs());
    lastHumanClickAt_ = now;
    state_ = CombatState::Spamming;
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
    // Propagate hidden tunables từ config xuống monitor mỗi lần re-enter.
    activity_.deathConfirmMs = cfg_.deathConfirmMs;
    activity_.mpDropEpsilon = cfg_.mpDropEpsilon;
    activity_.reset(now);
    lastPickAt_ = now;
    lastAttackAt_ = now;
}

void CombatFsm::stepBuffing(std::chrono::steady_clock::time_point now) {
    if (now < nextStepAt_) return;

    int idx = findDueSlot(now);
    if (idx < 0) {
        // Buff cycle xong: nếu spam mode → về Spamming với F1 priming, else về Arming.
        if (cfg_.spamSkillEnabled) {
            pendingF1AfterBuff_ = true;
            lastSpamAt_ = now;          // reset cadence để click F1+rc fire sớm
            state_ = CombatState::Spamming;
            return;
        }
        enterArming(now); return;
    }

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
    activity_.update(v.mpPct, now);

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
    bool mobDead = minDwellOk && activity_.mobLikelyDead(now);
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
    activity_.reset(now);

    int jitter = cfg_.engagementLockJitterMs > 0
        ? (std::rand() % (cfg_.engagementLockJitterMs + 1))
        : 0;
    engagementUntil_ = now + ms(cfg_.engagementLockMs + jitter);
}

void CombatFsm::stepSpamming(std::chrono::steady_clock::time_point now) {
    // Buff due → bay vào Buffing; pendingF1AfterBuff_ set lại khi buff xong (xem stepBuffing).
    if (findDueSlot(now) >= 0) {
        enterBuffing(now);
        return;
    }

    // Human-like left-click jitter — fire mỗi 5-10s tại safe spot ± 10px.
    if (now >= nextHumanClickAt_) {
        int jx = spamX_ + jitter10();
        int jy = spamY_ + jitter10();
        // 1. Left-click tại jitter pos.
        InputCmd c;
        c.priority = P3_Combat;
        c.fireAt = now;
        c.action = [jx, jy](IInputBackend& b) { b.sendLeftClick(jx, jy); };
        sched_.schedule(std::move(c));
        // 2. Quay cursor về safe spot sau ~100ms để right-click kế tiếp đúng vị trí.
        int sx = spamX_, sy = spamY_;
        InputCmd m;
        m.priority = P3_Combat;
        m.fireAt = now + ms(100);
        m.action = [sx, sy](IInputBackend& b) { b.sendMouseMove(sx, sy); };
        sched_.schedule(std::move(m));
        lastHumanClickAt_ = now;
        nextHumanClickAt_ = now + ms(rollHumanIntervalMs());
        return;   // skip right-click tick này
    }

    // Collision guard: vừa left-click <500ms → delay right-click.
    if ((now - lastHumanClickAt_) < ms(500)) return;

    // Interval check.
    int interval = std::clamp(cfg_.spamSkillIntervalMs, 100, 10000);
    if ((now - lastSpamAt_) < ms(interval)) return;

    // Fire skill.
    int cx = spamX_, cy = spamY_;
    if (pendingF1AfterBuff_) {
        WORD vk = cfg_.mainAttackKey;
        InputCmd k;
        k.priority = P3_Combat;
        k.fireAt = now;
        k.action = [vk](IInputBackend& b) { b.sendKeyTap(vk, 30); };
        sched_.schedule(std::move(k));

        InputCmd r;
        r.priority = P3_Combat;
        r.fireAt = now + ms(100);
        r.action = [cx, cy](IInputBackend& b) { b.sendRightClick(cx, cy); };
        sched_.schedule(std::move(r));
        pendingF1AfterBuff_ = false;
    } else {
        InputCmd r;
        r.priority = P3_Combat;
        r.fireAt = now;
        r.action = [cx, cy](IInputBackend& b) { b.sendRightClick(cx, cy); };
        sched_.schedule(std::move(r));
    }
    lastSpamAt_ = now;
}

void CombatFsm::tick(const VisionState& v, std::chrono::steady_clock::time_point now) {
    if (!enabled_) return;
    if (refill_ && refill_->busy()) return;   // pause combat khi refill đang chạy

    switch (state_) {
    case CombatState::Idle:
        // Spam mode: vào Spamming trực tiếp (không buff trước).
        // Non-spam mode: vào Buffing để chạy chu kỳ buff trước.
        if (cfg_.spamSkillEnabled) enterSpamming(now);
        else                       enterBuffing(now);
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
    case CombatState::Spamming:
        stepSpamming(now);
        break;
    }
}
