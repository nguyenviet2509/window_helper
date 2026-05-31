#include "pot-evaluator.h"
#include "pot-refill-scheduler.h"
#include "../core/logger.h"

using ms = std::chrono::milliseconds;

static bool cooledDown(std::chrono::steady_clock::time_point last,
                       std::chrono::steady_clock::time_point now,
                       int cdMs) {
    if (last.time_since_epoch().count() == 0) return true;
    return (now - last) >= ms(cdMs);
}

static int cooldownLeftMs(std::chrono::steady_clock::time_point last,
                          std::chrono::steady_clock::time_point now,
                          int cdMs) {
    if (last.time_since_epoch().count() == 0) return 0;
    auto elapsed = std::chrono::duration_cast<ms>(now - last).count();
    auto left = cdMs - static_cast<int>(elapsed);
    return left > 0 ? left : 0;
}

std::optional<PotAction> PotEvaluator::evalHp(const VisionState& v,
                                              std::chrono::steady_clock::time_point now) {
    if (refill_ && refill_->busy()) return std::nullopt;
    // Cảnh báo khi vision chuyển từ valid → invalid (chặn toàn bộ pot HP).
    if (!v.valid) {
        if (prevValid_) {
            Logger::instance().logf(LogLevel::Warn,
                "[pot.hp] vision invalid — bỏ tick (sẽ không bơm cho tới khi valid lại)");
            prevValid_ = false;
        }
        return std::nullopt;
    }
    if (!prevValid_) {
        Logger::instance().log(LogLevel::Info, "[pot.hp] vision valid trở lại");
        prevValid_ = true;
    }

    if (v.hpPct < cfg_.hpThreshold) ++hpConfirm_;
    else hpConfirm_ = 0;

    int cdLeft = cooldownLeftMs(lastHp_, now, cfg_.cooldownMs);

    // Log throttled mỗi ~1s để chẩn đoán tại sao không fire.
    if (std::chrono::duration_cast<ms>(now - lastHpDiagLog_).count() >= 1000) {
        lastHpDiagLog_ = now;
        Logger::instance().logf(LogLevel::Info,
            "[pot.hp] valid=1 hpPct=%.3f thr=%.3f confirm=%d/%d cdLeftMs=%d",
            v.hpPct, cfg_.hpThreshold, hpConfirm_, cfg_.confirmFrames, cdLeft);
    }

    if (hpConfirm_ >= cfg_.confirmFrames) {
        if (cdLeft == 0) {
            Logger::instance().logf(LogLevel::Info,
                "[pot.hp] FIRE vk=%d holdMs=%d hpPct=%.3f thr=%.3f",
                (int)cfg_.hpKey, cfg_.holdMs, v.hpPct, cfg_.hpThreshold);
            lastHp_ = now;
            hpConfirm_ = 0;
            return PotAction{0, cfg_.hpKey, cfg_.holdMs, "HP-pot"};
        } else {
            Logger::instance().logf(LogLevel::Debug,
                "[pot.hp] confirm đủ nhưng bị cooldown chặn cdLeftMs=%d", cdLeft);
        }
    }
    return std::nullopt;
}

std::optional<PotAction> PotEvaluator::evalMpSp(const VisionState& v,
                                                std::chrono::steady_clock::time_point now) {
    if (refill_ && refill_->busy()) return std::nullopt;
    if (!v.valid) return std::nullopt;

    // MP first.
    if (v.mpPct < cfg_.mpThreshold) ++mpConfirm_;
    else mpConfirm_ = 0;
    if (mpConfirm_ >= cfg_.confirmFrames && cooledDown(lastMp_, now, cfg_.cooldownMs)) {
        lastMp_ = now;
        mpConfirm_ = 0;
        return PotAction{1, cfg_.mpKey, cfg_.holdMs, "MP-pot"};
    }

    // SP next.
    if (v.spPct < cfg_.spThreshold) ++spConfirm_;
    else spConfirm_ = 0;
    if (spConfirm_ >= cfg_.confirmFrames && cooledDown(lastSp_, now, cfg_.cooldownMs)) {
        lastSp_ = now;
        spConfirm_ = 0;
        return PotAction{1, cfg_.spKey, cfg_.holdMs, "SP-pot"};
    }
    return std::nullopt;
}

std::optional<PotAction> PotEvaluator::evalRecall(const VisionState& v,
                                                  std::chrono::steady_clock::time_point now) {
    if (refill_ && refill_->busy()) return std::nullopt;
    if (!v.valid) return std::nullopt;
    if (v.hpPct < cfg_.hpRecallThreshold) {
        if (!hpBelowRecallTracking_) {
            hpBelowRecallTracking_ = true;
            hpBelowRecallSince_ = now;
        } else if ((now - hpBelowRecallSince_) >= ms(cfg_.hpRecallStableMs)
                   && cooledDown(lastRecall_, now, 10000)) {
            lastRecall_ = now;
            hpBelowRecallTracking_ = false;
            return PotAction{2, cfg_.recallKey, cfg_.holdMs, "Recall"};
        }
    } else {
        hpBelowRecallTracking_ = false;
    }
    return std::nullopt;
}
