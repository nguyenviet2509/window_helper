---
phase: 09
title: "ONNX Runtime C++ integration (CMake + minimal inference)"
status: pending
priority: P2
effort: 2d
blockers: [06]
---

## Context
Phase 2 cần inference YOLOv8 ONNX trong C++. Setup dep + minimal pipeline trước khi viết detector module.

## Goal
Build với ONNX Runtime; load model; run dummy inference trên test frame.

## Files
- Modify: `CMakeLists.txt` (root) — find_package onnxruntime
- Modify: `src/CMakeLists.txt` — link library
- New: `third-party/onnxruntime/` (prebuilt windows-x64 lib) HOẶC vcpkg
- New: `src/ml/onnx-session.h/cpp` — thin wrapper

## Implementation Steps

### 1. ONNX Runtime acquire
- Option A (preferred): vcpkg
  ```
  vcpkg install onnxruntime:x64-windows
  ```
- Option B: Download prebuilt từ GitHub releases (onnxruntime-win-x64-1.16.x.zip), unzip vào `third-party/onnxruntime/`.

### 2. CMake integration
```cmake
# Root CMakeLists.txt
find_package(onnxruntime CONFIG QUIET)
if(NOT onnxruntime_FOUND)
    set(ONNX_DIR "${CMAKE_SOURCE_DIR}/third-party/onnxruntime")
    add_library(onnxruntime SHARED IMPORTED)
    set_target_properties(onnxruntime PROPERTIES
        IMPORTED_LOCATION "${ONNX_DIR}/lib/onnxruntime.dll"
        IMPORTED_IMPLIB   "${ONNX_DIR}/lib/onnxruntime.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${ONNX_DIR}/include")
endif()
target_link_libraries(WindowHelper PRIVATE onnxruntime)
```

- Post-build: copy `onnxruntime.dll` next to `WindowHelper.exe`.

### 3. `OnnxSession` wrapper
```cpp
#pragma once
#include <onnxruntime_cxx_api.h>
#include <opencv2/core.hpp>
#include <vector>
#include <string>

class OnnxSession {
public:
    OnnxSession(const std::string& modelPath, int imgSize = 640);

    // Input: BGR cv::Mat any size. Auto-letterbox to imgSize.
    // Output: raw output tensor data (post-process in caller).
    struct Output {
        std::vector<float> data;
        std::vector<int64_t> shape;
    };
    Output run(const cv::Mat& bgr);

    int inputSize() const { return imgSize_; }

private:
    Ort::Env env_;
    Ort::SessionOptions opts_;
    Ort::Session session_;
    int imgSize_;
    std::string inputName_;
    std::string outputName_;
};
```

### 4. Preprocessing
- BGR → RGB
- Letterbox resize to 640×640 (preserve aspect)
- Normalize /255.0
- HWC → CHW
- Float32, batch=1

Reuse code: 1 utility `letterbox(src, dst, size)` trong `src/ml/preprocess.cpp`.

### 5. Smoke test
- Unit test: load `mob-detector.onnx`, feed 1 PNG, print output shape.
- Expected YOLOv8 output: `[1, 84, 8400]` (84 = 4 box + 80 class scores, hoặc tuỳ class count).

### 6. Runtime constraints
- CPU only (no GPU dep complexity). Inference ~30-80ms trên modern i5.
- Session reuse across frames (đắt khi load lại).
- Thread safety: 1 session per pipeline thread đủ.

## Edge Cases
- Model file missing → log error, disable mob detector (fallback stratified random). KHÔNG crash app.
- Model output shape mismatch (wrong class count) → log + disable.
- DLL not found → app log + skip ML features.

## Success Criteria
- Build sạch with onnxruntime linked.
- Smoke test infer 1 frame → output tensor shape correct.
- Inference time <100ms trên dev machine.

## Risks
- vcpkg setup phức tạp lần đầu. Document prerequisites trong README.
- Binary size +~15MB (onnxruntime.dll). Acceptable.
- License: onnxruntime MIT. OK.
