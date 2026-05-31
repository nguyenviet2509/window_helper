#pragma once
// MousePath: sinh cubic-Bezier path giả người thật giữa 2 điểm (client/screen coords).
// Dùng cho cả SendInput (screen) lẫn PostMessage (client) backend — caller tự quy đổi.
#include <vector>
#include <random>
#include <windows.h>

struct Waypoint {
    int x;
    int y;
    int delayMs;     // sleep trước khi gửi waypoint này
};

class MousePath {
public:
    // Sinh path Bezier từ `from` → `to`. `out` được clear và fill bằng waypoint trung gian
    // (không bao gồm điểm cuối — caller tự click ở `to`). Nếu distance < 30px → out trống
    // (caller snap thẳng, không cần path).
    static void generate(POINT from, POINT to,
                         std::mt19937& rng,
                         std::vector<Waypoint>& out);
};
