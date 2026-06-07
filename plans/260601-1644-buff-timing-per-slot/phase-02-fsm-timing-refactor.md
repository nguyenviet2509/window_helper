# Phase 02: FSM Timing Refactor

**Priority:** High | **Status:** pending | **Blocked by:** Phase 01

## Context
- File: [src/combat/combat-fsm.cpp](../../src/combat/combat-fsm.cpp) — `stepBuffing()` line 55-92.
- Schema mới từ phase 01.

## Goal
`stepBuffing()` dùng `rightClickDelayMs`, `animationMs`, `postBuffGapMs` thay cho `castDelayMs`.

## Changes

`src/combat/combat-fsm.cpp` — replace `stepBuffing()` body:

```cpp
void CombatFsm::stepBuffing(std::chrono::steady_clock::time_point now) {
    if (now < nextStepAt_) return;

    auto slot = buffs_.nextBuff();
    if (!slot) { enterArming(now); return; }

    HWND target = target_;
    WORD vk = slot->key;
    int hold = 30;

    // 1. F key tap at now
    InputCmd keyCmd;
    keyCmd.priority = P4_Buff;
    keyCmd.fireAt = now;
    keyCmd.action = [vk, hold](IInputBackend& b) { b.sendKeyTap(vk, hold); };
    sched_.schedule(std::move(keyCmd));

    // 2. Right-click confirm self-target sau rightClickDelayMs (sớm, không phải giữa animation)
    if (slot->rightClickAfter) {
        RECT r{}; GetClientRect(target, &r);
        int cx = (r.right + r.left) / 2;
        int cy = (r.bottom + r.top) / 2;
        InputCmd clickCmd;
        clickCmd.priority = P4_Buff;
        clickCmd.fireAt = now + ms(slot->rightClickDelayMs);
        clickCmd.action = [cx, cy](IInputBackend& b) { b.sendRightClick(cx, cy); };
        sched_.schedule(std::move(clickCmd));
    }

    // 3. Slot kế tiếp: chờ animation xong + safety gap
    int totalMs = slot->animationMs + slot->postBuffGapMs;
    nextStepAt_ = now + ms(totalMs);
    ++buffsDeliveredThisRound_;

    int enabledCount = 0;
    for (const auto& b : cfg_.buffs) if (b.enabled) ++enabledCount;
    if (buffsDeliveredThisRound_ >= enabledCount) {
        state_ = CombatState::Arming;
        nextStepAt_ = now + ms(totalMs);  // chờ buff cuối xong rồi mới arming
    }
}
```

**Lưu ý:**
- Bỏ `castDelayMs / 2` ở right-click → dùng `rightClickDelayMs` cố định nhỏ.
- Bỏ `castDelayMs + 200` ở end-of-round → dùng cùng `animationMs + postBuffGapMs`.

## Todo
- [ ] Replace `stepBuffing()` implementation theo snippet trên
- [ ] Build full project, verify compile sạch
- [ ] Smoke test: enable combat, quan sát log xem F2→F3→F4→F5 fire đúng thứ tự với khoảng cách `animationMs + postBuffGapMs`

## Success Criteria
- Build pass, không warning.
- Timeline log khớp config: F fire ngay, right-click sau `rightClickDelayMs`, slot kế sau `animationMs + postBuffGapMs`.

## Risks
- Round-complete branch dùng `totalMs` thay vì hằng số cũ → có thể dài hơn, OK vì cần đợi buff cuối xong.
- Nếu `rightClickDelayMs > animationMs` (config sai) → right-click rơi sau slot kế đã start. Phase 03 thêm UI validation.
