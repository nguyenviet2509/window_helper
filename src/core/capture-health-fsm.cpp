#include "capture-health-fsm.h"

using clk = std::chrono::steady_clock;

void CaptureHealthFsm::notifyFrameArrived(uint64_t hash) {
    lastFrameAt_ = clk::now();
    if (hash == lastHash_) {
        ++frozenStreak_;
    } else {
        frozenStreak_ = 0;
        lastHash_ = hash;
    }
    errorLatched_ = false;
}

void CaptureHealthFsm::notifyCaptureError() {
    errorLatched_ = true;
}

void CaptureHealthFsm::tick() {
    auto now = clk::now();
    int ageMs = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameAt_).count());

    CaptureHealth next = CaptureHealth::Healthy;
    if (errorLatched_ || ageMs >= thr_.stoppedAfterMs) {
        next = CaptureHealth::Stopped;
    } else if (ageMs >= thr_.unsafeAfterMs || frozenStreak_ >= thr_.frozenFramesUnsafe) {
        next = CaptureHealth::Unsafe;
    } else if (ageMs >= thr_.degradedAfterMs) {
        next = CaptureHealth::Degraded;
    }
    state_.store(next);
}
