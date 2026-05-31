#pragma once
// OutputGate: single decision point for "is it safe to send input right now?"
// Inputs: capture health, target HWND state, session lock, foreground req.
#include <windows.h>
#include <atomic>
#include "capture-health-fsm.h"

class OutputGate {
public:
    void setTarget(HWND h)                       { hwnd_ = h; }
    void setHealth(const CaptureHealthFsm* fsm)  { fsm_ = fsm; }
    void setRequireForeground(bool v)            { reqFg_ = v; }
    void setSessionLocked(bool v)                { sessionLocked_.store(v); }
    // Refill pause: khi true, scheduler drop mọi cmd KHÔNG có bypassRefillGate.
    void setRefillActive(bool v)                 { refillActive_.store(v); }
    bool refillActive() const noexcept           { return refillActive_.load(); }

    bool allowInput() const noexcept;

private:
    HWND hwnd_ = nullptr;
    const CaptureHealthFsm* fsm_ = nullptr;
    bool reqFg_ = false;
    std::atomic<bool> sessionLocked_{ false };
    std::atomic<bool> refillActive_{ false };
};
