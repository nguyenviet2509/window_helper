#pragma once
// Hue range auto-sampler. Cho 1 frame BGRA + ROI rectangle → tính HueRange[]
// đặc trưng pixel bar đầy. Multi-mode (bimodal) detect cho màu wrap-around (đỏ).
#include <opencv2/core.hpp>
#include <vector>
#include "roi.h"

class HueSampler {
public:
    struct Result {
        std::vector<HueRange> hues;    // 1 hoặc 2 range (wrap-around)
        int sampledPixels = 0;         // pixel pass saturation+value filter
        int totalPixels   = 0;         // pixel trong ROI (debug)
    };

    // bgra: full source frame. region: ROI (rectangle, ignore shape).
    // Pixel pass filter saturation>=satMin & value>=valMin mới được count vào hue histogram.
    // Trả về Result với 1-2 HueRange. Padding hueRangePadding mở rộng range mỗi đầu để
    // tolerate noise và variance giữa frame.
    Result sample(const cv::Mat& bgra, const BarRegion& region) const;

    int satMin = 80;
    int valMin = 60;
    int hueRangePadding = 5;
    // Threshold tỉ lệ "binCount / maxBin" để 1 hue bucket được consider thuộc range.
    // Cao = strict, hẹp range. Thấp = liberal, range rộng (dễ false positive).
    double binIncludeRatio = 0.15;
};
