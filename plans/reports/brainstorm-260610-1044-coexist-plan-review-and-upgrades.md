---
type: brainstorm-report
date: 2026-06-10
slug: coexist-plan-review-and-upgrades
status: approved
amendsPlan: ../260610-0928-multi-window-coexist-auto/
---

# Amendment: Plan 260610-0928 review & upgrades

## Review verdict
Plan hiện tại **CHƯA** đảm bảo "không chiếm chuột" tuyệt đối. Chỉ né được khi user actively gõ/di chuột. User idle passive (xem màn hình, đọc) vẫn thấy cursor nhảy giữa N PT mỗi 70-210ms khi auto fire.

## 3 nâng cấp được duyệt

### 1. Build separation: 2 exe targets (từ session trước — 10:27)
- src/CMakeLists.txt: thêm `add_executable(WindowHelperCoexist)` cạnh `WindowHelper` cũ.
- Share 80% source (capture/vision/combat/refill/backend/output-gate/humanizer/logger/config-loader). Component mới (Arbiter, PauseGate, UserActivityMonitor, WindowLifecycleManager, ProfileManager, UI tabs) **chỉ link vào exe mới**.
- Shared component mở rộng (vd InputScheduler thêm PauseGate ref) → dùng **optional / no-op default** để backward compat exe cũ. Không fork class.
- main split: `main.cpp` (cũ, unchanged) + `main-coexist.cpp` (mới).
- UI split: `ui/main-window.cpp` (cũ) + `ui/main-window-coexist.cpp` (mới — tabs dynamic).
- Output naming: cũ giữ `svc_xxxxx.exe`; mới `svc_xxxxx_v2.exe` (random suffix anti-AC).
- Build cả 2 mặc định; user có thể skip exe cũ qua `-DWH_BUILD_CLASSIC=OFF`.

### 2. Cursor save/restore + deep-idle policy

#### A. Cursor save/restore trong slot
- ForegroundArbiter trước khi acquire slot: `GetCursorPos(&savedCursor_)`.
- Sau releaseSlot: nếu cursor không bị user move (check `GetCursorPos == lastWriteByUs`) → `SetCursorPos(savedCursor_)`.
- Flicker chỉ 1-2 frame (~16-33ms) thay vì cursor đứng nguyên ở click target.
- API:
  ```cpp
  void ForegroundArbiter::setCursorRestoreEnabled(bool e);
  ```

#### C. Deep-idle threshold cho mouse-required action
- `UserActivityMonitor` thêm threshold thứ 3: `deepIdleMs` (default 30000).
- `PauseGate::isPausedForMouse(hwnd)` = pause OR `now - lastInput < deepIdleMs`.
- Mouse-required action (refill drag, click target) **chỉ thực hiện khi deep idle**.
- Keyboard-only skill (combat F-keys) vẫn dùng `isPaused()` thường (3s threshold).
- Update Phase 4 PauseGate API:
  ```cpp
  bool isPaused(HWND owner) const;            // 3s threshold (keyboard)
  bool isPausedForMouse(HWND owner) const;    // 30s threshold (mouse)
  ```
- Update Phase 3 InputScheduler: cmd có flag `requiresMouse` → check `isPausedForMouse` thay `isPaused`.
- PotRefillScheduler + combat click → mark `requiresMouse = true`.

#### Behavior summary
| User state | Keyboard skill | Mouse action |
|---|---|---|
| Actively gõ/di chuột (< 3s idle) | Pause | Pause |
| Idle 3-30s | Run (cursor không đổi nếu skill keyboard-only) | Pause |
| Idle ≥ 30s | Run | Run (với cursor save/restore) |

→ User ngồi xem màn hình: combat skill vẫn fire (cursor không nhảy), refill defer cho đến khi user rời bàn lâu.

### 3. Phase 0 POC sớm

Thêm `phase-00-poc-feasibility.md` ưu tiên P0, effort 1d, **làm TRƯỚC mọi phase khác**.

POC nội dung:
1. **WGC 3-session**: mở 3 capture đồng thời 3 PT windows; verify 0 crash trong 5 phút, FPS ≥ 30 mỗi session, GPU < 60%.
2. **Arbiter slot 70ms**: prototype arbiter + 3 thread acquire rotating; đo `fgFailures` rate; verify SetForegroundWindow không throttle quá 5%.
3. **Cursor save/restore latency**: đo thời gian GetCursorPos + SendInput click + SetCursorPos restore. Target < 16ms.
4. **GetLastInputInfo filter**: prototype self-input detection; đo false positive/negative rate với 100 manual + 1000 self input mixed.

Acceptance: 4/4 PASS → tiếp Phase 1. Fail → revisit design (vd N=2 hard cap, slot 100ms, hoặc skip cursor restore).

## Plan structure changes

| # | Phase | Status |
|---|---|---|
| **0** | **POC feasibility (NEW)** | added — P0, 1d, blocking |
| 1 | Window lifecycle manager | unchanged |
| 2 | ForegroundArbiter + cursor save/restore | +0.25d (cursor logic) |
| 3 | InputScheduler + PauseGate + requiresMouse flag | +0.1d |
| 4 | UserActivityMonitor + PauseGate (3 thresholds) | +0.1d |
| 5 | main.cpp wiring → **main-coexist.cpp** (new file) | unchanged effort |
| 6 | ProfileManager | unchanged |
| 7 | UI → **main-window-coexist.cpp** (new file) + cursor/deep-idle config | +0.25d |
| 8 | Calibration | unchanged |
| 9 | Test soak (extend with deep-idle scenarios) | +0.1d |

