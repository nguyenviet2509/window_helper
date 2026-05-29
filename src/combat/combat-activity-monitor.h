#pragma once
// Smart mob-death detect: track HP/MP samples in a ~2s sliding window. If
// neither has moved meaningfully, current target is likely dead -> repick.
#include <deque>

class CombatActivityMonitor {
public:
    void update(double hp, double mp);
    void reset();
    bool mobLikelyDead() const;

    // Tunable.
    int windowSize = 40;             // 40 samples * 50ms = 2.0s
    double mpDrainEpsilon = 0.010;   // <1% change considered "no MP drain"
    double hpDropEpsilon  = 0.010;

private:
    std::deque<double> hp_, mp_;
    static double maxMinusMin(const std::deque<double>& d);
};
