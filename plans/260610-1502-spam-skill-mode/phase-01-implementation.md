---
phase: 01
name: Implementation
status: complete
estimatedLoc: ~130
amendments:
  - ../reports/brainstorm-260610-1517-human-leftclick-jitter.md
---

# Phase 01 — Implementation

## Context Links
- Plan: [plan.md](plan.md)
- Brainstorm: [../reports/brainstorm-260610-1502-spam-skill-mode.md](../reports/brainstorm-260610-1502-spam-skill-mode.md)
- Amendment: [../reports/brainstorm-260610-1517-human-leftclick-jitter.md](../reports/brainstorm-260610-1517-human-leftclick-jitter.md)
- Current FSM: [src/combat/combat-fsm.cpp](../../src/combat/combat-fsm.cpp)
- Current UI: [src/ui/main-window.cpp](../../src/ui/main-window.cpp)
- Config struct: [src/state/game-state.h](../../src/state/game-state.h)
- Input backend interface: [src/input/i-input-backend.h](../../src/input/i-input-backend.h)

## Overview
- **Priority:** Medium (UX feature)
- **Status:** pending
- Single phase, ~130 LOC across 8 files (5 original + 3 input backend files cho `sendLeftClick`).

## Key Insights
- FSM hiện tại: `Idle → Buffing → Arming → Attacking`. Buff cycle + refill pause + pot priority đã chạy tốt → giữ nguyên, chỉ replace nhánh `Attacking` bằng `Spamming` khi `spamSkillEnabled`.
- Safe spot đã có (`buffSafeSpotXPct/YPct`) dùng cho buff confirm. Spam tái dùng cùng tọa độ.
- `sendRightClick(x,y)` của backend move-then-click; click cùng tọa độ → cursor perception đứng yên.

## Requirements

### Functional
- Checkbox "Spam skill" trong UI combat panel.
- InputInt "Interval (ms)" — clamp [100, 10000], default 1500.
- Bật spam khi F8 ON: cursor về safe spot 1 lần (mouse move), sau đó right-click lặp theo interval.
- Sau khi buff cycle hoàn tất: click kế tiếp = F1 tap + right-click (delay ~100ms).
- Buff/F9, pot, refill: hành vi không đổi.
- Persist 2 field mới qua config.json.

### Non-Functional
- Build pass MSVC, no new warnings.
- Không vượt 200 LOC/file (combat-fsm.cpp hiện 189 → cẩn thận, có thể chạm 240 → OK cho file FSM core, hoặc tách helper nếu cần).

## Architecture

### State machine update
```
Idle ──► Buffing ─► Arming ─► Attacking
              │                    │
              └─► (spam ON) ─► Spamming ◄┐
                                    │   │
                       (buff due) ──┘   │
                       (interval) ──────┘
```

### Data flow
1. UI toggle → `CombatConfig.spamSkillEnabled = true` → `updateConfig()` → FSM cache mới.
2. F8 ON → `enable(true)`:
   - Compute & cache `spamX_, spamY_` từ `GetClientRect(target_) * buffSafeSpot{X,Y}Pct`.
   - Schedule 1 mouse move tới `(spamX_, spamY_)`.
   - Set `pendingF1AfterBuff_ = true`.
3. `tick()`:
   - State `Attacking` + `spamSkillEnabled` → `stepSpamming()`.
   - State `Spamming` → `stepSpamming()`.
4. `stepSpamming(now)`:
   - Refill busy → return.
   - Buff due (`findDueSlot >= 0`) → `enterBuffing(now)`, set `pendingF1AfterBuff_ = true` sau buff (xử lý trong `stepBuffing` hoặc kiểm khi quay lại).
   - Chưa đủ interval → return.
   - Fire: nếu `pendingF1AfterBuff_` → tap F1 + schedule right-click +100ms; else right-click ngay.
   - `lastSpamAt_ = now`.

## Related Code Files

### Modify
- `src/state/game-state.h` — thêm 2 field vào `CombatConfig`.
- `src/combat/combat-fsm.h` — thêm state `Spamming`, method `stepSpamming`, member vars (gồm `nextHumanClickAt_/lastHumanClickAt_`).
- `src/combat/combat-fsm.cpp` — implement logic (gồm left-click humanizer).
- `src/ui/main-window.cpp` — UI controls.
- `src/config/config-loader.cpp` — load/save.
- `src/input/i-input-backend.h` — thêm pure virtual `sendLeftClick(int, int)`.
- `src/input/send-input-backend.h/.cpp` — implement `sendLeftClick`.
- `src/input/postmessage-backend.h/.cpp` — implement `sendLeftClick`.

### Read (context)
- `src/input/input-scheduler.h` — `InputCmd` struct, priorities.
- `src/input/input-backend.h` (or similar) — `sendKeyTap`, `sendRightClick`, `sendMouseMove` signatures.
- `src/combat/pot-refill-scheduler.h` — `busy()` để check trong `stepSpamming`.

