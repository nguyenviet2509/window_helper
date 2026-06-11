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
    int rightClickDelayMs = 100;  // F press -> right-click confirm self-target
    int animationMs = 800;        // full cast animation of skill
    int postBuffGapMs = 150;      // safety gap before next slot
    bool rightClickAfter = true;
    int rebuffIntervalSec = 300;  // re-cast slot này sau N giây (per-slot duration)
};

struct CombatConfig {
    bool enabled = false;
    bool buffEnabled = false;     // Master gate cho toàn bộ buff (F9). False = bot không cast buff.
    WORD mainAttackKey = VK_F1;   // arming key
    int repickMinDwellMs = 6000;  // hold attack at least this long before repicking
    int repickMaxDwellMs = 22000; // forced repick after this long (escape hatch cho mob bug/immortal)
    int deathConfirmMs = 2500;    // no-MP-drop window (ms) để declare mob dead
    double mpDropEpsilon = 0.005; // 0.5% threshold cho "MP went down" tick
    int attackRadiusMin = 50;
    int attackRadiusMax = 140;
    bool waitMpGate = true;
    double waitMpGateThreshold = 0.15;
    int attackCooldownMs = 350;
    int engagementLockMs = 2500;          // silence sau shift+right-click; bỏ qua click trong window này
    int engagementLockJitterMs = 800;     // uniform [0, jitter] ms cộng thêm mỗi engagement (anti-pattern)
    bool enableMousePath = true;          // Bezier mouse path trước mỗi click (anti-detect)
    // Safe spot cho right-click confirm self-target (% của client rect).
    // Pick chỗ KHÔNG có mob và KHÔNG có UI. Nếu click trúng mob -> game biến skill thành đánh thường.
    double buffSafeSpotXPct = 0.5;
    double buffSafeSpotYPct = 0.5;
    // Spam skill mode: bỏ qua mob targeting, chỉ right-click lặp tại safe spot.
    // Pot/refill/buff vẫn chạy bình thường. Sau buff cycle → click kế tiếp = F1 tap + right-click.
    // Humanizer: random 5-10s fire 1 left-click jitter ±10px quanh safe spot để fake người thật.
    bool spamSkillEnabled = false;
    int spamSkillIntervalMs = 1000;   // clamp [100, 10000]. Default ≈ CD skill phổ biến PT.
    std::vector<BuffSlotCfg> buffs = {
        {true, VK_F2, 100, 800, 150, true, 300},
        {true, VK_F3, 100, 800, 150, true, 300},
        {true, VK_F4, 100, 800, 150, true, 300},
        {true, VK_F5, 100, 800, 150, true, 300},
        {true, VK_F6, 100, 800, 150, true, 300},
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

// Per-bar vision ROI + hue ranges. Mặc định = giá trị hardcoded cũ ở main.cpp
// (PT 1010x789). Đổi qua config.json hoặc preset để chơi server khác (UI skin /
// resolution khác). Schema migration: nếu config.json thiếu section "vision" →
// fill defaults này, log warning, save lại.
struct VisionBarCfg {
    BarRegion region;
    std::vector<HueRange> hues;
};

struct VisionConfig {
    // Expected WGC frame size. Warning nếu mismatch (ROI có thể sai chỗ).
    int frameWidth  = 1010;
    int frameHeight = 789;
    // Defaults = hardcoded PT values (main.cpp pre-refactor).
    VisionBarCfg hp = { {403, 656, 22, 121, "rect", 0}, { {0,10}, {170,180} } };
    VisionBarCfg sp = { {383, 675, 11, 102, "rect", 0}, { {40, 80} } };
    VisionBarCfg mp = { {586, 655, 21, 121, "rect", 0}, { {100,130} } };
};

struct AppConfig {
    PotConfig pot;
    CombatConfig combat;
    PotRefillConfig refill;
    VisionConfig vision;
    // Backend mặc định khi khởi tạo input. PT (Xingcode3) hiện từ chối PostMessage
    // → giữ SendInput. Sau khi probe verdict OK với game khác, có thể đặt PostMessage.
    BackendKind defaultBackend = BackendKind::SendInput;
};
