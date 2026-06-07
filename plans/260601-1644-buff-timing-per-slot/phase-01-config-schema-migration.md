# Phase 01: Config Schema + Migration

**Priority:** High | **Status:** pending

## Context
- Brainstorm: [../reports/brainstorm-260601-1637-buff-timing-per-slot.md](../reports/brainstorm-260601-1637-buff-timing-per-slot.md)
- Files: [src/state/game-state.h](../../src/state/game-state.h), [src/config/config-loader.cpp](../../src/config/config-loader.cpp)

## Goal
Thay field `castDelayMs` trong `BuffSlotCfg` bằng 3 field mới. JSON cũ vẫn load được (migration fallback).

## Schema Changes

`src/state/game-state.h` — struct `BuffSlotCfg` (line ~25-31):

```cpp
struct BuffSlotCfg {
    bool enabled = true;
    WORD key = VK_F2;
    int rightClickDelayMs = 100;  // F press → right-click (chờ game đổi cursor self-target)
    int animationMs = 800;        // cast animation full của skill
    int postBuffGapMs = 150;      // safety gap trước slot kế tiếp
    bool rightClickAfter = true;
};
```

Default `CombatConfig::buffs` (line ~47-52) giữ nguyên 4 slot F2-F5, dùng default mới (rightClickDelayMs=100, animationMs=800, postBuffGapMs=150, rightClickAfter=true).

## Config Loader Changes

`src/config/config-loader.cpp`:

**`toJson(BuffSlotCfg)`** — ghi 3 field mới, KHÔNG ghi `castDelayMs`:
```cpp
j = {
    {"enabled", b.enabled},
    {"key", b.key},
    {"rightClickDelayMs", b.rightClickDelayMs},
    {"animationMs", b.animationMs},
    {"postBuffGapMs", b.postBuffGapMs},
    {"rightClickAfter", b.rightClickAfter}
};
```

**`fromJson(BuffSlotCfg)`** — migration logic:
```cpp
if (j.contains("enabled")) b.enabled = j["enabled"];
if (j.contains("key")) b.key = j["key"];
if (j.contains("rightClickAfter")) b.rightClickAfter = j["rightClickAfter"];

if (j.contains("animationMs")) {
    b.animationMs = j["animationMs"];
    b.rightClickDelayMs = j.value("rightClickDelayMs", 100);
    b.postBuffGapMs = j.value("postBuffGapMs", 150);
} else if (j.contains("castDelayMs")) {
    // Migration: map old castDelayMs → animationMs, set safe defaults
    b.animationMs = j["castDelayMs"];
    b.rightClickDelayMs = 100;
    b.postBuffGapMs = 150;
}
```

## Todo
- [ ] Update `BuffSlotCfg` struct in game-state.h (xóa `castDelayMs`, thêm 3 field mới)
- [ ] Update default `CombatConfig::buffs` initializer
- [ ] Update `toJson(BuffSlotCfg)` in config-loader.cpp
- [ ] Update `fromJson(BuffSlotCfg)` với migration fallback
- [ ] Build: chạy CMake build, fix compile errors (combat-fsm.cpp sẽ break vì còn ref `castDelayMs` — sẽ fix ở phase 02)
- [ ] Manual test: load `config.json` hiện tại (chỉ có `castDelayMs`), verify không crash, field mới có default đúng

## Success Criteria
- game-state.h compile riêng (header-only).
- config-loader.cpp compile (sau khi phase 02 fix FSM).
- Load config cũ → animationMs = old castDelayMs, các field khác = default.
- Save lại → JSON có field mới, không có `castDelayMs`.

## Risks
- Quên xử lý field thiếu trong JSON → dùng `j.value(key, default)` thay vì `j[key]`.
- Build break combat-fsm.cpp do còn dùng `castDelayMs` — phase 02 fix ngay.
