# Brainstorm: Buff Timing Per-Slot Refactor

**Date:** 2026-06-01 16:37
**Status:** Approved → plan created at `plans/260601-1644-buff-timing-per-slot/`

## Problem
`combat.buffs[].castDelayMs` đang gánh 2 vai trò: (a) delay F→right-click = `castDelayMs/2`, (b) gap sang slot kế = `castDelayMs`. Khi animation thực của skill dài hơn `castDelayMs`:
- Right-click rơi giữa animation → có thể bị nuốt.
- Key F kế tiếp fire khi animation cũ chưa xong → input lost hoặc cancel cast trước.
- Hệ quả: buff bị miss, vòng cycle 300s rebuff không đảm bảo full 4 buff.

Code hiện tại: [combat-fsm.cpp:55-92](../../src/combat/combat-fsm.cpp#L55).

## Context
- Right-click sau F = **confirm self-target** (game đổi cursor sau khi bấm skill, right-click apply lên bản thân).
- Animation 4 buff F2-F5 **khác nhau rõ rệt** → cần config per slot.
- Cycle rebuff = `cycleDurationSec` (default 300s), độc lập với timing trong 1 vòng buff.

## Approaches Evaluated

| # | Approach | Verdict |
|---|---|---|
| A | Tách `animationMs` + `postBuffGapMs` + `rightClickDelayMs` per slot | **Chọn** — mô hình đúng vật lý, tinh chỉnh per skill |
| B | Thêm `interBuffGuardMs` global, right-click late | Loại — không xử lý được animation khác nhau |
| C | Vision detect cast finish | Loại — YAGNI, over-engineer |

## Final Solution (A)

**`BuffSlotCfg` thêm 3 field:**
- `rightClickDelayMs` (default 100): delay F→right-click, đủ cho game đổi cursor self-target.
- `animationMs` (default 800): cast animation full của skill.
- `postBuffGapMs` (default 150): safety gap trước slot kế.

**Drop `castDelayMs`** sau migration.

**Timeline mới (mỗi slot, đo từ F press):**
```
t = 0                              F key tap
t = rightClickDelayMs              Right-click confirm self-target → cast start
t = animationMs + postBuffGapMs    Slot kế tiếp
```

**Migration:** JSON có `castDelayMs` mà không có `animationMs` → map `animationMs = castDelayMs`, `postBuffGapMs = 150`, `rightClickDelayMs = 100`. Save lần kế ghi field mới, không ghi field cũ.

## Files Touched
- [src/state/game-state.h](../../src/state/game-state.h) — `BuffSlotCfg` thêm 3 field, bỏ `castDelayMs`.
- [src/config/config-loader.cpp](../../src/config/config-loader.cpp) — `toJson`/`fromJson` + migration fallback.
- [src/combat/combat-fsm.cpp](../../src/combat/combat-fsm.cpp) — `stepBuffing()` dùng field mới.
- [src/ui/main-window.cpp](../../src/ui/main-window.cpp) — 3 DragInt per slot + warning nếu Σ > cycle*0.5.

## Risks
- Migration sai → log warning, default an toàn.
- `Σ (animationMs + postBuffGapMs) > cycleDurationSec` → UI warning, không block save.
- `rightClickDelayMs = 0` → right-click rơi vào map. Default 100ms.

## Success Criteria
- 4 buff F2-F5 fire đủ trong 1 vòng, không miss (test 5 cycle thực địa).
- Right-click luôn apply self-target (buff icon xuất hiện đủ 4).
- Load config cũ không crash, save lại có field mới.

## Unresolved
- Có giữ `castDelayMs` deprecated trong struct không? → Đề xuất xóa hẳn sau migration.
- Validation save block hay warning? → Warning only, user tự chịu.
