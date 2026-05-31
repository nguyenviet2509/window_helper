#pragma once
// PotRefillScheduler: tự nạp lại HP/SP/MP pot từ kho theo interval cố định.
// Flow: tap V (mở kho) -> move chuột tới slot -> Shift+1/2/3 -> tap V (đóng) -> restore cursor.
// Pause toàn bộ combat trong khi refill bằng OutputGate::setRefillActive(true).
// Abort an toàn nếu HP < hpCriticalAbortThreshold giữa chừng (đóng kho, release shift, backoff).
#include <chrono>
#include <functional>
#include <windows.h>
#include <opencv2/core.hpp>

#include "../state/game-state.h"
#include "../input/input-scheduler.h"
#include "../core/output-gate.h"
#include "../vision/roi.h"

class PotRefillScheduler {
public:
    // FrameSnapshotter: cấp frame BGRA mới nhất để probe pixel slot kho. Optional.
    using FrameSnapshotter = std::function<bool(cv::Mat&)>;

    PotRefillScheduler(InputScheduler& sched, OutputGate& gate, HWND target,
                       const PotRefillConfig& cfg);

    void setFrameSnapshotter(FrameSnapshotter fn) { snapshot_ = std::move(fn); }

    void enable(bool on) { enabled_ = on; }
    bool enabled() const { return enabled_; }
    bool busy() const    { return state_ != State::Idle; }

    void updateConfig(const PotRefillConfig& cfg) { cfg_ = cfg; }
    void setTarget(HWND h) { target_ = h; }

    void tick(const VisionState& v, std::chrono::steady_clock::time_point now);

    // For UI: giây tới refill kế tiếp của 1 slot ('h'/'s'/'m'). -1 nếu disabled.
    int secondsUntilNext(char which, std::chrono::steady_clock::time_point now) const;
    const char* stateName() const;

private:
    enum class State {
        Idle,
        OpenInv,        // đã tap V, chờ kho mở xong
        MoveSlot,       // đã schedule move mouse, chờ delay
        FireSlot,       // đã schedule Shift+N, chờ delay
        CloseInv,       // đã tap V đóng kho, chờ delay
        Cleanup,        // restore cursor, clear gate flag
        AbortCloseInv,  // abort: tap V đóng kho
        AbortCleanup,   // abort: restore cursor, set backoff
        CoreMove,       // HP pot hết: move chuột tới core (tp về làng)
        CoreClick,      // right-click core
    };
    enum class Slot { None, Hp, Sp, Mp };

    using TP = std::chrono::steady_clock::time_point;

    void beginRefill(TP now);
    void enterAbort(TP now);
    void doCleanup(TP now, bool aborted);
    void scheduleStep(int prio, TP fireAt, std::function<void(IInputBackend&)> action);
    bool anyDue(TP now) const;
    bool isDue(Slot s, TP now) const;
    Slot pickNextSlot();    // HP -> MP -> SP order; clears slotsPlanned_ as it goes
    WORD slotVk(Slot s) const;
    const PotRefillSlot& slotCfg(Slot s) const;
    TP& lastRefillAt(Slot s);
    void logState(const char* tag);

    InputScheduler& sched_;
    OutputGate& gate_;
    HWND target_;
    PotRefillConfig cfg_;

    bool enabled_ = false;
    State state_ = State::Idle;
    Slot currentSlot_ = Slot::None;

    TP nextStepAt_{};
    TP refillStartedAt_{};
    TP lastHpAt_{}, lastSpAt_{}, lastMpAt_{};
    TP abortBackoffUntil_{};
    POINT savedCursorScreen_{0, 0};
    bool slotsPlanned_[3] = { false, false, false };   // [Hp=0, Sp=1, Mp=2]
    bool corePending_ = false;        // HP slot rỗng -> trigger core teleport sau khi đóng kho
    FrameSnapshotter snapshot_;       // optional pixel probe source
};