### Create
- None (modular changes in-place).

## Implementation Steps

1. **Config struct** (`game-state.h`):
   ```cpp
   struct CombatConfig {
       // ... existing fields ...
       bool spamSkillEnabled = false;
       int  spamSkillIntervalMs = 1500;
   };
   ```

2. **FSM header** (`combat-fsm.h`):
   - `enum class CombatState { Idle, Buffing, Arming, Attacking, Spamming };`
   - Method: `void stepSpamming(std::chrono::steady_clock::time_point now);`
   - Member: `int spamX_ = 0, spamY_ = 0;`
   - Member: `std::chrono::steady_clock::time_point lastSpamAt_{};`
   - Member: `bool pendingF1AfterBuff_ = false;`
   - Member (humanizer): `std::chrono::steady_clock::time_point nextHumanClickAt_{}, lastHumanClickAt_{};`
   - Helper: `void cacheSpamSpot();` — compute từ `GetClientRect(target_)` + clamp pct.
   - Helper inline: `int rollHumanIntervalMs() const { return 5000 + (std::rand() % 5001); }` (range [5000, 10000])
   - Helper inline: `int jitter10() const { return (std::rand() % 21) - 10; }` (range [-10, +10])

3. **FSM impl** (`combat-fsm.cpp`):
   - `enable(bool on)`:
     - Nếu `on && cfg_.spamSkillEnabled`: `cacheSpamSpot()`, schedule mouse move P3, set `pendingF1AfterBuff_ = true`, `nextHumanClickAt_ = now + ms(rollHumanIntervalMs())`, state = `Spamming` (skip `enterArming`).
     - Else: hành vi cũ.
   - `updateConfig(cfg)`:
     - Detect spam toggle on/off. Nếu vừa toggle ON và `enabled_`: cache spot + schedule move + set flag + reset `nextHumanClickAt_`.
   - `stepBuffing` — sau khi `findDueSlot` trả -1 (buff cycle xong):
     ```cpp
     if (cfg_.spamSkillEnabled && enabled_) {
         pendingF1AfterBuff_ = true;
         state_ = CombatState::Spamming;
         lastSpamAt_ = now;   // reset cadence
         return;
     }
     enterArming(now);   // old path
     ```
   - Implement `stepSpamming(now)` với left-click humanizer:
     ```cpp
     void CombatFsm::stepSpamming(time_point now) {
         if (refill_ && refill_->busy()) return;
         if (findDueSlot(now) >= 0) { enterBuffing(now); return; }

         // 3. Human-like left-click jitter
         if (now >= nextHumanClickAt_) {
             int jx = spamX_ + jitter10();
             int jy = spamY_ + jitter10();
             InputCmd c;
             c.priority = P3_Combat;
             c.fireAt = now;
             c.action = [jx, jy](IInputBackend& b) { b.sendLeftClick(jx, jy); };
             sched_.schedule(std::move(c));
             lastHumanClickAt_ = now;
             nextHumanClickAt_ = now + ms(rollHumanIntervalMs());
             return;   // skip right-click tick này
         }

         // 4. Collision guard
         if ((now - lastHumanClickAt_) < ms(500)) return;

         // 5. Interval check
         if ((now - lastSpamAt_) < ms(cfg_.spamSkillIntervalMs)) return;

         // 6. Fire right-click (F1+rc nếu pendingF1AfterBuff_)
         if (pendingF1AfterBuff_) {
             WORD vk = cfg_.mainAttackKey;
             InputCmd k; k.priority = P3_Combat; k.fireAt = now;
             k.action = [vk](IInputBackend& b) { b.sendKeyTap(vk, 30); };
             sched_.schedule(std::move(k));
             int cx = spamX_, cy = spamY_;
             InputCmd r; r.priority = P3_Combat; r.fireAt = now + ms(100);
             r.action = [cx, cy](IInputBackend& b) { b.sendRightClick(cx, cy); };
             sched_.schedule(std::move(r));
             pendingF1AfterBuff_ = false;
         } else {
             int cx = spamX_, cy = spamY_;
             InputCmd r; r.priority = P3_Combat; r.fireAt = now;
             r.action = [cx, cy](IInputBackend& b) { b.sendRightClick(cx, cy); };
             sched_.schedule(std::move(r));
         }
         lastSpamAt_ = now;
     }
     ```
   - `tick()`: thêm case `Spamming`; trong case `Attacking`, branch:
     ```cpp
     case CombatState::Attacking:
         if (cfg_.spamSkillEnabled) stepSpamming(now);
         else stepAttacking(v, now);
         break;
     case CombatState::Spamming:
         stepSpamming(now);
         break;
     ```

