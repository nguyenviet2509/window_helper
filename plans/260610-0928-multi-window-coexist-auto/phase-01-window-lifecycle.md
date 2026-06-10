---
phase: 1
title: WindowLifecycleManager (continuous discovery + hot-plug)
status: pending
priority: P0
effort: 1d
---

# Phase 1 — WindowLifecycleManager

## Context
Plan cũ dùng `FindAllTargets()` 1 lần lúc start. Plan mới: background thread poll liên tục, diff HWND set, spawn/teardown context động, emit lifecycle events cho UI.

## Files to create
- `src/state/per-window-context.h` (~60 LOC) — struct gom pipeline (giống plan cũ + thêm `pausedByUser` flag).
- `src/state/window-lifecycle-manager.h` (~80 LOC)
- `src/state/window-lifecycle-manager.cpp` (~200 LOC)

## API

```cpp
// window-lifecycle-manager.h
class WindowLifecycleManager {
public:
    using Initializer = std::function<bool(PerWindowContext&)>;
    using Teardown    = std::function<void(PerWindowContext&)>;
    using OnAdded     = std::function<void(PerWindowContext*)>;
    using OnRemoved   = std::function<void(HWND, int /*index*/)>;

    WindowLifecycleManager(Initializer init, Teardown teardown,
                           OnAdded onAdded, OnRemoved onRemoved,
                           int maxWindows = 3,
                           std::chrono::milliseconds pollInterval = std::chrono::milliseconds(2500));
    void start();
    void stop();

    // Snapshot for UI (thread-safe copy).
    std::vector<HWND> currentHwnds() const;
    int count() const;

private:
    void pollLoop();
    void enumerate(std::vector<HWND>& out);
    bool isPristonTale(HWND h);

    Initializer init_;
    Teardown    teardown_;
    OnAdded     onAdded_;
    OnRemoved   onRemoved_;
    int maxWindows_;
    std::chrono::milliseconds interval_;
    std::atomic<bool> running_{false};
    std::thread th_;
    mutable std::mutex mu_;
    std::vector<std::unique_ptr<PerWindowContext>> ctxs_;  // owned
    // Tracking: HWND đã miss 1 poll → đợi 1 poll nữa mới teardown.
    std::map<HWND, int> missCount_;
};
```

## Lifecycle rules
- Poll mỗi 2.5s.
- `EnumWindows` filter `IsWindowVisible` + title contains "priston"/"ptmockgame" (case-insensitive).
- Diff:
  - New HWND not in ctxs → call `init_(ctx)`; nếu OK push + `onAdded_(ctx*)`. Nếu fail → skip + log.
  - HWND in ctxs nhưng không có trong enumerate → `missCount_[hwnd]++`. Nếu miss ≥ 2 consecutive → `teardown_(ctx)` + remove + `onRemoved_(hwnd, idx)`.
  - HWND vẫn còn → reset `missCount_[hwnd] = 0`.
- Hard cap N=3: nếu `ctxs_.size() == 3` và có HWND mới → skip + log warn 1 lần (track skipped set).
- `IsWindow(hwnd) == false` ngay lập tức → teardown ngay (không đợi 2 polls).

## PerWindowContext extension
```cpp
struct PerWindowContext {
    HWND hwnd = nullptr;
    std::wstring title;
    RECT lastKnownRect = {};   // dùng cho profile reassign matching
    int index = 0;
    std::string profileName = "Default";
    AppConfig cfg;
    ConfigBus bus;
    std::atomic<bool> pausedByUser{false};   // set by PauseGate observer
    // unique_ptr pipeline như plan cũ
    // ...
};
```

## Index assignment
- Khi spawn: `index = nextFreeIndex()` (0,1,2 — không nhất thiết theo poll order).
- Khi remove: free index, slot có thể được reuse cho HWND mới sau đó.
- UI tab label hiển thị `W{index}: {title-short}`.

## Thread safety
- `ctxs_` chỉ mutate trong pollLoop (1 thread). Read từ outside qua `currentHwnds()` snapshot dưới mutex.
- `init_`/`teardown_` chạy trên thread của lifecycle manager — phải nhanh; pipeline start là async (capture/vision có thread riêng).

## Todo
- [ ] `per-window-context.h` với field mới.
- [ ] `window-lifecycle-manager.h/.cpp`.
- [ ] Unit test thủ công: spawn fake EnumWindows trả 0→1→2→3→2 windows, verify callbacks.
- [ ] Add CMakeLists.

## Success criteria
- Mở PT giữa chừng → log "windowAdded W{i}" trong ≤ 3s.
- Đóng PT giữa chừng → log "windowRemoved W{i}" trong ≤ 5s (2 polls + IsWindow check).
- N=4 → log "skipped: max 3" 1 lần, không spam.

## Risks
- EnumWindows trên Windows với 100+ windows → ~1ms. Negligible mỗi 2.5s.
- Race: HWND mới được spawn pipeline, ngay sau đó user đóng PT → teardown trong khi init chưa xong. Mitigation: init synchronous (capture->start) trước khi push vào ctxs_; nếu fail → log + không push.
