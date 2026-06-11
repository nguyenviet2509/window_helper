---
type: brainstorm
slug: human-leftclick-jitter
date: 260610-1517
amends: ../260610-1502-spam-skill-mode/plan.md
---

# Brainstorm Amendment — Human-like Left-Click Jitter (Spam Mode)

## Problem
Right-click cadence cố định trong Spam mode (interval ms) trông quá máy. Thêm left-click ngẫu nhiên định kỳ tại safe spot để fake hành vi người.

## Decisions (locked qua AskUserQuestion)
| # | Decision |
|---|---|
| 1 | **Scope:** chỉ Spam mode ON. Mob targeting mode không đổi. |
| 2 | **Interval:** random uniform `[5000, 10000]` ms, re-draw sau mỗi click. |
| 3 | **Position:** jitter `±10px` quanh `(spamX_, spamY_)` mỗi click. |
| 4 | **Collision:** right-click skip tick nếu `now - lastHumanClickAt_ < 500ms`. |

## Final Solution

### Backend additions
- `src/input/i-input-backend.h`:
  ```cpp
  virtual void sendLeftClick(int x, int y) = 0;
  ```
- `src/input/send-input-backend.h/.cpp`: implement với `MOUSEEVENTF_LEFTDOWN/LEFTUP` (giống `sendRightClick` pattern).
- `src/input/postmessage-backend.h/.cpp`: implement với `WM_LBUTTONDOWN/WM_LBUTTONUP` (giống `sendRightClick` pattern).

### FSM additions (`combat-fsm.h/.cpp`)
- Members:
  ```cpp
  std::chrono::steady_clock::time_point nextHumanClickAt_{};
  std::chrono::steady_clock::time_point lastHumanClickAt_{};
  ```
- Helper inline:
  ```cpp
  // Range [5000, 10000] ms
  int rollHumanIntervalMs() const { return 5000 + (std::rand() % 5001); }
  int jitter10() const { return (std::rand() % 21) - 10; }   // [-10, +10]
  ```
- `enable(true)` khi `spamSkillEnabled`: `nextHumanClickAt_ = now + ms(rollHumanIntervalMs());`
- `stepSpamming(now)` flow update:
  ```
  1. refill busy → return
  2. buff due → enterBuffing, return
  3. now >= nextHumanClickAt_:
       jx = spamX_ + jitter10()
       jy = spamY_ + jitter10()
       schedule sendLeftClick(jx, jy) P3
       lastHumanClickAt_ = now
       nextHumanClickAt_ = now + ms(rollHumanIntervalMs())
       return
  4. (now - lastHumanClickAt_) < 500ms → return    // collision guard
  5. (now - lastSpamAt_) < intervalMs → return
  6. Fire right-click (F1+rc nếu pendingF1AfterBuff_, else rc thường)
     lastSpamAt_ = now
  ```

### LOC delta on top of phase-01 baseline (~80)
| File | +LOC |
|---|---|
| `src/input/i-input-backend.h` | +1 |
| `src/input/send-input-backend.h/.cpp` | +12 |
| `src/input/postmessage-backend.h/.cpp` | +12 |
| `src/combat/combat-fsm.h` | +3 |
| `src/combat/combat-fsm.cpp` | +20 |

**Total phase-01 sau amendment: ~130 LOC.**

## Risks
- Safe spot user pick "không có UI/mob" → left-click không gây side effect (mở UI, deselect target). User chịu trách nhiệm pick spot.
- Pattern jitter `[5000, 10000]` ms vẫn có thể detect bằng statistical analysis. Nếu cần "tier-2 anti-detect" → cộng thêm Gaussian noise hoặc burst pattern. Out of scope phase này.
- Priority `P3` cho left-click → pot/refill vẫn ưu tiên cao hơn (P2).

## Success Criteria (additions to phase-01)
- Trong Spam mode, left-click fire 1 lần mỗi 5-10s (đo qua log timestamps).
- Vị trí left-click jitter ±10px quanh safe spot mỗi lần (không cố định).
- Sau khi left-click fire, right-click kế tiếp delay ≥500ms.
- Mob mode (Spam OFF) hoàn toàn không bị ảnh hưởng.

## Unresolved
- Có expose interval/jitter range ra UI sau này không? Default: không (YAGNI).
