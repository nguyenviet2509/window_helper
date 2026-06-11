#pragma once
// Input backend abstraction. Concrete impls: PostMessage / SendInput.
#include <windows.h>

class IInputBackend {
public:
    virtual ~IInputBackend() = default;
    virtual void setTarget(HWND h) = 0;
    virtual void sendKeyTap(WORD vk, int holdMs) = 0;
    virtual void sendKeyDown(WORD vk) = 0;
    virtual void sendKeyUp(WORD vk) = 0;
    virtual void sendRightClick(int x, int y) = 0;
    virtual void sendShiftRightClick(int x, int y) = 0;
    virtual void sendLeftClick(int x, int y) = 0;
    // Di chuyển con trỏ đến (x,y) trong toạ độ client của target. Dùng bởi mouse-path.
    virtual void sendMouseMove(int x, int y) = 0;
    // Bật/tắt Bezier mouse path cho các click. Khi tắt → click "teleport" như cũ.
    virtual void setMousePathEnabled(bool enabled) = 0;
    virtual bool requiresForeground() const = 0;
};
