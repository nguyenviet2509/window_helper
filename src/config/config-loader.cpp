#include "config-loader.h"
#include <algorithm>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;

static void toJson(json& j, const BuffSlotCfg& b) {
    j = json{
        {"enabled", b.enabled},
        {"key", static_cast<int>(b.key)},
        {"rightClickDelayMs", b.rightClickDelayMs},
        {"animationMs", b.animationMs},
        {"postBuffGapMs", b.postBuffGapMs},
        {"rightClickAfter", b.rightClickAfter},
        {"rebuffIntervalSec", b.rebuffIntervalSec},
    };
}

// legacyCycleSec: nếu JSON cũ có top-level cycleDurationSec → dùng làm default cho slot
// thiếu rebuffIntervalSec. 0 = không có legacy → giữ default struct (300).
static void fromJson(const json& j, BuffSlotCfg& b, int legacyCycleSec) {
    if (j.contains("enabled"))         b.enabled = j["enabled"];
    if (j.contains("key"))             b.key = static_cast<WORD>(j["key"].get<int>());
    if (j.contains("rightClickAfter")) b.rightClickAfter = j["rightClickAfter"];

    // New schema (preferred).
    if (j.contains("animationMs")) {
        b.animationMs = j["animationMs"];
        b.rightClickDelayMs = j.value("rightClickDelayMs", 100);
        b.postBuffGapMs = j.value("postBuffGapMs", 150);
    } else if (j.contains("castDelayMs")) {
        // Migration: legacy castDelayMs -> animationMs, sane defaults for rest.
        b.animationMs = j["castDelayMs"];
        b.rightClickDelayMs = 100;
        b.postBuffGapMs = 150;
    }

    // Per-slot rebuff interval. Migration: nếu thiếu, fallback về legacy global cycle.
    if (j.contains("rebuffIntervalSec")) {
        b.rebuffIntervalSec = j["rebuffIntervalSec"];
    } else if (legacyCycleSec > 0) {
        b.rebuffIntervalSec = legacyCycleSec;
    }
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
        {"buffEnabled", c.buffEnabled},
        {"mainAttackKey", (int)c.mainAttackKey},
        {"repickMinDwellMs", c.repickMinDwellMs},
        {"repickMaxDwellMs", c.repickMaxDwellMs},
        {"deathConfirmMs", c.deathConfirmMs},
        {"mpDropEpsilon", c.mpDropEpsilon},
        {"attackRadiusMin", c.attackRadiusMin},
        {"attackRadiusMax", c.attackRadiusMax},
        {"waitMpGate", c.waitMpGate},
        {"waitMpGateThreshold", c.waitMpGateThreshold},
        {"attackCooldownMs", c.attackCooldownMs},
        {"engagementLockMs", c.engagementLockMs},
        {"engagementLockJitterMs", c.engagementLockJitterMs},
        {"enableMousePath", c.enableMousePath},
        {"buffSafeSpotXPct", c.buffSafeSpotXPct},
        {"buffSafeSpotYPct", c.buffSafeSpotYPct},
        {"spamSkillEnabled", c.spamSkillEnabled},
        {"spamSkillIntervalMs", c.spamSkillIntervalMs},
        {"buffs", buffs},
    };
}

