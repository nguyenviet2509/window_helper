# Phase 1 — Capture WGC + Vision Detector + Mock Skeleton

**Est:** 2 ngày
**Priority:** P0 — foundation
**Status:** code-ready (awaiting vcpkg install + first build)

## Implemented Files
- `src/CMakeLists.txt`
- `src/main.cpp` — dev harness (finds `Priston Tale` / `PtMockGame`, runs WGC + vision, logs via OutputDebugString)
- `src/capture/frame.h`, `i-frame-source.h`
- `src/capture/wgc-capture.{h,cpp}` — D3D11 + WGC free-threaded pool + staging-copy → cv::Mat
- `src/core/spsc-frame-slot.h` — 1-slot overwrite buffer
- `src/vision/roi.h` — BarRegion, HueRange, VisionState
- `src/vision/waterline.{h,cpp}` — fill-pct algorithm (bottom-up row scan)
- `src/vision/bar-detector.{h,cpp}` — BGRA→HSV inRange OR-union + optional circle mask
- `src/vision/vision-pipeline.{h,cpp}` — 20Hz loop, EMA(0.5) smoothing, callback API
- `mock/CMakeLists.txt`, `mock/src/main.cpp` — PtMockGame 800×600, 3 trackbars, 3 vertical bars

## Build (user)
```
# 1. Set VCPKG_ROOT env or pass -DCMAKE_TOOLCHAIN_FILE
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\bin\Release\PtMockGame.exe          # start mock first
.\build\bin\Release\WindowHelper.exe        # connect, dump to DebugView
```

## Known Risks (compile)
- vcpkg `imgui[dx11-binding,win32-binding]` resolves but is unused at Phase 1 — kept in manifest for Phase 4.
- WGC requires Windows SDK ≥ 10.0.18362; `IsCursorCaptureEnabled` setter requires Win10 2004+ (wrapped in try/catch).
- C++/WinRT headers from Windows SDK; no separate vcpkg port needed.

## Mục Tiêu
- WGC capture pipeline producing `cv::Mat` frames at 20Hz
- HP/MP/SP detector dùng waterline Method B (Section 28, 34)
- PtMockGame.exe skeleton render 3 bar đứng center-bottom 800×600

## Context
- Brainstorm Section 3 (WGC), 7 (detector), 28 (waterline), 33 (mock), 34 (bar đứng), 35 (vị trí center)

## Setup Dự Án

```
anonymous/
├── CMakeLists.txt
├── vcpkg.json
├── src/                                  # WindowHelper.exe
│   ├── main.cpp
│   ├── capture/
│   │   ├── i-frame-source.h
│   │   ├── wgc-capture.h/.cpp
│   │   └── frame.h
│   ├── vision/
│   │   ├── vision-pipeline.h/.cpp
│   │   ├── bar-detector.h/.cpp           # generic, shape-agnostic
│   │   ├── waterline.h/.cpp
│   │   └── roi.h
│   └── core/
│       └── spsc-frame-slot.h
└── mock/                                  # PtMockGame.exe
    ├── CMakeLists.txt
    └── src/
        ├── main.cpp
        ├── mock-window.cpp
        ├── orb-renderer.cpp              # render bar đứng
        └── simulator.cpp
```

## Implementation Steps

### 1.1 vcpkg.json + CMakeLists.txt root
```json
{
  "name": "pt-automation",
  "version": "0.1.0",
  "dependencies": [
    "opencv4",
    "nlohmann-json",
    {"name": "imgui", "features": ["dx11-binding", "win32-binding"]}
  ]
}
```

CMakeLists.txt setup C++17, MSVC flags `/W4 /permissive- /Zc:__cplusplus /utf-8 /MP`, `NOMINMAX WIN32_LEAN_AND_MEAN UNICODE`, OUTPUT_NAME `WindowHelper`.

### 1.2 IFrameSource Interface

```cpp
struct Frame {
    cv::Mat bgra;
    uint64_t seq = 0;
    std::chrono::steady_clock::time_point ts;
    RECT windowRect;
};

class IFrameSource {
public:
    virtual ~IFrameSource() = default;
    virtual bool start(HWND target) = 0;
    virtual void stop() = 0;
    virtual bool acquire(Frame& out, int timeoutMs) = 0;
};
```

