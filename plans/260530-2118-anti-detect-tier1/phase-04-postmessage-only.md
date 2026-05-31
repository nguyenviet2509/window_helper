# Phase 04 — PostMessage-only Feasibility

## Context Links
- Brainstorm §#4
- Files: `src/input/postmessage-backend.cpp`, `tools/postmessage-probe/probe.cpp`, `src/config/config-loader.{h,cpp}`

## Overview
- Priority: P1
- Status: pending (depends on Phase 01 sendMouseMove)
- Probe xem game nhận click + key qua PostMessage không. Nếu OK → đặt default backend = PostMessage.

## Key Insights
- SendInput đi qua kernel input stack → driver hook (Xingcode3) catch dễ.
- PostMessage gửi thẳng vào message queue process → bypass kernel hook.
- Nhiều game lọc `wParam`/`lParam` hoặc check foreground → có thể fail.

## Requirements
**Functional**
- Tool `postmessage-probe`: connect game window, gửi click+key, log response (HP bar move? char turn?).
- Config field `defaultBackend: "SendInput" | "PostMessage"` (default "SendInput" pre-test).
- UI selector trong main-window cho phép switch runtime (đã có hoặc thêm).
- Sau khi probe verdict = OK → đổi default → "PostMessage" qua commit riêng.

**Non-functional**
- Probe in `tools/`, không link vào main binary.
- Config backward compat: missing field → "SendInput".

## Architecture
```
probe.cpp:
  HWND game = findGameWindow();
  for each test {click L/R, key W/A/S/D/SHIFT, mouse-move-then-click}:
     postMessage(...)
     sleep, ask user "did it work? (y/n)"
     log result

config:
  defaultBackend default = "SendInput"
  → if probe OK, switch to "PostMessage"
```

## Related Code Files
**Modify**
- `tools/postmessage-probe/probe.cpp` — expand test cases (mouse move + click sequence, shift+rclick, key tap)
- `src/config/config-loader.{h,cpp}` — add `defaultBackend` enum field
- `src/ui/main-window.cpp` — backend selector reads default từ config
- `src/main.cpp` — backend factory dùng config value

## Implementation Steps
1. Expand probe.cpp: add menu options [1=LClick, 2=RClick, 3=Shift+RClick, 4=KeyTap(W/F1/Shift), 5=Move+Click, 6=All-in-sequence].
2. Run probe vs live game; user xác nhận từng test pass/fail; lưu kết quả ra `plans/reports/postmessage-probe-result.md`.
3. Add `defaultBackend` config field + parser.
4. UI: dropdown chọn backend đọc default từ config.
5. Nếu probe verdict ≥ 80% test pass (click + keys cơ bản) → commit đổi default sang "PostMessage". Nếu fail → giữ "SendInput", document lý do trong report.
6. Build + smoke.

## Todo List
- [ ] Expand probe test cases
- [ ] Run probe + collect verdict
- [ ] Write `postmessage-probe-result.md`
- [ ] Add `defaultBackend` config field
- [ ] Backend factory honor config
- [ ] UI selector dùng default
- [ ] (If pass) flip default to PostMessage
- [ ] Build + smoke

## Success Criteria
- Probe result documented với verdict per-test.
- Config field load OK, missing field fallback SendInput.
- Nếu verdict OK: 1h gameplay test với PostMessage backend, không miss combat input.
- Nếu verdict fail: documented; default giữ SendInput; mouse path (phase 01) còn giá trị giảm risk.

## Risk Assessment
| Risk | L | I | Mitigation |
|---|---|---|---|
| Probe ambiguous (1 số test pass, 1 số fail) | H | M | Per-action threshold; key-tap thường OK, mouse có thể fail → mixed backend strategy |
| Game ban PostMessage nguồn vì có flag IsAcceptable… | L | H | Fallback SendInput + mouse path |
| Probe yêu cầu user interaction → chậm | L | L | Acceptable, 1 lần verify |

## Next Steps
- Phase 05 verify chosen backend với combat session.
- Future: mixed backend (key via PostMessage, click via SendInput) nếu probe mixed.
