#pragma once
// Shared application state + config struct.
// VisionState is defined in vision/roi.h; re-export aliases here for convenience.
#include <string>
#include <vector>
#include <windows.h>

#include "../vision/roi.h"

struct PotConfig {
    WORD hpKey   = VK_F1;    // pot key (Phase 4 will override from config.json)
    WORD mpKey   = VK_F2;
    WORD spKey   = VK_F3;
    WORD recallKey = VK_F12;
    double hpThreshold = 0.55;     // trigger pot below this fill ratio
    double mpThreshold = 0.40;
    double spThreshold = 0.40;
    double hpRecallThreshold = 0.18;
    int hpRecallStableMs = 3000;   // HP below recall threshold for N ms -> recall
    int cooldownMs = 600;          // per-pot cooldown
    int confirmFrames = 2;         // consecutive sub-threshold frames before fire
    int holdMs = 30;
};

struct BuffSlotCfg {
    bool enabled = true;
    WORD key = VK_F2;
    int castDelayMs = 500;
    bool rightClickAfter = true;
};

struct CombatConfig {
    bool enabled = false;
    WORD mainAttackKey = VK_F1;   // arming key
    int cycleDurationSec = 300;   // re-buff after this many seconds
    int repickMinDwellMs = 2000;  // hold attack at least this long before repicking
    int repickMaxDwellMs = 15000; // forced repick after this long
    int attackRadiusMin = 60;
    int attackRadiusMax = 180;
    bool waitMpGate = true;
    double waitMpGateThreshold = 0.15;
    int attackCooldownMs = 350;
    std::vector<BuffSlotCfg> buffs = {
        {true, VK_F2, 600, true},
        {true, VK_F3, 600, true},
        {true, VK_F4, 600, true},
        {true, VK_F5, 600, true},
    };
};

struct AppConfig {
    PotConfig pot;
    CombatConfig combat;
};