### 1.3 WgcCapture Implementation
- WinRT init: `Direct3D11CaptureFramePool::CreateFreeThreaded`
- Subscribe `FrameArrived` event → copy staging texture → map → `cv::Mat` view → push vào SPSC slot
- Release frame, return latest snapshot trên `acquire()`
- Handle `IGraphicsCaptureItemInterop::CreateForWindow`
- Cursor capture OFF (`session_.IsCursorCaptureEnabled(false)`)
- DPI awareness: set `Per Monitor V2` qua manifest hoặc `SetProcessDpiAwarenessContext`

### 1.4 SpscFrameSlot (1-slot overwrite)
```cpp
template<class T>
class SpscFrameSlot {
    std::mutex m_;
    std::condition_variable cv_;
    std::optional<T> slot_;
public:
    void push(T v);
    bool pop(T& out, int timeoutMs);
};
```

### 1.5 Bar Detector (waterline, shape-agnostic)

```cpp
struct BarRegion {
    int x, y, w, h;
    std::string shape;   // "rect" | "circle"
    int radius = 0;
};

struct HueRange { int hMin, hMax; };

class BarDetector {
public:
    double computeFillRatio(const cv::Mat& bgra,
                            const BarRegion& region,
                            const std::vector<HueRange>& hueRanges) const;
private:
    double waterlineMethod(const cv::Mat& redMask,
                           const cv::Mat& shapeMask) const;
};
```

Algorithm: crop ROI → BGRA→HSV → inRange với hue ranges (HP wrap-around 0–10, 170–180) → optional shape mask (circle nếu shape="circle") → waterline scan từ đáy → fill_pct.

### 1.6 EMA Smoothing
```cpp
class EmaFilter {
    double alpha_, value_ = 0;
    bool init_ = false;
public:
    explicit EmaFilter(double alpha) : alpha_(alpha) {}
    double update(double raw) {
        if (!init_) { value_ = raw; init_ = true; }
        else value_ = alpha_ * raw + (1 - alpha_) * value_;
        return value_;
    }
};
```

### 1.7 VisionPipeline
```cpp
class VisionPipeline {
public:
    void run(std::stop_token st);   // 20Hz loop
private:
    IFrameSource& src_;
    BarDetector det_;
    EmaFilter hpEma_, mpEma_, spEma_;
    // Output: VisionState pushed to bus mỗi tick
};
```

### 1.8 PtMockGame.exe Skeleton

Win32 GUI 800×600 với:
- 3 bar đứng render bằng GDI `FillRect`:
  - HP: `RGB(220,30,30)` tại (355, 475, 12, 105)
  - SP: `RGB(220,180,40)` tại (383, 475, 12, 105)
  - MP: `RGB(50,100,220)` tại (411, 475, 12, 105)
- 3 trackbar điều chỉnh HP/MP/SP % (0–100)
- Window title `"Priston Tale"` (để FindWindow tool match)
- `WM_PAINT` redraw orb theo slider value

## Files
- Tạo: ~14 file mới (xem 1.1)
- Tổng LOC: ~700 (capture 200, vision 200, mock 300)

## Acceptance
- [ ] `cmake --build` thành công cả 2 exe
- [ ] WgcCapture start trên mock window, get frame
- [ ] BarDetector return 0.0–1.0 tỷ lệ chính xác từ frame mock
- [ ] Mock window slider thay đổi → frame thay đổi → detector phản ánh đúng
- [ ] FPS pipeline ổn định 20Hz, CPU < 3%

## Test Manual
1. Chạy `PtMockGame.exe`
2. Chạy `WindowHelper.exe` (test harness in dev — chưa có UI đầy đủ)
3. Kéo slider HP từ 100% → 0% trong mock
4. Verify detector output khớp slider value (±5%)

## Risks
- WGC fail trên Win10 < 1803: bắt buộc Win10 1809+
- HSV hue range không khớp theme PT thật → phase 7 tune
- DPI scaling sai → fix bằng manifest

## Next
Phase 2 dùng WgcCapture interface.
