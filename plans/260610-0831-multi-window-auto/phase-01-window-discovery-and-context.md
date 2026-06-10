---
phase: 1
title: Window Discovery + PerWindowContext skeleton
status: pending
priority: P1
effort: 0.5d
---

# Phase 1 — Window discovery + PerWindowContext

## Context
- Hiện tại `main.cpp::FindTarget()` trả 1 HWND đầu tiên. Cần liệt kê **tất cả** PT windows top-level.
- Cần struct gom pipeline per-window để Phase 4 wire dễ.

## Files to modify
- `src/main.cpp` — `FindTarget` → `FindAllTargets(std::vector<HWND>&)`.

## Files to create
- `src/state/per-window-context.h` (~50 LOC) — header-only struct.

## Implementation

### `src/state/per-window-context.h`
```cpp
#pragma once
#include <memory>
#include <windows.h>
#include "../capture/wgc-capture.h"
#include "../vision/vision-pipeline.h"
#include "../combat/combat-fsm.h"
#include "../combat/pot-refill-scheduler.h"
#include "../dispatch/action-dispatcher.h"
#include "../core/output-gate.h"
#include "../core/capture-health-fsm.h"
#include "../input/input-scheduler.h"

struct PerWindowContext {
    HWND hwnd = nullptr;
    std::wstring title;
    int index = 0;  // 0-based; dùng cho UI label & log tag

    // Phase 4b: per-window profile config
    std::string profileName = "Default";
    AppConfig   cfg;                       // owned per-window snapshot
    ConfigBus   bus;                       // per-window publish/subscribe

    std::unique_ptr<WgcCapture>           capture;
    std::unique_ptr<VisionPipeline>       vision;
    std::unique_ptr<OutputGate>           gate;
    std::unique_ptr<CaptureHealthFsm>     health;
    std::unique_ptr<CombatFsm>            combat;
    std::unique_ptr<PotRefillScheduler>   refill;
    std::unique_ptr<ActionDispatcher>     dispatcher;
    std::unique_ptr<InputScheduler>       sched;  // 1 scheduler per window (Phase 3 sẽ giải thích)
};
```

### `main.cpp` changes
```cpp
// Thay FindTarget(); giữ logic match "priston" case-insensitive.
void FindAllTargets(std::vector<HWND>& out) {
    struct Ctx { std::vector<HWND>* out; } ctx{&out};
    EnumWindows([](HWND h, LPARAM lp) -> BOOL {
        if (!IsWindowVisible(h)) return TRUE;
        wchar_t title[256] = {};
        if (GetWindowTextW(h, title, 256) <= 0) return TRUE;
        std::wstring t(title);
        std::transform(t.begin(), t.end(), t.begin(), ::towlower);
        if (t.find(L"priston") != std::wstring::npos ||
            t.find(L"ptmockgame") != std::wstring::npos) {
            reinterpret_cast<Ctx*>(lp)->out->push_back(h);
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));
}
```

- Nếu `targets.empty()` → MessageBox như cũ.
- Nếu `targets.size() == 1` → log Info, proceed N=1 (backward compat).
- Nếu `targets.size() >= 2` → log Info số window, lấy tối đa 2 đầu tiên cho Phase 1 (N=2 hard cap, ghi TODO N≥3).

## Todo
- [ ] Tạo `per-window-context.h`.
- [ ] Refactor `FindTarget` → `FindAllTargets` trong `main.cpp`.
- [ ] Hard cap N=2 + log warn nếu phát hiện >2.
- [ ] Compile check (`cmake --build`).

## Success criteria
- Build pass.
- Mở 2 PT → log "Found 2 Priston Tale windows: ...".
- Mở 1 PT → behaviour cũ giữ nguyên.

## Risks
- Title PT có thể trùng phần khác (vd Explorer mở thư mục "priston") → giữ filter `IsWindowVisible` + có thể thêm class name check sau nếu false positive.
