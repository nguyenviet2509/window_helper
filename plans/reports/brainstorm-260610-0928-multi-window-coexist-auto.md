---
type: brainstorm-report
date: 2026-06-10
slug: multi-window-coexist-auto
status: approved
relatedPlan: ../260610-0831-multi-window-auto/   # tham khảo; plan này độc lập
newPlanSlug: multi-window-coexist-auto
---

# Brainstorm: Auto multi-window co-exist (N=2-3 PT, không phiền user)

## Problem
Cần thêm 1 bản auto song song với bản dedicated-farm hiện đang plan (260610-0831). Yêu cầu:
- Support 2 hoặc 3 cửa sổ PT đồng thời.
- User dùng PC song song (web/chat) — auto KHÔNG được phiền cursor/keyboard.
- UI dạng tabs, mỗi tab = 1 PT window, profile config riêng.
- Detect window động: PT mở mới → auto thêm tab; PT đóng → tab biến mất.

## Constraints
- Windows = 1 system-wide cursor. Không có cursor isolation native.
- SendInput cần foreground (PostMessage không work với PT — đã verify).
- WGC capture chạy background OK.
- PT actions cần mix keyboard (skill) + mouse (refill drag, click target).

## Approaches đã đánh giá

| # | Approach | Verdict |
|---|---|---|
| 1 | Interception kernel driver (kỳ vọng isolate cursor) | **Loại** — Interception chỉ stealth ở HID level, KHÔNG isolate cursor system-wide |
| 2 | VM/Hyper-V chạy PT riêng | Loại — GPU passthrough heavy, perf hit, out of scope |
| 3 | Sandboxie Plus input isolation | Loại — không chắc PT chạy, setup phức tạp |
| 4 | Cursor park + flicker (như plan farm) | Loại cho use case này — user thấy flicker khi đang dùng máy |
| 5 | **Auto-pause on user activity + SendInput** | **Chosen** — pragmatic, đơn giản, hiệu quả thực tế |

## Chosen architecture

```
WindowLifecycleManager (NEW, background thread)
  ├─ poll EnumWindows 2.5s
  ├─ diff vs current contexts → spawn / teardown
  └─ hard cap N=3
        │
        ▼
PerWindowContext[N]  (dynamic; capture+vision+combat+refill+scheduler)
        │
        ▼
PauseGate (NEW) ─── reads UserActivityMonitor
        │ block flush khi user active
        ▼
ForegroundArbiter (slot 70-80ms, round-robin N variable, P0 preempt)
        │
        ▼
SendInputBackend (setTarget per slot)

UserActivityMonitor (NEW): GetLastInputInfo poll 200ms
  - idle ≥ 3s → resume all
  - input <3s → pause all (drain current slot, reject new)
```

## Components mới

| Component | LOC | Effort | Purpose |
|---|---|---|---|
| `WindowLifecycleManager` | ~120 | 0.5d | Continuous discovery; spawn/teardown PerWindowContext lifecycle |
| `UserActivityMonitor` | ~60 | 0.25d | `GetLastInputInfo` poll 200ms; emit pause/resume events |
| `PauseGate` | ~50 | 0.25d | Block scheduler flush; safe-interruptible check trước action mới |
| Arbiter N=3 tweak | ~30 | 0.25d | Slot 70ms, round-robin N variable, skip paused windows |
| UI dynamic tabs | +50 | 0.25d | ImGui tab add/remove; placeholder text khi 0 window |

**Total effort: ~7-9d** (gồm component dùng chung với plan farm; nếu dùng lại được code arbiter/InputScheduler ownerHwnd thì ~5-6d).

## Lifecycle rules

- **Detect new HWND**: init full pipeline + bind profile (default hoặc theo `lastAssignment.json` per index/title) → add tab → AUTO state OFF (user toggle thủ công).
- **HWND invalid 2 polls consecutive**: cancel pending arbiter requests cho HWND đó → stop capture → free context → remove tab. Profile assignment lưu lại theo title+index, reuse khi PT cùng title mở lại.
- **N=4 detected**: log warn "skipped: max 3", không spawn. UI hiện indicator.

## Auto-pause behavior

