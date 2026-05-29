#include "waterline.h"

namespace waterline {

double computeFillPct(const cv::Mat& fillMask, const cv::Mat& shapeMask, double rowMinFillRatio) {
    if (fillMask.empty()) return 0.0;
    const int H = fillMask.rows;
    const int W = fillMask.cols;
    const bool useShape = !shapeMask.empty();

    // Count total "shape" rows (rows where shape exists at all).
    int shapeRowCount = 0;
    for (int y = 0; y < H; ++y) {
        int inShape = useShape ? cv::countNonZero(shapeMask.row(y)) : W;
        if (inShape > 0) ++shapeRowCount;
    }
    if (shapeRowCount == 0) return 0.0;

    // Scan from bottom up; first contiguous "filled" rows count toward fill height.
    int filledRows = 0;
    for (int y = H - 1; y >= 0; --y) {
        int rowShape = useShape ? cv::countNonZero(shapeMask.row(y)) : W;
        if (rowShape == 0) continue;
        cv::Mat rowFill;
        if (useShape) cv::bitwise_and(fillMask.row(y), shapeMask.row(y), rowFill);
        else rowFill = fillMask.row(y);
        int filled = cv::countNonZero(rowFill);
        double ratio = static_cast<double>(filled) / rowShape;
        if (ratio >= rowMinFillRatio) {
            ++filledRows;
        } else {
            break;                                  // waterline reached
        }
    }
    return static_cast<double>(filledRows) / shapeRowCount;
}

}  // namespace waterline
