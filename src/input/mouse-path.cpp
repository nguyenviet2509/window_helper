#include "mouse-path.h"
#include <cmath>
#include <algorithm>

namespace {

double distance(POINT a, POINT b) {
    double dx = static_cast<double>(b.x - a.x);
    double dy = static_cast<double>(b.y - a.y);
    return std::sqrt(dx * dx + dy * dy);
}

// Cubic Bezier B(t) = (1-t)^3 P0 + 3(1-t)^2 t P1 + 3(1-t) t^2 P2 + t^3 P3
POINT cubicBezier(POINT p0, POINT p1, POINT p2, POINT p3, double t) {
    double u = 1.0 - t;
    double b0 = u * u * u;
    double b1 = 3.0 * u * u * t;
    double b2 = 3.0 * u * t * t;
    double b3 = t * t * t;
    return {
        static_cast<LONG>(b0 * p0.x + b1 * p1.x + b2 * p2.x + b3 * p3.x),
        static_cast<LONG>(b0 * p0.y + b1 * p1.y + b2 * p2.y + b3 * p3.y),
    };
}

}  // namespace

void MousePath::generate(POINT from, POINT to,
                         std::mt19937& rng,
                         std::vector<Waypoint>& out) {
    out.clear();
    double d = distance(from, to);
    if (d < 30.0) return;  // quá gần → snap

    // Đơn vị vector vuông góc với (from→to) để lệch control point ra hai bên.
    double dx = to.x - from.x;
    double dy = to.y - from.y;
    double nx = -dy / d;
    double ny = dx / d;

    std::uniform_real_distribution<double> magRange(0.15, 0.35);
    std::uniform_real_distribution<double> tEarly(0.15, 0.35);
    std::uniform_real_distribution<double> tLate(0.65, 0.85);
    std::uniform_int_distribution<int> coin(0, 1);

    double sign1 = coin(rng) ? 1.0 : -1.0;
    double sign2 = coin(rng) ? 1.0 : -1.0;
    double off1 = magRange(rng) * d * sign1;
    double off2 = magRange(rng) * d * sign2;

    double t1 = tEarly(rng);
    double t2 = tLate(rng);

    POINT p1 = {
        static_cast<LONG>(from.x + dx * t1 + nx * off1),
        static_cast<LONG>(from.y + dy * t1 + ny * off1),
    };
    POINT p2 = {
        static_cast<LONG>(from.x + dx * t2 + nx * off2),
        static_cast<LONG>(from.y + dy * t2 + ny * off2),
    };

    int count = static_cast<int>(std::clamp(d / 8.0, 5.0, 16.0));
    int totalMs = static_cast<int>(std::clamp(80.0 + d * 0.5, 80.0, 250.0));
    int stepMs = std::max(1, totalMs / count);

    out.reserve(static_cast<size_t>(count));
    // Bỏ qua điểm cuối (t=1) — caller click ở `to`.
    for (int i = 1; i <= count; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(count + 1);
        POINT p = cubicBezier(from, p1, p2, to, t);
        out.push_back({ static_cast<int>(p.x), static_cast<int>(p.y), stepMs });
    }
}
