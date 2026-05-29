#pragma once
// Capture health FSM: HEALTHY -> DEGRADED -> UNSAFE -> STOPPED.
// Drives OutputGate so input is blocked when frames are stale, frozen, or capture failed.
#include <atomic>
#include <chrono>

enum class CaptureHealth { Healthy, Degraded, Unsafe, Stopped };

struct CaptureHealthThresholds {
    int degradedAfterMs = 200;     // no fresh frame -> degraded
    int unsafeAfterMs   = 600;     // no fresh frame -> unsafe (block input)
    int stoppedAfterMs  = 3000;    // hard stop
    int frozenFramesUnsafe = 30;   // identical-frame streak -> unsafe
};

class CaptureHealthFsm {
public:
    void notifyFrameArrived(uint64_t frameHash);
    void notifyCaptureError();
    void tick();                                 // call at ~10Hz from gate/main loop
    CaptureHealth state() const { return state_.load(); }

    void setThresholds(CaptureHealthThresholds t) { thr_ = t; }

private:
    std::atomic<CaptureHealth> state_{ CaptureHealth::Healthy };
    std::chrono::steady_clock::time_point lastFrameAt_{ std::chrono::steady_clock::now() };
    uint64_t lastHash_ = 0;
    int frozenStreak_ = 0;
    bool errorLatched_ = false;
    CaptureHealthThresholds thr_{};
};
