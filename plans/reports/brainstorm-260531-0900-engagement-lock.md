# Brainstorm — Engagement Lock cho Shift+Right-Click

**Date:** 2026-05-31 09:00
**Branch:** master
**Status:** Approved — pending plan

## Problem

`CombatFsm::stepAttacking()` hiện gửi shift+right-click lặp mỗi `attackCooldownMs` (350ms) trong khi engagement. Mỗi tick lại `sweep_.pickAttackPosition()` → click tọa độ mới → game chuyển target liên tục → phá cơ chế auto-chain attack vốn đã được kick-off bởi click đầu tiên.

User feedback: shift+right-click trong PT đã tự động đánh liên tục mob đã chọn. Tool chỉ nên click 1 lần, im lặng ~5s, repick khi mob chết hoặc hết lock.

## Approaches Evaluated

| # | Tên | Pros | Cons |
|---|-----|------|------|
| A | Engagement Lock + jitter | Match đúng game mechanic; tách timer rõ; cho phép early-exit khi mob chết; jitter né pattern | Thêm 1 timer/field; nhẹ |
| B | Single-click-per-pick | KISS tối đa, bỏ luôn attackCooldownMs khi attacking | Mất khả năng cấu hình "min engagement" độc lập với "max dwell" |
| C | Adaptive cooldown theo HP/MP delta | Tự thích nghi tốc độ kill | Phức tạp, dễ flaky, YAGNI |

## Decision — Approach A

**Engagement Lock với jitter, early-exit khi mob chết.**

### Spec

1. Thêm `CombatConfig::engagementLockMs` (default 5000) + `engagementLockJitterMs` (default 500).
2. Thêm field trong `CombatFsm`: `std::chrono::steady_clock::time_point engagementUntil_`.
3. Trong `stepAttacking()`:
   - `bool inLock = now < engagementUntil_`.
   - `bool mobDead = activity_.mobLikelyDead()` (sau `repickMinDwellMs`).
   - `bool forceRepick = (now - lastPickAt_) >= repickMaxDwellMs`.
   - **Skip click** nếu `inLock && !mobDead && !forceRepick`.
   - Khi click: sample lock duration = `engagementLockMs ± uniform(0, jitterMs)` → set `engagementUntil_`.
4. Bỏ block "Continue hitting same spot at cooldown" (line 109-112) — không còn spam click trong engagement.
5. Giữ `attackCooldownMs` chỉ làm sàn cứng giữa 2 click liên tiếp khi repick (anti-burst, anti-detect).

### Files affected

- `src/state/game-state.h` — thêm 2 field vào `CombatConfig`.
- `src/config/config-loader.cpp` — to/fromJson cho 2 field.
- `src/combat/combat-fsm.h` — thêm `engagementUntil_`.
- `src/combat/combat-fsm.cpp` — sửa logic `stepAttacking()`.
- `src/ui/main-window.cpp` — (optional) expose slider Engagement Lock.

### Risks

- **Activity monitor false-positive** (HP/MP đứng yên dù mob còn sống): repick sớm gây re-target. Mitigation: `repickMinDwellMs` (2s hiện tại) đã chặn early death detection trong 2s đầu. Có thể nâng lên 3s nếu vẫn flaky.
- **Lock quá dài → idle khi mob chết im lặng (ranged không drain MP):** activity_ check HP drop của character cũng (mob counter-attack); nếu mob không phản đòn và mình không tốn MP → có thể stuck cho đến `repickMaxDwellMs`. Acceptable vì 15s cap.

### Success criteria

- Log `[combat] engagement start/end` cặp cân bằng.
- Khoảng cách giữa 2 shift+right-click trong cùng engagement = 0 (ngoài initial).
- Khoảng cách trung bình giữa 2 repick ≈ time-to-kill mob + ε.
- Manual test 5 phút: nhân vật giữ một mob đến chết, không chuyển target giữa chừng.

## Open Questions

- Có cần expose `engagementLockMs` lên UI hay để config-only? (Đề xuất: config-only ban đầu, thêm UI nếu cần tune frequent).
- Có cần log mỗi engagement (start/dead/timeout) để analyze offline?