4. **UI** (`main-window.cpp`):
   - Trong combat section, trên/dưới Safe spot sliders:
     ```cpp
     if (ImGui::Checkbox("Spam skill (chỉ right-click safe spot)", &c.spamSkillEnabled)) any = true;
     if (c.spamSkillEnabled) {
         int iv = c.spamSkillIntervalMs;
         if (ImGui::InputInt("Spam interval (ms)", &iv, 100, 500)) {
             c.spamSkillIntervalMs = std::clamp(iv, 100, 10000);
             any = true;
         }
     }
     ```
   - Gray-out mob-targeting sliders khi `c.spamSkillEnabled`:
     ```cpp
     ImGui::BeginDisabled(c.spamSkillEnabled);
     // ... attackRadius, repickDwell, engagementLock sliders ...
     ImGui::EndDisabled();
     ```

5. **Config persistence** (`config-loader.cpp`):
   - Load: `cfg.spamSkillEnabled = j.value("spamSkillEnabled", false);`
   - Load: `cfg.spamSkillIntervalMs = std::clamp(j.value("spamSkillIntervalMs", 1500), 100, 10000);`
   - Save: dump 2 field.

6. **Backend `sendLeftClick`** (3 files):
   - `i-input-backend.h`: thêm `virtual void sendLeftClick(int x, int y) = 0;`
   - `send-input-backend.h/.cpp`: implement với `MOUSEEVENTF_LEFTDOWN/LEFTUP` (mirror `sendRightClick`, đổi flag).
   - `postmessage-backend.h/.cpp`: implement với `WM_LBUTTONDOWN/WM_LBUTTONUP` (mirror `sendRightClick`).

7. **Compile check:** `./build.ps1` (hoặc cmake build) — fix warnings/errors.

## Todo List
- [ ] Add `spamSkillEnabled` + `spamSkillIntervalMs` vào `CombatConfig`.
- [ ] Add `sendLeftClick` vào `i-input-backend.h` + 2 implementations (SendInput + PostMessage).
- [ ] Add `Spamming` state, members (incl. `nextHumanClickAt_/lastHumanClickAt_`), `cacheSpamSpot()`, helper `rollHumanIntervalMs()/jitter10()` vào combat-fsm.h.
- [ ] Implement `stepSpamming()` logic (refill/buff/humanizer/collision-guard/interval/fire).
- [ ] Hook spam path trong `enable()` + `updateConfig()` + `stepBuffing()` + `tick()`.
- [ ] Add UI checkbox + InputInt + BeginDisabled wrapper cho mob sliders.
- [ ] Persist 2 field qua config-loader.
- [ ] Build + smoke test (cursor về safe spot 1 lần, cadence đúng, F1 sau buff, left-click jitter mỗi 5-10s, pot vẫn fire).

## Success Criteria
- Build pass, no new warnings.
- Manual test sequence:
  1. F8 ON, spam OFF → mob targeting bình thường.
  2. F8 OFF, tick spam, F8 ON → cursor về safe spot, right-click cadence = interval ±50ms, **không** shift+right-click.
  3. F9 ON đồng thời → buff cycle xen kẽ; lần click sau buff có F1 tap (~100ms) trước right-click.
  4. Set HP thấp giả → pot fire bình thường, không bị spam chặn.
  5. Untick spam giữa session → quay về mob targeting (Attacking state).
  6. Trong Spam mode: left-click fire 1 lần mỗi 5-10s (đo qua log/timing); vị trí jitter ±10px quanh safe spot mỗi lần (không cố định).
  7. Sau left-click fire, right-click kế tiếp delay ≥500ms (collision guard).

## Risk Assessment
| Risk | Mitigation |
|---|---|
| `combat-fsm.cpp` vượt 200 LOC (189 → ~240) | Acceptable cho FSM core; nếu vượt 250, tách `stepSpamming` ra file riêng `combat-fsm-spam.cpp`. |
| Buff cycle không set `pendingF1AfterBuff_` đúng (set sai chỗ → F1 tap nhầm lúc) | Set flag tại điểm thoát `stepBuffing` (khi `findDueSlot` trả -1), KHÔNG set trong từng tick của Buffing. |
| User đổi safe spot pct trong khi spam đang chạy → cursor stuck tại spot cũ | `updateConfig` detect pct thay đổi → re-cache `spamX_/Y_` + schedule mouse move. |
| `sendMouseMove` không tồn tại trong backend interface | Check `input-backend.h`. Nếu thiếu, dùng `sendRightClick` lần đầu coi như "move" (cursor sẽ tới đó), hoặc thêm `sendMouseMove` (1 liner). |

## Security Considerations
- N/A — không có auth/data flow. Input injection vẫn qua scheduler hiện hữu (anti-detect mousepath áp dụng nếu enabled).

## Next Steps
- Sau khi merge: cân nhắc gate spam theo MP (nếu skill cast cần MP > X) — out of scope phase này.
- Multi-window: spam config per-tab — chờ multi-window plan (260610-0928) hoàn tất rồi review interaction.
