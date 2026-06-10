---
phase: 7
title: Live calibration UI + window pin + audit log
status: pending
priority: P1
effort: 2d
---

# Phase 7 — Calibration + Pin + Audit

## Context
- Bar regions HP/MP/SP + refill inventory slots hiện hardcoded → user không thể tự calibrate khi PT thay UI / window resize / Paint-measure nhầm screen-space.
- Cần: live frame preview + click-to-set picker, lưu vào profile.
- Optional QoL: pin window về anchor khi AUTO ON.
- Diagnostic: audit log window rect thay đổi.

## Files to create
- `src/ui/calibration-panel.h/.cpp` (~250 LOC).
- `src/core/window-pin.h/.cpp` (~50 LOC).

## Files to modify
- `state/game-state.h` — add fields vào AppConfig (gồm cả `CursorParkConfig` từ amendment 09:08):
  ```cpp
  struct VisionConfig {
      BarConfig hpBar;
      BarConfig mpBar;
      BarConfig spBar;
  };
  struct AnchorConfig {
      int x = -1;     // -1 = unset, không pin
      int y = -1;
      bool pinOnAutoOn = false;
  };
  struct RefillSlotsConfig {
      // Tọa độ window-relative (như comment hiện tại). windowToClient runtime.
      std::vector<POINT> hp, mp, sp;
  };
  struct CursorParkConfig {
      int  x = 1;
      int  y = 1;
      bool enabled = true;  // chỉ active khi N>=2 windows
  };
  struct AppConfig {
      // ... existing
      VisionConfig       vision;
      AnchorConfig       anchor;
      RefillSlotsConfig  refillSlots;
      CursorParkConfig   cursorPark;  // Amendment 09:08
  };
  ```
- `main.cpp` — đọc bar config từ `ctx->cfg.vision` thay vì hardcoded MakeBar.
- `pot-refill-scheduler.cpp` — đọc slot coords từ `cfg.refillSlots` thay vì hardcoded.
- `config-loader.cpp` — load/save nested fields mới.

## A. Calibration panel

### UI layout
```
┌─ Calibration: W0 / MainChar ────────────────────┐
│ [LIVE PREVIEW 1010x789 (scaled to fit ~600px)]  │
│   ┌────────────────────────────────────────┐    │
│   │                                        │    │
│   │   [game frame realtime]                │    │
│   │   ░░ HP rect overlay (red)             │    │
│   │   ░░ MP rect overlay (blue)            │    │
│   │   ░░ SP rect overlay (green)           │    │
│   │   ○ inventory slot dots (yellow)       │    │
│   │                                        │    │
│   └────────────────────────────────────────┘    │
│ Mode: ( ) HP rect  ( ) MP rect  ( ) SP rect     │
│       ( ) Inventory HP  ( ) Inventory MP        │
│       ( ) Inventory SP  ( ) Anchor capture      │
│                                                 │
│ Selected rect: x=403 y=656 w=22 h=121           │
│ [Reset]  [Apply]  [Close]                       │
└─────────────────────────────────────────────────┘
```

### Interaction
- Click trên preview = top-left của rect mới; drag = bottom-right. Release → set rect.
- Mode "Inventory X" → click = add slot point. Right-click slot = remove.
- Mode "Anchor capture" → click button "Use current window position" → đọc `GetWindowRect` → save `anchor.x/y`.
- Apply → write vào `ctx->cfg`, debounced save profile.
- Live overlay vẽ qua ImDrawList lên preview, scaled.

### Frame preview
- Thêm `ID3D11ShaderResourceView* WgcCapture::getSRV()` → ImGui texture.
- Update mỗi 30-60ms (UI refresh rate); không cần khớp vision tick.
- Cần synchronize: SRV swap khi WGC write new frame — dùng atomic ptr swap.

## B. Window pin

### `src/core/window-pin.h`
```cpp
#pragma once
#include <windows.h>

class WindowPin {
public:
    // Đặt cửa sổ về (x, y), giữ size hiện tại.
    static bool moveTo(HWND h, int x, int y);
    // Đọc top-left hiện tại để dùng làm anchor.
    static bool readPosition(HWND h, int& x, int& y);
};
```

