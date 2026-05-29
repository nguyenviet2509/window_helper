#pragma once
// Region-of-interest types for bar / orb detection.
#include <string>
#include <vector>

struct HueRange { int hMin; int hMax; };          // OpenCV H in [0, 180]

struct BarRegion {
    int x = 0, y = 0, w = 0, h = 0;               // ROI in source frame (px)
    std::string shape = "rect";                   // "rect" | "circle"
    int radius = 0;                               // for "circle"
};

struct VisionState {
    double hpPct = 0;                             // 0..1
    double mpPct = 0;
    double spPct = 0;
    bool valid = false;
    uint64_t seq = 0;
};
