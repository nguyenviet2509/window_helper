#include "pot-evaluator.h"

using ms = std::chrono::milliseconds;

static bool cooledDown(std::chrono::steady_clock::time_point last,
                       std::chrono::steady_clock::time_point now,
                       int cdMs) {
    if (last.time_since_epoch().count() == 0) return true;
    return (now - last) >= ms(cdMs);
}

std::optional<PotAction> PotEvaluator::evalHp(const VisionState& v,
                                              std::chrono::steady_clock::time_point now) {
    if (!v.valid) return std::nullopt;
    if (v.hpPct < cfg_.hpThreshold) ++hpConfirm_;
    else hpConfirm_ = 0;

    if (hpConfirm_ >= cfg_.confirmFrames && cooledDown(lastHp_, now, cfg_.cooldownMs)) {
        lastHp_ = now;
        hpConfirm_ = 0;
        return PotAction{0, cfg_.hpKey, cfg_.holdMs, "HP-pot"};
    }
    return std::nullopt;
}

std::optional<PotAction> PotEvaluator::evalMpSp(const VisionState& v,
                                                std::chrono::steady_clock::time_point now) {
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
