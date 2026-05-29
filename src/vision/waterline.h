#pragma once
// Waterline algorithm: scan rows from bottom -> top, find first row with
// insufficient filled pixels. Fill pct = filled_height / total_height.
#include <opencv2/core.hpp>

namespace waterline {

// fillMask: 8U single channel, non-zero = "filled". shapeMask: optional, same size,
// non-zero where the bar/orb pixels live (used for circular orbs to ignore corners).
// rowMinFillRatio: row counts as "filled" if filledInRow / shapeInRow >= threshold.
double computeFillPct(const cv::Mat& fillMask,
                      const cv::Mat& shapeMask /*may be empty*/,
                      double rowMinFillRatio);

}  // namespace waterline
