#pragma once
// Foreground-only input via SendInput. Required when game rejects PostMessage
// (DirectInput exclusive / anti-cheat). Restores cursor after click.
#include "i-input-backend.h"

class SendInputBackend : public IInputBackend {
public:
    void setTarget(HWND h) override { target_ = h; }
    void sendKeyTap(WORD vk, int holdMs) override;
    void sendKeyDown(WORD vk) override;
    void sendKeyUp(WORD vk) override;
    void sendRightClick(int x, int y) override;
    void sendShiftRightClick(int x, int y) override;
    bool requiresForeground() const override { return true; }

private:
    HWND target_ = nullptr;
    void clientToScreenAbs(int xClient, int yClient, LONG& absX, LONG& absY) const;
    void pressKey(WORD vk, bool down);
    void mouseButton(bool right, bool down);
};
