---
name: buff-timing-per-slot
status: pending
created: 2026-06-01
mode: fast
blockedBy: []
blocks: []
---

# Buff Timing + Safe Spot Refactor

Fix 2 bug cùng module combat-fsm:
1. Race condition: animation buff cũ chưa xong đã chuyển slot kế → miss buff. Tách `castDelayMs` thành 3 field per-slot.
2. Right-click confirm self-target trúng mob → skill biến thành đánh thường, KHÔNG buff. Thêm safe spot % global.

## Context
- Brainstorm timing: [../reports/brainstorm-260601-1637-buff-timing-per-slot.md](../reports/brainstorm-260601-1637-buff-timing-per-slot.md)
- Brainstorm safe spot: [../reports/brainstorm-260601-1653-buff-safe-spot.md](../reports/brainstorm-260601-1653-buff-safe-spot.md)
- Bug location: [src/combat/combat-fsm.cpp:55-92](../../src/combat/combat-fsm.cpp#L55)

## Phases

| # | Phase | Status | Files |
|---|-------|--------|-------|
| 01 | [Config schema + migration](phase-01-config-schema-migration.md) | pending | game-state.h, config-loader.cpp |
| 02 | [FSM timing refactor](phase-02-fsm-timing-refactor.md) | pending | combat-fsm.cpp |
| 03 | [UI fields + validation](phase-03-ui-and-validation.md) | pending | main-window.cpp |
| 04 | [Buff safe spot](phase-04-buff-safe-spot.md) | pending | game-state.h, config-loader.cpp, combat-fsm.cpp, main-window.cpp |

## Dependencies
- 01 → 02 → 03 (timing fix, sequential).
- 04 độc lập với 03 nhưng cùng touch 4 file → khuyên chạy sau 02 để tránh merge conflict.

## Success Criteria
- Build sạch không warning.
- Load `config.json` cũ (chỉ có `castDelayMs`) → auto-migrate, save lại có field mới.
- Test thực địa 5 vòng cycle: 4 buff F2-F5 fire đủ, không miss, KHÔNG có lần nào biến thành đánh thường.