- `GetLastInputInfo` cho biết last input tick. Trừ tick từ SendInput app gây ra → khó (Windows không tag source). Workaround: app track timestamp lần SendInput cuối → so sánh với `lastInputTime` → nếu khớp ±50ms thì coi như input do app, ignore.
- Pause threshold: default 3s, configurable 1-10s (UI footer).
- Refill threshold riêng: default 5s (longer — refill là multi-step drag, không nên giữa chừng pause).
- **Transaction discipline**: action multi-step (vd refill drag inventory→quickslot) là 1 transaction. Bắt đầu rồi → finish (hoặc abort rollback) — không pause giữa chừng để tránh state corrupted.
- **Capture luôn chạy**: vision pipeline cập nhật bình thường khi pause, chỉ block output. Khi resume → state mới nhất sẵn sàng, không lag detect.

## Trade-offs

1. **N=3 throughput**: 33% time/window. PT action density thấp chịu được; nếu mob dày → miss. Mitigation: P0 emergency preempt (HP low) giữ như plan farm.
2. **Pause threshold UX**: 3s = balance. Expose config. User có thể tăng nếu khó chịu cursor flicker khi vừa nhả tay.
3. **Hot-plug delay**: 2.5s poll + ~50ms pipeline init → user thấy tab xuất hiện sau ~3s. Acceptable.
4. **Cursor vẫn flicker khi user idle**: SendInput inevitable. Né được khi user active (pause). Khi user idle thật → flicker không quan trọng.
5. **Mouse-required action timing**: refill drag chỉ start khi user idle ≥ 5s. Nếu user về bàn giữa drag → finish drag rồi pause (1-2 frame cursor visible).

## Risks

- **GetLastInputInfo lag**: API có lag 0-50ms. Mitigation: poll 200ms đủ responsive.
- **Pipeline tear-down race**: HWND invalid khi arbiter đang process request cho window đó. Mitigation: arbiter check `IsWindow(hwnd)` trước SetForegroundWindow; lifecycle manager wait cho arbiter quiesce (≤1 slot) trước free.
- **Profile reassignment khi PT đóng+mở lại**: HWND mới ≠ HWND cũ. Match theo title prefix + window position (last known rect). Best-effort; nếu fail → fallback default profile + log.
- **N=3 + arbiter slot 70ms**: dưới 70ms Windows foreground switch không kịp confirm. Cần POC đo thực tế; có thể fallback 80-100ms với N=3.

## Out of scope

- Cursor isolation thật (VM, driver-level).
- Multi-seat / per-window cursor.
- N ≥ 4 windows.
- Per-window resolution support (bar region shared, assume cùng resolution).
- Anti-cheat evasion driver-level.

## Success criteria

- 3 PT auto đồng thời, 0 miss combat khi user idle 30 phút.
- User input → toàn bộ auto pause trong < 250ms.
- Idle 3s → auto resume.
- Mở/đóng PT runtime → tab add/remove ≤ 3s, không crash.
- 4h soak chuyển user-active/idle 100 lần → no HWND leak, no deadlock.

## Relationship với plan 260610-0831

- Plan 260610-0831 = dedicated farm mode (no pause, N=2, cursor park).
- Plan mới này = co-exist mode (auto-pause, N=3 dynamic, hot-plug tabs).
- **Trùng component**: `ForegroundArbiter`, `InputScheduler.ownerHwnd`, `PerWindowContext`, `ProfileManager`, tabs UI base, calibration UI.
- **Khác component**: `WindowLifecycleManager` (vs static FindAllTargets), `UserActivityMonitor`, `PauseGate`, dynamic tab UI.
- Khuyến nghị: nếu plan 260610-0831 chưa start → cân nhắc skip nó, làm thẳng plan mới (superset). Nếu giữ cả 2 → 2 binary / 2 mode toggle.

## Next
Tạo plan folder `260610-0928-multi-window-coexist-auto/` với phases chi tiết.

## Unresolved questions
- Slot size cho N=3: 70ms hay 80ms — cần POC đo.
- Pause threshold optimal: 3s default đủ chưa, hay cần adaptive (vd user gõ liên tục → giữ pause lâu hơn)?
- Profile re-match khi HWND đổi: title+position đủ unique không, hay cần thêm process ID / start time?
- Keep plan 260610-0831 song song hay supersede?
