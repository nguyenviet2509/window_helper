# Phase 01 — Config Fields

**Priority:** P2
**Status:** pending
**Effort:** 15m

## Overview

Add two new fields to `CombatConfig` and wire JSON to/from serialization. Pure data-layer change; no behavior shift until P2.

## Key Insights

- `CombatConfig` lives in `src/state/game-state.h` (~line 30-45). Existing peers: `repickMinDwellMs`, `repickMaxDwellMs`, `attackCooldownMs`.
- JSON helpers in `src/config/config-loader.cpp` lines 57-79. Pattern: emit in to-json object literal, gated `j.contains(...)` read in from-json.
- Defaults from brainstorm: `engagementLockMs = 5000`, `engagementLockJitterMs = 500`.

## Requirements

- Add fields with defaults — back-compat for existing `config.json` (missing keys → defaults).
- Naming consistent with siblings (camelCase, `Ms` suffix).

## Related Files

Modify:
- `src/state/game-state.h`
- `src/config/config-loader.cpp`

## Implementation Steps

1. In `src/state/game-state.h`, inside `CombatConfig`, near `attackCooldownMs`:
   ```cpp
   int engagementLockMs = 5000;        // silence after a click; skip further clicks during this window
   int engagementLockJitterMs = 500;   // uniform [0, jitter] added per engagement
   ```
2. In `src/config/config-loader.cpp` to-json block (around line 63), add:
   ```cpp
   {"engagementLockMs", c.engagementLockMs},
   {"engagementLockJitterMs", c.engagementLockJitterMs},
   ```
3. In from-json block (around line 79), add:
   ```cpp
   if (j.contains("engagementLockMs")) c.engagementLockMs = j["engagementLockMs"];
   if (j.contains("engagementLockJitterMs")) c.engagementLockJitterMs = j["engagementLockJitterMs"];
   ```

## Todo

- [ ] Add 2 fields to `CombatConfig`
- [ ] Add to-json entries
- [ ] Add from-json reads (guarded)
- [ ] `cmake --build` clean

## Success Criteria

- Build green.
- Writing config then reading back round-trips both fields.
- Old `config.json` (without new keys) loads with defaults.

## Risks

- Low. Pure additive struct/JSON. Mitigation: defaults guarantee back-compat.
