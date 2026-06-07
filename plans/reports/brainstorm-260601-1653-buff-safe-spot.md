# Brainstorm: Buff Right-Click Safe Spot

**Date:** 2026-06-01 16:53
**Status:** Approved → merge vào plan `260601-1644-buff-timing-per-slot` (phase mới)

## Problem
[combat-fsm.cpp:72-78](../../src/combat/combat-fsm.cpp#L72) right-click ngay tại **center client rect** để confirm self-target buff. Center = chỗ mob thường đứng → right-click trúng mob → game biến skill thành **đánh thường, KHÔNG bao giờ buff**. Worst case không chấp nhận được.

## Constraints xác nhận
- Right-click trúng mob = attack mob (không cast buff).
- Game KHÔNG có self-cast hotkey → bắt buộc right-click.
- Vision mob detection chưa có (plan khác đang pending).

## Approaches Evaluated

| # | Approach | Verdict |
|---|---|---|
| A | Right-click offset sát character | Loại — mob melee đè character vẫn fail |
| B | Multiple right-click 2-3 vị trí | Loại — attack phụ reveal bot |
| C | ESC trước right-click | Loại — risk đóng UI khác |
| **D** | Right-click vào UI safe zone do user pick (% tọa độ) | **Chọn** — user biết game, pick chính xác |
| E | Self-cast hotkey | Loại — game không support |
| F | Đợi vision mob detection | Loại — block bởi plan khác |

## Final Solution (D)

**`CombatConfig` thêm 2 field global:**
```cpp
double buffSafeSpotXPct = 0.5;   // % của client width, 0.0-1.0
double buffSafeSpotYPct = 0.5;   // % của client height, 0.0-1.0
```

**`stepBuffing()` thay center calc:**
```cpp
RECT r{}; GetClientRect(target, &r);
int w = r.right - r.left;
int h = r.bottom - r.top;
int cx = r.left + (int)(w * cfg_.buffSafeSpotXPct);
int cy = r.top  + (int)(h * cfg_.buffSafeSpotYPct);
```

**UI (main-window.cpp) trong panel Combat:**
- 2 DragFloat: "Safe spot X (%)", "Safe spot Y (%)", range 0.0-1.0, step 0.01.
- Hint text: "Tọa độ % chỗ trống không có mob, không có UI. Test trong game trước khi farm."

**Config loader:** toJson/fromJson 2 field mới với default 0.5/0.5.

## Why %, không pixel
- Resolution-independent — đổi window size không phải reconfigure.
- Pick 1 lần, dùng mãi.

## Files Touched
- [src/state/game-state.h](../../src/state/game-state.h) — `CombatConfig` thêm 2 field.
- [src/config/config-loader.cpp](../../src/config/config-loader.cpp) — to/fromJson.
- [src/combat/combat-fsm.cpp](../../src/combat/combat-fsm.cpp) — `stepBuffing()` dùng pct thay vì /2.
- [src/ui/main-window.cpp](../../src/ui/main-window.cpp) — 2 slider + hint.

## Risks
- User pick spot có UI element → click trigger UI (chat/skill bar). Mitigation: hint text + default an toàn user tự test.
- User pick spot vẫn có mob đi qua → vẫn fail occasionally. Mitigation: chấp nhận, không có vision = không có 100%.
- Pct = 0.0 hoặc 1.0 → click ra ngoài client area → game ignore. Mitigation: clamp [0.05, 0.95] trong UI.

## Success Criteria
- 5 vòng cycle test thực địa: 4 buff fire đủ, KHÔNG có lần nào biến thành attack.
- User chỉnh slider → effect immediate sau Apply config.
- Default 0.5/0.5 không crash (dù có thể fail buff).

## Unresolved
- Có cần overlay marker hiển thị safe spot trên screenshot preview không? → KHÔNG, YAGNI. User test trong game thật là đủ.
- Có nên kết hợp ESC trước right-click cho double safety? → KHÔNG, risk đóng UI quan trọng. User pick đúng spot là đủ.
