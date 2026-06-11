---
name: Spam Skill Mode
slug: spam-skill-mode
date: 260610-1502
status: complete
mode: fast
type: implementation
estimatedLoc: ~130
brainstorm: ../reports/brainstorm-260610-1502-spam-skill-mode.md
amendments:
  - ../reports/brainstorm-260610-1517-human-leftclick-jitter.md
blockedBy: []
blocks: []
---

# Plan — Spam Skill Mode

## Source
Brainstorm: [brainstorm-260610-1502-spam-skill-mode.md](../reports/brainstorm-260610-1502-spam-skill-mode.md)

## Goal
Thêm chế độ "Spam skill": tick checkbox + nhập interval (ms) → bỏ qua mob targeting (sweep/repick/engagement-lock), chỉ right-click lặp tại safe spot. Pot/refill/buff giữ nguyên. Sau buff cycle → lần click kế tiếp = F1 tap + right-click.

## Decisions
- Interval: ms, clamp [100, 10000], default 1500.
- Move cursor về safe spot **1 lần** khi enable; subsequent clicks dùng `sendRightClick(cachedX, cachedY)` (cursor đứng yên tại spot).
- Spam pause khi đang Buffing hoặc refill busy (giữ nguyên priority hệ thống).
- Branch tại `tick()` (state machine repurpose) — không tách FSM mới (DRY).

### Amendment 260610-1517 — Human-like Left-Click Jitter
- Trong Spam mode: mỗi `random uniform [5000, 10000]` ms → fire 1 left-click tại `(spamX_ + rand±10, spamY_ + rand±10)` để fake hành vi người.
- Collision guard: right-click skip tick nếu left-click vừa fire <500ms trước.
- Backend: thêm `sendLeftClick(x, y)` vào i-input-backend + 2 implementations.
- Scope: chỉ Spam mode (Attacking/mob mode KHÔNG đổi).
- Không expose UI config — interval/jitter hardcoded (YAGNI).

## Phases

### Phase 01 — Implementation (single phase)
[phase-01-implementation.md](phase-01-implementation.md)

**Status:** pending
**Files:**
- `src/state/game-state.h` — +2 fields (`spamSkillEnabled`, `spamSkillIntervalMs`)
- `src/combat/combat-fsm.h` — +state `Spamming`, +`stepSpamming`, +`spamX_/spamY_/lastSpamAt_/pendingF1AfterBuff_/nextHumanClickAt_/lastHumanClickAt_`
- `src/combat/combat-fsm.cpp` — implement `stepSpamming` (gồm left-click humanizer), branch trong `tick()`, cache coords trong `enable()`, hook sau `stepBuffing` để chuyển `Spamming` khi spam ON
- `src/ui/main-window.cpp` — checkbox + InputInt + gray-out slider mob khi spam ON
- `src/config/config-loader.cpp` — load/save 2 field mới
- `src/input/i-input-backend.h` — `+sendLeftClick(int, int)`
- `src/input/send-input-backend.h/.cpp` — implement `sendLeftClick` (MOUSEEVENTF_LEFTDOWN/UP)
- `src/input/postmessage-backend.h/.cpp` — implement `sendLeftClick` (WM_LBUTTONDOWN/UP)

## Success Criteria
- Build pass, no warnings.
- Bật spam → cursor về safe spot 1 lần, right-click cadence đúng interval ±50ms.
- F9 ON: buff xen kẽ; click đầu sau buff có F1 tap + right-click (delay ~100ms).
- HP/MP thấp → pot fire bình thường (priority P2 > spam P3).
- Tắt spam giữa session → quay về `Attacking` mob targeting nếu F8 vẫn ON.

## Out of Scope
- Vision-driven spam (vd dừng spam khi safe spot có mob/UI).
- `sendRightClickNoMove()` backend variant.
- Per-window spam config (orthogonal với multi-window plan).
