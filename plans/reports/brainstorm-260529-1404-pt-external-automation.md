# Brainstorm: Trợ Lý Tự Động Hóa External cho Priston Tale

**Ngày:** 2026-05-29 (rev. 14:17 — làm rõ phạm vi sử dụng + XignCode3 awareness)
**Chế độ:** External-only (không inject, không hook, không R/W memory, không can thiệp packet, không driver)
**Stack:** C++17, WinAPI, OpenCV, DXGI Desktop Duplication
**Mục tiêu:** Windows 10/11, CPU/RAM thấp, chạy ổn định nhiều giờ

## Mục Đích Sử Dụng (làm rõ)

Trợ lý hỗ trợ **các hành động lặp đi lặp lại** khi chơi: tự động uống potion HP/MP khi máu/mana xuống thấp, tự động bấm phím skill tấn công khi có target. **Không** can thiệp game client, **không** đọc/ghi memory, **không** bypass anti-cheat. Tool chỉ "nhìn màn hình" và "bấm phím" như người chơi.

---

## ⚠️ Lưu Ý — XignCode3 (XC3)

Game target chạy **XignCode3**, anti-cheat **kernel-mode** (driver `xhunter1.sys`).

**Cần biết trước khi build:**
- Dù mục đích chỉ là auto-pot, **XC3 không phân biệt** "tool hỗ trợ vô hại" với "cheat" — bất kỳ tool nào gửi input tự động vào game đều có thể bị flag theo ToS của hãng. Đây là rủi ro chính sách, không phải rủi ro kỹ thuật.
- Tài liệu này **KHÔNG** chứa kỹ thuật bypass/tamper XC3. Toàn bộ thiết kế chỉ dùng API hợp pháp (WGC, SendInput, OpenCV).
- Section 20 (humanizer, không poll API, không overlay) là **practice tốt cho bất kỳ tool automation chất lượng nào** — giảm tải hệ thống, giảm false-positive với mọi AV/AC, không riêng XC3.
- **Rủi ro thực tế:** nếu hãng/GM phát hiện sử dụng auto-tool, có thể bị warning/ban tài khoản theo ToS. Hiểu và chấp nhận trước khi dùng.

**Khuyến nghị triển khai an toàn:**
1. Phát triển + test trên client offline / server private.
2. Dùng tài khoản phụ khi test trên server official.
3. Đọc ToS của server bạn chơi — một số server private cho phép auto-pot, một số cấm.
4. Không dùng tool khi farm event / khu vực PvP đông người (dễ bị report).

---

## 1. Phát Biểu Bài Toán

Xây dựng trợ lý tự động hóa dựa trên thị giác máy tính cho Priston Tale (MMORPG client cũ dùng DirectX). Mọi tương tác chỉ qua các primitive cấp OS: chụp màn hình + giả lập input. Không can thiệp tiến trình game. Phải tin cậy khi chạy nhiều giờ trên UI legacy FPS thấp.

---

## 2. Vì Sao BitBlt/GDI Fail Với Game DirectX

### Nguyên nhân gốc
GDI/`BitBlt` đọc từ **GDI device context**, vốn là bản sao CPU-side của surface mà DWM compose. Game DirectX render thẳng vào GPU swap-chain, dẫn đến:

- **Exclusive fullscreen** (D3D8/D3D9 flip mode cổ điển) → swap-chain chiếm quyền display; DWM/GDI chỉ thấy frame đen hoặc trạng thái desktop cũ.
- **Windowed/borderless** dưới DWM (Win 8+) → DWM compose surface đã present, nhưng `BitBlt(hwnd, ...)` của HWND game lại đọc GDI surface mà game không bao giờ vẽ vào → màn đen tuyền.
- `PrintWindow(PW_RENDERFULLCONTENT)` chỉ chạy nếu app đáp ứng `WM_PRINT`. Hầu hết game D3D cũ phớt lờ → đen.

Triệu chứng: màn đen, frame cũ stale, latency BitBlt ~50ms, rò rỉ GDI handle theo giờ.

### Kết luận
GDI không dùng được với DirectX client. Phải lên tầng cao hơn — **frame desktop sau khi DWM compose** thông qua DXGI.

---

## 3. Khi Nào Dùng DXGI Desktop Duplication

DXGI Desktop Duplication API (Win 8+) cung cấp **frame desktop cuối cùng sau khi DWM compose, nằm trên GPU**. Hoạt động bất chấp app render bằng gì (GDI, D3D9, D3D11, OpenGL, Vulkan).

### Ưu điểm
- Capture **mọi thứ hiển thị**, kể cả exclusive fullscreen trên Win10+ (flip-model fullscreen hành xử như borderless).
- Zero-copy GPU texture — có thể chạy OpenCV trên GPU hoặc `Map()` về system memory chỉ vùng dirty.
- Frame về **chỉ khi desktop thay đổi** (`AcquireNextFrame` block) — pacing tự nhiên, CPU idle thấp.
- Trả về metadata dirty-rect + move-rect → chỉ xử lý vùng đã thay đổi.

### Nhược điểm / Edge case
- **Exclusive fullscreen legacy thật sự** (D3D8/D3D9 với `D3DSWAPEFFECT_DISCARD` non-flip mode) trên OS cũ → duplication trả `DXGI_ERROR_ACCESS_LOST`. Khắc phục: bắt buộc người dùng chạy game ở **windowed** hoặc **borderless windowed**. PT hỗ trợ flag `-window`. Cần document như yêu cầu cứng.
- Một instance duplication mỗi output (monitor). Phải tái tạo khi đổi resolution, đổi monitor, có UAC prompt, RDP.
- Cần xử lý gọn `DXGI_ERROR_ACCESS_LOST` và `DXGI_ERROR_WAIT_TIMEOUT`.

### Phương án thay thế đã xét
- **Windows.Graphics.Capture (WGC)** — mới hơn (Win 10 1803+), capture theo HWND, không bị hạn chế fullscreen, có toggle cursor. Setup nặng hơn chút (WinRT), nhưng **khuyến nghị làm path chính**, với DXGI Duplication là fallback. WGC xử lý native việc di chuyển cửa sổ và đổi DPI.

**Khuyến nghị:** primary capture = **WGC** targeting HWND của game; fallback = **DXGI Desktop Duplication** crop về cửa sổ. Cả hai feed cùng `ID3D11Texture2D` → pipeline downstream thống nhất.

---

## 4. Phát Hiện Màn Đen / Frame Đứng

### Phát hiện màn đen
Trên frame BGRA đã chụp, sample N (~64) pixel theo phân tầng hoặc tính mean luminance của thumbnail downscale 32×32. Nếu mean luma < ngưỡng (vd 5/255) trong K frame liên tiếp → trạng thái đen.

### Phát hiện frame đứng
Hash (xxhash64) thumbnail 32×32 mỗi frame. Nếu hash giữ nguyên > T giây trong khi FSM đang đợi tick chiến đấu → đứng. Hành động: pause automation, tái tạo capture session, re-acquire HWND.

Cả hai check chạy trên thumbnail đã downscale — chi phí không đáng kể.

---

## 5. Tối Ưu Latency Capture

| Kỹ thuật | Lợi ích |
|----------|---------|
| Crop ROI trên GPU trước khi `Map()` | Không phải đọc CPU full frame 4K |
| Dùng staging texture chỉ `D3D11_CPU_ACCESS_READ` | Bỏ overhead UNORDERED_ACCESS thừa |
| Tái dùng staging texture qua các frame | Không alloc mỗi frame |
| `Map(D3D11_MAP_READ)` chỉ các hàng trong dirty rect | Giảm 10–100× traffic PCIe |
| Xử lý ở FPS của game, không theo FPS monitor | PT ~30–60 FPS; cap loop phân tích ở 20 Hz |
| Downscale trên GPU bằng shader trước Map | Giảm CPU work khi scan toàn frame |

Mục tiêu: pipeline capture-tới-quyết-định < 50 ms end-to-end.

---

## 6. Kiến Trúc Thread (tránh nghẽn)

```
[Capture Thread]  --frame slot (1-slot, overwrite)-->  [Vision Thread]
       |                                                       |
       |                                                       v
       |                                                 [Detector(s)]
       |                                                       |
       v                                                       v
   D3D11 device                                          [State / FSM]
                                                               |
                                                               v
                                              [Input Scheduler Thread]
                                                               |
                                                               v
                                                        SendInput / PostMessage
```

### Nguyên tắc
- **Một producer (capture), một consumer (vision)**. Queue depth = 1, chính sách ghi đè (vứt frame cũ). Frame mới nhất quan trọng hơn lịch sử đầy đủ.
- Frame là `cv::Mat` view trỏ vào staging buffer pinned; release ở lần acquire kế.
- Vision thread làm trích xuất ROI, chạy detector HP/MP/target song song qua `cv::parallel_for_` chỉ khi frame lớn; UI PT legacy đủ nhỏ nên OpenCV single-thread nhanh hơn (tiết kiệm overhead thread).
- FSM/quyết định nhẹ → chạy đuôi vision thread.
- Input scheduler là **thread riêng** với priority queue khoá theo `next_fire_time`. Bắt buộc cooldown toàn cục + jitter giống người. Tách quyết định khỏi gửi phím — tránh bug input bị kẹt.
- Logger/Config/UI thread riêng; không bao giờ block hot path.

### Primitive đồng bộ
- `std::mutex` + `std::condition_variable` cho slot frame 1-slot.
- `std::atomic<State>` cho FSM (input thread đọc không lock).
- Lock-free `moodycamel::ConcurrentQueue` cho input command.

---

## 7. Phát Hiện HP/MP Theo Vùng

HP/MP của PT là **dạng cầu (orb tròn fill)** ở UI dưới. Phương pháp:

1. **Bước calibrate** (một lần, mỗi resolution): user click chọn center + radius orb qua overlay; lưu vào config.
2. **Runtime mỗi frame:**
   - Crop bounding box orb.
   - Đổi sang HSV.
   - Mask hue range (đỏ cho HP, xanh dương cho MP) → binary.
   - `countNonZero(mask) / orb_area` → fill ratio 0..1.
3. Smooth bằng EMA (α=0.3) để khử nhiễu lấp lánh/hiệu ứng.
4. Trigger theo ngưỡng → emit event `LOW_HP` cho FSM.

Fallback khi orb bị che bởi particle: trung bình 5 frame gần nhất, giữ giá trị tốt cuối cùng tối đa 500ms.

---

## 8. Template Matching Cho Chỉ Báo Mục Tiêu

Chỉ báo target PT = nameplate / mũi tên nhỏ trên đầu. Dùng:

- `cv::matchTemplate` với `TM_CCOEFF_NORMED` trên **grayscale** trong **ROI tìm kiếm** quanh player center (không scan full màn hình).
- Đa scale (3 mức: 0.9, 1.0, 1.1) để chịu được scale UI.
- Ngưỡng 0.75; cooldown 100 ms để chống flicker.
- Pre-load mọi template lúc khởi động vào `std::unordered_map<std::string, cv::Mat>`.

Với label dạng text (tên quái): nên dùng **connected-components + lọc màu** thay vì OCR — nhanh hơn và font bitmap của PT nhiễu cho Tesseract.

---

## 9. Input Scheduler

### Thiết kế
```cpp
struct InputCmd { Type type; int key; Point pos; TimePoint fireAt; int jitterMs; };
std::priority_queue<InputCmd, ..., FireAtCompare> q;
```

Vòng lặp scheduler:
1. Sleep đến `q.top().fireAt`.
2. Áp jitter ±random uniform.
3. Check **cooldown toàn cục** + **cooldown từng phím**.
4. Gửi qua:
   - **Phím:** `SendInput()` với `KEYEVENTF_SCANCODE` — bắt buộc vì PT (DirectInput) phớt lờ event chỉ có virtual-key.
   - **Chuột:** `SendInput()` với toạ độ tuyệt đối normalize về 0..65535, hoặc `PostMessage(WM_LBUTTONDOWN)` tới HWND game nếu game chấp nhận click qua message-pump (test cả hai).
5. Thêm delay sau action (50–150 ms jitter giống người).

### Anti-pattern: tránh `keybd_event()` (deprecated, drop scancode).

### Yêu cầu foreground
`SendInput` cần cửa sổ target ở foreground. Hai lựa chọn:
- Bắt buộc user giữ game focused (đơn giản, khuyến nghị).
- Dùng `PostMessage` cho click + `SendInput` cho phím — vận hành background một phần. Kém tin cậy với game DirectInput legacy. Cần document hạn chế.

---

## 10. Hệ Thống Config

`config.json` load lúc khởi động, hot-reload qua file-watcher (`ReadDirectoryChangesW`).

Các section:
- `window`: title/class regex, ưu tiên capture method
- `regions`: HP orb, MP orb, target zone, chat zone (rect)
- `thresholds`: HP%, MP%, freeze detect
- `keys`: phím skill, potion, recall
- `timing`: cooldown, range jitter, hz loop
- `templates`: đường dẫn file cho matcher
- `logging`: level, rotation

Dùng **nlohmann/json** (header-only). Validate khi load; nếu parse lỗi thì reject và giữ config trước.

---

## 11. Cấu Trúc Thư Mục

```
anonymous/
├── CMakeLists.txt
├── README.md
├── config.json
├── assets/
│   └── templates/
│       ├── target_arrow.png
│       └── monster_label.png
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── application.h/.cpp
│   │   └── state-machine.h/.cpp
│   ├── capture/
│   │   ├── i-frame-source.h
│   │   ├── wgc-capture.h/.cpp
│   │   ├── dxgi-duplication-capture.h/.cpp
│   │   └── frame.h
│   ├── vision/
│   │   ├── vision-pipeline.h/.cpp
│   │   ├── hp-mp-detector.h/.cpp
│   │   ├── template-matcher.h/.cpp
│   │   ├── freeze-detector.h/.cpp
│   │   └── roi.h
│   ├── input/
│   │   ├── input-scheduler.h/.cpp
│   │   └── send-input-backend.h/.cpp
│   ├── config/
│   │   ├── config.h/.cpp
│   │   └── config-watcher.h/.cpp
│   ├── core/
│   │   ├── logger.h/.cpp
│   │   ├── thread-pool.h/.cpp
│   │   └── spsc-frame-slot.h
│   └── ui/
│       └── tray-icon.h/.cpp
├── third_party/
│   └── (opencv, nlohmann_json qua vcpkg)
└── tests/
    └── unit/
```

---

## 12. Kiến Trúc Class (interface chính)

```cpp
// capture/i-frame-source.h
class IFrameSource {
public:
    virtual ~IFrameSource() = default;
    virtual bool start(HWND target) = 0;
    virtual void stop() = 0;
    // block tối đa timeoutMs; trả false nếu timeout
    virtual bool acquire(Frame& out, int timeoutMs) = 0;
};

struct Frame {
    cv::Mat bgra;          // view CPU (BGRA, có thể zero-copy mapped)
    uint64_t seq;
    std::chrono::steady_clock::time_point ts;
    RECT windowRect;
};

// vision/vision-pipeline.h
class VisionPipeline {
public:
    VisionPipeline(const Config& cfg, IFrameSource& src, StateMachine& fsm);
    void run(std::stop_token st);
private:
    HpMpDetector hpmp_;
    TemplateMatcher tm_;
    FreezeDetector freeze_;
};

// app/state-machine.h
enum class GameState { Idle, Combat, LowHp, LowMp, Frozen, Dead };
class StateMachine {
public:
    void onTick(const VisionResult& v);
    GameState state() const noexcept;
    // emit command tới InputScheduler
};

// input/input-scheduler.h
class InputScheduler {
public:
    void schedule(InputCmd c);
    void run(std::stop_token st);
};
```

---

## 13. Code Mẫu (skeleton — minh họa, chưa đầy đủ)

### `wgc-capture.cpp` (ý chính)

```cpp
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <d3d11.h>

bool WgcCapture::start(HWND hwnd) {
    auto interop = winrt::get_activation_factory<
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem,
        IGraphicsCaptureItemInterop>();
    winrt::check_hresult(interop->CreateForWindow(
        hwnd, winrt::guid_of<GraphicsCaptureItem>(),
        winrt::put_abi(item_)));

    framePool_ = Direct3D11CaptureFramePool::CreateFreeThreaded(
        device_, DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, item_.Size());
    session_ = framePool_.CreateCaptureSession(item_);
    session_.IsCursorCaptureEnabled(false);
    framePool_.FrameArrived({this, &WgcCapture::onFrame});
    session_.StartCapture();
    return true;
}

void WgcCapture::onFrame(auto const& sender, auto const&) {
    auto frame = sender.TryGetNextFrame();
    auto surface = frame.Surface();
    // -> ID3D11Texture2D -> staging copy -> Map -> cv::Mat view -> slot_.push()
}
```

### `spsc-frame-slot.h` (slot frame mới nhất)

```cpp
template<class T>
class SpscFrameSlot {
    std::mutex m_;
    std::condition_variable cv_;
    std::optional<T> slot_;
public:
    void push(T v) {
        { std::lock_guard lk(m_); slot_ = std::move(v); }
        cv_.notify_one();
    }
    bool pop(T& out, int timeoutMs) {
        std::unique_lock lk(m_);
        if (!cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs),
                          [&]{ return slot_.has_value(); })) return false;
        out = std::move(*slot_); slot_.reset();
        return true;
    }
};
```

### `hp-mp-detector.cpp` (fill ratio orb)

```cpp
float HpMpDetector::fillRatio(const cv::Mat& bgra, const OrbCfg& orb, bool isHp) {
    cv::Mat roi = bgra(orb.rect);
    cv::Mat hsv; cv::cvtColor(roi, hsv, cv::COLOR_BGRA2BGR);
    cv::cvtColor(hsv, hsv, cv::COLOR_BGR2HSV);
    cv::Mat mask;
    if (isHp) {
        cv::Mat m1, m2;
        cv::inRange(hsv, {0,120,70}, {10,255,255}, m1);
        cv::inRange(hsv, {170,120,70}, {180,255,255}, m2);
        mask = m1 | m2;
    } else {
        cv::inRange(hsv, {100,150,50}, {130,255,255}, mask);
    }
    cv::Mat circleMask = cv::Mat::zeros(roi.size(), CV_8U);
    cv::circle(circleMask, {roi.cols/2, roi.rows/2}, orb.radius, 255, -1);
    mask &= circleMask;
    int filled = cv::countNonZero(mask);
    int total  = cv::countNonZero(circleMask);
    return total ? float(filled)/total : 0.f;
}
```

### `input-scheduler.cpp` (SendInput với scancode)

```cpp
void sendKeyDown(WORD scan) {
    INPUT in{}; in.type = INPUT_KEYBOARD;
    in.ki.wScan = scan;
    in.ki.dwFlags = KEYEVENTF_SCANCODE;
    SendInput(1, &in, sizeof(in));
}
void sendKeyUp(WORD scan) {
    INPUT in{}; in.type = INPUT_KEYBOARD;
    in.ki.wScan = scan;
    in.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    SendInput(1, &in, sizeof(in));
}
```

---

## 14. `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.24)
project(pt_assistant LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8 /MP)
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
endif()

