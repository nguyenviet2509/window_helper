#pragma once
// Config JSON load/save. Uses nlohmann::json. Missing keys fall back to defaults
// in AppConfig{}. Unknown keys are preserved via `advancedRaw_` round-trip.
#include <string>
#include <nlohmann/json.hpp>
#include "../state/game-state.h"

class ConfigLoader {
public:
    // Returns true if loaded from disk; false means defaults are used (still safe).
    bool load(const std::string& path, AppConfig& out);
    bool save(const std::string& path, const AppConfig& in) const;

    // True nếu config.json load OK nhưng thiếu "vision" section → caller nên save
    // lại để materialize defaults trên disk + log warning.
    bool visionMissing() const { return visionMissing_; }

private:
    nlohmann::json advancedRaw_;     // preserves unknown blocks
    bool visionMissing_ = false;
};
