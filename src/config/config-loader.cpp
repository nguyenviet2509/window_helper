#include "config-loader.h"
#include <fstream>
#include <filesystem>

using json = nlohmann::json;

static void toJson(json& j, const BuffSlotCfg& b) {
    j = json{
        {"enabled", b.enabled},
        {"key", static_cast<int>(b.key)},
        {"castDelayMs", b.castDelayMs},
        {"rightClickAfter", b.rightClickAfter},
    };
}

static void fromJson(const json& j, BuffSlotCfg& b) {
    if (j.contains("enabled"))         b.enabled = j["enabled"];
    if (j.contains("key"))             b.key = static_cast<WORD>(j["key"].get<int>());
    if (j.contains("castDelayMs"))     b.castDelayMs = j["castDelayMs"];
    if (j.contains("rightClickAfter")) b.rightClickAfter = j["rightClickAfter"];
}

static void toJson(json& j, const PotConfig& p) {
    j = json{
        {"hpKey", (int)p.hpKey}, {"mpKey", (int)p.mpKey},
        {"spKey", (int)p.spKey}, {"recallKey", (int)p.recallKey},
        {"hpThreshold", p.hpThreshold}, {"mpThreshold", p.mpThreshold},
        {"spThreshold", p.spThreshold}, {"hpRecallThreshold", p.hpRecallThreshold},
        {"hpRecallStableMs", p.hpRecallStableMs},
        {"cooldownMs", p.cooldownMs}, {"confirmFrames", p.confirmFrames},
        {"holdMs", p.holdMs},
    };
}

static void fromJson(const json& j, PotConfig& p) {
    if (j.contains("hpKey")) p.hpKey = (WORD)j["hpKey"].get<int>();
    if (j.contains("mpKey")) p.mpKey = (WORD)j["mpKey"].get<int>();
    if (j.contains("spKey")) p.spKey = (WORD)j["spKey"].get<int>();
    if (j.contains("recallKey")) p.recallKey = (WORD)j["recallKey"].get<int>();
    if (j.contains("hpThreshold")) p.hpThreshold = j["hpThreshold"];
    if (j.contains("mpThreshold")) p.mpThreshold = j["mpThreshold"];
    if (j.contains("spThreshold")) p.spThreshold = j["spThreshold"];
    if (j.contains("hpRecallThreshold")) p.hpRecallThreshold = j["hpRecallThreshold"];
    if (j.contains("hpRecallStableMs")) p.hpRecallStableMs = j["hpRecallStableMs"];
    if (j.contains("cooldownMs")) p.cooldownMs = j["cooldownMs"];
    if (j.contains("confirmFrames")) p.confirmFrames = j["confirmFrames"];
    if (j.contains("holdMs")) p.holdMs = j["holdMs"];
}

static void toJson(json& j, const CombatConfig& c) {
    json buffs = json::array();
    for (const auto& b : c.buffs) { json bj; toJson(bj, b); buffs.push_back(bj); }
    j = json{
        {"enabled", c.enabled},
        {"mainAttackKey", (int)c.mainAttackKey},
        {"cycleDurationSec", c.cycleDurationSec},
        {"repickMinDwellMs", c.repickMinDwellMs},
        {"repickMaxDwellMs", c.repickMaxDwellMs},
        {"attackRadiusMin", c.attackRadiusMin},
        {"attackRadiusMax", c.attackRadiusMax},
        {"waitMpGate", c.waitMpGate},
        {"waitMpGateThreshold", c.waitMpGateThreshold},
        {"attackCooldownMs", c.attackCooldownMs},
        {"buffs", buffs},
    };
}

static void fromJson(const json& j, CombatConfig& c) {
    if (j.contains("enabled")) c.enabled = j["enabled"];
    if (j.contains("mainAttackKey")) c.mainAttackKey = (WORD)j["mainAttackKey"].get<int>();
    if (j.contains("cycleDurationSec")) c.cycleDurationSec = j["cycleDurationSec"];
    if (j.contains("repickMinDwellMs")) c.repickMinDwellMs = j["repickMinDwellMs"];
    if (j.contains("repickMaxDwellMs")) c.repickMaxDwellMs = j["repickMaxDwellMs"];
    if (j.contains("attackRadiusMin")) c.attackRadiusMin = j["attackRadiusMin"];
    if (j.contains("attackRadiusMax")) c.attackRadiusMax = j["attackRadiusMax"];
    if (j.contains("waitMpGate")) c.waitMpGate = j["waitMpGate"];
    if (j.contains("waitMpGateThreshold")) c.waitMpGateThreshold = j["waitMpGateThreshold"];
    if (j.contains("attackCooldownMs")) c.attackCooldownMs = j["attackCooldownMs"];
    if (j.contains("buffs") && j["buffs"].is_array()) {
        c.buffs.clear();
        for (const auto& bj : j["buffs"]) {
            BuffSlotCfg b;
            fromJson(bj, b);
            c.buffs.push_back(b);
        }
    }
}

bool ConfigLoader::load(const std::string& path, AppConfig& out) {
    std::ifstream f(path);
    if (!f.good()) return false;
    json j;
    try { f >> j; } catch (...) { return false; }
    if (j.contains("pot"))      fromJson(j["pot"], out.pot);
    if (j.contains("combat"))   fromJson(j["combat"], out.combat);
    if (j.contains("advanced")) advancedRaw_ = j["advanced"];
    return true;
}

bool ConfigLoader::save(const std::string& path, const AppConfig& in) const {
    json j;
    toJson(j["pot"], in.pot);
    toJson(j["combat"], in.combat);
    if (!advancedRaw_.is_null()) j["advanced"] = advancedRaw_;

    // Atomic write: temp -> rename.
    std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp);
        if (!f.good()) return false;
        f << j.dump(2);
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    return !ec;
}
