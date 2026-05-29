#pragma once
// Walks through the configured buff list, skipping disabled slots.
// Wraps to start after delivering the last enabled buff.
#include <optional>
#include "../state/game-state.h"

class BuffSequencer {
public:
    explicit BuffSequencer(const std::vector<BuffSlotCfg>& slots) : slots_(slots) {}

    void reset() { idx_ = 0; }

    std::optional<BuffSlotCfg> nextBuff() {
        for (size_t tried = 0; tried < slots_.size(); ++tried) {
            size_t i = (idx_ + tried) % slots_.size();
            if (slots_[i].enabled) {
                idx_ = (i + 1) % slots_.size();
                return slots_[i];
            }
        }
        return std::nullopt;
    }

    bool empty() const {
        for (const auto& s : slots_) if (s.enabled) return false;
        return true;
    }

    void updateSlots(const std::vector<BuffSlotCfg>& slots) {
        slots_ = slots;
        idx_ = 0;
    }

private:
    std::vector<BuffSlotCfg> slots_;
    size_t idx_ = 0;
};
