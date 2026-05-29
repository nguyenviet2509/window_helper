#pragma once
// Generic shape-agnostic bar/orb fill detector.
// Pipeline: crop ROI -> BGRA->HSV -> inRange across hue ranges -> optional shape mask
//           -> waterline scan -> fill pct [0,1].
#include <opencv2/core.hpp>
#include "roi.h"

class BarDetector {
public:
    // bgra: full source frame (CV_8UC4). region: ROI in source coords.
    // hueRanges: OR-union of HSV hue bands (handles wrap-around e.g. HP red).
    double computeFillRatio(const cv::Mat& bgra,
                            const BarRegion& region,
                            const std::vector<HueRange>& hueRanges) const;

    // Knobs (tune in Phase 7).
    int satMin = 80;
    int valMin = 60;
    double rowMinFillRatio = 0.30;
};
