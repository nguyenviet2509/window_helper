#pragma once
// Background-friendly input via PostMessage. Works when game accepts WM_KEY*/WM_*BUTTON*
// while not foreground (verified via Phase 0 probe).
#include <atomic>
#include <random>
#include <vector>
#include "i-input-backend.h"
#include "mouse-path.h"

class PostMessageBackend : public IInputBackend {
public:
    PostMessageBackend();
    void setTarget(HWND h) override { target_ = h; }
    void sendKeyTap(WORD vk, int holdMs) override;
    void sendKeyDown(WORD vk) override;
    void sendKeyUp(WORD vk) override;
    void sendRightClick(int x, int y) override;
    void sendShiftRightClick(int x, int y) override;
    void sendMouseMove(int x, int y) override;
    void setMousePathEnabled(bool enabled) override { mousePathEnabled_.store(enabled); }
    bool requiresForeground() const override { return false; }

private:
    HWND target_ = nullptr;
    std::atomic<bool> mousePathEnabled_{true};
    std::mt19937 rng_;
    std::vector<Waypoint> pathBuf_;
    POINT lastSent_{ 0, 0 };          // PostMessage không có "cursor" thực — tự track
    static LPARAM keyLParam(WORD vk, bool up);
    void executePath(int xClient, int yClient, WPARAM moveWParam);
};
