#include "combat-activity-monitor.h"

void CombatActivityMonitor::update(double mp, TimePoint now) {
    // Bỏ qua sample đầu tiên (chưa có prev để so sánh).
    if (prevMp_ >= 0.0) {
        double delta = mp - prevMp_;
        if (delta <= -mpDropEpsilon) {
            // MP tụt -> player vẫn đang cast skill -> active combat.
            lastMpDropAt_ = now;
        }
        // MP đứng yên hoặc hồi lên: KHÔNG cập nhật timer.
        // Logic này đáp ứng spec "MP tụt rồi hồi vẫn tính là tụt":
        // timer chỉ reset khi có tick âm mới, không bị regen lên xoá dấu.
    }
    prevMp_ = mp;
}

void CombatActivityMonitor::reset(TimePoint now) {
    prevMp_ = -1.0;
    // 2.5s grace sau pick mob mới: tránh false-positive trước khi player
    // kịp cast skill đầu tiên trên target.
    lastMpDropAt_ = now;
}

bool CombatActivityMonitor::mobLikelyDead(TimePoint now) const {
    if (lastMpDropAt_ == TimePoint::min()) return false;
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - lastMpDropAt_).count();
    return elapsed >= deathConfirmMs;
}
