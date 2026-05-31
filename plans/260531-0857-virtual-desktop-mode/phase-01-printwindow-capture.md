# Phase 01 — PrintWindow Capture Backend

## Context
- Plan: [plan.md](plan.md)
- Existing: [src/capture/wgc-capture.cpp](../../src/capture/wgc-capture.cpp), [src/capture/i-frame-source.h](../../src/capture/i-frame-source.h)
- Vision consumer: [src/vision/vision-pipeline.cpp](../../src/vision/vision-pipeline.cpp)

## Overview
- Priority: P1
- Status: pending
- Mục tiêu: thêm `PrintWindowFrameSource` implement `IFrameSource` để dùng khi WGC không capture được cross-desktop.

## Key Insights
- `IFrameSource` đã abstract → chỉ cần class mới, không sửa pipeline.
- `PrintWindow` với `PW_RENDERFULLCONTENT` (Windows 8.1+) capture được DX windows.
- Output BGRA giống WGC → vision detector không đổi.

## Requirements
- F1: Class `PrintWindowFrameSource : IFrameSource` cùng signature.
- F2: Config flag `capture.backend = "wgc" | "printwindow"`.
- F3: Throughput ≥20Hz cho 800x600 window (vision pipeline target).

## Architecture
```
IFrameSource (interface, existing)
├── WgcFrameSource (existing) — DWM-based, low overhead, same desktop only
└── PrintWindowFrameSource (NEW) — GDI-based, cross-desktop capable
    ├── BitBlt-style: CreateCompatibleDC, PrintWindow(hwnd, hdc, 0x2)
    ├── GetDIBits → BGRA buffer
    └── Reuse buffer giữa các frame (avoid alloc)
```

## Related Code Files
- Create: `src/capture/print-window-frame-source.h`
- Create: `src/capture/print-window-frame-source.cpp`
- Update: `src/capture/CMakeLists.txt` add sources
- Update: `src/config/config-loader.cpp` thêm `captureBackend` field
- Update: `src/main.cpp` chọn backend theo config

## Implementation Steps
1. Header `print-window-frame-source.h`:
   ```cpp
   class PrintWindowFrameSource : public IFrameSource {
   public:
       explicit PrintWindowFrameSource(HWND target);
       ~PrintWindowFrameSource() override;
       bool acquire(Frame& out, int timeoutMs) override;
       void setTarget(HWND h) override;
   private:
       HWND target_;
       HDC memDc_ = nullptr;
       HBITMAP memBmp_ = nullptr;
       int bmpW_ = 0, bmpH_ = 0;
       std::vector<uint8_t> buf_;
       uint64_t seq_ = 0;
       void ensureBuffer(int w, int h);
   };
   ```
2. Implement `acquire`:
   - `GetClientRect(target_, &rc)` → w, h.
   - `ensureBuffer(w, h)`: nếu khác kích thước cũ → `DeleteObject` + `CreateCompatibleBitmap`.
   - `PrintWindow(target_, memDc_, PW_RENDERFULLCONTENT)` (PW_RENDERFULLCONTENT = 0x2).
   - `GetDIBits` với BITMAPINFOHEADER (32bpp top-down) → `buf_`.
   - Fill `out.bgra` (cv::Mat header trỏ vào `buf_.data()`), `out.seq = ++seq_`.
   - Return true nếu PrintWindow succeed.
3. Config:
   - Thêm field `std::string captureBackend = "wgc"` trong AppConfig.
   - Loader: parse JSON `"capture": { "backend": "printwindow" }`.
4. Main:
   - `std::unique_ptr<IFrameSource> src;`
   - `if (cfg.captureBackend == "printwindow") src = std::make_unique<PrintWindowFrameSource>(hwnd); else src = std::make_unique<WgcFrameSource>(...);`
5. Test smoke: build + chạy với config printwindow trên PT foreground bình thường, verify HP/MP đọc đúng.

## Todo
- [ ] Tạo print-window-frame-source.{h,cpp}
- [ ] Update CMakeLists
- [ ] Thêm config field + parser
- [ ] Wire main.cpp chọn backend
- [ ] Smoke test compile + chạy
- [ ] Benchmark: capture latency 800x600 (target <30ms p99)

## Success Criteria
- Build pass MSVC v143.
- PT foreground + `captureBackend=printwindow` → vision đọc HP/MP giống WGC ±2%.
- Capture latency p99 < 30ms.

## Risk Assessment
- Risk: `PrintWindow` trả về black frame với DX game → mitigation: flag PW_RENDERFULLCONTENT đã handle 90% case; nếu vẫn black thì fallback DXGI Desktop Duplication (out-of-scope phase này).
- Risk: GDI handle leak → RAII trong destructor, có cleanup ở `ensureBuffer` khi resize.

## Security Considerations
- N/A — GDI calls thuần.

## Next Steps
- Phase 02 sẽ consume `PrintWindowFrameSource` từ worker thread trên BotDesk.
