# Phase 4 — Config + ImGui UI + Calibrate

**Est:** 2 ngày
**Priority:** P0
**Status:** pending
**Depends:** Phase 3 (FSM + dispatcher)

## Mục Tiêu
- Config struct + JSON load/save (merge-preserving advanced block)
- Dear ImGui + D3D11 UI window theo Section 21.3
- Calibrate UI cho HP/MP/SP region (drag bbox)

## Files
```
src/config/
├── config.h                       # struct POD + nested
├── config-defaults.h               # default values + ranges
├── config-loader.h/.cpp            # load/save JSON
├── config-bus.h/.cpp               # atomic<shared_ptr<Config>>
└── config-validator.h/.cpp         # range clamp + invariants
src/ui/
├── main-window.h/.cpp              # ImGui + Win32 + D3D11 init
├── settings-panel.h/.cpp           # 5 collapsing headers
├── calibrate-overlay.h/.cpp        # drag bbox cho regions
├── status-bar.h/.cpp               # bottom info bar
└── theme.h/.cpp                    # ImGui light-gray classic
```

LOC: ~900.

## Implementation

### 4.1 Config Struct
```cpp
struct PotConfig {
    bool enabled;
    std::string key;
    int threshold_pct;
};

struct RecallConfig {
    bool enabled;
    std::string key;
    int hp_threshold_pct;
};

struct CombatConfig {
    bool auto_enabled;
    std::string main_attack_key;
    int target_pick_interval_ms;
    int cycle_duration_sec;
    int sweep_r_min, sweep_r_max;
    bool wait_mp_enabled;
    int wait_mp_min_pct;
};

struct BuffConfig {
    bool enabled;
    std::string key;
    int cast_delay_ms;
};

struct BarRegion {
    int x, y, w, h;
    std::string shape;   // "rect" | "circle"
    int radius = 0;
};

struct UiSettings {
    PotConfig hp, mp, sp;
    RecallConfig recall;
    CombatConfig combat;
    std::array<BuffConfig, 4> buffs;
    BarRegion hp_bar, mp_bar, sp_bar;
};

struct AdvancedConfig {
    int hp_emergency_pct;
    int pot_cooldown_ms;
    int confirm_frames;
    // ... humanizer, vision, safety, window, hotkey
    nlohmann::json raw;     // giữ nguyên block chưa expose để merge-preserve
};

struct Config {
    UiSettings ui;
    AdvancedConfig advanced;
};
```

### 4.2 ConfigLoader
- `load(path)`: parse JSON, validate range + invariants, fallback default nếu fail.
- `save(path, config)`: atomic temp + rename. Merge `advanced.raw` để preserve fields chưa map.
- `watchHotReload(path)`: `ReadDirectoryChangesW` → reload qua ConfigBus.

### 4.3 ConfigBus
```cpp
class ConfigBus {
public:
    void publish(std::shared_ptr<const Config> c);
    std::shared_ptr<const Config> snapshot() const;
private:
    std::atomic<std::shared_ptr<const Config>> current_;
};
```

Worker thread `snapshot()` lock-free mỗi tick.

### 4.4 ImGui MainWindow Setup
- WinRT D3D11 device (reuse với WGC nếu cùng device)
- `imgui_impl_win32.h` + `imgui_impl_dx11.h`
- Theme: light gray classic (FrameBg, WindowBg light)
- Font: Segoe UI 14px
- Vsync ON, 60 FPS cap; minimize → 1 FPS

### 4.5 SettingsPanel — 5 Collapsing Headers

```cpp
void SettingsPanel::draw(Config& cfg) {
    drawStatusRow(cfg);                   // Trạng thái + Attach + AUTO toggle
    
    if (ImGui::CollapsingHeader("Recovery", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawPotRow("Hồi HP", cfg.ui.hp);
        drawPotRow("Hồi MP", cfg.ui.mp);
        drawPotRow("Hồi SP", cfg.ui.sp);
        drawRecallRow(cfg.ui.recall);
    }
    if (ImGui::CollapsingHeader("Combat", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawCombatRows(cfg.ui.combat);
    }
    if (ImGui::CollapsingHeader("Buff")) {
        for (int i = 0; i < 4; ++i) drawBuffRow(i, cfg.ui.buffs[i]);
    }
    if (ImGui::CollapsingHeader("Calibrate")) {
        drawCalibrateButtons(cfg);
        drawBarPreview(cfg);
    }
}
```

Mỗi row dùng `ImGui::Checkbox` + `ImGui::Combo` (phím) + `ImGui::DragInt` (số) với min/max clamp.

### 4.6 CalibrateOverlay
- User click "Calibrate HP" → overlay full-screen với:
  - Vision frame thumbnail
  - 2 mode: "Click 2 points" hoặc "Drag rect"
  - Hiển thị waterline detect realtime với row pattern visualization
  - Save → update `Config.ui.hp_bar`

### 4.7 StatusBar
Bottom row: `CPU 2% | RAM 85MB | Capture 30fps | Gate: ● | State: Combat | HP=87% MP=42%`

### 4.8 Auto-Save 500ms Debounce
```cpp
class ConfigDebouncer {
    std::chrono::steady_clock::time_point lastChange_;
    bool dirty_ = false;
public:
    void markDirty();
    bool shouldFlush(TimePoint now) const;  // dirty + (now - lastChange > 500ms)
};
```

ImGui change → markDirty → main loop check shouldFlush → save.

## Default Values (config-defaults.h)
Lấy từ brainstorm Section 24, 35:
- HP: enabled true, key "1", threshold 60, emergency 40, cooldown 600ms, confirm 2
- MP: enabled true, key "2", threshold 10, cooldown 600ms
- SP: enabled false, key "3", threshold 10, cooldown 600ms
- Recall: enabled true, key "F12", threshold 15, duration 3.0s, cooldown 5s
- Combat: F1, pick 4000ms, cycle 300s, sweep 50–200px
- Buff[0..3]: F2/F3/F4/F5, cast_delay 500ms, default enabled = [true,true,true,false]
- Regions: HP(355,475,12,105), SP(383,475,12,105), MP(411,475,12,105), shape "rect"
- Humanizer: jitter sigma 45, break 25–50 / 3–8s, session 40–110 / 6–14m, missclick 2%
- Vision: 20Hz, EMA 0.30, black_luma 5, freeze 4.0s
- Safety: degraded 1.5s, unsafe 60s, recreate 2.0s, pause foreground/lock true
- Hotkey: F8 start/stop

## Acceptance
- [ ] Config load/save round-trip không mất advanced fields
- [ ] UI all 5 collapsing section render đúng controls
- [ ] Slider/input clamp đúng range, không cho giá trị bậy
- [ ] Auto-save 500ms debounce hoạt động (verify file mtime sau edit)
- [ ] Hot-reload: sửa config.json bằng Notepad → UI refresh
- [ ] Calibrate UI: drag rect → coords lưu đúng → preview waterline update
- [ ] AUTO toggle bật/tắt FSM (verify CombatFSM enter BUFFING)

## Risks
- ImGui input race với atomic config publish → publish ngay sau edit (mỗi frame OK do single UI thread)
- Calibrate overlay full-screen: dùng layered transparent window, top-most, click-through ngoài rect area

## Next
Phase 5 cosmetic + naming.
