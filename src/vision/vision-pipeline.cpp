#include "vision-pipeline.h"
#include "../core/logger.h"
#include <chrono>

VisionPipeline::VisionPipeline(IFrameSource& src, BarConfig hp, BarConfig mp, BarConfig sp)
    : src_(src), hpCfg_(std::move(hp)), mpCfg_(std::move(mp)), spCfg_(std::move(sp)) {}

void VisionPipeline::setCallback(std::function<void(const VisionState&)> cb) {
    std::lock_guard<std::mutex> g(cbMu_);
    cb_ = std::move(cb);
}

cv::Mat VisionPipeline::snapshotLatestFrame() const {
    std::lock_guard<std::mutex> g(frameMu_);
    return latestFrame_.clone();
}

void VisionPipeline::updateConfig(BarConfig hp, BarConfig mp, BarConfig sp) {
    std::lock_guard<std::mutex> g(cfgMu_);
    hpCfg_ = std::move(hp);
    mpCfg_ = std::move(mp);
    spCfg_ = std::move(sp);
    hpEma_.reset();
    mpEma_.reset();
    spEma_.reset();
}

void VisionPipeline::start() {
    if (running_.exchange(true)) return;
    th_ = std::thread([this] { runLoop(); });
}

void VisionPipeline::stop() {
    if (!running_.exchange(false)) return;
    if (th_.joinable()) th_.join();
}

void VisionPipeline::runLoop() {
    using clock = std::chrono::steady_clock;
    const auto period = std::chrono::milliseconds(50);          // 20 Hz target
    auto next = clock::now();

    Frame f;
    bool loggedFrameSize = false;
    while (running_.load()) {
        next += period;
        if (src_.acquire(f, 100)) {
            if (!loggedFrameSize) {
                loggedFrameSize = true;
                Logger::instance().logf(LogLevel::Info,
                    "[vision] FIRST FRAME SIZE: %d x %d (cols x rows)",
                    f.bgra.cols, f.bgra.rows);
            }
            { std::lock_guard<std::mutex> g(frameMu_); latestFrame_ = f.bgra.clone(); }
            BarConfig hpSnap, mpSnap, spSnap;
            { std::lock_guard<std::mutex> g(cfgMu_);
              hpSnap = hpCfg_; mpSnap = mpCfg_; spSnap = spCfg_; }
            double hpRaw = det_.computeFillRatio(f.bgra, hpSnap.region, hpSnap.hues);
            double mpRaw = det_.computeFillRatio(f.bgra, mpSnap.region, mpSnap.hues);
            double spRaw = det_.computeFillRatio(f.bgra, spSnap.region, spSnap.hues);

            VisionState s;
            s.hpPct = hpEma_.update(hpRaw);
            s.mpPct = mpEma_.update(mpRaw);
            s.spPct = spEma_.update(spRaw);
            s.valid = true;
            s.seq = f.seq;

            std::function<void(const VisionState&)> cb;
            { std::lock_guard<std::mutex> g(cbMu_); cb = cb_; }
            if (cb) cb(s);
        }
        auto now = clock::now();
        if (next > now) std::this_thread::sleep_until(next);
        else next = now;                                         // drift catch-up
    }
}
