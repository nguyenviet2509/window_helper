# Phase 1 — Vision config + Calibration UI

## Context
- Plan: [plan.md](plan.md)
- Brainstorm: `plans/reports/brainstorm-260611-1038-multi-server-portability-anticheat.md`
- Blocker: ROI/hue hard-coded ở [src/main.cpp:157-159](../../src/main.cpp#L157-L159).

## Overview
- **Priority**: P0 (bắt buộc trước, độc lập anti-cheat)
- **Status**: pending
- Move toàn bộ vision config (HP/MP/SP ROI + hue ranges) ra `config.json`, build ImGui tab calibrate trực quan, support per-server preset switching.

## Key insights
- ROI hiện assume frame WGC = 1010x789. Server khác đổi resolution/UI skin → chết. ROI/hue là **configuration**, không phải code.
- `BarConfig` struct đã có sẵn — chỉ cần expose qua JSON.
- Calibration phải intuitive: drag chọn ROI + auto-sample hue → 1-2 phút cho 1 bar.

## Requirements

**Functional:**
- Đọc `vision.hp/mp/sp.{region, hues}` từ `config.json`.
- UI tab "Calibrate" với:
  - Live preview WGC frame.
  - Drag chuột chọn rect ROI cho từng bar.
  - Nút "Sample hue" → auto-compute hue range từ pixel trong ROI (khi bar đầy).
  - Overlay debug realtime: vẽ ROI + % detect lên preview.
- Per-server preset: dropdown load `config-{server}.json` từ `presets/` dir.
- Schema migration: detect old config (không có `vision` section) → auto-fill defaults hiện tại + warning.

**Non-functional:**
- Hot-reload: đổi ROI trong UI áp dụng ngay, không cần restart.
- Backward compat: existing config.json vẫn load được.

## Architecture
```
config.json
├── vision (NEW)
│   ├── frameWidth/Height          // expected WGC size, warning if mismatch
│   ├── hp: { region: {x,y,w,h,shape}, hues: [{lo,hi},...] }
│   ├── sp: { ... }
│   └── mp: { ... }
├── combat / pot / refill          // existing

src/
├── state/game-state.h             // add VisionConfig struct
├── config/config-loader.cpp       // parse vision section + migration
├── ui/
│   ├── main-window.cpp            // wire calibration tab
│   └── calibration-tab.{h,cpp}    // NEW — drag ROI + hue sampler
└── vision/
    └── hue-sampler.{h,cpp}        // NEW — compute hue range from pixels
```

Flow: UI drag → emit `VisionConfig` patch → `ConfigBus.publish` → `VisionPipeline` re-init bars (hot-reload).

## Related files

**Modify:**
- [src/main.cpp](../../src/main.cpp) — remove hardcoded `MakeBar(...)` calls, build bars from `cfg.vision`.
- [src/state/game-state.h](../../src/state/game-state.h) — add `VisionConfig`, embed vào `AppConfig`.
- [src/config/config-loader.cpp](../../src/config/config-loader.cpp) — JSON parse/save + migration.
- [src/ui/main-window.cpp](../../src/ui/main-window.cpp) — add calibration tab; preset dropdown.
- [src/vision/vision-pipeline.cpp](../../src/vision/vision-pipeline.cpp) — accept config updates (hot-reload bars).

**Create:**
- `src/ui/calibration-tab.h` / `.cpp` (~150 LOC) — drag rect, sample button, overlay.
- `src/vision/hue-sampler.h` / `.cpp` (~80 LOC) — HSV histogram → range với padding.
- `presets/README.md` — hướng dẫn share preset.

**Delete:** none.

## Implementation steps

1. **Schema design** — define `VisionConfig` struct trong [game-state.h](../../src/state/game-state.h) (region + hues per bar).
2. **Config loader** — parse `vision` JSON section; migration: nếu thiếu → fill defaults hiện tại từ main.cpp:157-159, log warning, save lại.
3. **Wire main.cpp** — bỏ hardcoded `MakeBar`, build `BarConfig` từ `cfg.vision`.
4. **Hot-reload path** — `VisionPipeline::updateConfig(VisionConfig)`; subscribe `ConfigBus`.
5. **Hue sampler module** — sample pixels HSV, lấy hue histogram, trả về `[lo, hi]` (multi-mode nếu bimodal như HP có 2 range).
6. **Calibration tab UI** — ImGui tab "Calibrate":
   - Texture preview frame WGC mới nhất.
   - 3 button "Chọn ROI HP/MP/SP" → enter drag mode, click-drag trên preview chọn rect.
   - "Sample hue" button → gọi `hue-sampler` trên ROI.
   - Realtime overlay: rect màu + text "HP 87%" debug.
   - "Save preset" / "Load preset" file dialog → `presets/*.json`.
7. **Preset switching** — dropdown list `presets/*.json`, chọn = swap full vision config.
8. **Test** — calibrate 1 ROI mới giả lập (resize mock game window khác), confirm detect đúng.

## Todo
- [ ] Define `VisionConfig` struct
- [ ] Update config-loader (parse + migration + save)
- [ ] Remove hardcoded ROI in main.cpp
- [ ] `VisionPipeline::updateConfig` hot-reload
- [ ] `hue-sampler` module
- [ ] `calibration-tab` UI
- [ ] Preset dropdown + file IO
- [ ] Migration test (load old config without `vision`)
- [ ] Calibrate test trên mock game resize

## Success criteria
- Mở tool với config thiếu `vision` → auto-fill defaults, log warning, không crash.
- Calibrate 1 server mới (chưa biết ROI) trong < 5 phút từ zero.
- Switch preset không cần restart, vision detect chạy ngay với config mới.
- Existing server hiện tại vẫn chạy đúng sau migration.

## Risks
| Risk | Mitigation |
|---|---|
| Hue sampler false positive (background nhiễu) | Sample khi bar đầy + variance filter; cho user adjust range thủ công sau auto-sample |
| Drag ROI sai khi cửa sổ game scaled | Lock preview tỉ lệ 1:1; hiển thị tọa độ realtime |
| Hot-reload race condition (vision tick đang đọc bar cũ) | Atomic swap pointer / shared_ptr<const BarConfig> |

## Security
N/A (local config IO; không network).

## Next steps
→ Phase 2 (Stealth) sau khi Phase 1 stable + test 1 server khác thành công.