### Trigger
- Trong `main.cpp::win.setOnCombatToggle`:
  ```cpp
  if (on && ctx->cfg.anchor.pinOnAutoOn &&
      ctx->cfg.anchor.x >= 0 && ctx->cfg.anchor.y >= 0) {
      WindowPin::moveTo(ctx->hwnd, ctx->cfg.anchor.x, ctx->cfg.anchor.y);
  }
  ```

### Calibration panel "Use current position" button:
```cpp
int x, y;
if (WindowPin::readPosition(ctx->hwnd, x, y)) {
    ctx->cfg.anchor.x = x; ctx->cfg.anchor.y = y;
    dirty = true;
}
```

### Risks
- PT có thể block SetWindowPos (DirectInput exclusive / fullscreen) → moveTo log warn nếu rect không đổi sau call.

## C. Audit log

### Logic
- Thêm thread tick (hoặc trong existing health tick):
  ```cpp
  RECT lastRect[N]; bool first[N] = {true,...};
  every 5s:
    for each ctx:
        RECT r; GetWindowRect(ctx->hwnd, &r);
        if (!first[i] && memcmp(&r, &lastRect[i], sizeof(RECT)) != 0) {
            LOG_WARN("[w%d] Window rect changed: (%d,%d %dx%d) -> (%d,%d %dx%d)",
                     i, lastRect[i].left, lastRect[i].top, w_old, h_old,
                     r.left, r.top, w_new, h_new);
        }
        lastRect[i] = r; first[i] = false;
  ```

## Migration

### Seed defaults vào profile lần đầu (Phase 4b ensureDefaultProfile)
```cpp
AppConfig defaults;
// Seed bar regions hiện tại từ main.cpp.
defaults.vision.hpBar = MakeBar(403, 656, 22, 121, { {0,10}, {170,180} });
defaults.vision.mpBar = MakeBar(586, 655, 21, 121, { {100,130} });
defaults.vision.spBar = MakeBar(383, 675, 11, 102, { {40, 80} });
// Seed inventory slots từ pot-refill-scheduler.cpp constants.
defaults.refillSlots.hp = { /* paste from current hardcoded */ };
defaults.refillSlots.sp = { /* ... */ };
defaults.refillSlots.mp = { /* ... */ };
defaults.anchor = {-1, -1, false};
```

## Phase 5 UI integration
- Tab UI thêm nút "Calibrate" → open CalibrationPanel modal cho window đó.
- Tab UI thêm checkbox "Pin window on AUTO" + display anchor (x, y) + "Capture position" button.

## Todo
- [ ] Add `vision/anchor/refillSlots` fields vào AppConfig.
- [ ] config-loader: load/save nested.
- [ ] Migration seed defaults trong ProfileManager.ensureDefaultProfile.
- [ ] `WgcCapture::getSRV()` accessor + atomic swap.
- [ ] CalibrationPanel: preview render, mode picker, click handler, overlay.
- [ ] WindowPin::moveTo + readPosition.
- [ ] Pin trigger trong AUTO toggle handler.
- [ ] Audit log thread (hoặc piggyback health tick).
- [ ] Test: move window, calibrate qua UI, verify HP/MP/SP đọc đúng.

## Success criteria
- Move PT window bất kỳ vị trí → HP/MP/SP vẫn đúng (vì bar coord frame-relative).
- User mở Calibrate, click HP bar trong preview → coord lưu vào profile-{name}.json → restart app → load đúng.
- Pin ON + AUTO toggle ON → window snap về anchor.
- Audit log warn khi user drag window > 5s sau lần check trước.

## Risks
- ImGui::Image với D3D texture cross-thread → cần ImGui_ImplDX11_NewFrame + SRV passed correctly. Test kỹ ban đầu.
- Drag-to-select rect trên preview: cần scale từ screen coord (ImGui) → frame coord. Lưu ý ratio.
- SetWindowPos bị PT reject: log warn, không retry vô hạn.

## Open
- Có cần "Test detection" button trong CalibrationPanel để hiển thị % fill realtime ngay sau khi set rect (verify trước khi save)? Đáng làm — thêm 0.25d.
