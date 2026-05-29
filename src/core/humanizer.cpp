#include "humanizer.h"
#include <algorithm>
#include <cmath>

using clock_t_ = std::chrono::steady_clock;

Humanizer::Humanizer(HumanizerConfig cfg)
    : cfg_(cfg),
      rng_(std::random_device{}()),
      sessionStart_(clock_t_::now()) {
    scheduleNextBreak();
    scheduleNextSessionEnd();
}

int Humanizer::rndInt(int lo, int hi) {
    if (hi <= lo) return lo;
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng_);
}

void Humanizer::scheduleNextBreak() {
    int s = rndInt(cfg_.breakEveryMinSec, cfg_.breakEveryMaxSec);
    nextBreakAt_ = clock_t_::now() + std::chrono::seconds(s);
}

void Humanizer::scheduleNextSessionEnd() {
    int s = rndInt(cfg_.sessionRuntimeMinSec, cfg_.sessionRuntimeMaxSec);
    nextSessionEndAt_ = clock_t_::now() + std::chrono::seconds(s);
}

std::chrono::milliseconds Humanizer::gaussianJitter(int priority) {
    if (priority <= cfg_.skipPriorityUpTo) return std::chrono::milliseconds(0);
    std::lock_guard<std::mutex> g(mu_);
    std::normal_distribution<double> nd(0.0, cfg_.jitterSigmaMs);
    double ms = std::abs(nd(rng_));
    if (ms > 4 * cfg_.jitterSigmaMs) ms = 4 * cfg_.jitterSigmaMs;
    return std::chrono::milliseconds(static_cast<int>(ms));
}

bool Humanizer::shouldMissClick(int priority) {
    if (priority <= cfg_.skipPriorityUpTo) return false;
    std::lock_guard<std::mutex> g(mu_);
    std::uniform_real_distribution<double> u(0.0, 1.0);
    return u(rng_) < cfg_.missClickProb;
}

void Humanizer::jitterPos(int& x, int& y, int priority) {
    if (priority <= cfg_.skipPriorityUpTo) return;
    std::lock_guard<std::mutex> g(mu_);
    int off = cfg_.missClickOffsetPx;
    std::uniform_int_distribution<int> d(-off, off);
    x += d(rng_);
    y += d(rng_);
}

bool Humanizer::inBreakWindow() {
    std::lock_guard<std::mutex> g(mu_);
    auto now = clock_t_::now();
    if (now < breakUntil_) return true;
    if (now >= nextBreakAt_) {
        int dur = rndInt(cfg_.breakDurMinSec, cfg_.breakDurMaxSec);
        breakUntil_ = now + std::chrono::seconds(dur);
        scheduleNextBreak();
        return true;
    }
    return false;
}

bool Humanizer::inSessionPause() {
    std::lock_guard<std::mutex> g(mu_);
    auto now = clock_t_::now();
    if (now < sessionPauseUntil_) return true;
    if (now >= nextSessionEndAt_) {
        int dur = rndInt(cfg_.sessionPauseMinSec, cfg_.sessionPauseMaxSec);
        sessionPauseUntil_ = now + std::chrono::seconds(dur);
        sessionStart_ = sessionPauseUntil_;
        scheduleNextSessionEnd();
        return true;
    }
    return false;
}

void Humanizer::notifyActionFired(int /*priority*/) {
    // Reserved for future per-priority counters (telemetry).
}
