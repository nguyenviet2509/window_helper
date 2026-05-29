#include "vision-pipeline.h"
#include <chrono>

VisionPipeline::VisionPipeline(IFrameSource& src, BarConfig hp, BarConfig mp, BarConfig sp)
    : src_(src), hpCfg_(std::move(hp)), mpCfg_(std::move(mp)), spCfg_(std::move(sp)) {}

void VisionPipeline::setCallback(std::function<void(const VisionState&)> cb) {
    std::lock_guard<std::mutex> g(cbMu_);
    cb_ = std::move(cb);
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
    while (running_.load()) {
        next += period;
        if (src_.acquire(f, 100)) {
            double hpRaw = det_.computeFillRatio(f.bgra, hpCfg_.region, hpCfg_.hues);
            double mpRaw = det_.computeFillRatio(f.bgra, mpCfg_.region, mpCfg_.hues);
            double spRaw = det_.computeFillRatio(f.bgra, spCfg_.region, spCfg_.hues);

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
