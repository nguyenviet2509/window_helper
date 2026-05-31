#pragma once
// Shared application state + config struct.
// VisionState is defined in vision/roi.h; re-export aliases here for convenience.
#include <string>
#include <vector>
#include <windows.h>

#include "../vision/roi.h"

struct PotConfig {
    // Pot hotkeys ingame (PT): 1 = HP, 2 = SP, 3 = MP.
    WORD hpKey   = '1';
    WORD mpKey   = '3';
    WORD spKey   = '2';
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
    int engagementLockMs = 5000;          // silence sau shift+right-click; bỏ qua click trong window này
    int engagementLockJitterMs = 500;     // uniform [0, jitter] ms cộng thêm mỗi engagement (anti-pattern)
    bool enableMousePath = true;          // Bezier mouse path trước mỗi click (anti-detect)
    std::vector<BuffSlotCfg> buffs = {
        {true, VK_F2, 600, true},
        {true, VK_F3, 600, true},
        {true, VK_F4, 600, true},
        {true, VK_F5, 600, true},
    };
};

// Inventory pot-refill: tự mở kho (V) → move chuột tới slot → Shift+1/2/3 → đóng kho.
// Chạy theo interval riêng từng slot. Pause toàn bộ combat trong khi refill.
struct PotRefillSlot {
    int intervalSec = 0;   // 0 = disabled. Tọa độ slot trong kho được hardcode trong pot-refill-scheduler.cpp.
};

struct PotRefillConfig {
    bool enabled = false;
    WORD inventoryToggleKey = 'V';
    int inventoryOpenDelayMs  = 400;
    int inventoryCloseDelayMs = 200;
    int mouseMoveDelayMs   = 150;
    int postHotkeyDelayMs  = 200;
    int refillTimeoutMs    = 10000;       // force CLEANUP nếu treo
    double hpCriticalAbortThreshold = 0.30;
    int abortBackoffMs = 30000;
    PotRefillSlot hp;   // Shift+1
    PotRefillSlot sp;   // Shift+2
    PotRefillSlot mp;   // Shift+3
};

enum class BackendKind { SendInput, PostMessage };

struct AppConfig {
    PotConfig pot;
    CombatConfig combat;
    PotRefillConfig refill;
    // Backend mặc định khi khởi tạo input. PT (Xingcode3) hiện từ chối PostMessage
    // → giữ SendInput. Sau khi probe verdict OK với game khác, có thể đặt PostMessage.
    BackendKind defaultBackend = BackendKind::SendInput;
};
