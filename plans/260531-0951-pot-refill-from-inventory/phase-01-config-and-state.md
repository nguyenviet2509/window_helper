# Phase 1 — Config Schema & State

## Overview
- **Priority**: P0 (blocks all later phases)
- **Status**: pending
- **Effort**: S

Add `PotRefillSlot` + `PotRefillConfig` structs, wire into `AppConfig`, add JSON serialization. Also update `PotConfig` defaults to match real game keys.

## Files to Modify
- `src/state/game-state.h` — add structs, update PotConfig defaults
- `src/config/config-loader.cpp` — to/fromJson for PotRefillConfig
- `config.json` — add `refill` section with example slots

## Changes

### `src/state/game-state.h`
Add after `PotConfig`:
```cpp
struct PotRefillSlot {
    int x = 0;
    int y = 0;
    int intervalSec = 0;   // 0 = disabled
};

struct PotRefillConfig {
    bool enabled = false;
    WORD inventoryToggleKey = 'V';
    int inventoryOpenDelayMs  = 400;
    int inventoryCloseDelayMs = 200;
    int mouseMoveDelayMs   = 150;
    int postHotkeyDelayMs  = 200;
    int refillTimeoutMs    = 10000;
    double hpCriticalAbortThreshold = 0.30;
    int abortBackoffMs = 30000;
    PotRefillSlot hp;   // Shift+1
    PotRefillSlot sp;   // Shift+2
    PotRefillSlot mp;   // Shift+3
};
```

Update `PotConfig` defaults:
```cpp
WORD hpKey = '1';
WORD spKey = '2';
WORD mpKey = '3';
```

Update `AppConfig`:
```cpp
struct AppConfig {
    PotConfig pot;
    CombatConfig combat;
    PotRefillConfig refill;   // NEW
    BackendKind defaultBackend = BackendKind::SendInput;
};
```

### `src/config/config-loader.cpp`
Add to/from JSON helpers for `PotRefillSlot` and `PotRefillConfig`, mirror pattern from `PotConfig`. Wire `refill` field in main AppConfig serializer.

### `config.json`
Slot coords = **center of bounding box** đo từ kho game (client coords):
- HP: bbox (268,462)-(290,484) → center (279, 473)
- SP: bbox (268,484)-(291,501) → center (279, 492)
- MP: bbox (268,506)-(290,528) → center (279, 517)

Append:
```json
"refill": {
  "enabled": false,
  "inventoryToggleKey": 86,
  "inventoryOpenDelayMs": 400,
  "inventoryCloseDelayMs": 200,
  "mouseMoveDelayMs": 150,
  "postHotkeyDelayMs": 200,
  "refillTimeoutMs": 10000,
  "hpCriticalAbortThreshold": 0.30,
  "abortBackoffMs": 30000,
  "hp": { "x": 279, "y": 473, "intervalSec": 0 },
  "sp": { "x": 279, "y": 492, "intervalSec": 0 },
  "mp": { "x": 279, "y": 517, "intervalSec": 0 }
}
```

## Todo
- [ ] Add structs to `game-state.h`
- [ ] Update PotConfig key defaults
- [ ] Add `AppConfig::refill`
- [ ] to/fromJson in config-loader
- [ ] Update `config.json` example
- [ ] Compile check

## Success Criteria
- Project compiles.
- Loading/saving config preserves refill section round-trip.
- Old `config.json` without refill section still loads (defaults applied).

## Risks
- **Changing PotConfig key defaults** = breaks existing user configs that don't override them. Mitigation: defaults only apply when field absent in JSON; existing configs with explicit keys unaffected.
