---
phase: 05
title: "Config schema + UI calibration cho target frame ROI"
status: pending
priority: P1
effort: 3h
blockers: [02, 04]
---

## Context
- Target frame ROI ở góc trên-phải. Tọa độ phụ thuộc client size + UI layout.
- Pattern hiện tại: HP/MP/SP bar ROI configurable trong `config.json` + UI calibration ([main-window.cpp](src/ui/main-window.cpp)).

## Goal
User calibrate target-frame mob HP bar ROI giống pattern HP/MP/SP. Expose grace period + sector count.

## Files
- Modify: `src/state/game-state.h` (AppConfig thêm `BarConfig mobHp`)
- Modify: `src/config/config-loader.cpp` (parse `mob_hp_roi`, `mob_hp_hues`, `attackSectorCount`, `targetLockGracePeriodMs`)
- Modify: `config.json` + `dist/config.json` + `dist-test/config.json` (template)
- Modify: `src/ui/main-window.cpp` (ROI picker tab "Mob HP" + slider fields)
- Modify: `docs/ui-parameters-guide.md` (document new fields)

## Implementation Steps

### 1. `AppConfig` schema
```cpp
struct VisionConfig {
    BarConfig hp, mp, sp;
    BarConfig mobHp;   // target-frame mob HP bar
};
struct AppConfig {
    PotConfig pot;
    CombatConfig combat;
    PotRefillConfig refill;
    VisionConfig vision;   // OR keep bars at top-level + add mobHp
    BackendKind defaultBackend = BackendKind::SendInput;
};
```
- Hoặc đơn giản: thêm `BarConfig mobHpBar` vào `AppConfig` (theo pattern flat hiện tại).

### 2. `config.json` template thêm:
```json
{
  "mobHp": {
    "roi": { "x": 870, "y": 30, "w": 130, "h": 14 },
    "hues": [{ "hMin": 0, "hMax": 12 }, { "hMin": 165, "hMax": 180 }]
  },
  "combat": {
    "attackSectorCount": 8,
    "targetLockGracePeriodMs": 250
  }
}
```

### 3. UI calibration tab "Mob HP Bar"
- Pattern same as HP/MP/SP tabs.
- ROI rectangle picker (drag overlay on screenshot).
- Hue range slider(s).
- Live preview: draw detected fill ratio + locked indicator.
- Persist to `config.json` on apply.

### 4. UI add sliders trong tab Combat
- `Attack sector count` (4..16, default 8).
- `Target lock grace (ms)` (50..500, default 250).
- Note tooltip: "Vision-based engagement supersede `engagementLockMs` khi pipeline valid".

### 5. ConfigLoader
- Parse + validate (clamp). Default values nếu missing.
- `updateConfig` propagate sang `CombatFsm` + `VisionPipeline` (cần restart vision pipeline khi ROI đổi).

## Edge Cases
- ROI w/h = 0 → disable detector (pipeline coi `targetLocked=false` luôn). FSM fallback timer.
- Hue range invalid → empty mask → fill 0 → no lock detected. Log warning.
- Pipeline restart khi ROI changes runtime → cần stop/start. Pattern: just rebuild detector inside pipeline (no thread restart).

## Success Criteria
- User load `config.json` với mobHp ROI valid → bot detect lock đúng.
- UI calibration tab hoạt động: drag ROI → live preview update → save → reload.
- Docs `ui-parameters-guide.md` cập nhật mục mới.

## Risks
- ROI picker code phức tạp. Reuse code HP/MP/SP picker, không viết lại.
- `targetLockGracePeriodMs` trên UI dễ tune sai → confusing. Default + tooltip rõ.
