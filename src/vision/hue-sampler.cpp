#include "hue-sampler.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>

HueSampler::Result HueSampler::sample(const cv::Mat& bgra, const BarRegion& region) const {
    Result r{};
    if (bgra.empty() || region.w <= 0 || region.h <= 0) return r;

    // Clamp ROI vào frame để tránh out-of-bound.
    int x = std::max(0, region.x);
    int y = std::max(0, region.y);
    int w = std::min(region.w, bgra.cols - x);
    int h = std::min(region.h, bgra.rows - y);
    if (w <= 0 || h <= 0) return r;

    cv::Mat roi = bgra(cv::Rect(x, y, w, h));
    cv::Mat bgr, hsv;
    cv::cvtColor(roi, bgr, cv::COLOR_BGRA2BGR);
    cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);
    r.totalPixels = w * h;

    // Hue histogram: OpenCV hue range [0, 180).
    int hist[180] = {};
    for (int yy = 0; yy < hsv.rows; ++yy) {
        const cv::Vec3b* row = hsv.ptr<cv::Vec3b>(yy);
        for (int xx = 0; xx < hsv.cols; ++xx) {
            int hueVal = row[xx][0];
            int satVal = row[xx][1];
            int valVal = row[xx][2];
            if (satVal < satMin || valVal < valMin) continue;
            if (hueVal >= 0 && hueVal < 180) {
                hist[hueVal]++;
                r.sampledPixels++;
            }
        }
    }
    if (r.sampledPixels == 0) return r;

    // Find peak bin → threshold = binIncludeRatio * peak.
    int peak = 0;
    for (int i = 0; i < 180; ++i) peak = std::max(peak, hist[i]);
    int threshold = std::max(1, (int)(peak * binIncludeRatio));

    // Mark each bin "included" if count >= threshold. Build contiguous segments,
    // treating hue như circular (wrap 179 → 0).
    bool included[180] = {};
    for (int i = 0; i < 180; ++i) included[i] = hist[i] >= threshold;

    // Tìm các đoạn liên tục. Wrap-around: nếu included[0] && included[179] → đoạn
    // bao qua biên (vd HP đỏ). Xử lý bằng cách rotate sao cho bin chưa included
    // đứng đầu, rồi scan tuyến tính.
    int startOffset = 0;
    if (included[0] && included[179]) {
        for (int i = 0; i < 180; ++i) {
            if (!included[i]) { startOffset = i; break; }
        }
        // Nếu tất cả bin included (rare, full-spectrum) → trả về 1 range full.
        if (startOffset == 0) {
            r.hues.push_back(HueRange{0, 179});
            return r;
        }
    }

    int segStart = -1;
    for (int k = 0; k < 180; ++k) {
        int i = (k + startOffset) % 180;
        if (included[i]) {
            if (segStart < 0) segStart = i;
        } else {
            if (segStart >= 0) {
                int prev = (k - 1 + startOffset) % 180;
                int lo = std::max(0, segStart - hueRangePadding);
                int hi = std::min(179, prev + hueRangePadding);
                r.hues.push_back(HueRange{lo, hi});
                segStart = -1;
            }
        }
    }
    if (segStart >= 0) {
        int prev = (180 - 1 + startOffset) % 180;
        int lo = std::max(0, segStart - hueRangePadding);
        int hi = std::min(179, prev + hueRangePadding);
        r.hues.push_back(HueRange{lo, hi});
    }

    // Wrap-around fixup: nếu startOffset > 0 và lúc đầu included[0] && included[179]
    // → segment đầu (chứa bin gần 179) và segment cuối (chứa bin gần 0) thực ra là
    // 1 range duy nhất → merge thành 2 HueRange phản ánh wrap đúng theo cách
    // BarDetector xử lý (OR-union 2 range).
    // Khi BarDetector union {0, 10} ∪ {170, 179} đều match đỏ → đã tự nhiên handle
    // wrap. Do đó KHÔNG cần merge ở đây, để BarDetector tự union là chính xác.

    return r;
}
