#pragma once
// Frame: single captured frame BGRA + metadata.
#include <chrono>
#include <cstdint>
#include <windows.h>
#include <opencv2/core.hpp>

struct Frame {
    cv::Mat bgra;                                       // 4-channel BGRA, deep-owned
    uint64_t seq = 0;
    std::chrono::steady_clock::time_point ts{};
    RECT windowRect{};                                  // target window client rect at capture time
};
