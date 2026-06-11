---
type: brainstorm
date: 2026-06-10 16:32
slug: refill-startup-delay
status: approved
---

# Brainstorm: Refill Startup Delay (Chờ sau F8)

## Problem Statement
Khi `refill.enabled = true` và user bấm F8 bật combat, [pot-refill-scheduler.cpp:82](src/combat/pot-refill-scheduler.cpp#L82) trả `isDue=true` ngay tick đầu vì `lastRefillAt = 0` (chưa từng refill). Refill fire instant → user không có thời gian setup char (vào vị trí, mở kho có sẵn, etc.).

## Requirements
- Sau khi bấm F8 bật combat, đợi N giây mới cho phép refill lần đầu của mỗi slot
- Per-slot config (HP/SP/MP riêng) — flexible cho các use case khác nhau
- Reset timer mỗi lần combat toggle OFF→ON (qua F8 hoặc UI)
- Default 60s, backward compat với config cũ
- Không thêm log noise

## Approaches Evaluated

### A. Per-slot `startupDelaySec` field (CHỌN)
- Thêm `startupDelaySec` vào `PotRefillSlot` struct
- Track `combatEnabledAt_` trong `PotRefillScheduler`
- Gate trong `isDue()`: nếu slot chưa refill lần nào → check `now - combatEnabledAt_ >= startupDelaySec`
- **Pro**: flexible, KISS, cô lập trong scheduler
- **Con**: 3 field UI thêm

### B. Global startup delay
- 1 field chung cho toàn bộ refill
- **Pro**: ít field UI
- **Con**: không flexible khi user muốn HP nạp sớm, MP nạp trễ

### C. Reuse `intervalSec` làm delay lần đầu
- Lần đầu chờ đủ `intervalSec` rồi mới fire
- **Pro**: zero field mới
- **Con**: cứng nhắc — HP 60s = phải đợi 60s, không tách bạch được "setup time" vs "refill cycle"

## Final Solution

### Config Schema ([game-state.h](src/state/game-state.h))
```cpp
struct PotRefillSlot {
    bool enabled = false;
    int intervalSec = 0;
    int startupDelaySec = 60;   // NEW: delay sau F8 trước lần refill đầu
};
```

### Scheduler State ([pot-refill-scheduler.h](src/combat/pot-refill-scheduler.h))
- Thêm member: `TP combatEnabledAt_{};`
- Modify `enable(bool on)`: chỉ set `combatEnabledAt_ = now()` khi transition `false→true` (tránh hot-reload reset)

### Gate Logic ([pot-refill-scheduler.cpp:82](src/combat/pot-refill-scheduler.cpp#L82))
```cpp
if (last.time_since_epoch().count() == 0) {
    if (combatEnabledAt_.time_since_epoch().count() == 0) return false;
    return (now - combatEnabledAt_) >= sec(sc.startupDelaySec);
}
```

### UI ([main-window.cpp](src/ui/main-window.cpp) tab Refill)
- 3 `DragInt` per slot: label `"Chờ sau F8 (giây)"`, range `[0, 600]`, default 60

### Config Loader ([config-loader.cpp](src/config/config-loader.cpp))
- Serialize/deserialize `startupDelaySec`
- Default 60 khi key thiếu (backward compat)

## Edge Cases
1. **Combat OFF→ON liên tục**: reset timer mỗi lần. ✓ Intended
2. **Refill checkbox toggle giữa session**: KHÔNG reset, lần đầu vẫn tính từ F8 gốc
3. **`startupDelaySec = 0`**: behavior cũ, fire ngay
4. **Slot đã từng refill (`last != 0`)**: bypass startup gate, dùng `intervalSec`
5. **Hot-reload config khi đang ON**: `enable(true)` được gọi lần 2 → check transition, KHÔNG reset

## Files Affected
- `src/state/game-state.h` — thêm field
- `src/combat/pot-refill-scheduler.h` — thêm `combatEnabledAt_`, modify `enable()` signature/logic
- `src/combat/pot-refill-scheduler.cpp` — modify `isDue()`, `enable()`
- `src/config/config-loader.cpp` — serialize/deserialize + default
- `src/ui/main-window.cpp` — 3 DragInt UI

## Risk Assessment
- **Risk thấp**: thay đổi cô lập, không đụng combat FSM
- **Edge case #5** cần test: hot-reload không được reset timer accidentally

## Success Criteria
1. Bấm F8 với HP startupDelay=60s → HP refill fire sau ~60s, KHÔNG fire ngay
2. F8 OFF rồi F8 ON lại → timer reset, đợi lại 60s
3. Config cũ (thiếu field) load OK, auto-fill default 60s
4. Slot đã refill ít nhất 1 lần → behavior cũ (interval-based)

## Unresolved Questions
None.
