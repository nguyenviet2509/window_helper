---
phase: 3
title: InputScheduler owner-aware + arbiter integration
status: pending
priority: P0
effort: 0.5d
---

# Phase 3 — InputScheduler ownerHwnd + arbiter

## Context
- Mỗi InputScheduler per window (Phase 1 ctx có 1 unique_ptr `sched`).
- Backend `SendInputBackend` shared (1 instance) — arbiter set target trước slot.
- Scheduler trước khi flush cmd phải acquire slot từ arbiter; trong slot có thể drain nhiều cmd cùng owner.

## Files to modify
- `src/input/input-scheduler.h` — thêm `HWND owner_`, ctor nhận arbiter ref.
- `src/input/input-scheduler.cpp` — wrap dispatch bằng `acquireSlot/releaseSlot`.

## Design

### Header diff
```cpp
class ForegroundArbiter; // fwd

class InputScheduler {
public:
    InputScheduler(IInputBackend& backend, Humanizer& human, OutputGate& gate,
                   ForegroundArbiter& arbiter, HWND owner);
    // ... unchanged ...
private:
    ForegroundArbiter& arbiter_;
    HWND owner_;
};
```

### runLoop changes (pseudocode)
```cpp
while (running_) {
    cv.wait until top of queue ready hoặc stop;
    if (q.empty()) continue;
    InputCmd cmd = q.top(); q.pop();
    unlock;

    // Acquire FG slot for this owner.
    if (!arbiter_.acquireSlot(owner_, cmd.priority, std::chrono::milliseconds(500))) {
        // FG fail → drop cmd, increment counter, continue.
        gatedDrops_++;
        continue;
    }
    // Backend target già được arbiter set (Phase 2 ensureForeground gọi từ arbiter,
    // backend.setTarget được arbiter gọi sau ensureForeground OK).
    // → cần: ForegroundArbiter cũng giữ ref tới backend? KHÔNG — đơn giản hơn: scheduler tự set.
    backend_.setTarget(owner_);

    // Drain cmds same-owner cho đến khi slot expire hoặc khác owner ở top.
    do {
        if (gate_.allow(cmd)) {
            human_.applyJitter(cmd);
            cmd.action(backend_);
            fired_++;
        } else {
            gatedDrops_++;
        }
        if (arbiter_.shouldYield(owner_)) break;
        // Peek tiếp.
        lock;
        if (q.empty() || q.top().priority > cmd.priority + 0) { unlock; break; }
        // Chỉ drain cmd của cùng "process" — vì scheduler này chỉ phục vụ 1 owner,
        // bất kỳ cmd nào trong queue cũng là của owner_. Tiếp tục.
        cmd = q.top(); q.pop();
        unlock;
    } while (true);

    arbiter_.releaseSlot(owner_);
}
```

### Preempt hook
- Khi scheduler `schedule(cmd)` được gọi và `cmd.priority <= P0`:
  - Sau khi push queue, gọi `arbiter_.notifyHigherPriorityWaiting(cmd.priority)`.
  - Nếu scheduler khác đang giữ slot với priority cao hơn (số lớn hơn), `shouldYield` của arbiter sẽ trả true → slot kia release sớm → scheduler này acquire được.

## Backend lifecycle note
- `SendInputBackend` shared giữa N schedulers. Mỗi acquire → setTarget(owner) → SendInput. Vì slot mutex hoá qua arbiter, không race với scheduler khác.
- `OutputGate` per-window (đã trong PerWindowContext) — vẫn gọi `gate.setTarget(owner)` once trong main.cpp khi tạo, sau đó allow() check qua HWND target.

## Files to modify (recap)
- `src/input/input-scheduler.h` + `.cpp`
- `src/main.cpp` — đổi ctor call để pass arbiter + owner.

## Todo
- [ ] Add arbiter ref + owner_ vào InputScheduler.
- [ ] Refactor runLoop: acquire → drain → release.
- [ ] Preempt notify khi schedule P0.
- [ ] Compile check.

## Success criteria
- 1 scheduler/owner; 2 scheduler chia FG fair (slot count ~50/50 sau 100 actions).
- P0 cmd của scheduler B preempt slot của scheduler A đang chạy P4.

## Risks
- Drain quá lâu (cmd P0 mới tới của owner_ chính → vẫn drain tiếp). Cần guard: re-check `arbiter.shouldYield` mỗi cmd → OK.
- `acquireSlot` timeout 500ms → cmd có thể bị drop nếu FG cứ fail. Log để giám sát.
