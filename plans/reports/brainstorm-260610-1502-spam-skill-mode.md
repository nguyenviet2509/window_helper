---
type: brainstorm
slug: spam-skill-mode
date: 260610-1502
---

# Brainstorm — Spam Skill Mode

## Problem
Auto cơ bản đã hoàn thiện. Cần thêm chế độ "Spam skill": tick checkbox → bỏ qua toàn bộ cơ chế đánh quái (sweep, repick, engagement-lock, vision mob targeting), chỉ di chuyển trỏ chuột về safe spot 1 lần khi bật rồi right-click lặp theo interval (ms). Pot/refill/buff giữ nguyên.

## Decisions (locked qua AskUserQuestion)
| # | Quyết định |
|---|---|
| 1 | **Behavior:** sau buff cycle hoàn tất → next click = F1 tap + right-click. Click thường (không buff trước đó) → chỉ right-click. |
| 2 | **Interval unit:** milliseconds, range [100, 10000], default 1500. |
| 3 | **Buff interaction:** giữ nguyên — F9 ON vẫn buff định kỳ; spam pause khi đang buffing (giống refill). |
| 4 | **Mouse:** move tới safe spot 1 lần khi enable, click tại chỗ (sendRightClick(cachedX, cachedY) mỗi lần — cursor "đứng yên" từ góc nhìn user). |

## Approaches Evaluated

**A. Branch tại tick() — repurpose Attacking nhánh (CHỌN)**
- Pros: DRY, không thêm state mới phức tạp; tái dùng Buffing/Arming/refill-pause/pot-priority.
- Cons: tick() có nhánh if/else thêm 1 lớp.

**B. Tách FSM riêng (SpamFsm)**
- Pros: separation of concerns rõ ràng.
- Cons: duplicate logic buff/refill-pause; vi phạm YAGNI cho scope nhỏ này.

**C. Bỏ FSM, dùng standalone ticker scheduler**
- Pros: cực kỳ đơn giản nếu không cần buff sync.
- Cons: không phối hợp được với buff cycle (cần F1 sau buff).

→ **A** thắng vì tái dùng max, ít LOC nhất, vẫn xử lý đúng F1-after-buff requirement.

## Final Solution

### Config additions ([src/state/game-state.h](src/state/game-state.h))
```cpp
struct CombatConfig {
    // ... existing ...
    bool spamSkillEnabled = false;
    int  spamSkillIntervalMs = 1500;   // clamp [100, 10000]
};
```

### FSM ([src/combat/combat-fsm.h/.cpp](src/combat/combat-fsm.cpp))
- Thêm state `Spamming`.
- `enable(true)` khi `spamSkillEnabled`:
  - Cache safe-spot coords (`spamX_, spamY_`) tính từ client rect + buffSafeSpotXPct/YPct.
  - Schedule 1 `sendMouseMove(spamX_, spamY_)` với priority P3.
  - Set `pendingF1AfterBuff_ = true` (lần click đầu cũng có F1 tap, cho consistency).
- `stepBuffing` sau khi `findDueSlot` trả -1: nếu `spamSkillEnabled` → set `pendingF1AfterBuff_ = true` và chuyển `Spamming` (thay vì `enterArming`).
- `stepSpamming(now)`:
  ```cpp
  if (refill_ && refill_->busy()) return;       // pot-refill pause
  if (findDueSlot(now) >= 0) { enterBuffing(now); return; }   // buff due → buff
  if (now - lastSpamAt_ < ms(cfg_.spamSkillIntervalMs)) return;
  if (pendingF1AfterBuff_) {
      // F1 tap + right-click sau ~100ms
      schedule tap(mainAttackKey, 30ms);
      schedule rightClick(spamX_, spamY_) at +100ms;
      pendingF1AfterBuff_ = false;
  } else {
      schedule rightClick(spamX_, spamY_);
  }
  lastSpamAt_ = now;
  ```
- `tick()`:
  ```cpp
  case CombatState::Attacking:
      if (cfg_.spamSkillEnabled) stepSpamming(now);
      else stepAttacking(v, now);
      break;
  case CombatState::Spamming:
      stepSpamming(now); break;
  ```
- `updateConfig`: nếu spam toggled khi đang chạy → re-cache coords, reset `lastSpamAt_`.

### UI ([src/ui/main-window.cpp](src/ui/main-window.cpp))
Trong combat panel, gần safe-spot sliders:
```
[x] Spam skill (bỏ qua đánh quái, chỉ right-click safe spot)
    Interval: [1500] ms     (InputInt, clamp 100..10000)
```
Khi `spamSkillEnabled` ON: disable (gray out) các slider `attackRadius*`, `repickDwell*`, `engagementLock*` để UX rõ — chúng không có tác dụng.

### Config persistence ([src/config/config-loader.cpp](src/config/config-loader.cpp))
Load/save 2 field mới, default fallback.

## Files Changed
| File | LOC delta |
|---|---|
| `src/state/game-state.h` | +2 |
| `src/combat/combat-fsm.h` | +6 |
| `src/combat/combat-fsm.cpp` | +50 |
| `src/ui/main-window.cpp` | +20 |
| `src/config/config-loader.cpp` | +6 |

**Tổng ~80 LOC.**

## Risks
- **Cursor re-move mỗi click:** backend `sendRightClick(x,y)` move-then-click; cursor về cùng tọa độ → perception vẫn "đứng yên". Né scope creep `sendRightClickNoMove()`.
- **Spam khi MP=0:** không gate MP. Skill miss cast nhưng pot HP/MP vẫn cứu char.
- **Pot priority:** pot/refill priority cao hơn spam (P2 > P3) → không xung đột.

## Success Criteria
- Build pass, no warnings.
- Bật spam → cursor về safe spot 1 lần, right-click cadence đúng interval ±50ms.
- F9 ON: buff cycle xen kẽ; lần click đầu sau buff có F1 tap trước right-click ~100ms.
- HP thấp → pot fire bình thường, không bị spam chặn.
- Tắt spam giữa session → FSM quay về `Attacking` (mob targeting) nếu F8 vẫn ON.

## Unresolved
1. Log mỗi N click để debug? Default: im lặng.
2. Khi `spamSkillEnabled` ON nhưng F8 OFF — vẫn gray-out slider mob hay chỉ khi cả hai ON? Đề xuất: gray-out ngay khi spam ON, không phụ thuộc F8.
