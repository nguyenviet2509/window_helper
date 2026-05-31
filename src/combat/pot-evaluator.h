#pragma once
// PotEvaluator: emits HP / MP / SP / Recall actions with per-resource cooldown
// and N-frame confirm to avoid single-frame vision glitches.
#include <chrono>
#include <optional>
#include "../vision/roi.h"
#include "../state/game-state.h"

struct PotAction {
    int priority;       // P0..P2
    WORD vk;
    int holdMs;
    const char* tag;
};

class PotRefillScheduler;   // fwd decl: skip eval khi refill đang chạy

class PotEvaluator {
public:
    explicit PotEvaluator(const PotConfig& cfg) : cfg_(cfg) {}

    std::optional<PotAction> evalHp(const VisionState& v, std::chrono::steady_clock::time_point now);
    std::optional<PotAction> evalMpSp(const VisionState& v, std::chrono::steady_clock::time_point now);
    std::optional<PotAction> evalRecall(const VisionState& v, std::chrono::steady_clock::time_point now);

    void updateConfig(const PotConfig& cfg) { cfg_ = cfg; }
    void setRefillScheduler(const PotRefillScheduler* r) { refill_ = r; }

private:
    PotConfig cfg_;
    int hpConfirm_ = 0, mpConfirm_ = 0, spConfirm_ = 0;
    using TP = std::chrono::steady_clock::time_point;
    TP lastHp_{}, lastMp_{}, lastSp_{}, lastRecall_{};
    TP hpBelowRecallSince_{};
    bool hpBelowRecallTracking_ = false;
    // Chẩn đoán HP — throttle log mỗi ~1s + nhớ trạng thái valid trước đó.
    TP lastHpDiagLog_{};
    bool prevValid_ = true;
    const PotRefillScheduler* refill_ = nullptr;
};