static void fromJson(const json& j, CombatConfig& c) {
    if (j.contains("enabled")) c.enabled = j["enabled"];
    if (j.contains("buffEnabled")) c.buffEnabled = j["buffEnabled"];
    if (j.contains("mainAttackKey")) c.mainAttackKey = (WORD)j["mainAttackKey"].get<int>();
    // Legacy global cycle: dùng làm fallback cho slot thiếu rebuffIntervalSec.
    int legacyCycleSec = j.value("cycleDurationSec", 0);
    if (j.contains("repickMinDwellMs")) c.repickMinDwellMs = j["repickMinDwellMs"];
    if (j.contains("repickMaxDwellMs")) c.repickMaxDwellMs = j["repickMaxDwellMs"];
    if (j.contains("deathConfirmMs")) c.deathConfirmMs = j["deathConfirmMs"];
    if (j.contains("mpDropEpsilon")) c.mpDropEpsilon = j["mpDropEpsilon"];
    if (j.contains("attackRadiusMin")) c.attackRadiusMin = j["attackRadiusMin"];
    if (j.contains("attackRadiusMax")) c.attackRadiusMax = j["attackRadiusMax"];
    if (j.contains("waitMpGate")) c.waitMpGate = j["waitMpGate"];
    if (j.contains("waitMpGateThreshold")) c.waitMpGateThreshold = j["waitMpGateThreshold"];
    if (j.contains("attackCooldownMs")) c.attackCooldownMs = j["attackCooldownMs"];
    if (j.contains("engagementLockMs")) c.engagementLockMs = j["engagementLockMs"];
    if (j.contains("engagementLockJitterMs")) c.engagementLockJitterMs = j["engagementLockJitterMs"];
    if (j.contains("enableMousePath")) c.enableMousePath = j["enableMousePath"];
    if (j.contains("buffSafeSpotXPct")) c.buffSafeSpotXPct = j["buffSafeSpotXPct"];
    if (j.contains("buffSafeSpotYPct")) c.buffSafeSpotYPct = j["buffSafeSpotYPct"];
    if (j.contains("spamSkillEnabled")) c.spamSkillEnabled = j["spamSkillEnabled"];
    if (j.contains("spamSkillIntervalMs"))
        c.spamSkillIntervalMs = std::clamp(j["spamSkillIntervalMs"].get<int>(), 100, 10000);
    if (j.contains("buffs") && j["buffs"].is_array()) {
        // Backfill: nếu config cũ có ít slot hơn default, giữ default cho slot còn thiếu.
        // Tránh wipe slot mới (vd F6) khi user load config.json cũ chỉ có 4 slot.
        CombatConfig def;
        std::vector<BuffSlotCfg> loaded;
        for (const auto& bj : j["buffs"]) {
            BuffSlotCfg b;
            fromJson(bj, b, legacyCycleSec);
            loaded.push_back(b);
        }
        if (loaded.size() < def.buffs.size()) {
            for (size_t i = loaded.size(); i < def.buffs.size(); ++i)
                loaded.push_back(def.buffs[i]);
        }
        c.buffs = std::move(loaded);
    }
}

static void toJson(json& j, const PotRefillSlot& s) {
    j = json{ {"intervalSec", s.intervalSec} };
}

static void fromJson(const json& j, PotRefillSlot& s) {
    if (j.contains("intervalSec")) s.intervalSec = j["intervalSec"];
}

static void toJson(json& j, const PotRefillConfig& r) {
    json jhp, jsp, jmp;
    toJson(jhp, r.hp); toJson(jsp, r.sp); toJson(jmp, r.mp);
    j = json{
        {"enabled", r.enabled},
        {"inventoryToggleKey", (int)r.inventoryToggleKey},
        {"inventoryOpenDelayMs", r.inventoryOpenDelayMs},
        {"inventoryCloseDelayMs", r.inventoryCloseDelayMs},
        {"mouseMoveDelayMs", r.mouseMoveDelayMs},
        {"postHotkeyDelayMs", r.postHotkeyDelayMs},
        {"refillTimeoutMs", r.refillTimeoutMs},
        {"hpCriticalAbortThreshold", r.hpCriticalAbortThreshold},
        {"abortBackoffMs", r.abortBackoffMs},
        {"hp", jhp}, {"sp", jsp}, {"mp", jmp},
    };
}

static void fromJson(const json& j, PotRefillConfig& r) {
    if (j.contains("enabled")) r.enabled = j["enabled"];
    if (j.contains("inventoryToggleKey")) r.inventoryToggleKey = (WORD)j["inventoryToggleKey"].get<int>();
    if (j.contains("inventoryOpenDelayMs")) r.inventoryOpenDelayMs = j["inventoryOpenDelayMs"];
    if (j.contains("inventoryCloseDelayMs")) r.inventoryCloseDelayMs = j["inventoryCloseDelayMs"];
    if (j.contains("mouseMoveDelayMs")) r.mouseMoveDelayMs = j["mouseMoveDelayMs"];
    if (j.contains("postHotkeyDelayMs")) r.postHotkeyDelayMs = j["postHotkeyDelayMs"];
    if (j.contains("refillTimeoutMs")) r.refillTimeoutMs = j["refillTimeoutMs"];
    if (j.contains("hpCriticalAbortThreshold")) r.hpCriticalAbortThreshold = j["hpCriticalAbortThreshold"];
    if (j.contains("abortBackoffMs")) r.abortBackoffMs = j["abortBackoffMs"];
    if (j.contains("hp")) fromJson(j["hp"], r.hp);
    if (j.contains("sp")) fromJson(j["sp"], r.sp);
    if (j.contains("mp")) fromJson(j["mp"], r.mp);
}

