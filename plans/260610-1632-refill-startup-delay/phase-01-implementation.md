---
phase: 1
title: "Implementation"
status: pending
estimated_loc: ~8
---

# Phase 01: Implementation

## Context Links
- Brainstorm: [brainstorm-260610-1632-refill-startup-delay.md](../reports/brainstorm-260610-1632-refill-startup-delay.md)
- Overview: [plan.md](plan.md)

## Overview
- **Priority**: P2
- **Effort**: ~8 LOC, 1 file modify + 1 header tweak
- **Description**: F8 ON → set last*At_ = now() để lần refill đầu fire sau intervalSec.

## Key Insights
- `enable(bool)` được gọi cả ở init ([main.cpp:141](../../src/main.cpp#L141)) và hot-reload ([main.cpp:206](../../src/main.cpp#L206)).
- Phải check transition `false→true` để tránh hot-reload reset timer accidentally.
- `lastRefillAt = 0` sentinel sẽ KHÔNG còn xảy ra trong runtime sau khi user F8 ON lần đầu — branch `last == 0` trong `isDue` trở thành dead path (chỉ xảy ra khi combat chưa từng ON).

## Implementation Steps

### Step 1: Header ([pot-refill-scheduler.h](../../src/combat/pot-refill-scheduler.h#L20))
Đổi `void enable(bool on) { enabled_ = on; }` thành:
```cpp
void enable(bool on);
```

### Step 2: Impl ([pot-refill-scheduler.cpp](../../src/combat/pot-refill-scheduler.cpp))
Thêm method ngay sau constructor (~line 60):
```cpp
// F8 transition OFF->ON: reset last*At_ = now() để lần refill đầu fire sau intervalSec
// (thay vì instant). Hot-reload với enabled=true mà đang ON: KHÔNG reset.
void PotRefillScheduler::enable(bool on) {
    if (on && !enabled_) {
        auto now = std::chrono::steady_clock::now();
        lastHpAt_ = lastSpAt_ = lastMpAt_ = now;
    }
    enabled_ = on;
}
```

### Step 3: Build check
Chạy [build.ps1](../../build.ps1) → verify compile clean.

### Step 4: Manual smoke test
- Set HP `intervalSec=10` cho nhanh test
- F8 ON → đếm 10s → confirm refill fire (không phải instant)
- F8 OFF, đợi vài giây, F8 ON lại → confirm refill fire sau thêm 10s nữa

## Todo List
- [ ] Step 1: Modify header — change `enable()` to declaration
- [ ] Step 2: Implement `enable()` in .cpp với transition logic
- [ ] Step 3: Compile check
- [ ] Step 4: Manual smoke test (F8 ON → fire sau intervalSec)
- [ ] Step 5: Edge case test — F8 OFF→ON giữa session → timer reset

## Success Criteria
1. Compile clean
2. F8 ON với intervalSec=N → refill không fire trong N giây đầu
3. F8 OFF→ON lại → timer restart
4. Hot-reload config khi đang ON → timer KHÔNG reset

## Risk
- **Risk thấp**: 1 file modify, ~8 LOC, logic cô lập

## Unresolved Questions
None.
