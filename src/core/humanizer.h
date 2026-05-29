#pragma once
// Humanizer: jitter timing, miss-click pos, break/session windows.
// Skips priority >= P0/P1/P2 (kSkipPriorityUpTo). Applies only to combat/buff.
#include <chrono>
#include <random>
#include <mutex>

struct HumanizerConfig {
    double jitterSigmaMs = 45.0;
    int breakEveryMinSec = 25 * 60;
    int breakEveryMaxSec = 50 * 60;
    int breakDurMinSec = 3;
    int breakDurMaxSec = 8;
    int sessionRuntimeMinSec = 40 * 60;
    int sessionRuntimeMaxSec = 110 * 60;
    int sessionPauseMinSec = 6 * 60;
    int sessionPauseMaxSec = 14 * 60;
    double missClickProb = 0.02;
    int missClickOffsetPx = 5;
    int skipPriorityUpTo = 2;     // priorities <= 2 (HP/MP/SP/Recall) bypass jitter+breaks
};

class Humanizer {
public:
    explicit Humanizer(HumanizerConfig cfg = {});

    // Returns ms delay to add before firing an action of the given priority.
    std::chrono::milliseconds gaussianJitter(int priority);
    bool shouldMissClick(int priority);
    void jitterPos(int& x, int& y, int priority);

    // Returns true if currently inside a short break or long session-pause window.
    bool inBreakWindow();
    bool inSessionPause();

    void notifyActionFired(int priority);

private:
    HumanizerConfig cfg_;
    std::mt19937 rng_;
    std::mutex mu_;

    std::chrono::steady_clock::time_point sessionStart_;
    std::chrono::steady_clock::time_point nextBreakAt_;
    std::chrono::steady_clock::time_point breakUntil_;
    std::chrono::steady_clock::time_point sessionPauseUntil_;
    std::chrono::steady_clock::time_point nextSessionEndAt_;

    int rndInt(int lo, int hi);
    void scheduleNextBreak();
    void scheduleNextSessionEnd();
};
