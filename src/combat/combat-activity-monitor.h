#pragma once
// MP-only mob-death detect: track timestamp of last "MP went down" tick.
// Nếu không có tick âm nào trong deathConfirmMs window -> mob coi như đã chết.
// "MP tụt rồi hồi lại" vẫn tính là active vì timer chỉ reset khi có tick âm mới,
// regen lên không xóa dấu (khác với logic max-min cũ bị nhiễu bởi regen).
#include <chrono>

class CombatActivityMonitor {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    void update(double mp, TimePoint now);
    void reset(TimePoint now);
    bool mobLikelyDead(TimePoint now) const;

    // Tunable (loaded from config.json, hidden — không expose UI).
    int deathConfirmMs = 2500;     // no-MP-drop window để declare dead
    double mpDropEpsilon = 0.005;  // 0.5% threshold cho "MP went down" tick

private:
    double prevMp_ = -1.0;                          // sentinel "no sample yet"
    TimePoint lastMpDropAt_ = TimePoint::min();     // last "MP went down" event
};
