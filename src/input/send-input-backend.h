#pragma once
// Foreground-only input via SendInput. Required when game rejects PostMessage
// (DirectInput exclusive / anti-cheat). Restores cursor after click.
#include <atomic>
#include <random>
#include <vector>
#include "i-input-backend.h"
#include "mouse-path.h"

class SendInputBackend : public IInputBackend {
public:
    SendInputBackend();
    void setTarget(HWND h) override { target_ = h; }
    void sendKeyTap(WORD vk, int holdMs) override;
    void sendKeyDown(WORD vk) override;
    void sendKeyUp(WORD vk) override;
    void sendRightClick(int x, int y) override;
    void sendShiftRightClick(int x, int y) override;
    void sendMouseMove(int x, int y) override;
    void setMousePathEnabled(bool enabled) override { mousePathEnabled_.store(enabled); }
    bool requiresForeground() const override { return true; }

private:
    HWND target_ = nullptr;
    std::atomic<bool> mousePathEnabled_{true};
    std::mt19937 rng_;
    std::vector<Waypoint> pathBuf_;   // reusable, tránh alloc trong hot path
    void clientToScreenAbs(int xClient, int yClient, LONG& absX, LONG& absY) const;
    void pressKey(WORD vk, bool down);
    void mouseButton(bool right, bool down);
    void moveAbs(LONG absX, LONG absY);
    // Bezier path từ cursor hiện tại (screen) → target client (x,y).
    void executePath(int xClient, int yClient);
};
