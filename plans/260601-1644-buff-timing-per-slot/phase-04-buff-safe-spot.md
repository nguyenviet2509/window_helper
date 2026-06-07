# Phase 04: Buff Right-Click Safe Spot

**Priority:** High | **Status:** pending | **Blocked by:** Phase 02

## Context
- Brainstorm: [../reports/brainstorm-260601-1653-buff-safe-spot.md](../reports/brainstorm-260601-1653-buff-safe-spot.md)
- Bug: right-click center có thể trúng mob → skill biến thành đánh thường, KHÔNG buff.

## Goal
Right-click confirm self-target fire ở **safe spot do user pick** (% client rect) thay vì center.

## Changes

### 1. `src/state/game-state.h` — `CombatConfig`

Thêm 2 field sau `engagementLockJitterMs`:
```cpp
double buffSafeSpotXPct = 0.5;   // % client width
double buffSafeSpotYPct = 0.5;   // % client height
```

### 2. `src/config/config-loader.cpp`

**`toJson(CombatConfig)`** — thêm:
```cpp
{"buffSafeSpotXPct", c.buffSafeSpotXPct},
{"buffSafeSpotYPct", c.buffSafeSpotYPct},
```

**`fromJson(CombatConfig)`** — thêm:
```cpp
if (j.contains("buffSafeSpotXPct")) c.buffSafeSpotXPct = j["buffSafeSpotXPct"];
if (j.contains("buffSafeSpotYPct")) c.buffSafeSpotYPct = j["buffSafeSpotYPct"];
```

### 3. `src/combat/combat-fsm.cpp` — `stepBuffing()`

Replace right-click coord calc:
```cpp
if (slot->rightClickAfter) {
    RECT r{}; GetClientRect(target, &r);
    int w = r.right - r.left;
    int h = r.bottom - r.top;
    double px = std::clamp(cfg_.buffSafeSpotXPct, 0.05, 0.95);
    double py = std::clamp(cfg_.buffSafeSpotYPct, 0.05, 0.95);
    int cx = r.left + (int)(w * px);
    int cy = r.top  + (int)(h * py);
    // ... rest of click scheduling unchanged
}
```

### 4. `src/ui/main-window.cpp` — Combat panel

Thêm sau `cycleDurationSec` slider, trước buff list:
```cpp
any |= ImGui::SliderScalar("Safe spot X (%)", ImGuiDataType_Double,
    &draft_.combat.buffSafeSpotXPct, &kZero, &kOne, "%.2f");
any |= ImGui::SliderScalar("Safe spot Y (%)", ImGuiDataType_Double,
    &draft_.combat.buffSafeSpotYPct, &kZero, &kOne, "%.2f");
ImGui::TextDisabled("Toa do chuot phai confirm self-target. Pick cho trong, khong UI, khong mob.");
```

(Hoặc dùng `DragFloat` cast từ double — match style hiện tại.)

## Todo
- [ ] Thêm 2 field vào `CombatConfig` (game-state.h)
- [ ] Update toJson/fromJson (config-loader.cpp)
- [ ] Update `stepBuffing()` dùng pct + clamp [0.05, 0.95]
- [ ] Thêm 2 slider + hint text vào Combat panel (main-window.cpp)
- [ ] Build sạch
- [ ] Manual test: chỉnh slider → save → reload → giữ giá trị
- [ ] Test thực địa: pick safe spot trong game, chạy 5 cycle, verify KHÔNG có lần nào biến thành attack

## Success Criteria
- 4 buff fire đủ trong 5 cycle test, 0 lần biến thành đánh thường.
- Slider chỉnh real-time, save/reload round-trip OK.
- Default 0.5/0.5 không crash (nhưng có thể fail buff nếu mob ở giữa).

## Risks
- User pick spot có UI → trigger UI action. Mitigation: hint text + user test trước khi farm.
- Pct edge case (0/1) → click ra ngoài. Mitigation: clamp [0.05, 0.95] trong code.
- Mob đi qua safe spot lúc buff → vẫn fail. Không có vision = không 100%. Chấp nhận.
