#pragma once
// Frame source interface — implementations: WgcCapture, MockReplaySource.
#include <windows.h>
#include "frame.h"

class IFrameSource {
public:
    virtual ~IFrameSource() = default;
    virtual bool start(HWND target) = 0;
    virtual void stop() = 0;
    // Block up to timeoutMs for a new frame. Returns false on timeout.
    virtual bool acquire(Frame& out, int timeoutMs) = 0;
};
