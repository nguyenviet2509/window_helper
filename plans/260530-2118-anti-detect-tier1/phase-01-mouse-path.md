# Phase 01 — Mouse Path Bezier

## Context Links
- Brainstorm §#1: `plans/reports/brainstorm-260530-2118-anti-detect-tier1.md`
- Files: `src/input/i-input-backend.h`, `src/input/send-input-backend.{h,cpp}`, `src/input/postmessage-backend.{h,cpp}`

## Overview
- Priority: P1
- Status: pending
- Sinh path Bezier 3-điểm từ cursor hiện tại → target trước mỗi click, gửi waypoint theo step ~16-30ms.

## Key Insights
- Tổng path 80-250ms; nếu distance <30px → skip path (snap).
- Skip path khi click POT (HP/MP/SP) — gọi click trực tiếp, không qua wrapper path.
- Control point: lệch perpendicular ±(distance*0.15..0.35), randomize sign.

## Requirements
**Functional**
- `MousePath::generate(from, to, rng) -> vector<Waypoint{x,y,delayMs}>`.
- Mỗi backend implement `sendMouseMove(x,y)`; wrapper `moveTo(target)` chạy path rồi click.
- Config flag `enableMousePath` (bool, default true).

**Non-functional**
- File `mouse-path.cpp` <150 LOC.
- Không alloc trong hot loop (reserve vector).
- Total click latency ≤ 300ms.

## Architecture
```
combat-fsm → backend.sendShiftRightClick(x,y)
              ├─ if enableMousePath && dist>30: MousePath::generate → loop sendMouseMove+sleep
              └─ sendButton(down/up)
```

`sendMouseMove`:
- SendInput: `INPUT.mi MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE` (screen coord, normalized 0..65535).
- PostMessage: `WM_MOUSEMOVE` lParam=MAKELPARAM(clientX,clientY).

## Related Code Files
**Create**
- `src/input/mouse-path.h`
- `src/input/mouse-path.cpp`

**Modify**
- `src/input/i-input-backend.h` — add `virtual void sendMouseMove(int x,int y)=0;`
- `src/input/send-input-backend.{h,cpp}` — impl sendMouseMove; integrate path vào sendRightClick/sendShiftRightClick/sendLeftClick
- `src/input/postmessage-backend.{h,cpp}` — tương tự
- `src/config/config-loader.{h,cpp}` — `enableMousePath` field
- `CMakeLists.txt` — add `mouse-path.cpp` sources
- `src/combat/pot-evaluator.cpp` — đảm bảo path click pot dùng API skip-path (hoặc thêm overload `sendKeyTap` không đụng — chỉ click; nếu pot là KEY không phải click thì OK)

## Implementation Steps
1. Tạo `mouse-path.h` định nghĩa struct Waypoint + class MousePath với `generate(POINT from, POINT to, std::mt19937& rng, std::vector<Waypoint>& out)`.
2. Implement Bezier cubic (P0=from, P1=ctrl, P2=ctrl2, P3=to). Sampling step 16-30ms, dynamic count = clamp(distance/8, 5, 16).
3. Add `sendMouseMove` vào interface + 2 backend.
4. Thêm helper `executePath` (private) trong mỗi backend; gọi từ sendRightClick/sendShiftRightClick/sendLeftClick.
5. Thêm `enableMousePath` config + getter; backend đọc từ config singleton hoặc nhận qua setter.
6. Build (MSVC) — fix compile errors.
7. Manual smoke: chạy tool đứng ngoài game, xem log cursor move.

## Todo List
- [ ] Create `mouse-path.h`
- [ ] Create `mouse-path.cpp` (Bezier + sampling)
- [ ] Update `i-input-backend.h` interface
- [ ] Implement `sendMouseMove` trong SendInput backend
- [ ] Implement `sendMouseMove` trong PostMessage backend
- [ ] Integrate path vào 3 click methods (both backends)
- [ ] Add `enableMousePath` config
- [ ] Confirm pot click không bị delay (verify pot-evaluator dùng key, không click)
- [ ] CMake add sources, build OK
- [ ] Smoke test cursor move

## Success Criteria
- Build pass MSVC, no new warnings.
- Khi click 1 target xa 200px, cursor di chuyển có trajectory cong, total <250ms.
- Pot trigger (HP<threshold) → key tap tức thì (<5ms), không bị path chen.
- Toggle `enableMousePath=false` → behavior cũ.

## Risk Assessment
| Risk | L | I | Mitigation |
|---|---|---|---|
| Path quá chậm → miss combat tick | M | H | Cap total 300ms; distance<30 skip |
| SendInput absolute coord sai vì multi-monitor | M | M | Dùng `GetSystemMetrics(SM_CX/CYVIRTUALSCREEN)` |
| PostMessage WM_MOUSEMOVE bị game ignore | M | L | Verify ở phase 04 |
| Vector alloc per click → GC pressure | L | L | Thread-local reusable buffer |

## Next Steps
- Phase 04 dùng `sendMouseMove` PostMessage để probe.
- Phase 05 verify cursor visible smooth.
