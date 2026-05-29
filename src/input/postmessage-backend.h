#pragma once
// Background-friendly input via PostMessage. Works when game accepts WM_KEY*/WM_*BUTTON*
// while not foreground (verified via Phase 0 probe).
#include "i-input-backend.h"

class PostMessageBackend : public IInputBackend {
public:
    void setTarget(HWND h) override { target_ = h; }
    void sendKeyTap(WORD vk, int holdMs) override;
    void sendKeyDown(WORD vk) override;
    void sendKeyUp(WORD vk) override;
    void sendRightClick(int x, int y) override;
    void sendShiftRightClick(int x, int y) override;
    bool requiresForeground() const override { return false; }

private:
    HWND target_ = nullptr;
    static LPARAM keyLParam(WORD vk, bool up);
};