**Total effort: 8.5d → ~10d** (gồm Phase 0 + cursor/deep-idle additions).

## CMake target structure

```cmake
# src/CMakeLists.txt

# Common sources (link vào cả 2 exe)
set(WH_COMMON_SRC
    capture/wgc-capture.cpp
    vision/vision-pipeline.cpp
    vision/bar-detector.cpp
    vision/waterline.cpp
    input/postmessage-backend.cpp
    input/send-input-backend.cpp
    input/input-scheduler.cpp
    input/mouse-path.cpp
    core/humanizer.cpp
    core/output-gate.cpp
    core/capture-health-fsm.cpp
    core/logger.cpp
    combat/pot-evaluator.cpp
    combat/combat-activity-monitor.cpp
    combat/combat-fsm.cpp
    combat/pot-refill-scheduler.cpp
    dispatch/action-dispatcher.cpp
    config/config-loader.cpp
)

# Classic exe (giữ nguyên)
if(NOT DEFINED WH_BUILD_CLASSIC OR WH_BUILD_CLASSIC)
    add_executable(WindowHelper WIN32
        main.cpp
        ${WH_COMMON_SRC}
        ui/main-window.cpp
        ui/tray-icon.cpp
        resources/version.rc
    )
    # ... existing config (OUTPUT_NAME random svc_xxxxx)
endif()

# Coexist exe (mới)
if(NOT DEFINED WH_BUILD_COEXIST OR WH_BUILD_COEXIST)
    add_executable(WindowHelperCoexist WIN32
        main-coexist.cpp
        ${WH_COMMON_SRC}
        # New components — coexist only
        dispatch/foreground-arbiter.cpp
        core/user-activity-monitor.cpp
        core/pause-gate.cpp
        state/window-lifecycle-manager.cpp
        config/profile-manager.cpp
        ui/main-window-coexist.cpp
        ui/tray-icon.cpp
        ui/calibration-panel.cpp
        core/window-pin.cpp
        core/audit-log.cpp
        resources/version.rc
    )
    # OUTPUT_NAME random với suffix _v2
    string(TIMESTAMP _wh_ts2 "%s")
    string(RANDOM LENGTH 5 ALPHABET "abcdefghijklmnopqrstuvwxyz0123456789"
           RANDOM_SEED "${_wh_ts2}_v2" _wh_suffix2)
    set(WH_COEXIST_OUTPUT_NAME "svc_${_wh_suffix2}_v2")
    set_target_properties(WindowHelperCoexist PROPERTIES
        OUTPUT_NAME "${WH_COEXIST_OUTPUT_NAME}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    target_link_libraries(WindowHelperCoexist PRIVATE
        opencv_core opencv_imgproc opencv_imgcodecs
        nlohmann_json::nlohmann_json
        imgui::imgui
        user32 gdi32 d3d11 dxgi dxguid windowsapp runtimeobject wtsapi32 shell32 comctl32
    )
    target_compile_definitions(WindowHelperCoexist PRIVATE _WIN32_WINNT=0x0A00 WH_COEXIST=1)
endif()
```

## Backward compat rules (Strict)

Để bản cũ không break khi mở rộng shared component:
1. **Không thêm required param vào ctor shared class** — chỉ thêm overload hoặc default-init param.
2. **PauseGate / Arbiter là nullable ref** trong InputScheduler — default `nullptr` → behavior cũ (no pause, no arbiter wrap).
3. **InputCmd.requiresMouse / inTransaction** default `false` → cũ không set field này, behavior không đổi.
4. **Không xoá / rename public API** của shared class.
5. **Test acceptance**: sau khi merge, build `WindowHelper` (cũ) chạy smoke test trên 1 PT → phải pass.

## Trade-off thừa nhận

1. **Cursor save/restore không bulletproof**: nếu user di chuột giữa `SetCursorPos savedCursor_` và `restoreCursor_` (race ~10ms) → restore đè lên cursor user. Mitigation: check `pause.isPaused` ngay trước restore — nếu user vừa active → skip restore.
2. **Deep-idle 30s**: user rời bàn 25s rồi quay lại → mouse action vẫn pending → 5s nữa fire → user vừa ngồi xuống thấy cursor nhảy. Acceptable; configurable.
3. **2 binary maintain cost**: thay đổi shared component phải verify 2 exe build + smoke. Test matrix x2.

## Success criteria bổ sung
- Build cả 2 exe pass trong CI.
- Smoke test classic exe sau khi merge các thay đổi shared: 1 PT auto như cũ.
- Cursor restore latency p95 < 20ms (sau Phase 0 POC verify).
- User idle 3-30s: chỉ keyboard skill fire; mouse action queue, không cursor disrupt.
- User idle ≥ 30s: full auto.

## Unresolved questions
- Deep-idle 30s: optimal hay user muốn tunable trong UI? (Đã có config field, default 30s.)
- Cursor restore: có nên disable khi user "luôn ở bàn" (vd active gần đây dù idle 3s)? Hiện tại restore active luôn khi auto fire.
- Phase 0 POC fail thì sao? — defined ở amendment: fallback N=2 / slot 100ms / skip cursor restore. Cần update plan response.
