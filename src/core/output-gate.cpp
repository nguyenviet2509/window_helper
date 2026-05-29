#include "output-gate.h"

bool OutputGate::allowInput() const noexcept {
    if (!hwnd_ || !IsWindow(hwnd_)) return false;
    if (IsIconic(hwnd_)) return false;
    if (sessionLocked_.load()) return false;
    if (fsm_) {
        auto h = fsm_->state();
        if (h == CaptureHealth::Unsafe || h == CaptureHealth::Stopped) return false;
    }
    if (reqFg_ && GetForegroundWindow() != hwnd_) return false;
    return true;
}
