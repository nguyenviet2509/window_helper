---
type: brainstorm-report
date: 2026-06-10
slug: cursor-sharing-strategy
status: approved
amendsPlan: ../plan.md
---

# Amendment: Cursor sharing strategy (multi-window)

## Problem
Windows = 1 cursor per Desktop. 2 PT dùng SendInput trên cùng desktop → cursor jump qua lại giữa 2 window → visual jitter.

## Decision: Approach 1+3 combined (teleport + park cursor + disable Bezier)
- Mặc định **tắt mouse path Bezier** khi N≥2 windows. PT không ban vì teleport click (user verify).
- Cursor **teleport** vào target trước click; sau slot release → **park** về `(cursorParkX, cursorParkY)` mặc định (0,0) hoặc cấu hình.
- Cursor chỉ "flicker" giữa target và park point — không bay lượn giữa 2 window.

## AppConfig changes
```cpp
struct CombatConfig {
    // ... existing
    bool enableMousePath = true;  // existing; auto-set false khi N>=2
};

struct CursorParkConfig {
    int x = 0;
    int y = 0;
    bool enabled = true;   // false = không park, cursor giữ tại điểm click cuối
};
struct AppConfig {
    // ... existing
    CursorParkConfig cursorPark;  // mới — shared (không per-window)
};
```

## Behavior changes

### Phase 2 (ForegroundArbiter)
- Sau `releaseSlot(owner)`:
  - Nếu `cursorPark.enabled` → `SetCursorPos(park.x, park.y)`.

### Phase 3 (InputScheduler / SendInputBackend)
- Khi `main.cpp` detect `targets.size() >= 2`:
  - `backend.setMousePathEnabled(false)` ghi đè config combat.enableMousePath.
  - Log warn: "Multi-window: Bezier mouse path disabled".

### Phase 5 (UI)
- Add field "Cursor park (x, y)" trong UI global (footer), không per-tab.
- Hiển thị note "Mouse path auto-disabled in multi-window mode" khi N≥2.

## Effort: +0.25d

## Risks
- `SetCursorPos(0, 0)` có thể trigger Windows mouse handler / hover events trên taskbar / start menu → chọn park x,y vào vùng vô hại (mặc định 1,1 hoặc góc màn hình nơi không có UI).
- Nếu user thực sự dùng máy trong lúc auto OFF → cursor park vẫn áp dụng khi AUTO OFF? → KHÔNG. Chỉ park trong khi AUTO ON ở ít nhất 1 window.

## Unresolved
- N=1 (single window): có nên áp dụng teleport+park không? → Giữ behavior cũ (Bezier ON, không park). Chỉ trigger khi N≥2.
- Future: Virtual Desktop mode (plan 260531-0857) — POC riêng nếu user muốn dùng máy song song.
