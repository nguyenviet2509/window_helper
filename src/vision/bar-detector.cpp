#include "bar-detector.h"
#include "waterline.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>

double BarDetector::computeFillRatio(const cv::Mat& bgra,
                                     const BarRegion& r,
                                     const std::vector<HueRange>& hueRanges) const {
    if (bgra.empty() || r.w <= 0 || r.h <= 0) return 0.0;
    // Clamp ROI to frame.
    int x = std::clamp(r.x, 0, bgra.cols - 1);
    int y = std::clamp(r.y, 0, bgra.rows - 1);
    int w = std::min(r.w, bgra.cols - x);
    int h = std::min(r.h, bgra.rows - y);
    if (w <= 0 || h <= 0) return 0.0;

    cv::Mat roi = bgra(cv::Rect(x, y, w, h));
    cv::Mat bgr, hsv;
    cv::cvtColor(roi, bgr, cv::COLOR_BGRA2BGR);
    cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);

    // Build OR-union mask across hue ranges.
    cv::Mat fillMask = cv::Mat::zeros(hsv.size(), CV_8UC1);
    for (const auto& hr : hueRanges) {
        cv::Mat m;
        cv::inRange(hsv,
                    cv::Scalar(hr.hMin, satMin, valMin),
                    cv::Scalar(hr.hMax, 255, 255), m);
        cv::bitwise_or(fillMask, m, fillMask);
    }

    // Optional circular shape mask (orb shape).
    cv::Mat shapeMask;
    if (r.shape == "circle" && r.radius > 0) {
        shapeMask = cv::Mat::zeros(h, w, CV_8UC1);
        cv::circle(shapeMask, cv::Point(w / 2, h / 2), r.radius, cv::Scalar(255), cv::FILLED);
    }

    return waterline::computeFillPct(fillMask, shapeMask, rowMinFillRatio);
}
