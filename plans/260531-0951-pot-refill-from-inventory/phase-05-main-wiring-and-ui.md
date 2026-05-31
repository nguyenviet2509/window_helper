# Phase 5 — Main Wiring & UI Countdown

## Overview
- **Priority**: P1
- **Status**: pending
- **Effort**: S
- **Depends on**: Phase 3, 4

Instantiate `PotRefillScheduler` in `main.cpp`, call `tick()` each frame, wire skip pointers, and show countdown + state in `main-window` UI.

## Files to Modify
- `src/main.cpp` — instantiate, tick, wire pointers, hot-reload config
- `src/ui/main-window.cpp` — add UI section (read-only)
- `src/ui/main-window.h` — accept reference to scheduler

## Changes

### `src/main.cpp`
1. Construct after `InputScheduler` + `OutputGate`:
   ```cpp
   PotRefillScheduler refill(scheduler, outputGate, targetHwnd, app.refill);
   refill.enable(app.refill.enabled);
   ```
2. Wire pointers:
   ```cpp
   combatFsm.setRefillScheduler(&refill);
   potEval.setRefillScheduler(&refill);
   ```
3. In main tick loop, call BEFORE combat/pot:
   ```cpp
   refill.tick(visionState, now);
   ```
4. On config hot-reload: `refill.updateConfig(app.refill); refill.enable(app.refill.enabled);`
5. On target HWND change: `refill.setTarget(newHwnd);`

### `src/ui/main-window.cpp`
Add new collapsible section "Pot Refill":
```
[x] Enabled (read-only from config)
State: IDLE | OPENING | REFILLING_HP | ...
Next refill:
  HP: 4m 32s  (interval=5m 0s)
  SP: --      (disabled)
  MP: 8m 12s  (interval=10m 0s)
Slots (client coords):
  HP: (1234, 567)
  SP: (1280, 567)
  MP: (1326, 567)
```

No edit controls (YAGNI — user edits config.json). Just visibility for debugging.

### `src/ui/main-window.h`
Accept `const PotRefillScheduler&` in constructor or via setter.

## Todo
- [ ] Add PotRefillScheduler include + member to main
- [ ] Construct after scheduler/gate
- [ ] Wire setRefillScheduler on combat + pot
- [ ] Call tick() in loop before combat/pot
- [ ] Hot-reload config wiring
- [ ] Add UI section displaying state + countdowns
- [ ] Compile check

## Success Criteria
- App launches with `enabled=false` → no behavior change.
- Set `enabled=true` + interval=5s → after 5s see logs `[refill] BEGIN`.
- UI shows live countdown ticking down each second.

## Risks
- **Tick ordering**: refill.tick must run BEFORE combatFsm.tick + potEval.eval so that refill's gate flag + busy() take effect same frame.
- **Hot-reload**: changing intervalSec at runtime — lastRefillAt_ unchanged. Acceptable.
