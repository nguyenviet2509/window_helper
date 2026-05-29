#pragma once
// Windows.Graphics.Capture (WGC) frame source.
// Requires Win10 1809+. Uses D3D11 + IGraphicsCaptureItemInterop::CreateForWindow.
#include "i-frame-source.h"
#include "../core/spsc-frame-slot.h"
#include <atomic>
#include <memory>

class WgcCapture : public IFrameSource {
public:
    WgcCapture();
    ~WgcCapture() override;
    bool start(HWND target) override;
    void stop() override;
    bool acquire(Frame& out, int timeoutMs) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    SpscFrameSlot<Frame> slot_;
    std::atomic<uint64_t> seq_{0};
    HWND target_ = nullptr;
};
