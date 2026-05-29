#include "combat-activity-monitor.h"
#include <algorithm>

void CombatActivityMonitor::update(double hp, double mp) {
    hp_.push_back(hp);
    mp_.push_back(mp);
    while ((int)hp_.size() > windowSize) hp_.pop_front();
    while ((int)mp_.size() > windowSize) mp_.pop_front();
}

void CombatActivityMonitor::reset() {
    hp_.clear();
    mp_.clear();
}

bool CombatActivityMonitor::mobLikelyDead() const {
    if ((int)mp_.size() < windowSize) return false;
    // MP drain = max - min over window. (Player casts -> MP drops -> max-min > eps.)
    double mpRange = maxMinusMin(mp_);
    double hpRange = maxMinusMin(hp_);
    return mpRange < mpDrainEpsilon && hpRange < hpDropEpsilon;
}

double CombatActivityMonitor::maxMinusMin(const std::deque<double>& d) {
    if (d.empty()) return 0;
    auto [lo, hi] = std::minmax_element(d.begin(), d.end());
    return *hi - *lo;
}