static void toJson(json& j, const VisionBarCfg& b) {
    json hues = json::array();
    for (const auto& h : b.hues) hues.push_back(json{ {"lo", h.hMin}, {"hi", h.hMax} });
    j = json{
        {"region", { {"x", b.region.x}, {"y", b.region.y},
                     {"w", b.region.w}, {"h", b.region.h},
                     {"shape", b.region.shape}, {"radius", b.region.radius} }},
        {"hues", hues},
    };
}

static void fromJson(const json& j, VisionBarCfg& b) {
    if (j.contains("region")) {
        const auto& r = j["region"];
        if (r.contains("x")) b.region.x = r["x"];
        if (r.contains("y")) b.region.y = r["y"];
        if (r.contains("w")) b.region.w = r["w"];
        if (r.contains("h")) b.region.h = r["h"];
        if (r.contains("shape")) b.region.shape = r["shape"].get<std::string>();
        if (r.contains("radius")) b.region.radius = r["radius"];
    }
    if (j.contains("hues") && j["hues"].is_array()) {
        b.hues.clear();
        for (const auto& hj : j["hues"]) {
            HueRange h{};
            if (hj.contains("lo")) h.hMin = hj["lo"];
            if (hj.contains("hi")) h.hMax = hj["hi"];
            b.hues.push_back(h);
        }
    }
}

static void toJson(json& j, const VisionConfig& v) {
    json jhp, jsp, jmp;
    toJson(jhp, v.hp); toJson(jsp, v.sp); toJson(jmp, v.mp);
    j = json{
        {"frameWidth", v.frameWidth},
        {"frameHeight", v.frameHeight},
        {"hp", jhp}, {"sp", jsp}, {"mp", jmp},
    };
}

static void fromJson(const json& j, VisionConfig& v) {
    if (j.contains("frameWidth"))  v.frameWidth  = j["frameWidth"];
    if (j.contains("frameHeight")) v.frameHeight = j["frameHeight"];
    if (j.contains("hp")) fromJson(j["hp"], v.hp);
    if (j.contains("sp")) fromJson(j["sp"], v.sp);
    if (j.contains("mp")) fromJson(j["mp"], v.mp);
}

static const char* backendToString(BackendKind k) {
    return k == BackendKind::PostMessage ? "PostMessage" : "SendInput";
}
static BackendKind backendFromString(const std::string& s) {
    return s == "PostMessage" ? BackendKind::PostMessage : BackendKind::SendInput;
}

bool ConfigLoader::load(const std::string& path, AppConfig& out) {
    std::ifstream f(path);
    if (!f.good()) return false;
    json j;
    try { f >> j; } catch (...) { return false; }
    if (j.contains("pot"))      fromJson(j["pot"], out.pot);
    if (j.contains("combat"))   fromJson(j["combat"], out.combat);
    if (j.contains("refill"))   fromJson(j["refill"], out.refill);
    // Migration: nếu thiếu "vision" → giữ defaults (đã là giá trị hardcoded cũ),
    // visionMissing_ = true để caller biết save lại file.
    if (j.contains("vision"))   fromJson(j["vision"], out.vision);
    else                        visionMissing_ = true;
    if (j.contains("defaultBackend") && j["defaultBackend"].is_string())
        out.defaultBackend = backendFromString(j["defaultBackend"].get<std::string>());
    if (j.contains("advanced")) advancedRaw_ = j["advanced"];
    return true;
}

bool ConfigLoader::save(const std::string& path, const AppConfig& in) const {
    json j;
    toJson(j["pot"], in.pot);
    toJson(j["combat"], in.combat);
    toJson(j["refill"], in.refill);
    toJson(j["vision"], in.vision);
    j["defaultBackend"] = backendToString(in.defaultBackend);
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
