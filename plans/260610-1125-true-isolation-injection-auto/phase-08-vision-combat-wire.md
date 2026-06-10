---
phase: 8
title: Vision/Combat/Refill wire to InjectionInputBackend
status: pending
priority: P2
effort: 1d
---

# Phase 8 — Wire logic

## Context
Vision + Combat FSM + Refill Scheduler đã có (plan 0928). Đổi backend từ SendInput → Injection. Logic không đổi.

## Files modify
- `src/combat/combat-fsm.cpp` — không đổi (đã abstract qua IInputBackend).
- `src/combat/pot-refill-scheduler.cpp` — verify mouse drag sequence dùng setMousePos + Down/Up đúng.
- `src/dispatch/action-dispatcher.cpp` — không đổi.
- `src/input/input-scheduler.cpp` — handle null arbiter + null pause gracefully (Phase 6 noted).

## Verify points

### Mouse drag refill
Refill drag sequence:
1. `backend.sendMouseMove(inventoryPotX, inventoryPotY)` — set fabricated cursor pos
2. `backend.sendMouseDown(L)` — set button bit
3. `backend.sendMouseMove(quickslotX, quickslotY)` — update pos while button held
4. `backend.sendMouseUp(L)` — clear button bit

PT đọc 4 ticks giữa các steps → register drag. **Critical**: thời gian giữa các steps phải > 1 PT poll interval (~16ms @ 60Hz). Set delay 30-50ms between steps.

### Click target
1. `backend.sendMouseMove(targetX, targetY)`
2. `backend.sendMouseDown(L)`
3. (50-100ms)
4. `backend.sendMouseUp(L)`

### Keyboard skill
1. `backend.sendKeyDown(VK_F1)`
2. (30-60ms)
3. `backend.sendKeyUp(VK_F1)`

## Concurrent multi-window
3 InjectionInputBackend instance (1 per PerProcessContext) — không share state. Mỗi backend write riêng IPC mapping → fabricated state riêng từng PT process. **No foreground arbiter needed.**

## Todo
- [ ] Run combat FSM with InjectionInputBackend stub → verify call sequence.
- [ ] Run refill scheduler → verify drag steps timing.
- [ ] Test N=3 concurrent: 3 backend write IPC độc lập, không cross-talk.

## Success criteria
- Combat skill F1 trong PT register (verify PT character cast skill).
- Refill drag inventory → quickslot work (verify PT character holds pot).
- 3 PT concurrent: combat fires đúng window, không nhầm.

## Risks
- Delay timing giữa drag steps phụ thuộc PT poll rate. Nếu PT poll 30Hz → cần ≥33ms. Test Phase 9 calibrate.