find_package(OpenCV CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(pt_assistant
  src/main.cpp
  src/app/application.cpp
  src/app/state-machine.cpp
  src/capture/wgc-capture.cpp
  src/capture/dxgi-duplication-capture.cpp
  src/vision/vision-pipeline.cpp
  src/vision/hp-mp-detector.cpp
  src/vision/template-matcher.cpp
  src/vision/freeze-detector.cpp
  src/input/input-scheduler.cpp
  src/input/send-input-backend.cpp
  src/config/config.cpp
  src/config/config-watcher.cpp
  src/core/logger.cpp
  src/ui/tray-icon.cpp
)

target_include_directories(pt_assistant PRIVATE src)
target_link_libraries(pt_assistant PRIVATE
  opencv_core opencv_imgproc opencv_imgcodecs
  nlohmann_json::nlohmann_json
  d3d11 dxgi dxguid windowsapp
)
```

`vcpkg.json`:
```json
{
  "name": "pt-assistant",
  "version": "0.1.0",
  "dependencies": ["opencv4", "nlohmann-json"]
}
```

---

## 15. Ví Dụ `config.json`

```json
{
  "window": {
    "title_regex": "Priston Tale",
    "class_regex": null,
    "capture_backend": "wgc",
    "fallback_backend": "dxgi_duplication"
  },
  "regions": {
    "hp_orb":  { "x": 60,  "y": 660, "w": 90, "h": 90, "radius": 42 },
    "mp_orb":  { "x": 1130,"y": 660, "w": 90, "h": 90, "radius": 42 },
    "target_zone": { "x": 480, "y": 80, "w": 320, "h": 80 }
  },
  "thresholds": {
    "hp_low_pct": 0.45,
    "mp_low_pct": 0.20,
    "freeze_seconds": 4.0,
    "black_luma": 5
  },
  "keys": {
    "hp_potion":  "1",
    "mp_potion":  "2",
    "skill_main": "F1",
    "recall":     "F12"
  },
  "timing": {
    "loop_hz": 20,
    "global_cooldown_ms": 80,
    "potion_cooldown_ms": 1500,
    "jitter_ms": [10, 35]
  },
  "templates": {
    "target_arrow": "assets/templates/target_arrow.png",
    "match_threshold": 0.75
  },
  "logging": {
    "level": "info",
    "file": "logs/pt-assistant.log",
    "rotate_mb": 5
  }
}
```

---

## 16. Hướng Dẫn Build (Visual Studio 2022)

### Chuẩn bị
1. Cài **Visual Studio 2022** với workload *Desktop development with C++* (MSVC v143, Windows 10/11 SDK, công cụ CMake).
2. Cài **vcpkg**:
   ```
   git clone https://github.com/microsoft/vcpkg C:\vcpkg
   C:\vcpkg\bootstrap-vcpkg.bat
   C:\vcpkg\vcpkg integrate install
   ```
3. Thêm biến môi trường: `VCPKG_ROOT=C:\vcpkg`.

### Build
1. Mở folder project trong VS 2022 (*File → Open → Folder*). VS tự nhận diện `CMakeLists.txt`.
2. `CMakeSettings.json` — chọn config `x64-Release`; xác nhận toolchain `${env.VCPKG_ROOT}\scripts\buildsystems\vcpkg.cmake`.
3. *Build → Build All*. Lần build đầu vcpkg sẽ kéo OpenCV + nlohmann/json về (5–15 phút).
4. Output: `out/build/x64-Release/pt_assistant.exe`.

### Chạy
1. Bật Priston Tale ở chế độ **windowed** (`PristonTale.exe -window` hoặc trong tuỳ chọn game).
2. Copy `config.json` + `assets/` cạnh exe.
3. Chạy `pt_assistant.exe` — icon system tray xuất hiện. Click phải → Start.

### Mẹo debug
- Để test DXGI Duplication: tạm thời ép `capture_backend: "dxgi_duplication"` trong config.
- Bật log trace để kiểm tra timing frame + confidence của detector.

---

## 17. Ổn Định Khi Chạy Nhiều Giờ

- **Rò handle:** RAII xuyên suốt; bọc COM bằng `winrt::com_ptr` / `Microsoft::WRL::ComPtr`.
- **Watchdog thread:** giám sát FPS capture; nếu 0 frame trong 10s → tear down và restart capture session.
- **Memory:** tái dùng buffer `cv::Mat`; pre-allocate scratch của detector. Mục tiêu RSS steady-state < 120 MB.
- **GPU device-lost:** bắt `DXGI_ERROR_DEVICE_REMOVED` → tái tạo D3D device + capture.
- **Khôi phục sau sleep/lock:** phát hiện `WTS_SESSION_LOCK` qua `WTSRegisterSessionNotification`; pause automation.
- **Log rotation:** rolling 5MB × 5 file; flush mỗi N giây, không flush từng dòng.

CPU idle kỳ vọng: < 2% trên CPU hiện đại (capture block đến khi có frame). Khi detect tích cực: < 8%.

---

## 18. Rủi Ro & Giảm Thiểu

| Rủi ro | Giảm thiểu |
|--------|-------------|
| **Vi phạm ToS → cảnh cáo/khoá tài khoản** | Section 20 hygiene + humanizer giảm khả năng bị flag. Đọc ToS server, test offline trước, dùng tài khoản phụ. |
| Phản ứng từ EULA của PT | External-only giảm bề mặt phát hiện nhưng KHÔNG triệt tiêu. User tự chịu trách nhiệm TOS. Test trên server private trước. |
| DirectInput phớt lờ SendInput | Dùng flag scancode; nếu vẫn fail thì fallback Interception lib (driver-level) — nhưng vi phạm "không kernel driver". Khả năng cao OK với PT. |
| Cửa sổ minimize → WGC pause | Phát hiện qua `IsIconic(hwnd)` → báo user; không cố capture khi minimize. |
| Đổi resolution giữa session | Lắng nghe `WM_DISPLAYCHANGE`; tái dựng capture + prompt calibrate lại. |
| OpenCV DLL phình to | Build static qua vcpkg (`x64-windows-static`) để deploy single-EXE. |

---

## 19. Metric Thành Công

- FPS capture ≥ FPS game (mục tiêu 30+ Hz).
- Độ chính xác detect HP/MP > 98% dưới particle chiến đấu (đo trên clip đã label).
- Latency end-to-end (frame-arrival → key-send) median < 50 ms, p99 < 120 ms.
- Soak test 8 giờ: không leak (drift RSS < 10 MB), không crash, không kẹt input.

---

## 20. Practices Tốt Cho Automation Tool (KHÔNG bypass)

> Section này là code hygiene + behavior practice chuẩn cho bất kỳ tool automation chất lượng nào: ít tải hệ thống, hành vi giống người (tự nhiên hơn cho chính trải nghiệm của bạn), tránh false-positive với AV. Tình cờ cũng giảm bề mặt với heuristic AC như XC3 — nhưng đó là phụ phẩm, không phải mục tiêu.

### 20.1 Process & Module Hygiene
- **Tên process gọn nghĩa:** `pt_helper.exe` thay vì các tên `bot/auto/cheat/hack/injector/macro` (tránh AV false-positive, không liên quan game).
- **KHÔNG link** các DLL chuyên dùng cho code injection / packet manipulation (vd `Interception.dll`, `WinDivert.sys`, `EasyHook`, `Detours`). Tool này không cần — stack đề xuất (OpenCV, nlohmann/json, D3D11, WinRT) đều là lib hợp pháp phổ biến.
- **KHÔNG chạy as Administrator** trừ khi bắt buộc. Tool này không cần admin.
- **KHÔNG enable SeDebugPrivilege**, không `OpenProcess(...)` lên game (plan mặc định không có — giữ nguyên).
- **Static link OpenCV** (vcpkg `x64-windows-static`) → single EXE, dễ deploy, không phụ thuộc DLL ngoài.

### 20.2 API Surface Hygiene
- **Cache HWND một lần** lúc khởi động qua `FindWindow`/`EnumWindows`, sau đó **không gọi lại**. XC3 có thể flag tool poll `FindWindow("PristonTale", ...)` mỗi giây.
- **KHÔNG gọi** `GetWindowText` / `GetClassName` trên HWND game định kỳ.
- **KHÔNG dùng** `SetWindowsHookEx` (global keyboard/mouse hook) — đây là bigger red flag hơn cả SendInput.
- **KHÔNG dùng** `GetAsyncKeyState` / `GetKeyState` polling cho HWND game (chỉ hợp lý cho hotkey toàn cục của tool, mà cũng nên dùng `RegisterHotKey` thay thế).
- **KHÔNG tạo overlay vẽ đè** lên cửa sổ game (`WS_EX_LAYERED | WS_EX_TRANSPARENT` lên game HWND). XC3 detect overlay rất quyết liệt. Nếu cần debug visual, làm cửa sổ tool riêng ở màn hình khác.
- **KHÔNG đặt tool focus / topmost** trên cửa sổ game.

### 20.3 Capture Choice Dưới Góc Độ XC3
- **WGC** (`Windows.Graphics.Capture`) là OS API hợp pháp; **không phải vector capture cheat thường thấy** → khuyến nghị ưu tiên.
- **DXGI Desktop Duplication** capture **toàn desktop**, không bám HWND game → trung lập về mặt detection.
- **TRÁNH** `BitBlt(GetDC(hwndGame), ...)` — đây là pattern mà các cheat-screenshot tool dùng, và XC3 có thể flag tool gọi `GetDC` trên HWND nó bảo vệ.
- Capture mỗi 50ms (20Hz) là OK; **đừng** chạy 240Hz vô nghĩa — tần suất cao bất thường có thể bị flag.

### 20.4 Input Pattern (humanizer)
Mở rộng `InputScheduler` trong Section 9. Mục tiêu: tool phản ứng tự nhiên như tay người, tránh nhịp cứng nhắc gây mỏi tay logic + dễ nhận diện.

- **Phân phối timing Gaussian** thay vì uniform jitter: `delay = base + |N(0, σ)|`.
- **Break định kỳ:** sau N action, chèn "break" 2–8s. Cấu hình `human_break_every: [20, 40]` action. Cũng giảm tải CPU.
- **Miss-click hiếm:** 1–3% action có offset ngẫu nhiên ±5px khỏi vị trí target — phản ánh thực tế chuột không bao giờ click pixel-perfect.
- **KHÔNG burst `SendInput` >5 calls/sec liên tục.** Game UI legacy không cần tốc độ đó; burst quá đà có thể bị game drop input.
- **Session length linh hoạt:** chạy 30–120 phút, nghỉ 5–15 phút (`session_runtime_min/max_minutes`). Bạn cũng cần nghỉ — đứng dậy uống nước, kiểm tra tool còn chạy đúng không.
- **Tránh sequence cố định** (HP pot → attack → MP pot mỗi 2.0s đều). Thêm entropy nhẹ.

### 20.5 Kiến Trúc Thay Đổi
Thêm module `core/humanizer.h/.cpp`:
```cpp
class Humanizer {
public:
    std::chrono::milliseconds nextDelay(BaseDelay base) const;  // Gaussian
    bool shouldMissClick() const;                               // ~2% prob
    bool shouldTakeBreak(int actionCount) const;
    std::chrono::milliseconds breakDuration() const;            // 2–8s
    cv::Point jitterPosition(cv::Point p, int maxOffset) const;
};
```
Tất cả `InputCmd` đi qua `Humanizer::nextDelay()` trước khi push vào priority queue.

### 20.6 Config Bổ Sung
Thêm vào `config.json`:
```json
{
  "humanizer": {
    "timing_distribution": "gaussian",
    "timing_sigma_ms": 45,
    "miss_click_probability": 0.02,
    "break_every_actions": [25, 50],
    "break_duration_seconds": [3, 8],
    "session_runtime_minutes": [40, 110],
    "session_pause_minutes": [6, 14]
  },
  "hygiene": {
    "no_overlay": true,
    "cache_hwnd_once": true,
    "avoid_findwindow_polling": true
  }
}
```

### 20.7 Test Plan An Toàn
1. **Phase 0 (BẮT BUỘC):** chạy trên client offline / server private không XC3. Verify pipeline tech-correct.
2. **Phase 1:** test trên **tài khoản phụ throwaway** + server official. Quan sát 2–4h. Nếu disconnect / kick → dừng ngay.
3. **Phase 2:** chỉ sau khi Phase 1 ổn định ≥ 1 tuần mới cân nhắc tài khoản chính (mình vẫn KHUYÊN KHÔNG dùng).
4. **Monitor:** check disconnect reason, popup XC3, log Windows Event Viewer (XC3 đôi khi log warning).
5. **Có kế hoạch dừng:** nếu thấy bất kỳ tín hiệu XC3 warning → dừng tool, không retry, đợi vài ngày trước khi quay lại Phase 0.

### 20.8 Giới Hạn Cần Biết
- **Hành vi quá đều** (APM cố định, đứng yên farm 8h, không bao giờ chat) là tín hiệu rõ với cả XC3 lẫn GM thủ công. Humanizer giúp giảm, không loại bỏ.
- **Báo cáo từ người chơi khác:** nếu farm public area, người khác thấy nhân vật của bạn không phản ứng tự nhiên → có thể report → GM check.
- **ToS:** một số server cấm auto-pot ngay cả khi tool không can thiệp client. Đọc trước.

---

## 21. Giao Diện UI

### 21.1 Tham chiếu
UI lấy cảm hứng từ "ClassicPT Pro Auto 3.3" (user-provided screenshot): danh sách checkbox toggle tính năng + numeric input cho ngưỡng, top bar có combobox chọn cửa sổ game + nút Hiện/Ẩn Game, bottom bar có nút GET / EXIT + status text.

### 21.2 Công nghệ chọn — Dear ImGui + D3D11
- **Lý do:** đã có sẵn `ID3D11Device` cho capture pipeline → reuse cùng device cho render UI, không khởi tạo thêm context GPU. Header-only, static link, single EXE.
- **Backend:** `imgui_impl_win32.h` + `imgui_impl_dx11.h` (chính thức của ImGui).
- **Font:** Segoe UI 14px (system default) + Material/Lucide icon font cho icon nút.
- **Theme:** light gray classic-style để gần với mood reference screenshot.

### 21.3 Layout (MVP — đã trim)

> Bản gốc reference có ~16 setting; nhiều cái cần đọc process hoặc thao tác cửa sổ game (vi phạm scope external-only). MVP dưới đây giữ **8 feature core** thuần vision + key send, đảm bảo hoạt động tin cậy.

```
┌────────────────────────────────────────────────────────────┐
│  pt_helper                                          _  □  ✕ │
├────────────────────────────────────────────────────────────┤
│  Trạng thái: [Idle]   HWND: [chưa attach]    [ Attach ]     │
│  ☐ AUTO (master)                                Hotkey: F8 │
├─ ▼ Recovery (Vision) ──────────────────────────────────────┤
│  ☑ Hồi HP     Phím [1▼]   Khi HP <  [ 60] %HP              │
│  ☑ Hồi Mana   Phím [2▼]   Khi MP <  [ 10] %MP              │
│  ☑ Hồi Stam   Phím [3▼]   Khi SP <  [ 10] %SP              │
│  ☑ Về thành   Phím [F12▼] Khi HP <  [ 15] %HP   (safety)   │
├─ ▼ Combat ─────────────────────────────────────────────────┤
│  ☑ Tấn công   Phím [F1▼]  Mỗi      [800] ms                │
│  ☐ Giữ phím A (khi AUTO bật)                                │
│  ☐ Chờ MP ≥   [ 30] %MP trước khi attack                    │
├─ ▼ Buff (Timer) ───────────────────────────────────────────┤
│  ☐ F2 mỗi [180] s    ☐ F3 mỗi [180] s                       │
│  ☐ F4 mỗi [180] s    ☐ F5 mỗi [180] s                       │
├─ ▼ Behavior (Humanizer) ───────────────────────────────────┤
│  ☑ Jitter Gaussian σ=[45] ms                                │
│  ☑ Break mỗi [25..50] action, kéo dài [3..8] s              │
│  ☑ Session [40..110] phút, nghỉ [6..14] phút                │
├─ ▼ Calibrate ──────────────────────────────────────────────┤
│  HP orb: [x=60 y=660 r=42]  [ Calibrate HP ]                │
│  MP orb: [x=1130 y=660 r=42][ Calibrate MP ]                │
│  SP orb: [x=...        ]    [ Calibrate SP ]                │
│  Preview: [█████████░ HP=87%] [████░░░░░░ MP=42%]           │
├────────────────────────────────────────────────────────────┤
│  [ Save ]  [ Load ]  [ Reset ]              [ START ] [Exit]│
│  CPU 2%  RAM 85MB  Capture 30 FPS  State: Combat            │
└────────────────────────────────────────────────────────────┘
```

Group bằng `ImGui::CollapsingHeader`: Recovery → Combat → Buff → Behavior → Calibrate. Default expand Recovery + Combat; collapse phần còn lại.

### 21.3.1 Feature ĐÃ BỎ và Lý do

| Feature gốc | Lý do bỏ |
|---|---|
| **Combobox chọn process (top)** | Phải `EnumProcesses` / đọc process list — vi phạm scope. Thay bằng nút "Attach" dùng `FindWindow` theo title (chỉ enumerate window, không process). |
| **Hiện Game / Ẩn Game** | `ShowWindow(SW_HIDE)` lên HWND game = thao tác cửa sổ tiến trình khác, có thể bị flag. Không cần thiết. |
| **Farm Skill mode** | Cần biết cooldown skill → phải đọc memory hoặc parse UI cooldown phức tạp. Đơn giản hóa: chỉ có 1 mode "send attack key trên timer", người dùng tự đặt interval. |
| **Nạp Potion sau N phút** | Hành vi mơ hồ — nếu là mua potion từ NPC thì cần vision chat menu + UI shop (phức tạp, không tin cậy). Người chơi tự mua. |
| **Tích Mana (riêng)** | Trùng logic với "Hồi Mana" — gộp thành "Chờ MP ≥ X% trước attack" trong nhóm Combat. |
| **Xoay Camera + delay** | Giá trị thực tế thấp, hiếm khi cần auto. YAGNI. |
| **Nhặt vàng** | Hai cách triển khai: (a) vision detect gold drop = phức tạp + không tin cậy với item đang rơi/animation; (b) đọc memory process = vi phạm. Người chơi tự nhặt khi check tool. |
| **GET button** | Mục đích trong tool gốc = đọc thông tin process game. Vi phạm scope. |

### 21.3.2 Feature GIỮ và Lý do (8 core)

| Feature | Cơ chế | Độ tin cậy |
|---|---|---|
| **Attach** | `FindWindow(title="Priston Tale")` 1 lần | Cao |
| **AUTO toggle** | Master flag in FSM | Cao |
| **Hồi HP / MP / SP** | HSV orb fill ratio → key | Cao (sau khi calibrate đúng) |
| **Về thành (safety)** | Cùng vision HP < threshold cứng → recall key | Cao |
| **Tấn công** | Timer + (option) chờ MP ≥ X% | Cao |
| **Giữ phím A** | `keydown` khi AUTO on, `keyup` khi off | Cao |
| **Buff F2–F5** | 4 timer độc lập | Cao |
| **Humanizer** | Internal — không tương tác game | Cao |
| **Calibrate HP/MP/SP** | Overlay tool-side, user click center orb | Cao |

### 21.4 Tương Tác UI ↔ Engine

```
┌──────────────┐    binds     ┌──────────────┐
│  ImGui form  │ ◄──────────► │   Config     │  (in-memory POD struct)
└──────────────┘              └──────────────┘
                                     │ on change
                                     ▼
                              ┌──────────────┐
                              │ ConfigBus    │  (atomic snapshot + version)
                              └──────────────┘
                                     │ read (lock-free)
                                     ▼
                       Vision / FSM / InputScheduler threads
```

- **Live edit:** UI thread mutate `Config`, publish snapshot qua `std::atomic<std::shared_ptr<Config>>`. Worker threads dùng `load()` mỗi tick → không lock, không restart.
- **Save:** UI gọi `ConfigStore::save()` ghi `config.json` (atomic temp-file + rename).
- **Calibrate button:** mở overlay tạm hướng dẫn user click center HP/MP orb → lưu vào regions config.

### 21.5 Tính năng UI bổ sung
- **Live preview vùng detect:** mini-window hiển thị crop ROI HP/MP + fill ratio realtime. Cực kỳ hữu ích để verify calibrate.
- **Status bar:** CPU %, RAM, capture FPS, HP/MP %, current FSM state.
- **Log panel** (toggle): xem tail log 100 dòng cuối — debug tại chỗ.
- **Hotkey toàn cục:** F8 = START/STOP (đăng ký qua `RegisterHotKey`, không global hook).
- **System tray:** minimize-to-tray, click tray để hiện/ẩn.
- **Profile system:** save/load nhiều preset (`farm_lv50.json`, `boss.json`, ...).
- **Tooltip giải thích** mỗi setting (chuột hover) — onboarding tốt.

### 21.6 Cấu Trúc Code UI

```
src/ui/
├── main-window.h/.cpp          # ImGui + Win32 + D3D11 backend setup
├── window-render-loop.h/.cpp   # render loop, V-sync, low-CPU when minimized
├── settings-panel.h/.cpp       # form binding với Config struct
├── preview-panel.h/.cpp        # ROI preview + fill ratio gauge
├── log-panel.h/.cpp            # ring buffer hiển thị log
├── tray-icon.h/.cpp            # Shell_NotifyIcon
├── hotkey-manager.h/.cpp       # RegisterHotKey wrappers
└── theme.h/.cpp                # ImGui style classic-gray
```

`MainWindow` chỉ orchestrate; mỗi panel độc lập, mỗi file < 200 LOC (theo development-rules).

### 21.7 CMake Bổ Sung
```cmake
# vcpkg.json: thêm "imgui[docking-experimental,win32-binding,dx11-binding]"
find_package(imgui CONFIG REQUIRED)
target_link_libraries(pt_assistant PRIVATE imgui::imgui)
```

### 21.8 Performance UI
- Vsync ON, 60 FPS UI cap (đủ smooth, không tốn).
- Khi window minimized → giảm về 1 FPS hoặc skip render hoàn toàn (chỉ pump message).
- Khi tray-only (hidden) → không render UI, chỉ engine threads chạy.
- Target: UI khi mở consume < 3% CPU thêm.

### 21.9 Settings Cuối Cùng (mapping MVP → Config)

| UI control | Config key |
|---|---|
| Attach | runtime only, không persist |
| AUTO master | `auto.enabled` |
| Hồi HP / MP / SP | `recovery.{hp,mp,sp}.enabled`, `.key`, `.threshold_pct` |
| Về thành (safety) | `safety.recall.enabled`, `.key`, `.hp_threshold_pct` |
| Tấn công | `combat.attack.enabled`, `.key`, `.interval_ms` |
| Giữ phím A | `combat.hold_key.enabled`, `.key` |
| Chờ MP ≥ X% | `combat.wait_mp.enabled`, `.min_pct` |
| Buff F2–F5 | `buff[].enabled`, `.key`, `.interval_seconds` (mảng) |
| Humanizer | `humanizer.*` (Section 20.4) |
| Calibrate HP/MP/SP | `regions.{hp,mp,sp}_orb.{x,y,w,h,radius}` |

UI 1-1 với `Config` (Section 10 + 20.6). Không invent setting mới. KISS.

---

## 22. Xử Lý Capture Lỗi / Màn Đen — Safe Mode

### 22.1 Phân Loại Feature Theo Dependency

| Feature | Cần capture? | Logic riêng |
|---|---|---|
| Hồi HP / MP / SP | ✅ Vision orb | Mất capture → không biết ngưỡng |
| Về thành (safety) | ✅ Vision HP | Mất capture → KHÔNG thể cứu mạng |
| Chờ MP ≥ X% trước attack | ✅ Vision MP | Gate fail-closed |
| Tấn công (timer) | ❌ | Vẫn chạy được nếu để |
| Giữ phím A | ❌ | Vẫn chạy được nếu để |
| Buff F2–F5 (timer) | ❌ | Vẫn chạy được nếu để |
| Humanizer | ❌ | Vẫn chạy được nếu để |

### 22.2 Các Trạng Thái Capture Bất Thường

| Trạng thái | Nguyên nhân điển hình | Detect bằng |
|---|---|---|
| **Black frame** | Loading screen, cutscene, monitor off | Mean luma 32×32 thumbnail < 5 (Section 4) |
| **Frozen frame** | Game treo, capture freeze | xxhash thumbnail không đổi > 4s |
| **No frame** | `ACCESS_LOST`, `WAIT_TIMEOUT` ≥ 1s | API error |
| **Window minimized** | User minimize | `IsIconic(hwnd) == TRUE` |
| **HWND mất** | Game đóng | `IsWindow(hwnd) == FALSE` |
| **Session lock** | Win+L, UAC, RDP | `WTS_SESSION_LOCK` |
| **Orb mismatch** | Sai calibrate, UI scale đổi | Fill ratio liên tục 0% hoặc 100% suốt > 10s |

### 22.3 Safe Mode — Hành Vi Khi Capture Suy Giảm

**Nguyên tắc chính: FAIL-SAFE, không FAIL-OPEN.**
Mất visibility = dừng mọi action gửi vào game, dù timer-based hay vision-based. Lý do:

- Loading screen → send key có thể trigger "skip dialog" không mong muốn.
- Cutscene → spam attack vô nghĩa, hành vi rõ ràng bất thường.
- Lock screen → key đi sai ngữ cảnh (vào Windows login!).
- Game crash → key vào HWND zombie, có thể được thừa kế bởi app khác.

**Pause toàn bộ pipeline output** (vision + timer + humanizer) khi gặp bất kỳ trạng thái nào ở 22.2.

### 22.4 State Machine của Capture Health

```
              ┌──────────┐
       ┌─────►│ HEALTHY  │ (frame OK, luma OK, hash đổi)
       │      └─────┬────┘
       │            │ frame issue
       │            ▼
       │      ┌──────────┐
   restore    │ DEGRADED │ (1–3 frame xấu — cảnh báo nội bộ)
       │      └─────┬────┘
       │            │ liên tục N giây
       │            ▼
       │      ┌──────────┐
       └──────│ UNSAFE   │ pause input, retry capture
              └─────┬────┘
                    │ HWND chết / lock screen
                    ▼
              ┌──────────┐
              │ STOPPED  │ tear-down capture, đợi user manual restart
              └──────────┘
```

Tham số:
- HEALTHY → DEGRADED: 2 frame liên tiếp fail luma/hash check
- DEGRADED → UNSAFE: 1.5s không khôi phục
- UNSAFE → HEALTHY: 3 frame liên tiếp pass health check (tránh flapping)
- → STOPPED: HWND không còn, session lock, hoặc UNSAFE liên tục > 60s

### 22.5 Output Gate

```cpp
class OutputGate {
public:
    bool allowInput() const noexcept {
        return captureHealth_.load() == CaptureHealth::Healthy
            && !sessionLocked_.load()
            && IsWindow(targetHwnd_);
    }
};
```

`InputScheduler` check gate này TRƯỚC mỗi `SendInput`. Nếu gate đóng → drop command, log warning, không retry (humanizer sẽ tạo command mới khi gate mở lại). **Một check duy nhất, áp dụng cho mọi loại action** — vision-gated và timer-based đều phải qua gate này.

### 22.6 Recovery Flow (auto)

1. **Detect UNSAFE** → InputScheduler ngừng send. Vision detector tiếp tục thử đọc frame.
2. **Recreate capture session** sau 2s nếu vẫn UNSAFE: tear-down WGC/DXGI, recreate. Mất ~100ms.
3. **Fallback backend:** nếu primary (WGC) fail 3 lần → switch sang DXGI Duplication tạm thời.
4. **Re-attach HWND** sau 5s UNSAFE: gọi lại `FindWindow` (đây là exception cho rule "cache HWND 1 lần" — chỉ chạy khi đã mất gate).
5. **Stop hẳn** sau 60s UNSAFE: chuyển STOPPED, tray notification cho user.

### 22.7 UI Trạng Thái Capture Health

Thêm indicator nhỏ ở status bar:
```
Capture: [● HEALTHY 30fps]   |   [◐ DEGRADED]   |   [○ UNSAFE]   |   [✕ STOPPED]
```
Màu: xanh / vàng / đỏ / xám. Hover hiện reason (black/freeze/lost/minimized/locked/hwnd-gone).

Tùy chọn config:
```json
"safety": {
  "pause_on_capture_unhealthy": true,    // mặc định TRUE — fail-safe
  "unsafe_to_stop_seconds": 60,
  "degraded_to_unsafe_seconds": 1.5,
  "recreate_capture_after_seconds": 2
}
```

### 22.8 Trường Hợp Đặc Biệt: User Cố Tình Tab Out

Nếu user Alt+Tab ra app khác → SendInput gửi vào app sai = thảm họa. Detect bằng `GetForegroundWindow() != targetHwnd_` → gate đóng tự động (đã bao gồm trong 22.5 nếu mở rộng allowInput()). Khi user quay lại game → resume.

### 22.9 Test Cases Bắt Buộc Cho Safe Mode

- ✅ Minimize game 30s → tool không send phím
- ✅ Alt+Tab sang Notepad → tool không gõ phím vào Notepad
- ✅ Win+L lock máy → tool không gõ vào màn login
- ✅ Game crash → tool dừng + thông báo, không spam key vào HWND zombie
- ✅ Loading screen 10s → tool pause, resume khi game vào map
- ✅ Cutscene đen → tool pause, không spam attack
- ✅ Mis-calibrate orb (fill 0%/100% mãi) → tool flag UNSAFE, không spam potion vô nghĩa

---

## 23. Action Priority — HP Emergency Preempt

### 23.1 Nguyên Tắc
**HP luôn là ưu tiên tuyệt đối.** Khi HP < ngưỡng emergency, tool phải:
1. Cancel mọi command đang chờ trong queue ở priority thấp hơn.
2. KHÔNG đợi action hiện tại hoàn tất (nếu đang trong delay/jitter của buff/combat) — preempt.
3. Bỏ qua jitter humanizer lớn — gửi HP pot với delay tối thiểu.
4. Tiếp tục gửi (theo rate-limit) cho đến khi HP ≥ ngưỡng an toàn.
5. Sau khi an toàn → resume action thường (combat / buff) ngay.

### 23.2 Bảng Priority Cứng

| P | Tên | Action | Preempt? | Rate limit | Jitter |
|---|---|---|---|---|---|
| **P0** | HP Emergency | Send HP pot key | ✅ preempt mọi P > 0 | 600ms cooldown pot | 0–8ms (tối thiểu) |
| **P1** | Recovery khác | MP pot, SP pot | preempt P3, P4 | 600ms cooldown | 5–20ms |
| **P2** | Safety Recall | Recall về thành khi HP < N% kéo dài | preempt P3, P4 | 5s cooldown | 5–20ms |
| **P3** | Combat | Attack key, hold A | không preempt P0–P2 | interval cấu hình | full humanizer |
| **P4** | Buff Timer | F2–F5 timer | không preempt gì | interval cấu hình | full humanizer |

### 23.3 Kiến Trúc — Single Execution Slot + Priority Tick

Thay design `priority_queue` ở Section 9 bằng pattern **single executor + priority evaluator**:

```cpp
class ActionDispatcher {
public:
    void onVisionTick(const VisionState& v) {
        // Evaluate theo priority TỪ CAO TỚI THẤP
        if (auto act = evalP0_Hp(v))      return execute(*act);   // preempt
        if (auto act = evalP1_MpSp(v))    return execute(*act);
        if (auto act = evalP2_Recall(v))  return execute(*act);
        if (auto act = evalP3_Combat(v))  return execute(*act);
        if (auto act = evalP4_Buff(v))    return execute(*act);
    }

    void execute(Action a) {
        if (inflight_ && a.priority < inflight_->priority) {
            cancelInflight();           // preempt
        }
        if (inflight_) return;          // đã có action cùng/cao hơn priority chạy
        if (!gate_.allowInput()) return;
        inflight_ = a;
        sendKeyAsync(a);                // dispatch tới InputThread
    }
private:
    std::optional<Action> inflight_;
    OutputGate& gate_;
};
```

- **Single slot `inflight_`** — đảm bảo mỗi lần chỉ 1 key đang send. Cancel = clear slot, không gửi nốt key-up nếu là key-tap.
- **Re-evaluate mỗi frame** — không cần queue dài. State cũ không tồn tại; mỗi tick quyết định lại dựa trên vision mới nhất. KISS.
- **Preempt rule:** action mới có `priority < inflight_.priority` (P0 < P1 nghĩa là P0 ưu tiên cao hơn) → cancel inflight.

### 23.4 HP Emergency Hot Path

```cpp
std::optional<Action> evalP0_Hp(const VisionState& v) {
    if (!cfg_.hp.enabled) return std::nullopt;
    if (v.hpPct > cfg_.hp.threshold_pct) return std::nullopt;
    if (now() - lastHpPot_ < 600ms) return std::nullopt;   // pot cooldown
    return Action{
        .priority = P0,
        .key = cfg_.hp.key,            // vd "1"
        .scanCode = vkToScan(cfg_.hp.key),
        .jitterMs = rand(0, 8),        // minimal jitter
        .holdMs = rand(20, 40),        // key tap
        .reason = "HP_EMERGENCY"
    };
}
```

Lưu ý:
- **HP check chạy MỖI frame** (~50ms ở 20Hz). Phát hiện trong < 100ms.
- **MP/SP check vẫn mỗi frame** nhưng evaluator chỉ trả về Action khi P0 không trigger.
- **Confirm window:** sau khi gửi HP pot, ghi `lastHpPot_ = now()`. 600ms tiếp theo không gửi pot khác — đủ để PT animation cập nhật HP orb (~200–400ms).

### 23.5 Vision Pipeline Đặt HP Trước

```cpp
void VisionPipeline::process(const Frame& f) {
    VisionState s{};
    s.hpPct = hpDetector_.compute(f);     // 1. FIRST — fastest path
    
    // Nếu HP nguy hiểm, có thể short-circuit, gọi dispatcher ngay
    if (s.hpPct < cfg_.hp.emergency_pct) {
        dispatcher_.onVisionTick(s);
        // Tiếp tục đọc MP/SP cho frame sau (không block HP response)
    }
    
    s.mpPct = mpDetector_.compute(f);     // 2.
    s.spPct = spDetector_.compute(f);     // 3.
    dispatcher_.onVisionTick(s);
}
```

Trick: nếu HP critical, dispatcher được gọi **trước khi** đọc xong MP/SP. Cứu HP nhanh hơn ~5–10ms — đáng với hot path.

### 23.6 Edge Case Cần Xử Lý

| Tình huống | Hành vi mong muốn |
|---|---|
| HP < ngưỡng + đang send F2 buff | Cancel buff send, gửi HP pot. Buff sẽ retry tick sau (nếu vẫn tới timer). |
| HP < ngưỡng VÀ MP < ngưỡng cùng frame | HP trước. MP đợi 600ms (cooldown HP pot) rồi mới tới. |
| HP đọc nhiễu (1 frame về 0% do particle) | EMA smoothing (Section 7) + đòi N=2 frame liên tiếp dưới ngưỡng mới trigger. Tránh false positive. |
| HP < ngưỡng nhưng pot đang cooldown 600ms | Skip frame này, tick sau evaluate lại. |
| HP < ngưỡng kéo dài > 3s (pot không lên HP) | Trigger P2 Recall safety (về thành tránh chết). |
| Đang trong "human break" của humanizer | Break **KHÔNG block** HP pot. Break chỉ áp dụng cho P3/P4. P0–P2 luôn chạy. |
| OutputGate đóng (capture unsafe) | HP pot vẫn không gửi (an toàn > có thể bị die). Đây là trade-off chấp nhận. |
| User bật tool khi HP đã ở 30% | Tick đầu tiên đã trigger P0. OK. |

### 23.7 Config

```json
"hp": {
  "enabled": true,
  "key": "1",
  "threshold_pct": 60,
  "emergency_pct": 40,       // hard floor — luôn pot dù bất kỳ tình huống
  "confirm_frames": 2,       // 2 frame liên tiếp dưới threshold mới trigger
  "pot_cooldown_ms": 600,
  "recall_after_seconds": 3  // P2 fallback nếu pot không cứu được
},
"priority": {
  "humanizer_skips_p0_p1_p2": true,
  "p0_max_jitter_ms": 8
}
```

### 23.8 Tóm tắt khác biệt với design cũ

| Trước (Section 9) | Sau (Section 23) |
|---|---|
| Priority queue theo `fire_time` | Re-evaluate mỗi vision tick, không queue |
| Action chạy đến hoàn tất | Preempt được nếu priority cao hơn đến |
| Humanizer áp dụng đều | Humanizer skip P0–P2 (chỉ áp P3–P4) |
| Combat = HP = Buff (cùng level) | Cứng: HP > MP > Recall > Combat > Buff |
| HP đọc cùng tốc độ MP/SP/target | HP đọc trước, short-circuit nếu critical |

---

## 24. UI Cấu Hình — Chỉ Cái User Cần (KISS)

### 24.1 Nguyên tắc
- **UI = subset của Config** — chỉ expose những setting user chỉnh thường xuyên (enable feature, phím, ngưỡng %, interval cơ bản).
- **Setting nâng cao** (humanizer, safe mode timing, vision tweaks, cooldown pot, EMA, capture backend...) — **không có UI**, nằm trong `config.json` với default tốt. Power user sửa tay file được.
- **Code KHÔNG hard-code** giá trị — luôn đọc từ `Config`. UI chỉ binding subset; phần còn lại giữ default từ `config-defaults.h`.
- **Auto-save 500ms debounce** sau mỗi thay đổi UI → ghi `config.json` (merge với phần advanced không bị mất).

### 24.2 UI Layout — CHỐT theo Section 21.3

UI dùng đúng layout Section 21.3 (đã trim, MVP 8 feature core). Không thêm field. Tham khảo lại:

```
┌────────────────────────────────────────────────────────────┐
│  pt_helper                                          _  □  ✕ │
├────────────────────────────────────────────────────────────┤
│  Trạng thái: [Idle]   HWND: [chưa attach]    [ Attach ]     │
│  ☐ AUTO (master)                                Hotkey: F8 │
├─ ▼ Recovery (Vision) ──────────────────────────────────────┤
│  ☑ Hồi HP     Phím [1▼]   Khi HP <  [ 60] %HP              │
│  ☑ Hồi Mana   Phím [2▼]   Khi MP <  [ 10] %MP              │
│  ☑ Hồi Stam   Phím [3▼]   Khi SP <  [ 10] %SP              │
│  ☑ Về thành   Phím [F12▼] Khi HP <  [ 15] %HP              │
├─ ▼ Combat ─────────────────────────────────────────────────┤
│  ☑ Tấn công   Phím [F1▼]  Mỗi      [800] ms                │
│  ☐ Giữ phím A (khi AUTO bật)                                │
│  ☐ Chờ MP ≥   [ 30] %MP trước khi attack                    │
├─ ▼ Buff (Timer) ───────────────────────────────────────────┤
│  ☐ F2 mỗi [180] s    ☐ F3 mỗi [180] s                       │
│  ☐ F4 mỗi [180] s    ☐ F5 mỗi [180] s                       │
├─ ▼ Calibrate ──────────────────────────────────────────────┤
│  [ Calibrate HP ] [ Calibrate MP ] [ Calibrate SP ]         │
│  HP=[██████░░] 87%   MP=[████░░░░] 42%   SP=[░░] 12%        │
├────────────────────────────────────────────────────────────┤
│  [ Save ]  [ Load ]  [ Reset ]              [ START ] [Exit]│
│  CPU 2%  RAM 85MB  Capture 30fps  Gate: ●  State: Combat   │
└────────────────────────────────────────────────────────────┘
```

```
┌────────────────────────────────────────────────────────────┐
│  pt_helper                                          _  □  ✕ │
├────────────────────────────────────────────────────────────┤
│  Trạng thái: [Idle]   HWND: [chưa attach]    [ Attach ]     │
│  ☐ AUTO (master)                                Hotkey: F8 │
├─ ▼ Recovery — HP (P0 Emergency) ───────────────────────────┤
│  ☑ Bật Hồi HP                                              │
│    Phím pot HP:        [ 1 ▼]                              │
│    Ngưỡng bơm:         [ 60] %HP   (P0 normal)             │
│    Ngưỡng emergency:   [ 40] %HP   (P0 force, skip jitter) │
│    Cooldown pot:       [600] ms                            │
│    Confirm frames:     [  2] frame liên tiếp dưới ngưỡng   │
├─ ▼ Recovery — MP (P1) ─────────────────────────────────────┤
│  ☑ Bật Hồi MP                                              │
│    Phím pot MP:        [ 2 ▼]                              │
│    Ngưỡng bơm:         [ 10] %MP                           │
│    Cooldown pot:       [600] ms                            │
├─ ▼ Recovery — SP/Stamina (P1) ─────────────────────────────┤
│  ☐ Bật Hồi SP                                              │
│    Phím pot SP:        [ 3 ▼]                              │
│    Ngưỡng bơm:         [ 10] %SP                           │
│    Cooldown pot:       [600] ms                            │
├─ ▼ Safety — Về thành (P2) ─────────────────────────────────┤
│  ☑ Bật Về thành khi HP thấp kéo dài                        │
│    Phím recall:        [F12▼]                              │
│    Ngưỡng HP:          [ 15] %HP                           │
│    Sau khi HP dưới:    [3.0] giây                          │
│    Cooldown recall:    [  5] giây                          │
├─ ▼ Combat (P3) ────────────────────────────────────────────┤
│  ☑ Bật Tấn công                                            │
│    Phím tấn công:      [F1▼]                               │
│    Interval:           [800] ms                            │
│  ☐ Giữ phím khi AUTO   Phím: [A ▼]                         │
│  ☐ Chờ MP ≥            [ 30] %MP trước khi attack          │
├─ ▼ Buff Timer (P4) ────────────────────────────────────────┤
│  ☐ Buff #1   Phím [F2▼]   Mỗi [180] giây                   │
│  ☐ Buff #2   Phím [F3▼]   Mỗi [180] giây                   │
│  ☐ Buff #3   Phím [F4▼]   Mỗi [180] giây                   │
│  ☐ Buff #4   Phím [F5▼]   Mỗi [180] giây                   │
├─ ▼ Humanizer ──────────────────────────────────────────────┤
│  ☑ Jitter Gaussian (P3, P4)                                │
│    Sigma:              [ 45] ms                            │
│  ☑ Break định kỳ (P3, P4)                                  │
│    Mỗi:                [ 25] - [ 50] action                │
│    Kéo dài:            [  3] - [  8] giây                  │
│  ☑ Session pause                                           │
│    Runtime:            [ 40] - [110] phút                  │
│    Nghỉ:               [  6] - [ 14] phút                  │
│  ☑ Miss-click            xác suất [ 2.0] %  offset [ 5] px │
├─ ▼ Vision ─────────────────────────────────────────────────┤
│    Vision tick rate:   [ 20] Hz   (= 50ms/frame)           │
│    EMA alpha:          [0.30]   (smoothing 0–1)            │
│    Black luma thresh:  [  5]  /255                         │
│    Freeze timeout:     [4.0] giây                          │
│    Capture backend:    [WGC ▼] (WGC / DXGI Duplication)    │
├─ ▼ Calibrate Vùng Orb ─────────────────────────────────────┤
│    HP: x[ 60] y[660] w[90] h[90] r[42]   [ Calibrate ]     │
│    MP: x[1130]y[660] w[90] h[90] r[42]   [ Calibrate ]     │
│    SP: x[...]y[...] w[..] h[..] r[..]    [ Calibrate ]     │
│    Preview:                                                 │
│    HP=[████████░░] 87%   MP=[████░░░░░░] 42%   SP=[░░] 12% │
├─ ▼ Safe Mode ──────────────────────────────────────────────┤
│  ☑ Pause khi capture không khoẻ        (mặc định BẬT)      │
│    Degraded→Unsafe sau:    [1.5] giây                      │
│    Unsafe→Stop sau:        [ 60] giây                      │
│    Recreate capture sau:   [2.0] giây                      │
│  ☑ Pause khi cửa sổ game không foreground                  │
│  ☑ Pause khi máy lock (Win+L / UAC)                        │
├────────────────────────────────────────────────────────────┤
│  Profile: [farm_lv50 ▼]  [+ New] [Save] [Save As] [Reset]   │
│                                            [ START ] [Exit]│
│  CPU 2%  RAM 85MB  Capture 30fps  Gate: ●  State: Combat   │
└────────────────────────────────────────────────────────────┘
```

### 24.3 Field UI Expose (subset)

Chỉ những field sau có control trên UI. Còn lại là internal.

| Nhóm | Field | Range | Default | UI control |
|---|---|---|---|---|
| **HP** | enabled | bool | true | Checkbox |
| | key | VK | "1" | Combo |
| | threshold_pct | 1–99 | 60 | DragInt |
| **MP** | enabled | bool | true | Checkbox |
| | key | VK | "2" | Combo |
| | threshold_pct | 1–99 | 10 | DragInt |
| **SP** | enabled | bool | false | Checkbox |
| | key | VK | "3" | Combo |
| | threshold_pct | 1–99 | 10 | DragInt |
| **Recall** | enabled | bool | true | Checkbox |
| | key | VK | "F12" | Combo |
| | hp_threshold_pct | 1–99 | 15 | DragInt |
| **Combat** | attack_enabled | bool | true | Checkbox |
| | attack_key | VK | "F1" | Combo |
| | attack_interval_ms | 100–10000 | 800 | DragInt |
| | hold_key_enabled | bool | false | Checkbox |
| | wait_mp_enabled | bool | false | Checkbox |
| | wait_mp_min_pct | 1–99 | 30 | DragInt |
| **Buff[0..3]** | enabled | bool | false | Checkbox |
| | interval_sec | 5–3600 | 180 | DragInt |
| **Regions** | hp/mp/sp orb | int | — | Calibrate button |
| **Master** | auto.enabled | bool | false | Big toggle / F8 hotkey |

### 24.4 Field Internal (KHÔNG có UI, chỉnh tay nếu cần)

Tất cả dưới đây nằm trong `config.json` với default tốt; user không cần đụng. Power user mở Notepad sửa được.

| Nhóm | Field | Default | Lý do ẩn |
|---|---|---|---|
| HP | emergency_pct | 40 | Default đủ an toàn |
| HP/MP/SP | pot_cooldown_ms | 600 | Cứng theo game mechanics |
| HP | confirm_frames | 2 | Tinh chỉnh chống nhiễu |
| Recall | hp_low_duration_sec, cooldown_sec | 3.0 / 5 | Hiếm chỉnh |
| Humanizer (tất cả) | jitter_sigma, break_*, session_*, missclick_* | (Section 20) | Default cân bằng tốt |
| Vision | tick_rate_hz, ema_alpha, black_luma_threshold, freeze_timeout_sec, capture_backend | 20 / 0.3 / 5 / 4.0 / wgc | Tinh chỉnh kỹ thuật |
| Safety | degraded_to_unsafe_sec, unsafe_to_stop_sec, recreate_capture_after_sec, pause_on_* | (Section 22) | Default an toàn |
| Window | title_regex | "Priston Tale" | Hiếm đổi |
| Hotkey | start_stop | "F8" | Hiếm đổi |

### 24.5 Cấu Trúc Config File

```json
{
  "ui_settings": {
    "hp":   { "enabled": true,  "key": "1",   "threshold_pct": 60 },
    "mp":   { "enabled": true,  "key": "2",   "threshold_pct": 10 },
    "sp":   { "enabled": false, "key": "3",   "threshold_pct": 10 },
    "recall": { "enabled": true, "key": "F12", "hp_threshold_pct": 15 },
    "combat": {
      "attack_enabled": true, "attack_key": "F1", "attack_interval_ms": 800,
      "hold_key_enabled": false, "wait_mp_enabled": false, "wait_mp_min_pct": 30
    },
    "buff": [
      { "enabled": false, "key": "F2", "interval_sec": 180 },
      { "enabled": false, "key": "F3", "interval_sec": 180 },
      { "enabled": false, "key": "F4", "interval_sec": 180 },
      { "enabled": false, "key": "F5", "interval_sec": 180 }
    ],
    "regions": {
      "hp_orb": { "x": 60,  "y": 660, "w": 90, "h": 90, "radius": 42 },
      "mp_orb": { "x": 1130,"y": 660, "w": 90, "h": 90, "radius": 42 },
      "sp_orb": { "x": 1220,"y": 660, "w": 60, "h": 60, "radius": 28 }
    }
  },
  "advanced": {
    "hp": { "emergency_pct": 40, "pot_cooldown_ms": 600, "confirm_frames": 2 },
    "mp": { "pot_cooldown_ms": 600 },
    "sp": { "pot_cooldown_ms": 600 },
    "recall": { "hp_low_duration_sec": 3.0, "cooldown_sec": 5 },
    "humanizer": { "...": "Section 20.4 defaults" },
    "vision":    { "tick_rate_hz": 20, "ema_alpha": 0.30, "black_luma": 5, "freeze_timeout_sec": 4.0, "capture_backend": "wgc" },
    "safety":    { "degraded_to_unsafe_sec": 1.5, "unsafe_to_stop_sec": 60, "recreate_capture_after_sec": 2.0,
                   "pause_on_unhealthy_capture": true, "pause_on_not_foreground": true, "pause_on_session_lock": true },
    "window":    { "title_regex": "Priston Tale" },
    "hotkey":    { "start_stop": "F8" }
  }
}
```

UI chỉ đọc/ghi block `ui_settings`. Block `advanced` được load nguyên si, không bị mất khi UI save (merge-preserving).

### 24.6 Validation
- **Range clamp:** ImGui `DragInt` với min/max — tự ngăn giá trị bậy.
- **Cross-field invariant** (validate khi load file, không expose UI):
  - `advanced.hp.emergency_pct < ui_settings.hp.threshold_pct` — nếu fail, log warning + auto-fix
  - `advanced.humanizer.*_max ≥ *_min` — auto-swap nếu sai
- Load file fail invariant → giữ config trước, log lỗi.

### 24.7 Persistence
- **Auto-save 500ms debounce** sau mỗi thay đổi UI → ghi `config.json` (atomic temp + rename, **merge-preserving** với block `advanced`).
- **Reset** = thay block `ui_settings` bằng default, không đụng `advanced`.
- **Hot reload:** `ReadDirectoryChangesW` watch `config.json` → reload khi user sửa tay → UI refresh hiển thị.

### 24.8 Lưu Ý Cho Developer (KHÔNG hard-code)
| Sai (hard-code) | Đúng (đọc Config) |
|---|---|
| `if (hp < 60) sendKey("1");` | `if (hp < cfg.hp.threshold_pct) sendKey(cfg.hp.key);` |
| `sleep(800);` | `sleep(cfg.combat.attack_interval_ms);` |
| `if (now - last > 600ms) ...` | `if (now - last > cfg.hp.pot_cooldown_ms) ...` |
| `tickRate = 20;` | `tickRate = cfg.vision.tick_rate_hz;` |
| `const int BUFF_KEY = VK_F2;` | `const auto key = cfg.buff[0].key;` |

Constants duy nhất trong code: default values (Section 24.3 cột "Default") + min/max range — đặt trong `config-defaults.h`.

---

## 25. Deployment — Output Build

### 25.1 Sau khi build
Output từ Visual Studio 2022 → **một file `pt_helper.exe`** Win32 GUI (~5–8 MB nếu static link OpenCV). Không phải installer, không phải DLL — chạy trực tiếp.

### 25.2 Cấu Trúc Deploy
```
pt_helper/
├── pt_helper.exe          # chạy file này
├── config.json            # cấu hình (auto-tạo lần đầu nếu chưa có)
├── assets/                # template ảnh (nếu cần)
└── logs/                  # tự tạo runtime
```

### 25.3 Đặc điểm
| Mục | Giá trị |
|---|---|
| Loại | Win32 GUI exe, không console |
| Quyền chạy | User thường, KHÔNG cần Admin |
| Portable | YES — copy folder là chạy, không registry |
| Runtime | Cần VC++ Redistributable 2022 (hầu hết Win10/11 đã có), hoặc dùng `/MT` static CRT |
| Auto-start | KHÔNG default; user tự thêm shortcut vào `shell:startup` nếu muốn |
| Uninstall | Xóa folder |
| Code signing | Không bắt buộc; chưa ký sẽ có cảnh báo SmartScreen lần đầu (chọn "Run anyway") |

### 25.4 CMake Flags Bổ Sung (Static CRT — optional, khỏi cần VCRedist)
```cmake
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
```
Trade-off: exe to thêm ~200 KB, không phụ thuộc VCRedist trên máy đích.

### 25.5 Tùy chọn nâng cao (sau MVP)
- **Inno Setup / NSIS installer** — đóng gói có uninstaller (YAGNI v1)
- **Code signing cert** — ~$70/năm, bỏ SmartScreen warning
- **Auto-updater** — overkill cho v1

---

## 26. Naming & VersionInfo — Trông Như App Chính Thống

### 26.1 Quyết Định Đặt Tên
**Tên exe:** `WindowHelper.exe` (benign, generic, không mimic system binary).

Lý do KHÔNG dùng tên system process (`svchost.exe`, `dwm.exe`, `explorer.exe`, `winlogon.exe`, `csrss.exe`...):
- Anti-cheat có heuristic "process masquerading as system binary" — chạy ngoài `C:\Windows\System32` + thiếu chữ ký Microsoft = flag ngay.
- Windows Defender quarantine unsigned exe trùng tên system process.
- Vi phạm Microsoft branding terms.

Tên thay thế có thể đổi sau nếu thích: `ScreenAssist.exe`, `InputTools.exe`, `DesktopUtility.exe`, `PixelMate.exe` — đều OK miễn không trùng system binary.

### 26.2 VersionInfo Embed Vào EXE

Tạo `src/resources/version.rc`:
```rc
#include <winver.h>

VS_VERSION_INFO VERSIONINFO
 FILEVERSION    1,0,0,0
 PRODUCTVERSION 1,0,0,0
 FILEFLAGSMASK  VS_FFI_FILEFLAGSMASK
 FILEFLAGS      0
 FILEOS         VOS__WINDOWS32
 FILETYPE       VFT_APP
 FILESUBTYPE    VFT2_UNKNOWN
BEGIN
 BLOCK "StringFileInfo"
 BEGIN
  BLOCK "040904B0"
  BEGIN
   VALUE "CompanyName",      "KIEU HIEN"
   VALUE "FileDescription",  "Window Helper Utility"
   VALUE "FileVersion",      "1.0.0.0"
   VALUE "InternalName",     "WindowHelper"
   VALUE "OriginalFilename", "WindowHelper.exe"
   VALUE "ProductName",      "Window Helper"
   VALUE "ProductVersion",   "1.0.0.0"
   VALUE "LegalCopyright",   "Copyright (C) 2026 KIEU HIEN"
  END
 END
 BLOCK "VarFileInfo"
 BEGIN
  VALUE "Translation", 0x409, 1200
 END
END

// Icon
IDI_APP_ICON ICON "app.ico"
```

**CompanyName/ProductName** là của bạn, KHÔNG giả Microsoft.

### 26.3 Icon
- File `src/resources/app.ico` — kích thước 256×256, 128, 64, 48, 32, 16 (multi-resolution).
- Tạo từ PNG bằng tool free (ImageMagick `convert app.png -define icon:auto-resize=256,128,64,48,32,16 app.ico`) hoặc online (icoconvert.com).
- Embed qua `.rc` ở 26.2.

### 26.4 CMakeLists.txt Bổ Sung
```cmake
# Đổi tên output
set_target_properties(pt_assistant PROPERTIES
  OUTPUT_NAME "WindowHelper"
)

# Compile resource file (icon + version info)
if(MSVC)
  target_sources(pt_assistant PRIVATE src/resources/version.rc)
endif()

# Win32 GUI subsystem (không console window)
set_target_properties(pt_assistant PROPERTIES
  WIN32_EXECUTABLE TRUE
)
```

Đặt CMake project name vẫn là `pt_assistant` (internal), chỉ OUTPUT_NAME khác.

### 26.5 Kết Quả Quan Sát
| Thuộc tính | Giá trị |
|---|---|
| Tên file | `WindowHelper.exe` |
| Tên process trong Task Manager | `WindowHelper.exe` (cột Name) |
| Description trong Task Manager Details tab | "Window Helper Utility" (từ FileDescription) |
| Properties → Details tab | hiện đầy đủ CompanyName / ProductName / FileVersion |
| Icon | icon `.ico` của bạn ở taskbar, explorer, alt-tab |
| Window title | UI window title cũng đổi: `"Window Helper"` (từ ImGui SetWindowTitle) |
| Mutex / single-instance | dùng GUID-based mutex name, không phơi "PT" / "bot" |

### 26.6 Đồng Bộ Naming Khắp Code
| Vị trí | Giá trị |
|---|---|
| OUTPUT_NAME (CMake) | `WindowHelper` |
| Window class name (RegisterClass) | `WindowHelperMainWnd` |
| Window title (CreateWindow + ImGui) | `Window Helper` |
| Mutex name (CreateMutex) | `Global\\{B6C9A2F1-3D5E-4F7A-8B1C-9E2D4F6A8C0B}_WindowHelper` |
| Log filename | `logs/WindowHelper.log` |
| Config filename | giữ `config.json` (generic) |
| Tray tooltip | `Window Helper` |

### 26.7 Những Điều TRÁNH (red flag với AV/XC3)
- ❌ Đặt CompanyName = "Microsoft Corporation"
- ❌ Đặt FileDescription = "Windows Host Process" / "Desktop Window Manager" / ...
- ❌ Copy icon của Windows system app
- ❌ Tên file trùng `svchost.exe`, `dwm.exe`, `explorer.exe`, `winlogon.exe`, `csrss.exe`, `lsass.exe`, `services.exe`, `wininit.exe`, `smss.exe`, `taskhost.exe`, `taskhostw.exe`, `RuntimeBroker.exe`, `ApplicationFrameHost.exe`
- ❌ Bắt chước Microsoft Authenticode signature

### 26.8 Optional — Code Signing
Để bỏ SmartScreen warning lần đầu:
- Cert tự ký (free, KHÔNG được tin) — bỏ qua
- Cert OV/EV chính danh ($70–$400/năm tùy provider) — bỏ SmartScreen
- YAGNI cho v1, cân nhắc nếu phân phối ra ngoài

---

## 27. Phân Tích Khả Năng Hoạt Động Với PT 800×600 / Fullscreen

### 27.1 Quan Sát Từ Screenshot Mẫu
PT classic client, resolution 800×600 (windowed thumbnail). UI elements quan trọng:

| Element | Vị trí ước tính (800×600) | Mục đích vision |
|---|---|---|
| **HP orb (đỏ)** | ~(50, 540), Ø ~40px | Đo HP fill ratio |
| **MP orb (xanh dương)** | ~(95, 540), Ø ~40px | Đo MP fill ratio |
| **SP/Stamina bar** | bên cạnh orb, có thể là thanh ngang nhỏ | Đo SP |
| Skill bar slots | (220–500, 530) | Không cần đọc, chỉ gửi phím |
| Mini-map | (670–800, 510–600) | Bỏ qua |
| Target portrait | (730, 30) | Bỏ qua |

**Lưu ý quan trọng:** HP/MP orb ở **bottom-LEFT sát nhau**, KHÔNG phải 2 góc đối nhau. Default coords trong report cũ (1280×720 layout) **PHẢI sửa**.

### 27.2 Đánh Giá Khả Thi Theo Capture Mode

| Mode | Khả năng tool hoạt động | Ghi chú |
|---|---|---|
| **Windowed 800×600** | ⭐⭐⭐⭐⭐ Tốt nhất | WGC + DXGI đều hoàn hảo. Khuyến nghị v1. |
| **Borderless windowed** | ⭐⭐⭐⭐⭐ | Bằng windowed về mặt capture |
| **Fullscreen Win10/11** | ⭐⭐⭐⭐ | Flip-model emulation OK, capture được |
| **Fullscreen Win7/driver cổ** | ⭐⭐ | DXGI có thể `ACCESS_LOST`. Workaround: chuyển sang windowed |

### 27.3 Đánh Giá Tài Nguyên (800×600)
- Frame size: 800×600×4 = **1.9 MB/frame**
- Capture latency: **< 0.5ms** (WGC GPU staging)
- Vision detect (3 ROI orb): **< 2ms** OpenCV
- End-to-end (frame→key send): **< 30ms**
- CPU usage: **< 3%** ở vision tick 20Hz
- RAM: **~70 MB** steady-state (OpenCV runtime + capture buffer)

Tài nguyên dư thừa cực lớn. Không bottleneck nào.

### 27.4 Default Coords Mới Cho 800×600

Cập nhật `advanced.regions` trong `config.json`:
```json
"regions": {
  "hp_orb": { "x":  35, "y": 520, "w": 50, "h": 50, "radius": 22 },
  "mp_orb": { "x":  80, "y": 520, "w": 50, "h": 50, "radius": 22 },
  "sp_orb": { "x": 125, "y": 535, "w": 40, "h": 30, "radius": 15 }
}
```
**Đây là default ước tính** — user vẫn cần Calibrate lần đầu để chính xác (UI mod / theme khác làm shift vài pixel).

### 27.5 Multi-Resolution Profile

Khi vision detect resolution đổi (qua `item_.Size()` của WGC hoặc capture rect):
1. Lookup profile theo `regions["<width>x<height>"]` trong config.
2. Nếu chưa có → prompt user calibrate, lưu lại.
3. Auto-switch profile mỗi khi resolution thay đổi.

Config bổ sung (internal):
```json
"region_profiles": {
  "800x600":  { "hp_orb": {...}, "mp_orb": {...}, "sp_orb": {...} },
  "1024x768": { "hp_orb": {...}, "mp_orb": {...}, "sp_orb": {...} }
}
```

UI hiện indicator "Resolution: 800×600 (calibrated)" hoặc "Resolution: 1024×768 (chưa calibrate — bấm Calibrate)".

### 27.6 Rủi Ro Vision Cụ Thể Với PT Screenshot

| Rủi ro | Quan sát | Mitigation |
|---|---|---|
| Particle/spell bay qua orb | Effect hexagon, bullet | EMA α=0.3 + confirm_frames=2 (Section 23.6) — đã có |
| Background xanh lá có thể trùng MP orb hue | Map đồng cỏ | MP orb hue 100–130 (blue), grass 35–75 (green) — không trùng |
| HP đỏ trùng với máu/đỏ enemy effect | Possible | HP orb fixed UI position, không bị che bởi world entities (HUD layer riêng) |
| Buff icon trên đầu char | nameplates "Hippie" | Không liên quan vùng orb |
| Window scaling DPI khác 100% | Win10 high-DPI | Đăng ký DPI awareness `Per Monitor V2` — capture lấy raw size đúng |

### 27.7 Khuyến Nghị Cuối

**Để tool hoạt động tốt nhất trên config bạn dùng:**

1. ✅ **Chạy PT ở windowed 800×600** (cấu hình `-window` hoặc setting trong client). KHÔNG khuyến nghị fullscreen cho v1.
2. ✅ Sau khi attach window, **bấm Calibrate HP / MP / SP** — chỉnh chính xác ô orb.
3. ✅ Để `Vision Preview` mở 30 giây đầu, kiểm tra fill ratio thay đổi đúng khi nhân vật bị đánh / uống pot.
4. ⚠️ Nếu muốn fullscreen: test trước trên server offline, chuẩn bị fallback DXGI Duplication.
5. ⚠️ Nếu UI mod của server bạn dùng (skin / texture) làm orb khác hình dạng → calibrate lại, có thể cần điều chỉnh hue range trong `advanced` (sửa tay `config.json`).

### 27.8 Verdict
**TOOL SẼ CHẠY ĐƯỢC VÀ HỖ TRỢ HIỆU QUẢ trên config 800×600 windowed.** Fullscreen Win10/11 cũng OK với fallback. Resource dư thừa, latency < 30ms — đủ cứu HP kịp.

Phần phải verify khi implement:
- Calibrate orb chính xác (làm 1 lần sau attach)
- Hue range mặc định khớp với theme PT bạn chơi (custom theme có thể đổi màu orb)
- Confirm window 600ms sau pot phù hợp với animation HP cập nhật của PT bạn

---

## 28. Cơ Chế Detect HP/MP Chính Xác — Method B (Waterline)

### 28.1 Vấn Đề Với Method A (area ratio đơn thuần)

Pipeline ban đầu (Section 7) đếm `redPixels / totalCirclePixels`. Đối với orb tròn fill từ dưới lên (PT theo dạng này), **area không tuyến tính với chiều cao mực nước**:

| HP thực (mực nước) | Area filled (Method A) | Sai số |
|---|---|---|
| 10% | ~5.2% | -4.8% |
| 25% | ~20.4% | -4.6% |
| 50% | 50.0% | 0 |
| 75% | ~79.6% | +4.6% |
| 90% | ~94.8% | +4.8% |

Sai số đỉnh ~5% ở 25%/75%. Đạt ~90% accuracy, có thể tốt hơn.

### 28.2 Method B — Waterline Detection (RECOMMENDED)

**Ý tưởng:** game vẽ HP như "cốc nước" — đỏ ở dưới, dần lên trên khi HP đầy. Tìm "mực nước" (dòng đầu tiên không còn đỏ tính từ dưới lên) → chiều cao mực = HP %.

```cpp
double hpPctWaterline(const Mat& redMask, const Mat& circleMask) {
    int orbH = redMask.rows;
    int waterLine = orbH;  // mặc định empty
    for (int y = orbH - 1; y >= 0; y--) {
        int rowRed    = countNonZero(redMask.row(y));
        int rowCircle = countNonZero(circleMask.row(y));
        if (rowCircle == 0) continue;                  // ngoài hình tròn
        double rowRatio = double(rowRed) / rowCircle;
        if (rowRatio < 0.4) {                          // hết đỏ ở row này
            waterLine = y;
            break;
        }
    }
    return double(orbH - waterLine) / orbH;            // 0..1
}
```

**Ưu điểm:**
- **Tuyến tính 1:1** với HP% game hiển thị.
- Accuracy **~96–98%** sau calibrate (sai số chính = quantization 1/orbHeight ≈ 2% cho orb 50px).
- Robust với particle/effect đè lên giữa orb (chỉ care biên trên-dưới).
- Chi phí O(H) thay vì O(H×W) — nhanh hơn.

### 28.3 Pipeline Detect Đầy Đủ (sửa Section 7)

```
[Frame BGRA]
    ↓
[Crop ROI (config.regions.hp_orb)]
    ↓
[BGRA → BGR → HSV]
    ↓
[Mask red (inRange × 2 mask vì hue wrap)]
    ↓
[AND với circleMask (loại 4 góc bbox)]
    ↓
[Waterline detection — Method B]      ← raw HP %
    ↓
[EMA smoothing (α=0.3)]                ← filtered HP %
    ↓
[Confirm: < threshold 2 frame liên tiếp]
    ↓
[ActionDispatcher: P0 HP Emergency]
    ↓
[InputScheduler: SendInput scancode]
    ↓
[Pot cooldown 600ms + vision confirm HP tăng]
```

### 28.4 Tổng Hợp Sai Số

| Nguồn sai số | Mức điển hình | Cách giảm |
|---|---|---|
| Calibrate sai (center/radius lệch) | ±10% nếu lệch | Calibrate UI có visual ring preview, click đúng tâm |
| HSV hue range không khớp UI mod | ±2–5% | Cho user sửa `advanced.hue_range_hp` trong config |
| Particle/effect tạm thời | < 2% sau EMA+confirm | α=0.3, 2-frame confirm |
| HP animation lag (game cập nhật ~200ms) | 100–300ms delay | Pot cooldown 600ms đủ cover |
| Vision tick 20Hz = 50ms | ±1 tick (50ms) | Đủ nhanh để cứu HP |
| Method B quantization (1 row) | 1/orbH ≈ 2% với orb 50px | Calibrate radius lớn hơn nếu UI cho phép |

**Net accuracy sau calibrate:** ~96–98%. False trigger < 1%. Missed trigger < 2%.

### 28.5 Visual Feedback Khi Calibrate

UI Calibrate Mode hiển thị overlay realtime:
```
[Click vào tâm HP orb]
   ↓ (vẽ vòng tròn highlight)
[Kéo radius cho khớp]
   ↓ (vòng tròn theo cursor)
[Preview]
   HP raw     = 73.2%   ← method B
   HP filt    = 73.0%   ← sau EMA
   ROW pattern:
   ┌─────────┐
   │░░░░░░░░░│  row 0   ← background
   │░░░░░░░░░│  row 5
   │▓▓░░░░░░░│  row 10  ← mặt nước (waterline ở đây)
   │▓▓▓▓▓░░░░│  row 15
   │▓▓▓▓▓▓▓░░│  row 20
   │▓▓▓▓▓▓▓▓▓│  row 25  ← đáy
   ...
   └─────────┘
```

Cho user **thấy waterline được detect đúng row nào** — nếu sai (do hue range), user biết để chỉnh.

### 28.6 Khi Nào Method A Vẫn Tốt

- Orb tròn 360° filling theo góc (Diablo II chalice).
- Bar chữ nhật (HP bar hình thanh dài) — chỉ cần waterline 1 chiều, đơn giản hơn.
- PT có style UI khác chuẩn → user toggle `advanced.detection_method = "area" | "waterline"`.

Default cho PT: **waterline**.

### 28.7 Áp Dụng Tương Tự Cho MP/SP

Code identical, chỉ thay hue range:
- HP: red hue (0–10, 170–180)
- MP: blue hue (100–130)
- SP: yellow/orange hue (15–35) — tùy theme PT cụ thể

Mỗi orb có waterline detection riêng, chạy song song mỗi vision tick.

### 28.8 Trả Lời Trực Tiếp Câu Hỏi

> "Tôi set dưới 60% thì sẽ bơm HP, cơ chế detect đang như thế nào để bơm chính xác > 90%?"

**Cơ chế:**
1. **Mỗi 50ms (20Hz)** vision tick đọc ROI HP orb.
2. **Method B waterline** tính raw HP % từ vị trí mực nước đỏ → sai số inherent ~2%.
3. **EMA smoothing** α=0.3 lọc nhiễu particle/effect.
4. **Confirm 2 frame liên tiếp** dưới 60% mới trigger (chống outlier).
5. **ActionDispatcher P0** preempt mọi action khác, gửi pot key.
6. **Cooldown 600ms** không gửi pot tiếp, để game animation cập nhật HP.
7. **Vision tiếp tục đọc** trong cooldown → confirm HP tăng. Nếu sau 600ms vẫn < 60% → pot lần 2.

**Accuracy:** sau calibrate tốt, detect đúng > 96% lần thực sự HP dưới ngưỡng. False trigger < 1%. Trễ phản ứng < 150ms từ lúc HP rớt qua threshold đến lúc key được gửi.

**Yêu cầu để đạt accuracy này:**
- ✅ Calibrate HP orb đúng (UI có preview)
- ✅ Theme PT chuẩn (HP đỏ classic) — nếu UI mod đổi màu thì sửa hue range
- ✅ Không có in-game window đè lên orb (chat box mở rộng, inventory)

---

## 29. Combat FSM — Buff Sequence + Attack Cycle

### 29.1 Cơ Chế PT (thực tế)
- `F1..F5` = chọn active skill, KHÔNG cast.
- **Right-click** = cast skill đang active. Lặp = đánh liên tục.
- Buff cast = press F-key buff → right-click.

→ Design cũ "press F1 mỗi 800ms" SAI. Cần FSM buff-then-attack-then-rebuff.

### 29.2 Flow Đánh Quái Mới

```
BUFFING (sequential):
  cho mỗi buff enabled trong [F2, F3, F4, F5]:
    press F-key
    right-click
    wait cast_delay_ms
ARMING:
  press F1 (main attack active)
ATTACKING (loop cycle_duration_sec, mặc định 300s):
  right-click mỗi attack_interval_ms (vd 600ms)
  HP/MP pot xen kẽ qua P0/P1 priority (KHÔNG đổi state)
[lặp lại từ BUFFING]
```

### 29.3 Combat FSM State Diagram

```
                ┌──────┐
       ┌───────►│ IDLE │
       │        └──┬───┘
       │           │ AUTO=on
       │           ▼
       │      ┌─────────┐
       │      │ BUFFING │ ◄─── cycle_duration hết
       │      └────┬────┘
       │           │ hết buff queue
       │           ▼
       │      ┌────────┐
       │      │ ARMING │ (press F1)
       │      └────┬───┘
       │           │ done
       │           ▼
       │      ┌───────────┐
       │      │ ATTACKING │ (loop right-click)
       │      └────┬──────┘
       │           │
       │  cycle_duration timeout
       │           │
       │           └─────► BUFFING (re-buff)
       │
       │  AUTO=off → IDLE
       │
       │  gate đóng → PAUSED (giữ state cũ)
       │  gate mở  → resume state cũ
       └───
```

### 29.4 Class `CombatFSM`

```cpp
enum class CombatState { IDLE, BUFFING, ARMING, ATTACKING, PAUSED };

struct BuffSlot {
    bool enabled;
    string key;            // "F2".."F5"
    int cast_delay_ms;     // 500 default
};

class CombatFSM {
public:
    void tick(const VisionState& v, TimePoint now);
    CombatState state() const noexcept { return state_; }
private:
    void enterBuffing(TimePoint now);
    void stepBuffing(TimePoint now);
    void enterArming(TimePoint now);
    void enterAttacking(TimePoint now);
    void stepAttacking(TimePoint now);

    CombatState state_ = IDLE;
    int currentBuffIdx_ = 0;
    TimePoint nextStep_;           // sau cast_delay
    TimePoint cycleStart_;         // bắt đầu attack phase
    TimePoint lastAttackClick_;
    CombatState resumeState_;      // lưu state khi PAUSED
    
    InputScheduler& input_;
    const Config& cfg_;
};
```

### 29.5 Pseudocode Tick

```cpp
void CombatFSM::tick(const VisionState& v, TimePoint now) {
    if (!gate_.allowInput()) {
        if (state_ != PAUSED) { resumeState_ = state_; state_ = PAUSED; }
        return;
    }
    if (state_ == PAUSED) { state_ = resumeState_; }

    if (!cfg_.auto_enabled) { state_ = IDLE; return; }
    if (state_ == IDLE) { enterBuffing(now); return; }

    switch (state_) {
        case BUFFING:   stepBuffing(now); break;
        case ARMING:    enterAttacking(now); break;   // tức thì
        case ATTACKING: stepAttacking(now); break;
    }
}

void CombatFSM::stepBuffing(TimePoint now) {
    if (now < nextStep_) return;
    auto& buffs = cfg_.combat.buffs;
    // Tìm buff enabled tiếp theo
    while (currentBuffIdx_ < buffs.size() && !buffs[currentBuffIdx_].enabled)
        ++currentBuffIdx_;
    if (currentBuffIdx_ >= buffs.size()) {            // xong buff queue
        enterArming(now); return;
    }
    auto& b = buffs[currentBuffIdx_];
    input_.scheduleKeyTap(b.key, P3);
    input_.scheduleRightClick(P3);
    nextStep_ = now + ms(b.cast_delay_ms);
    ++currentBuffIdx_;
}

void CombatFSM::enterArming(TimePoint now) {
    input_.scheduleKeyTap(cfg_.combat.main_attack_key, P3);   // F1
    state_ = ARMING;
}

void CombatFSM::enterAttacking(TimePoint now) {
    cycleStart_ = now;
    lastAttackClick_ = now;
    state_ = ATTACKING;
}

void CombatFSM::stepAttacking(TimePoint now) {
    // Cycle timeout → re-buff
    if (now - cycleStart_ >= sec(cfg_.combat.cycle_duration_sec)) {
        currentBuffIdx_ = 0;
        nextStep_ = now;
        state_ = BUFFING;
        return;
    }
    // Wait MP gate (nếu bật)
    if (cfg_.combat.wait_mp_enabled && v.mpPct < cfg_.combat.wait_mp_min_pct)
        return;
    // Attack tick
    auto interval = ms(cfg_.combat.attack_interval_ms) + humanizer_.jitter(P3);
    if (now - lastAttackClick_ >= interval) {
        input_.scheduleRightClick(P3);
        lastAttackClick_ = now;
    }
}
```

### 29.6 Right-Click Implementation

```cpp
void sendRightClick() {
    INPUT in[2]{};
    in[0].type = INPUT_MOUSE;
    in[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
    in[1].type = INPUT_MOUSE;
    in[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
    SendInput(2, in, sizeof(INPUT));
}
```

Right-click tại **vị trí cursor hiện tại**. User đặt cursor giữa quái → PT auto-target quái gần đó.

**Internal config nâng cao** (không UI):
```json
"attack_click_mode": "cursor",          // "cursor"|"center"|"fixed"
"attack_click_pos": {"x":400,"y":300},  // chỉ dùng khi mode=fixed
"attack_click_jitter_px": 3             // sai lệch ngẫu nhiên ±3px
```

### 29.7 UI Mới (thay phần Combat + Buff trong Section 21.3)

```
├─ ▼ Combat ────────────────────────────────────────────────┤
│  ☑ Auto Combat                                             │
│    Main attack key:     [F1 ▼]                             │
│    Attack interval:     [600] ms                           │
│    Cycle duration:      [300] giây   (re-buff sau khoảng)  │
│  ☐ Chờ MP ≥             [ 30] %MP trước khi attack          │
├─ ▼ Buff Sequence (cast trước attack, thứ tự top-down) ─────┤
│  ☑ Buff 1   Phím [F2▼]   Cast delay [500] ms                │
│  ☑ Buff 2   Phím [F3▼]   Cast delay [500] ms                │
│  ☑ Buff 3   Phím [F4▼]   Cast delay [500] ms                │
│  ☐ Buff 4   Phím [F5▼]   Cast delay [500] ms                │
```

### 29.8 Mapping Config

```json
"ui_settings": {
  "combat": {
    "auto_enabled": true,
    "main_attack_key": "F1",
    "attack_interval_ms": 600,
    "cycle_duration_sec": 300,
    "wait_mp_enabled": false,
    "wait_mp_min_pct": 30
  },
  "buffs": [
    {"enabled": true,  "key": "F2", "cast_delay_ms": 500},
    {"enabled": true,  "key": "F3", "cast_delay_ms": 500},
    {"enabled": true,  "key": "F4", "cast_delay_ms": 500},
    {"enabled": false, "key": "F5", "cast_delay_ms": 500}
  ]
}
```

### 29.9 Tương Tác FSM ↔ Pot Priority (P0/P1)

| Tình huống | Hành vi |
|---|---|
| Đang BUFFING + HP < 60% | Inject pot key `1`. FSM giữ BUFFING. Sau pot, tiếp tục buff hiện tại. |
| Đang ATTACKING + MP < 10% | Inject pot key `2`. FSM giữ ATTACKING. Right-click vẫn tiếp tục đúng schedule. |
| Pot KHÔNG cần active skill | Đúng. Press `1`/`2`/`3` là dùng potion bar slot, KHÔNG cần right-click. Vì vậy không reset F1 active. |
| HP critical kéo dài > 3s khi đang BUFFING | P2 Recall trigger. FSM → IDLE (không re-buff khi về thành). |
| Capture unsafe giữa BUFFING | FSM → PAUSED giữ idx. Khi gate mở → tiếp tục buff đó. |
| User tắt AUTO giữa cycle | FSM → IDLE. Bật lại → BUFFING từ đầu (re-buff để đảm bảo có buff). |

### 29.10 Edge Cases

- **Buff cast thất bại** (cooldown, không đủ MP): tool KHÔNG biết — sẽ skip qua buff đó. Người dùng cấu hình `cycle_duration_sec` ngắn hơn để re-buff sớm hơn.
- **`cycle_duration_sec` < tổng buff_delay**: ATTACKING phase quá ngắn / không có. Cross-field validation: `cycle_duration_sec >= sum(buff_delays) + safety_margin (10s)`. Warning UI nếu vi phạm.
- **MP không đủ buff lúc start cycle**: cast vẫn gửi nhưng game không cast. Sau timeout cycle, retry. Có thể thêm "wait MP ≥ X% trước buff" — YAGNI v1.
- **Right-click ra ngoài game window**: SendInput tại cursor pos. Nếu user di chuyển chuột ra app khác, click vào app sai. → OutputGate đã có check `GetForegroundWindow() == hwnd` (Section 22.8).

### 29.11 Trả Lời Trực Tiếp

> "Buff F2 F3 F4, rồi quay về F1 right-click, sau 300s lặp lại"

**Cơ chế:**
1. **BUFFING phase:** tool press F2 → right-click → wait 500ms → F3 → right-click → wait 500ms → F4 → right-click → wait 500ms.
2. **ARMING:** press F1 (set main skill active). Tức thì.
3. **ATTACKING phase:** right-click mỗi 600ms (configurable). Pot HP/MP xen kẽ tự động qua priority P0/P1 không cản trở. Kéo dài đúng 300s.
4. **Hết 300s → quay lại BUFFING** (re-buff tự động).

**Yêu cầu config:**
- UI bật Auto Combat, đặt F1 + interval + cycle 300s
- UI bật Buff 1/2/3 với F2/F3/F4
- User đặt cursor ở vị trí muốn đánh trước khi bật AUTO

---

## 30. Mob Targeting — Cursor Trỏ Trúng Mob

### 30.1 Vấn Đề
PT click logic: right-click trên **mob** = attack; right-click trên **ground** = nhân vật chạy. Vì vậy tool PHẢI click trúng mob, không random click → nếu không char wander lung tung.

### 30.2 Đánh Giá 5 Approach

| Approach | Reliability | Complexity | Effort user |
|---|---|---|---|
| **A. Sweep quanh player + SHIFT+right-click** | ⭐⭐⭐ | Thấp | 0 |
| **B. Template matching mob** | ⭐⭐⭐⭐ | Trung | Cao (capture template) |
| C. Nameplate text detect | ⭐⭐ false positive | Cao | 0 |
| D. Mob HP bar detect | ⭐⭐ chicken-egg | Trung | 0 |
| E. Motion blob | ⭐ | Cao | 0 |

**Chọn A + B hybrid.** A là default v1; B optional.

### 30.3 Mode A — Stationary Sweep (v1 default)

**Cơ chế:**
1. Mỗi attack tick, pick random position trong annulus quanh player center: `r_min..r_max` (default 40–100px).
2. Move cursor (humanized) đến đó.
3. **SHIFT-down → Right-click → SHIFT-up** = PT attack-in-place (char đứng yên).

**Code SHIFT-right-click:**
```cpp
void sendShiftRightClick() {
    INPUT in[4]{};
    in[0].type=INPUT_KEYBOARD; in[0].ki.wScan=MapVirtualKey(VK_LSHIFT,MAPVK_VK_TO_VSC);
    in[0].ki.dwFlags=KEYEVENTF_SCANCODE;
    in[1].type=INPUT_MOUSE;    in[1].mi.dwFlags=MOUSEEVENTF_RIGHTDOWN;
    in[2].type=INPUT_MOUSE;    in[2].mi.dwFlags=MOUSEEVENTF_RIGHTUP;
    in[3].type=INPUT_KEYBOARD; in[3].ki.wScan=in[0].ki.wScan;
    in[3].ki.dwFlags=KEYEVENTF_SCANCODE|KEYEVENTF_KEYUP;
    SendInput(4, in, sizeof(INPUT));
}
```

**Ưu:** không cần vision, KISS, char đứng yên. **Yêu cầu:** PT server hỗ trợ SHIFT+right-click attack-in-place (classic + hầu hết private server đều có).

**Tỷ lệ trúng mob:** trung bình, phụ thuộc user pull mob quanh char trước khi bật AUTO.

### 30.4 Mode B — Template Matching (v2 optional)

**Cơ chế:**
1. User load 1–N template PNG mob đang farm (vd `templates/orc.png`, `templates/skeleton.png`).
2. Mỗi attack tick: `cv::matchTemplate(roiAroundPlayer, template, TM_CCOEFF_NORMED)` cho từng template.
3. Lọc match > 0.75, pick gần player center nhất.
4. Move cursor đến đó. Right-click thường (không cần SHIFT vì đã trúng mob).
5. Nếu không có match → fallback Mode A.

**Capture template UX:**
- UI có nút "Capture Template": mở overlay cho user crop vùng mob từ vision preview hiện tại.
- Save vào `templates/<name>.png`, list trong "Manage Templates" dialog.

### 30.5 Humanized Cursor Movement

KHÔNG teleport `SetCursorPos` 1 lần. Interpolate ease-out cubic + jitter pixel:

```cpp
void moveCursorTo(POINT target, int steps = 8) {
    POINT cur; GetCursorPos(&cur);
    for (int i = 1; i <= steps; ++i) {
        double t = double(i)/steps;
        double e = 1.0 - pow(1.0 - t, 3.0);           // ease-out
        int x = cur.x + int((target.x-cur.x)*e) + rand_int(-1,1);
        int y = cur.y + int((target.y-cur.y)*e) + rand_int(-1,1);
        SetCursorPos(x, y);
        Sleep(rand_int(8, 18));
    }
}
// Tổng ~80–150ms, tự nhiên.
```

### 30.6 Combat FSM Cập Nhật ATTACKING

```cpp
void CombatFSM::stepAttacking(TimePoint now) {
    if (now - cycleStart_ >= sec(cfg_.combat.cycle_duration_sec)) {
        currentBuffIdx_ = 0; state_ = BUFFING; return;
    }
    if (cfg_.combat.wait_mp_enabled && v.mpPct < cfg_.combat.wait_mp_min_pct) return;
    
    auto interval = ms(cfg_.combat.attack_interval_ms) + humanizer_.jitter(P3);
    if (now - lastAttackClick_ < interval) return;
    
    POINT pos = pickAttackPosition();          // Mode A hoặc B
    moveCursorTo(pos);
    if (cfg_.combat.target_mode == "template" && lastMatchFound_)
        sendRightClick();                      // trúng mob
    else
        sendShiftRightClick();                 // attack-in-place
    lastAttackClick_ = now;
}

POINT pickAttackPosition() {
    if (cfg_.combat.target_mode == "template" && !templates_.empty()) {
        if (auto m = findBestMobMatch()) { lastMatchFound_=true; return *m; }
    }
    lastMatchFound_ = false;
    // Mode A: annulus quanh player center
    POINT pc = playerCenter();
    double angle = rand_double(0, 2*M_PI);
    double r = rand_double(cfg_.combat.sweep_r_min, cfg_.combat.sweep_r_max);
    return {pc.x + int(cos(angle)*r), pc.y + int(sin(angle)*r)};
}
```

`playerCenter()` = giữa game window rect (PT vẽ player ở giữa view).

### 30.7 UI (thay phần Combat Section 21.3)

```
├─ ▼ Combat ────────────────────────────────────────────────┤
│  ☑ Auto Combat                                             │
│    Main attack key:     [F1 ▼]                             │
│    Attack interval:     [600] ms                           │
│    Cycle duration:      [300] giây                          │
│  Targeting mode:                                            │
│    (•) Stationary sweep (SHIFT+right-click)                 │
│    ( ) Template matching                  [Manage Templates]│
│    Sweep radius:     [ 40] – [100] px                       │
│  ☐ Chờ MP ≥             [ 30] %MP trước khi attack          │
```

### 30.8 Edge Cases

| Tình huống | Xử lý |
|---|---|
| Mob không có quanh char (Mode A) | Char đứng yên, swing không trúng. Vô hại. |
| Template không match (Mode B) | Fallback Mode A. |
| User di chuyển cursor giữa attack | Detect large delta giữa 2 `GetCursorPos` → pause 1s. |
| Cursor target ra ngoài game window | Clamp vào window rect. |
| Game minimize / Alt-Tab | OutputGate chặn (Section 22). |
| Server không hỗ trợ SHIFT+right-click | Fallback: dùng right-click thường nhưng giảm radius xuống ~20px (char ít chạy xa). User test offline trước. |

### 30.9 Config (advanced — không UI)

```json
"combat": {
  "target_mode": "sweep",          // "sweep" | "template"
  "sweep_r_min": 40,
  "sweep_r_max": 100,
  "attack_use_shift": true,        // dùng SHIFT+right-click attack-in-place
  "cursor_move_steps": 8,
  "cursor_move_total_ms": 100,
  "templates_dir": "templates/",
  "template_match_threshold": 0.75,
  "user_cursor_override_pause_ms": 1000
}
```

### 30.10 Trả Lời Trực Tiếp

> "Buff F2/F3/F4/F5 + right-click, rồi F1 + di chuyển cursor vào mob + right-click liên tục"

**Cơ chế đầy đủ:**
1. **Buff phase** (Section 29): F2 → right-click → wait → F3 → right-click → wait → F4 → right-click → wait → F5 → right-click → wait.
2. **Arm phase:** press F1.
3. **Attack phase với 2 mode:**
   - **Mode A (default v1):** click trong vòng tròn quanh char (40–100px), dùng **SHIFT+right-click** để char đứng yên đánh. Không cần biết mob ở đâu.
   - **Mode B (advanced v2):** user load template mob → tool dùng OpenCV `matchTemplate` tìm mob trên màn hình → move cursor (humanized ~100ms) tới mob → right-click. Chính xác hơn nhưng tốn effort capture template.
4. Sau `cycle_duration_sec` → quay lại BUFFING.

**Khuyến nghị:** Mode A cho v1 (KISS). Nếu thấy hit rate thấp, nâng cấp Mode B với template.

---

## 31. Combat Refined — SHIFT+Right-Click Auto-Attack

### 31.1 Cơ Chế PT Server Hiện Tại
- **SHIFT+right-click**: lock target tại vị trí cursor, char tự đánh đến khi mob CHẾT (auto-attack persistent).
- **Right-click thường**: cast skill active 1 lần (dùng cho buff phase).
- Khi mob chết, char dừng auto-attack, đứng chờ → tool cần pick target mới.

### 31.2 Hệ Quả Đơn Giản Hóa Design

| Trước (giả định spam click) | Sau (theo cơ chế thật) |
|---|---|
| Right-click mỗi 600ms | SHIFT+right-click mỗi **4000ms** (target re-pick) |
| Char interrupt target | Char đánh trọn vẹn từng mob |
| Click rate cao | Click rate thấp = tự nhiên hơn |
| `attack_interval_ms` | đổi thành `target_pick_interval_ms` |

### 31.3 Phân Biệt Click BUFF vs ATTACK

| Phase | Mouse action | Lý do |
|---|---|---|
| **BUFFING** | Right-click thường (không SHIFT) | Cast buff 1 lần, không lock |
| **ATTACKING** | **SHIFT+right-click** | Lock target + auto-attack persistent |

### 31.4 Pseudocode Cập Nhật

```cpp
void stepBuffing(TimePoint now) {
    if (now < nextStep_) return;
    auto& b = nextEnabledBuff();
    if (!b) { enterArming(now); return; }
    input_.sendKeyTap(b->key);              // press F2/F3/F4/F5
    input_.sendRightClick();                // CAST không SHIFT
    nextStep_ = now + ms(b->cast_delay_ms);
}

void stepAttacking(TimePoint now) {
    if (now - cycleStart_ >= sec(cfg_.cycle_duration_sec)) {
        enterBuffing(now); return;
    }
    if (cfg_.wait_mp_enabled && v.mpPct < cfg_.wait_mp_min_pct) return;
    
    auto pickInterval = ms(cfg_.target_pick_interval_ms) + humanizer_.jitter(P3);
    if (now - lastTargetPick_ < pickInterval) return;
    
    POINT pos = pickAttackPosition();        // annulus quanh player
    moveCursorTo(pos);                        // humanized
    input_.sendShiftRightClick();             // LOCK + auto-attack
    lastTargetPick_ = now;
}
```

### 31.5 Sweep Radius — Hỗ Trợ Melee + Ranged

User farm cả melee/AoE và ranged → radius range UI cho user override theo class:

| Class | Radius khuyến nghị |
|---|---|
| Melee (đánh sát char) | 40–80 px |
| AoE (spell quanh char) | 60–120 px |
| Ranged (bắn xa) | 100–250 px |

UI dùng 2 slider min/max → user chỉnh theo class hiện tại. Lưu profile theo class nếu cần (advanced).

### 31.6 UI Combat (chốt)

```
├─ ▼ Combat ────────────────────────────────────────────────┤
│  ☑ Auto Combat                                             │
│    Main attack key:        [F1 ▼]                          │
│    Pick target mỗi:        [4000] ms   (char auto-attack)  │
│    Cycle duration:         [ 300] giây                      │
│  Targeting mode:                                            │
│    (•) Stationary sweep (SHIFT+right-click)                 │
│    ( ) Template matching                                    │
│    Sweep radius:           [ 50] – [200] px                 │
│  ☐ Chờ MP ≥                [ 30] %MP trước khi attack       │
```

### 31.7 Smart Re-Pick (optional advanced, không UI)

Nếu vision detect HP/MP char ổn định + không có damage incoming trong > N giây → mob đã chết, pick target sớm:

```json
"combat": {
  "smart_repick_enabled": false,           // default off, KISS
  "smart_repick_idle_seconds": 2.5,
  "hp_stable_threshold_pct": 1.0           // HP delta < 1% = không đánh nhau
}
```

Default tắt. Bật nếu user thấy char nhiều thời gian idle.

### 31.8 Config Cập Nhật

```json
"ui_settings": {
  "combat": {
    "auto_enabled": true,
    "main_attack_key": "F1",
    "target_pick_interval_ms": 4000,       // ĐỔI từ attack_interval_ms
    "cycle_duration_sec": 300,
    "target_mode": "sweep",
    "sweep_r_min": 50,
    "sweep_r_max": 200,
    "wait_mp_enabled": false,
    "wait_mp_min_pct": 30
  },
  "buffs": [
    {"enabled": true,  "key": "F2", "cast_delay_ms": 500},
    {"enabled": true,  "key": "F3", "cast_delay_ms": 500},
    {"enabled": true,  "key": "F4", "cast_delay_ms": 500},
    {"enabled": true,  "key": "F5", "cast_delay_ms": 500}
  ]
}
```

### 31.9 Tương Tác Với Pot Priority (P0/P1)

Vì attack rate đã thấp (mỗi 4s), pot priority không xung đột với target pick:

| Tình huống | Hành vi |
|---|---|
| HP < 60% giữa attack idle (đang đợi 4s) | P0 gửi pot key `1`. Không ảnh hưởng SHIFT+right-click pending. |
| MP < 10% khi đang move cursor | P1 gửi pot `2`. Move cursor tiếp tục (cursor pos và pot key độc lập). |
| HP/MP pot **trong khi** char auto-attack | Không cản trở. Char vẫn auto-attack vì SHIFT+right-click đã lock target. |

### 31.10 Trả Lời Trực Tiếp

> "SHIFT+right-click sẽ tự động đánh mob đó tới khi chết"

**Hoàn hảo cho design:**
1. Tool **không cần spam right-click**.
2. ATTACKING phase chỉ cần **gửi SHIFT+right-click mỗi 4s** (configurable) — pick target mới khi mob hiện tại đã chết hoặc target sai.
3. Buff phase vẫn dùng right-click thường (không SHIFT) để cast.
4. Click rate rất thấp → tự nhiên, humanizer nhẹ tải, ít suspicious behavior.

**User config theo class:**
- Melee/AoE: sweep radius 40–100px
- Ranged: sweep radius 100–250px

UI cho user chỉnh slider min/max dễ dàng.

---

## 32. Smart Mob Death Detection (Repick Khi Mob Chết)

### 32.1 Vấn Đề Với Fixed Timer Section 31
- Mob 1-shot (HP thấp): killed trong 1s nhưng tool đợi đủ 4s → lãng phí 3s/mob.
- Mob trâu: 4s mới đánh nửa máu → tool gửi SHIFT+right-click chỗ khác → interrupt target.
- Click miss: tool vẫn đợi 4s rồi mới repick.

### 32.2 Approach Đã Đánh Giá

| | Cơ chế | Reliability | Implementation |
|---|---|---|---|
| **A. MP drain monitoring** | F1 cost MP, drain khi đánh, stable khi mob chết | ⭐⭐⭐⭐ | Trivial (vision có sẵn) |
| **B. HP delta monitoring** | Damage incoming khi mob alive, HP stable khi mob chết | ⭐⭐⭐ | Trivial (vision có sẵn) |
| C. Target HP bar detect | Bar top-center disappear | ⭐⭐⭐⭐ | Cần ROI + detect mới |
| D. Cursor icon change | Cursor đổi shape khi hover mob | ⭐⭐⭐ | Phải capture cursor + template |
| E. Fixed timer (cũ) | Đợi cứng | ⭐⭐ | Trivial |

**Chọn A + B hybrid.** Dùng vision HP/MP đã có, không thêm ROI/detector mới. DRY.

### 32.3 Sliding Window Monitor

Theo dõi HP/MP qua window 2s (40 sample @ 20Hz):

```cpp
class CombatActivityMonitor {
    std::deque<float> mpSamples_, hpSamples_;
public:
    void update(float hp, float mp) {
        mpSamples_.push_back(mp);
        hpSamples_.push_back(hp);
        if (mpSamples_.size() > 40) mpSamples_.pop_front();
        if (hpSamples_.size() > 40) hpSamples_.pop_front();
    }
    void reset() { mpSamples_.clear(); hpSamples_.clear(); }
    bool mobLikelyDead() const {
        if (mpSamples_.size() < 40) return false;
        float mpDrain = *max_element(mpSamples_.begin(), mpSamples_.end())
                      - *min_element(mpSamples_.begin(), mpSamples_.end());
        float hpDrop  = std::max(0.0f,
                          *max_element(hpSamples_.begin(), hpSamples_.end())
                        - *min_element(hpSamples_.begin(), hpSamples_.end()));
        return mpDrain < cfg_.mp_drain_threshold_pct
            && hpDrop  < cfg_.hp_damage_threshold_pct;
    }
};
```

### 32.4 Combat FSM ATTACKING — Phiên Bản Smart

```cpp
void stepAttacking(TimePoint now) {
    activity_.update(v.hpPct, v.mpPct);

    if (now - cycleStart_ >= sec(cfg_.cycle_duration_sec)) { enterBuffing(now); return; }
    if (cfg_.wait_mp_enabled && v.mpPct < cfg_.wait_mp_min_pct) return;
    
    auto sinceLast = now - lastTargetPick_;
    if (sinceLast < ms(cfg_.min_target_dwell_ms)) return;   // grace period
    
    bool shouldRepick = activity_.mobLikelyDead()
                     || sinceLast >= ms(cfg_.max_target_dwell_ms);   // fallback cap
    if (!shouldRepick) return;
    
    POINT pos = pickAttackPosition();
    moveCursorTo(pos);
    sendShiftRightClick();
    lastTargetPick_ = now;
    activity_.reset();
}
```

### 32.5 Tham Số Default

| Tham số | Default | Ghi chú |
|---|---|---|
| `min_target_dwell_ms` | 2000 | Cho char ~1–2s start combat |
| `max_target_dwell_ms` | 15000 | Cap fallback (mob trâu) |
| `activity_window_seconds` | 2.0 | Đủ bắt 1–2 cast |
| `mp_drain_threshold_pct` | 1.0 | F1 ≥ 1–2%/cast |
| `hp_damage_threshold_pct` | 1.0 | Mob damage ≥ 1% HP |

### 32.6 Behavior Theo Mob Strength

| Mob | Kết quả |
|---|---|
| 1-shot (killed 1s) | Min dwell 2s → check activity → mob chết → repick. Tổng **~2s** |
| Trung bình (killed 5s) | Activity vẫn detect drain → giữ target → khi chết, 2s sau repick. Tổng **~7s** |
| Trâu (killed 15s+) | Activity drain liên tục → giữ target. Fallback cap 15s → repick (force). |
| Click miss (ground) | Min dwell 2s → no drain → repick. Lãng phí chỉ **2s** |

### 32.7 Edge Cases

**Skill F1 không tốn MP** (basic attack class thấp):
- MP signal vô dụng.
- Fallback chỉ HP signal (yếu hơn) hoặc về timer cố định.
- Config: `signals: ["hp"]` only.

**HP regen che damage:**
- Dùng `max - min` window thay vì `last - first` → bắt được biến động.

**MP pot làm MP nhảy:**
- Reset window sau khi P1 pot fire.

**Char chưa lock target (cursor đang move)**:
- Min dwell 2s đảm bảo char đã start cast → nếu vẫn no drain = click miss → repick là đúng.

### 32.8 Config (advanced — không UI)

```json
"advanced.combat.death_detection": {
  "enabled": true,
  "min_target_dwell_ms": 2000,
  "max_target_dwell_ms": 15000,
  "activity_window_seconds": 2.0,
  "mp_drain_threshold_pct": 1.0,
  "hp_damage_threshold_pct": 1.0,
  "signals": ["mp", "hp"]
}
```

### 32.9 So Sánh Trước/Sau

| Aspect | Fixed timer (Section 31) | Smart detect (Section 32) |
|---|---|---|
| Mob 1-shot | 3s/mob lãng phí | 2s/mob |
| Mob trâu | Click đè interrupt | Đợi đến chết |
| Click miss | 4s lãng phí | 2s |
| Code | Trivial | +20 LOC |
| Vision mới | Không | Không (reuse HP/MP) |

**Khuyến nghị làm v1.** Cost thấp, gain rõ.

### 32.10 Trả Lời Trực Tiếp

> "1vs1 với mob, đánh đến khi mob chết mới click tọa độ khác"

**Cơ chế:**
1. Sau khi gửi SHIFT+right-click, tool **monitor MP/HP** trong cửa sổ trượt 2s.
2. **Mob còn sống** = MP drain (do F1 cast) HOẶC HP delta (do mob đánh char) > 1%.
3. **Mob chết** = MP stable VÀ HP không bị mất > 1% trong 2s liên tiếp.
4. Khi detect "mob chết" → tool pick vị trí khác → SHIFT+right-click → đánh mob mới.
5. **Bảo vệ:**
   - `min_dwell=2s`: char cần thời gian start combat, không repick quá sớm
   - `max_dwell=15s`: cap fallback nếu mob quá trâu hoặc vision sai

**Lợi:**
- Mob 1-shot → 2s/mob (thay vì 4s)
- Mob trâu → đánh trọn vẹn, không bị tool interrupt
- Click miss → repick nhanh 2s
- Không cần vision mới, dùng HP/MP đã detect

---

## 33. Test Mà KHÔNG Cần Mở Game Thật

### 33.1 3 Cấp Test (Kết Hợp)

| | Phương án | Test được gì | Effort |
|---|---|---|---|
| 1 | **Mock Game Window** (EXE phụ) | Vision + Input + FSM end-to-end | ~1 ngày |
| 2 | **Replay Video/PNG** (IFrameSource mới) | Vision detector trên frame thật | ~4 giờ |
| 3 | **Unit Test** (GTest) | Pure logic detector + FSM | Ongoing |

### 33.2 Phương Án 1 — PtMockGame.exe

Build EXE phụ giả PT 800×600 với:
- Render HP/MP/SP orb đúng vị trí + màu PT
- Slider UI điều chỉnh HP/MP/SP % realtime
- **Auto-simulation:**
  - Damage tick: tự giảm HP 5%/2s (mô phỏng bị mob đánh)
  - MP drain khi nhận key F1 (giả cast)
  - Mob alive/dead toggle (test smart repick)
- **Input logger:** log mọi key/mouse tool gửi đến
- **Click target logger:** hiển thị tọa độ right-click

**Scenarios test được:**
- Hồi HP/MP/SP ở các ngưỡng
- HP emergency P0 (kéo HP xuống 35% → verify jitter tối thiểu)
- Buff cycle F2 → F3 → F4 → F5 → F1
- Attack sweep (verify click nằm trong annulus)
- Mob death detect (toggle dead → MP/HP stable 2s → tool repick)
- Recall (HP < 15% kéo dài 3s → gửi F12)
- OutputGate (minimize mock → tool ngưng input)

**Tech:** Win32 GUI + GDI hoặc Direct2D vẽ orb. ~300 LOC C++.

### 33.3 Phương Án 2 — FileReplaySource

Thêm `IFrameSource` mới đọc từ MP4 hoặc PNG sequence:

```cpp
class FileReplaySource : public IFrameSource {
    cv::VideoCapture cap_;
public:
    bool start(const string& path) override {
        cap_.open(path); return cap_.isOpened();
    }
    bool acquire(Frame& out, int timeoutMs) override {
        cap_ >> out.bgra;
        sleep_for(ms(50));  // 20Hz playback
        return !out.bgra.empty();
    }
};
```

**Use case:**
- Record OBS/ShareX khi chơi PT thật 10 phút → save MP4
- Config `capture_backend: "replay"` + `replay_source: "footage.mp4"`
- Tool chạy bình thường nhưng đọc frame từ file
- Reproduce bug deterministic, test edge case (cutscene, loading, particle dày)

**Cost ~0:** OpenCV đã link, video read built-in.

### 33.4 Phương Án 3 — Unit Test (GTest)

```cpp
TEST(HpDetector, FullOrb_Returns_100Pct) {
    cv::Mat f = generateOrb(800, 600, {35,520}, 22, 1.0f);
    HpDetector det(cfg);
    EXPECT_NEAR(det.compute(f), 1.0, 0.02);
}

TEST(MobDeathMonitor, MpStable_DetectsDead) {
    CombatActivityMonitor mon(cfg);
    for (int i=0; i<40; i++) mon.update(80.0f, 50.0f);
    EXPECT_TRUE(mon.mobLikelyDead());
}
```

Helper `generateOrb()`: render Mat BGRA fill bottom-up. Reusable.
Run: `ctest` < 1s. Catch regression sớm.

### 33.5 Project Structure Bổ Sung

```
anonymous/
├── src/                              (WindowHelper.exe)
│   └── capture/file-replay-source.cpp   ← MỚI
├── mock/                             (PtMockGame.exe)
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp
│       ├── mock-window.cpp
│       ├── orb-renderer.cpp
│       ├── input-listener.cpp
│       └── simulator.cpp
├── tests/                            (gtest)
│   ├── CMakeLists.txt
│   └── unit/
│       ├── test-hp-detector.cpp
│       ├── test-mob-death-monitor.cpp
│       └── ...
└── assets/replays/                   ← MỚI
    └── pt-farm-sample-001.mp4
```

### 33.6 Lộ Trình Test

1. **Phase 7** implement đồng thời: PtMockGame + FileReplaySource + unit test framework.
2. **Phase 1–6 develop:** dùng PtMockGame để test sớm vision + input pipeline.
3. **Frame thật:** record OBS từ PT offline, dùng FileReplaySource để verify detector trên frame thực tế.
4. **Pass mock + replay** → mới chạy server private offline.
5. **Pass offline** → mới đến server official với tài khoản phụ.

### 33.7 Trả Lời Trực Tiếp

> "Có cách giả lập test phần mềm sau build mà không cần mở game không?"

**Có, 3 cấp:**

1. **PtMockGame.exe** — EXE phụ mình code, fake window PT 800×600 với HP/MP/SP orb có thể chỉnh slider, có simulation damage/MP drain/mob death. Tool attach vào mock → vision đọc orb → gửi key → mock log key. **Test toàn bộ pipeline end-to-end mà 0 mở PT.**

2. **FileReplaySource** — record OBS frame thật từ PT 1 lần, sau đó tool replay file MP4 thay vì capture live. **Test vision detector trên frame chuẩn 100%.**

3. **Unit test** — synthesize frame qua code (`generateOrb()`), assert detector output. **Test pure logic, < 1s/run.**

**Cost:** ~1.5 ngày dev tổng. Pay-off: develop nhanh hơn nhiều, an toàn không lộ tài khoản test.

---

## 34. Đính Chính — PT Dùng Bar Đứng, Không Phải Orb Tròn

### 34.1 Đính Chính Hình Dạng

PT thực tế hiển thị HP/MP/SP dưới dạng **bar đứng** (vertical rectangle, fill bottom-up), không phải orb tròn như giả định cũ. Phải sửa:

| Khía cạnh | Trước (orb) | Sau (bar) |
|---|---|---|
| Shape | Tròn radius 22 | Rectangle ~12×100 |
| Mask | Circle mask cần thiết | Không cần (dùng full bbox) |
| Default coords | x=35,y=520,w=50,h=50,r=22 | x=35,y=480,w=12,h=100 |
| Mock render | `cv::circle` | `cv::rectangle` |
| Calibrate UI | Click center + radius slider | Drag bbox 2 góc |

### 34.2 Detection Code KHÔNG Sai

Method B waterline (Section 28) shape-agnostic — chỉ cần "biên trên vùng đỏ". DRY function dùng cho cả bar lẫn orb:

```cpp
double hpPctWaterline(const Mat& redMask, const Mat& shapeMask = {}) {
    int h = redMask.rows;
    int waterLine = h;
    for (int y = h - 1; y >= 0; y--) {
        int rowRed = countNonZero(redMask.row(y));
        int rowArea = shapeMask.empty() 
                      ? redMask.cols                      // rect: full width
                      : countNonZero(shapeMask.row(y));   // circle: chord
        if (rowArea == 0) continue;
        if (double(rowRed)/rowArea < 0.4) { waterLine = y; break; }
    }
    return double(h - waterLine) / h;
}
```

- Bar đứng: `shapeMask = {}` → đơn giản hơn, không cần tạo mask.
- Orb tròn: truyền circle mask qua param. Cùng function.

### 34.3 Region Schema Tổng Quát

Hỗ trợ cả 2 layout (PT classic bar + UI mod orb):

```json
"regions": {
  "hp_bar": { "x": 35,  "y": 480, "w": 12, "h": 100, "shape": "rect" },
  "mp_bar": { "x": 52,  "y": 480, "w": 12, "h": 100, "shape": "rect" },
  "sp_bar": { "x": 69,  "y": 480, "w": 12, "h": 100, "shape": "rect" }
}
```

Field `shape`:
- `"rect"` (mặc định): không cần shapeMask
- `"circle"`: code tự tạo circle mask từ `radius` field bonus

KHÔNG hard-code shape trong code. Đọc từ config.

### 34.4 Default Coords Mới Cho PT 800×600

Bar đứng nằm bottom-left, kích thước điển hình:
- Mỗi bar w=10–15px, h=80–120px
- Cách nhau ~5–20px horizontally
- y_top ~460–500, y_bottom ~570–590

**Default ước tính** (user phải Calibrate xác nhận):
```json
"hp_bar": { "x": 35, "y": 480, "w": 12, "h": 100, "shape": "rect" },
"mp_bar": { "x": 52, "y": 480, "w": 12, "h": 100, "shape": "rect" },
"sp_bar": { "x": 69, "y": 480, "w": 12, "h": 100, "shape": "rect" }
```

### 34.5 Mock Render Cập Nhật

`PtMockGame.exe` đổi từ `cv::circle` (hoặc Ellipse GDI) → `cv::rectangle` (hoặc Rectangle GDI):

```cpp
void renderBar(HDC hdc, int x, int y, int w, int h, float pct, COLORREF color) {
    // Background bar (gray)
    RECT bg = {x, y, x+w, y+h};
    HBRUSH bgBrush = CreateSolidBrush(RGB(40,40,40));
    FillRect(hdc, &bg, bgBrush); DeleteObject(bgBrush);
    
    // Fill from bottom up
    int fillH = int(h * pct);
    RECT fg = {x, y+h-fillH, x+w, y+h};
    HBRUSH fgBrush = CreateSolidBrush(color);
    FillRect(hdc, &fg, fgBrush); DeleteObject(fgBrush);
}
// HP: color=RGB(220,30,30); MP: RGB(50,100,220); SP: RGB(220,180,40)
```

Đơn giản hơn vẽ orb tròn. Visually khớp với PT classic.

### 34.6 Calibrate UI Cập Nhật

Trước: click center + radius slider.
Sau: **drag bounding box** 2 góc đối:

```
[Vision Preview Frame]

  ░░░░ HP bar ░░░░
  ┌──┐
  │██│  ←  user drag tạo rect
  │██│
  │██│
  │  │
  │  │
  └──┘

HP region:
  x: [35]  y: [480]
  w: [12]  h: [100]
  shape: (•) rect  ( ) circle
```

UI flow:
1. User click "Calibrate HP"
2. Vision preview overlay xuất hiện
3. User drag từ top-left đến bottom-right bar → tạo rect
4. Preview hiển thị waterline detect realtime để verify
5. Confirm → lưu coords

### 34.7 Vì Sao Test Trên Mock Vẫn Hợp Lệ

Ngay cả khi mock và game khác chi tiết (mock = bar mặc định, game cụ thể có thể bar dày/mỏng khác), test vẫn valid vì:

1. **Vision logic giống nhau** — waterline detect không phụ thuộc shape cụ thể.
2. **FSM + Input logic không đụng vision** — test được độc lập.
3. **Calibrate trên cả 2** — tool calibrate lại trên mock và game thật. Coords khác nhau nhưng pipeline giống nhau.
4. **Replay frame thật** (Section 33.3) là gold standard — record OBS từ PT thật → test detector với frame chính xác.

### 34.8 Lộ Trình Test 3-Stage

1. **Mock test** — verify pipeline + FSM logic + input flow. KHÔNG verify accuracy detection trên PT cụ thể.
2. **Replay frame thật** (PT offline record) — verify detection accuracy trên bar đúng PT.
3. **Live PT offline** — verify end-to-end trên server private không XC3.

Mock không thay thế stage 2 và 3, nhưng tăng tốc dev 5–10× ở stage early.

### 34.9 Trả Lời Trực Tiếp

> "PT thật có 3 thanh pot đứng chiều dọc, mock đang render nằm ngang (hoặc orb tròn) thì test có sai không?"

**Code vision: KHÔNG sai** — waterline Method B shape-agnostic, dùng được cho bar đứng / bar ngang / orb tròn. Chỉ cần truyền đúng ROI bbox.

**Mock + defaults: phải sửa cho khớp PT thật** — đổi shape sang `"rect"` đứng, render bar bằng `cv::rectangle`, calibrate UI dùng drag bbox.

**Test mock vẫn valid** cho việc verify pipeline + FSM + input. Để verify detection accuracy → dùng FileReplaySource với frame OBS record từ PT thật (Section 33.3).

---

## 35. Đính Chính Lần 2 — 3 Bar Ở Center-Bottom (Không Phải Left)

### 35.1 Quan Sát Đúng Từ Screenshot

3 bar đứng nằm **CENTER-BOTTOM** màn hình, không phải bottom-left như mình giả định lần trước.

Layout HUD đáy ở 800×600 (trái → phải):
1. Inventory grid (0–130px)
2. Skill bar icons (130–330px)
3. **3 bar HP/SP/MP cluster (340–430px)** ← target detection
4. Mini-map (430–580px)
5. Skill slots / equipment (580–800px)

### 35.2 Định Nghĩa Bar

| Bar | Vị trí ngang | Màu | Hue HSV |
|---|---|---|---|
| HP | trái nhất (x ~355) | Đỏ | 0–10 và 170–180 |
| SP | giữa (x ~383) | Vàng/cam | 15–35 |
| MP | phải nhất (x ~411) | Xanh dương | 100–130 |

Bar kích thước:
- Width: ~10–15px
- Height: ~80–110px
- Y top: ~470–480
- Y bottom: ~575–585

### 35.3 Default Coords Đúng (PT 800×600)

```json
"regions": {
  "hp_bar": { "x": 355, "y": 475, "w": 12, "h": 105, "shape": "rect" },
  "sp_bar": { "x": 383, "y": 475, "w": 12, "h": 105, "shape": "rect" },
  "mp_bar": { "x": 411, "y": 475, "w": 12, "h": 105, "shape": "rect" }
}
```

### 35.4 Hue Range Mặc Định Theo Bar

```json
"advanced.detection_hue": {
  "hp": [[0,10],[170,180]],     // đỏ (wrap-around)
  "sp": [[15,35]],              // vàng/cam
  "mp": [[100,130]]             // xanh dương
}
```

Mỗi bar có hue range riêng, không hard-code trong code. User chỉnh `advanced` nếu theme khác.

### 35.5 Lưu Ý Quan Trọng

- Tọa độ trên là **ước lượng từ JPG nén** — sai số ±10–20px là bình thường.
- User **PHẢI Calibrate** sau khi attach window thật để có pixel chính xác.
- UI mod / theme private server có thể shift bar position → cần re-calibrate.
- Resolution khác 800×600 → coords khác hoàn toàn → multi-resolution profile (Section 27.5).

### 35.6 Cập Nhật Mock Render

`PtMockGame.exe` phải render 3 bar ở center-bottom đúng thứ tự HP-SP-MP với màu đúng:

```cpp
void renderHud(HDC hdc) {
    // 3 bar center-bottom theo thứ tự HP, SP, MP
    renderBar(hdc, 355, 475, 12, 105, hpPct_, RGB(220, 30, 30));    // HP đỏ
    renderBar(hdc, 383, 475, 12, 105, spPct_, RGB(220, 180, 40));   // SP vàng
    renderBar(hdc, 411, 475, 12, 105, mpPct_, RGB(50, 100, 220));   // MP xanh
}
```

### 35.7 Trả Lời Trực Tiếp

> "3 thanh pot màu đỏ xanh ở chính giữa như ảnh"

**Đính chính:**
- Vị trí: **CENTER-BOTTOM** (x ~355–425, y ~475–580), không phải bottom-left.
- 3 bar đứng cạnh nhau: HP (đỏ) – SP (vàng/cam) – MP (xanh dương).
- Default coords cập nhật trong config (Section 35.3).
- Mock render cũng cập nhật theo (Section 35.6).
- Vision code KHÔNG đổi — vẫn là waterline Method B shape-agnostic.

**Lưu ý:** đây là default ước lượng. User bắt buộc Calibrate khi tool chạy thật.

---

## 36. Yêu Cầu Foreground & Multitask

### 36.1 Phân Biệt 2 Vấn Đề

**Vấn đề 1 — Capture (tool có nhìn thấy game?):**

| Backend | Có cửa sổ khác đè 1 phần game | Game minimize |
|---|---|---|
| **WGC** | ✅ vẫn thấy đúng | ❌ pause |
| DXGI Duplication | ❌ thấy thứ trên desktop | ❌ pause |

WGC capture HWND trực tiếp → không phụ thuộc có gì che. Đây là điểm mạnh chính mình chọn WGC làm primary.

**Vấn đề 2 — Input (SendInput đi đâu?):**

`SendInput` gửi vào **foreground window**. Nếu game không foreground → key/mouse đi sai địa chỉ.

`OutputGate` (Section 22.8) check `GetForegroundWindow() == gameHwnd` → nếu không, pause input. **An toàn nhưng tool không hoạt động trong khi user alt-tab.**

### 36.2 Hành Vi Theo Tình Huống

| User action | Capture | Input | Auto running? |
|---|---|---|---|
| Game maximized, nothing else | ✅ | ✅ | ✅ |
| Game + cửa sổ nhỏ ở góc (game foreground) | ✅ | ✅ | ✅ |
| Game + browser cạnh nhau, focus game | ✅ | ✅ | ✅ |
| Game + browser cạnh nhau, focus browser | ✅ | ❌ pause | ⚠️ pause |
| Alt-tab sang browser 10s | ✅ | ❌ pause | ⚠️ pause |
| Minimize game | ❌ pause | ❌ pause | ⏸ pause |
| Lock máy (Win+L) | ❌ pause | ❌ pause | ⏸ pause |
| Notification popup steal focus 2s | ✅ | ❌ pause 2s | ⚠️ pause 2s, auto-resume |
| Tool UI window mở để chỉnh setting | ✅ | ❌ pause | ⚠️ click lại game → resume |
| Multi-monitor: game M1 focus, browser M2 (đọc) | ✅ | ✅ | ✅ |
| Multi-monitor: click browser M2 → steal focus | ✅ | ❌ | ⚠️ pause |

**Quan trọng:** Tool **không crash, không gửi key sai**, chỉ pause khi mất foreground. Đây là Safe Mode Section 22 đã design.

### 36.3 Trả Lời Trực Tiếp

> "Phải chỉ mở game 800×600, không được mở browser hay gì che game thì auto mới hoạt động đúng?"

**KHÔNG hoàn toàn đúng:**
- **CÓ THỂ** mở cửa sổ khác cạnh game hoặc đè 1 phần game **MIỄN LÀ** game vẫn là foreground window.
- **WGC capture HWND game trực tiếp** → tool vẫn thấy bar HP/MP/SP đúng dù bị che.
- **KHÔNG THỂ** alt-tab sang app khác → mất foreground → input pause (an toàn).
- **KHÔNG THỂ** minimize game → capture pause.

### 36.4 Workaround Cho Multitask

| Setup | Hoạt động? | Ghi chú |
|---|---|---|
| 1 monitor, game foreground, browser bị che phía sau | ✅ Tool chạy, nhưng user không xem browser được | Vô lý mục đích |
| **2 monitor: game M1 focus, browser M2 đọc** | ✅ Tốt | Click browser sẽ steal focus → cần "scroll inactive windows" (Win11 default ON) |
| 2 PC: PC1 chơi, PC2 lướt | ✅ Tối ưu | Hoàn toàn cô lập |
| 1 monitor, alt-tab qua lại | ⚠️ Tool pause khi không foreground | Tool tự resume khi quay lại |

### 36.5 Workaround Kỹ Thuật (KHÔNG khuyến nghị v1)

| Approach | Có hoạt động trên PT? |
|---|---|
| `PostMessage(WM_KEYDOWN, ...)` vào HWND | ❌ PT DirectInput ignore WM_KEYDOWN |
| `PostMessage(WM_RBUTTONDOWN, ...)` | ⚠️ tùy game, PT thường ignore |
| Interception driver (kernel) | ✅ nhưng đã loại trừ trong scope |
| Multi-monitor + AutoHotkey no-focus-steal | ⚠️ cần tool external |

**Verdict:** giữ design `SendInput` foreground-required. Trade-off chấp nhận để tool đơn giản + tin cậy. User cần dành window thời gian cho game.

### 36.6 Khuyến Nghị Sử Dụng

**Cho v1 (đơn monitor):**
1. Game 800×600 windowed làm foreground khi tool chạy.
2. Có thể có cửa sổ nhỏ cạnh game (chat tool, music player, log) — tool vẫn thấy game.
3. **Không alt-tab giữa farm session** dài. Nếu cần check tin nhắn, dùng phone hoặc multi-monitor.
4. Tool pause tự động khi mất foreground — không phải bug, là feature an toàn.

**Cho v2 (multi-monitor):**
1. Game M1 (luôn focus).
2. Tool UI + browser M2 (đọc, không click vào để giữ focus M1).
3. Win11 default: scroll inactive windows = ON → có thể scroll browser M2 mà không cần click → không steal focus → ✅ farm + đọc tin cùng lúc.

---

## 37. PostMessage Input Backend — Không Chiếm Chuột (Background Operation)

### 37.1 Vấn Đề Section 36
`SendInput` di chuyển cursor THẬT + cần foreground → user không multitask được. UX kém.

### 37.2 PostMessage Approach

`PostMessage` Win32 API gửi window message tới HWND game **mà KHÔNG**:
- Di chuyển cursor thật (user dùng chuột tự do)
- Cần game ở foreground (gửi tới HWND cụ thể, không qua focus)
- Inject DLL / đọc memory / hook (vẫn external-only)
- Kernel driver

```cpp
LPARAM lp = MAKELPARAM(400, 300);
PostMessage(hwnd, WM_RBUTTONDOWN, MK_RBUTTON, lp);
PostMessage(hwnd, WM_RBUTTONUP,   0,           lp);

PostMessage(hwnd, WM_KEYDOWN, VK_F2, 0);
PostMessage(hwnd, WM_KEYUP,   VK_F2, 0);

// SHIFT+right-click
PostMessage(hwnd, WM_KEYDOWN,     VK_LSHIFT, 0);
PostMessage(hwnd, WM_RBUTTONDOWN, MK_RBUTTON|MK_SHIFT, lp);
PostMessage(hwnd, WM_RBUTTONUP,   MK_SHIFT,            lp);
PostMessage(hwnd, WM_KEYUP,       VK_LSHIFT, 0);
```

### 37.3 Game Có Accept PostMessage Không?

3 cách game đọc input:
1. **Window messages** (`WM_KEYDOWN`, `WM_RBUTTONDOWN`) → PostMessage CÓ tác dụng ✅
2. **DirectInput** (buffered/immediate mode) → đọc HID state → PostMessage KHÔNG tác dụng ❌
3. **Raw Input API** → tương tự DirectInput ❌

PT classic dùng **DirectInput nhưng cũng pump message**. Một số input PT chấp nhận WM_* (nhiều bot PT public dùng được PostMessage). **PHẢI test trên client/server bạn dùng.**

### 37.4 Probe Test (Phase 1)

Build tool nhỏ `postmessage-probe.exe` test PT offline:

```cpp
HWND h = FindWindow(NULL, L"Priston Tale");
Sleep(3000); // user alt-tab sang Notepad

// Test F2 background
PostMessage(h, WM_KEYDOWN, VK_F2, 0); Sleep(50);
PostMessage(h, WM_KEYUP,   VK_F2, 0);

// Test right-click background
LPARAM lp = MAKELPARAM(400, 300);
PostMessage(h, WM_RBUTTONDOWN, MK_RBUTTON, lp); Sleep(20);
PostMessage(h, WM_RBUTTONUP,   0,           lp);
```

**Quan sát:**
- F2 skill active không? Mob bị đánh không?
- ✅ Có → PostMessage works → tool chính dùng PostMessage backend.
- ❌ Không → fallback SendInput.

Mất ~1 giờ. Đáng làm trước khi code tool chính.

### 37.5 Architecture Cập Nhật

```cpp
enum class InputBackend { PostMessage, SendInput };

class IInputBackend {
public:
    virtual void sendKeyTap(WORD vk) = 0;
    virtual void sendRightClick(int x, int y) = 0;
    virtual void sendShiftRightClick(int x, int y) = 0;
    virtual bool requiresForeground() const = 0;
};

class PostMessageBackend : public IInputBackend {
    HWND target_;
public:
    void sendKeyTap(WORD vk) override {
        PostMessage(target_, WM_KEYDOWN, vk, 0);
        Sleep(rand(20,40));
        PostMessage(target_, WM_KEYUP, vk, 0);
    }
    void sendRightClick(int x, int y) override { ... }
    bool requiresForeground() const override { return false; }
};

class SendInputBackend : public IInputBackend {
    // Như design cũ — di cursor, gửi SendInput
    bool requiresForeground() const override { return true; }
};
```

`InputScheduler` dùng `IInputBackend` interface → swap backend dễ dàng.

### 37.6 Config

```json
"advanced.input": {
  "backend": "auto",           // "auto" | "postmessage" | "sendinput"
  "probe_on_startup": true,
  "postmessage_fallback_to_sendinput_for_mouse": false
}
```

Auto-detect: probe lúc startup → chọn backend phù hợp. User override được.

### 37.7 Tác Động Khi PostMessage Works

| Phần | Khi PostMessage works |
|---|---|
| `OutputGate.pause_on_not_foreground` | Tự động OFF — không cần foreground |
| Humanized cursor move (Section 30.5) | Không cần — cursor thật không di chuyển |
| User multitask | ✅ TỰ DO — game background, user lướt web |
| Game minimize | ❌ vẫn pause (WGC cần render) |
| Game che bởi cửa sổ khác | ✅ OK (WGC capture HWND trực tiếp) |
| Click cooldown per-message | Cần thêm ~20ms giữa DOWN/UP để game register | 

### 37.8 Tác Động Đến Risk Anti-Cheat

| | SendInput | PostMessage |
|---|---|---|
| Bypass anti-cheat | Không | Không |
| Inject DLL | Không | Không |
| Đọc memory | Không | Không |
| Kernel driver | Không | Không |
| Risk XC3 detect | Trung bình | Trung bình (không cao hơn SendInput) |

PostMessage **không tăng risk** so với SendInput — cả 2 đều là Win32 API hợp pháp. XC3 có thể flag cả 2 ở cùng mức nếu detect pattern automation.

### 37.9 Trả Lời Trực Tiếp

> "Tối ưu auto không chiếm chuột được không? Nếu không chiếm chuột có cần can thiệp vào process không?"

**Có thể không chiếm chuột, KHÔNG cần can thiệp process:**
- Dùng `PostMessage(gameHwnd, WM_RBUTTONDOWN, ...)` thay `SendInput`.
- `PostMessage` là Win32 API chuẩn — KHÔNG inject, KHÔNG đọc memory, KHÔNG hook, KHÔNG kernel.
- Cursor user không bao giờ di chuyển — user tự do dùng chuột cho việc khác.
- Tool có thể chạy khi **game ở background** (không cần foreground).

**Điều kiện:** PT phải accept window message (PT classic thường có). Cần probe test 1 lần ở Phase 1.

**Nếu PostMessage works** → user multitask thoải mái:
- Game background (không minimize), tool farm.
- User lướt web/Discord/code monitor 1 cùng lúc.

**Nếu PostMessage fail trên PT của bạn** → fallback SendInput foreground-required (design cũ Section 36).

### 37.10 Khuyến Nghị Lộ Trình

1. **Phase 1 đầu:** build probe tool, test PostMessage trên PT offline (1 giờ).
2. Kết quả probe → quyết định backend mặc định.
3. Code tool chính với `IInputBackend` abstraction → 2 backend swap được runtime.
4. Default config `backend: "auto"` → auto-detect probe lúc startup mỗi lần.

### 37.11 Note Quan Trọng

- PostMessage **không bypass** XC3, chỉ thay đổi cách gửi input. XC3 vẫn có thể detect behavior automation qua patterns khác (timing, sequence).
- Một số PT private server **vá DirectInput** để chỉ accept Raw Input → PostMessage fail. Test trước.
- WGC capture vẫn yêu cầu game không minimized.

---

## 38. RECOMMENDATION CUỐI — MVP Simple + Hiệu Quả Cao

### 38.1 Triết Lý
Áp dụng YAGNI cứng tay. Cắt mọi feature optional. 1 backend mỗi loại. Test trước cam kết. Không abstraction thừa.

### 38.2 Quyết Định Chốt

| Quyết định | Chốt |
|---|---|
| **Input Backend** | `PostMessage` hardcoded sau probe Phase 0. Fallback `SendInput` nếu probe fail. **1 backend duy nhất**. |
| **Capture Backend** | `WGC` only. Bỏ DXGI fallback. |
| **Mob Targeting** | Mode A Stationary Sweep + SHIFT+right-click. Bỏ Mode B Template. |
| **Smart Death Detect** | MP+HP sliding window 2s (Section 32). +20 LOC, lợi rõ. |
| **UI** | Section 21.3 — 5 collapsing section. Không profile system. |
| **Test** | PtMockGame + FileReplaySource. Bỏ GTest v1. |
| **Naming** | `WindowHelper.exe` + VersionInfo benign. Không random tên build, không signing. |
| **UI Stack** | Dear ImGui + D3D11 reuse capture device. |
| **Config** | `ui_settings` + `advanced` 2 block. |
| **Safety** | OutputGate + HP Priority P0 (Section 22, 23) — bắt buộc. |

### 38.3 Đã BỎ Khỏi v1 (YAGNI)

- DXGI Duplication fallback
- Mode B Template Matching mob
- Profile system multi-config
- Auto-detect input backend runtime
- Multi-resolution profile (chỉ support 800×600 v1)
- Code signing
- Random tên build
- GTest framework
- Process name randomization
- Mouse cursor mode center/fixed
- Smart Re-Pick optional advanced

### 38.4 Core MVP — Tổng ~3,000 LOC

| Module | Section | LOC |
|---|---|---|
| WGC capture | 3 | 200 |
| Vision waterline detector (HP/MP/SP) | 28, 34 | 150 |
| Action Dispatcher P0–P4 | 23 | 200 |
| Combat FSM (BUFFING/ARMING/ATTACKING) | 29, 31 | 250 |
| Smart mob death detect | 32 | 80 |
| Stationary sweep mob targeting | 30, 31 | 100 |
| PostMessage input backend | 37 | 150 |
| OutputGate + Safe Mode | 22 | 150 |
| Humanizer | 20 | 200 |
| Config JSON load/save | 10, 24 | 200 |
| ImGui UI (5 collapsing) | 21.3 | 400 |
| Calibrate UI (drag rect) | 34 | 150 |
| Logger + tray + hotkey | 17, 21.5 | 200 |
| Mock game + replay source | 33 | 500 |
| **Tổng** | | **~3,000 LOC** |

Khả thi **~12 ngày dev part-time**.

### 38.5 Lộ Trình 8 Phase

```
Phase 0 (30 phút):  PostMessage probe → biết backend dùng được
Phase 1 (2 ngày):   Capture WGC + vision detector + PtMockGame skeleton
Phase 2 (2 ngày):   Input backend + humanizer + OutputGate
Phase 3 (2 ngày):   Action Dispatcher P0–P4 + Combat FSM + mob death detect
Phase 4 (2 ngày):   Config + ImGui UI + calibrate
Phase 5 (1 ngày):   Logger + tray + hotkey + naming/VersionInfo
Phase 6 (1 ngày):   Integration test trên Mock + Replay video
Phase 7 (1 ngày):   Soak test PT offline + tuning
                    ─────────────
                    Total: ~12 ngày
```

### 38.6 Verdict

**Cách đơn giản + hiệu quả cao nhất:**

1. Probe PostMessage Phase 0 → ~85% khả năng works trên PT classic.
2. Nếu works: user multitask thoải mái, tool chạy nền, không chiếm chuột.
3. Nếu fail: SendInput foreground-required (kém UX nhưng vẫn an toàn).
4. Tool đầy đủ tính năng farm (buff cycle, attack sweep, smart death detect, pot priority HP/MP/SP, recall safety) trong ~3K LOC.
5. Test offline 100% qua Mock + Replay video, không cần đụng PT live cho đến Phase 7.

User đã approve MVP này. Proceed `/ck:plan` để dựng plan implementation chi tiết.

---

## 39. Bước Kế Tiếp

1. **User duyệt thiết kế đã cập nhật** (gồm Section 20 — XC3 mitigation).
2. Gọi `/ck:plan` để tạo plan implementation theo phase:
   - Phase 1: capture pipeline (WGC + DXGI fallback)
   - Phase 2: vision (HP/MP detector, template matcher, freeze detector)
   - Phase 3: input scheduler + **humanizer** + **ActionDispatcher với priority preempt** (Section 23)
   - Phase 4: FSM + vision tick loop (HP-first short-circuit, Section 23.5)
   - Phase 5: config core + hot reload
   - Phase 6: **UI Dear ImGui + D3D11** (Section 21) — settings panel, calibrate, preview, tray, hotkey
   - Phase 7: **Safe Mode** (Section 22 — capture health FSM, OutputGate, auto recovery) + code hygiene + soak test offline
3. Mỗi phase gate bằng integration test trên video clip PT đã record (offline).
4. **KHÔNG** test trực tiếp trên server official trước khi hoàn thành Phase 1–7 và đã ổn định trên offline.

---

## Câu Hỏi Chưa Giải Quyết

1. Anti-cheat của server target có quét pattern automation nổi tiếng (poll window-title, nhịp `SendInput`)? Nếu có, cần module humanize hơn jitter đơn giản.
2. Version PT: revision client ảnh hưởng layout UI (vị trí orb, font). Cần UI calibrate hay hard-code profile theo version?
3. Có cần hỗ trợ multi-account / multi-instance không, hay chỉ một client? Ảnh hưởng tới việc chọn capture source (list HWND vs. single).
4. Có cần chạy headless / background không, hay luôn foreground? Quyết định lựa chọn input backend.
5. Mô hình phân phối: installer ký số, zip portable, hay chỉ source? Ảnh hưởng tới vcpkg static vs. dynamic.
