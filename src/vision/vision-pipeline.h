#pragma once
// 20Hz vision pipeline: capture latest frame, detect HP/MP/SP fill, EMA-smooth, publish.
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include "../capture/i-frame-source.h"
#include "bar-detector.h"
#include "roi.h"

class EmaFilter {
public:
    explicit EmaFilter(double alpha) : alpha_(alpha) {}
    double update(double raw) {
        if (!init_) { value_ = raw; init_ = true; }
        else value_ = alpha_ * raw + (1.0 - alpha_) * value_;
        return value_;
    }
    void reset() { init_ = false; value_ = 0; }
private:
    double alpha_;
    double value_ = 0;
    bool init_ = false;
};

struct BarConfig {
    BarRegion region;
    std::vector<HueRange> hues;
};

class VisionPipeline {
public:
    VisionPipeline(IFrameSource& src, BarConfig hp, BarConfig mp, BarConfig sp);

    void setCallback(std::function<void(const VisionState&)> cb);
    void start();
    void stop();

    // Ad-hoc snapshot frame mới nhất (BGRA). Trả false nếu chưa có frame.
    // Deep-copy để caller dùng an toàn ngoài lock.
    bool snapshotLatest(cv::Mat& out);

private:
    void runLoop();

    IFrameSource& src_;
    BarConfig hpCfg_, mpCfg_, spCfg_;
    BarDetector det_;
    EmaFilter hpEma_{0.5}, mpEma_{0.5}, spEma_{0.5};

    std::function<void(const VisionState&)> cb_;
    std::thread th_;
    std::atomic<bool> running_{false};
    std::mutex cbMu_;

    std::mutex frameMu_;
    cv::Mat latestBgra_;   // deep-owned snapshot for ad-hoc sampling (e.g. inventory probe)
};
