---
phase: 9
title: Integration test + hot-plug + N=3 soak + pause cycles
status: pending
priority: P1
effort: 0.75d
---

# Phase 9 — Test & soak

## Context
Integration test cho toàn hệ thống mới. Khác plan cũ: thêm hot-plug scenarios + pause/resume cycles + N=3 stress.

## Test scenarios

### Smoke
1. 0 windows → app start, UI empty state.
2. 1 window → tab xuất hiện, AUTO toggle works.
3. 2 windows → 2 tabs, arbiter chia slot.
4. 3 windows → 3 tabs, slot 70ms.
5. 4 windows → window thứ 4 skipped + log warn.

### Hot-plug
6. Start với 2 PT → đóng 1 → tab remove ≤ 5s, còn 1 tab.
7. Start với 1 PT → mở thêm 2 PT giữa chừng → 3 tabs ≤ 3s mỗi tab.
8. Mở/đóng PT 10 lần liên tục → không leak HWND (kiểm tra qua process explorer / handle count).

### Pause/resume
9. AUTO ON 2 windows → user di chuột → toàn bộ pause < 250ms.
10. User idle 3s → resume.
11. Manual pause F9 → toàn bộ pause vô thời hạn → F9 → resume.
12. Refill transaction giữa chừng user active → drag complete trước khi pause.

### Stress
13. N=3 + AUTO ON + 30 phút continuous → 0 miss combat khi user idle.
14. N=3 + 4h soak + pause/resume cycle mỗi 30s → no deadlock, no leak.
15. Profile reassign 50 lần (đóng PT, đổi profile, mở PT lại) → assignment match > 90%.

### Profile/UI
16. Profile dropdown switch → hot-reload config, combat behavior thay đổi.
17. Per-tab config edit → save → reload app → giữ nguyên.

## Tooling
- Manual test (PT là closed-source game; không automated test).
- Logging: tag `[w{i}]` + arbiter slot count + monitor state changes.
- Optional: handle count via `GetProcessHandleCount` log mỗi 5 phút trong soak.

## Acceptance metrics
- Hot-plug: 100% spawn/teardown success trong 20 cycles.
- Pause latency: p95 < 250ms.
- Resume latency: p95 < 250ms (sau khi idle threshold reached).
- N=3 slot fairness: variance < 15% trong 1000 slots.
- 4h soak: 0 crash, handle count stable (±50).
- Profile reassign accuracy: > 90% (title+rect match).

## Todo
- [ ] Manual run scenario 1-17, ghi log + screenshot tab UI.
- [ ] 30 phút N=3 soak (record metrics).
- [ ] 4h soak overnight.
- [ ] Bug report mỗi failure → fix → retest.

## Success criteria
- Tất cả scenario PASS.
- 4h soak clean.
- Performance budget: CPU < 25% với N=3 (vision ~5%/window + capture overhead).

## Risks
- PT crash during test → cần distinguish PT crash vs app crash. Capture stderr/audit log.
- WGC 3 capture session đồng thời: cần verify Windows cho phép (chưa POC). Nếu fail → fallback N=2 hard cap, log feature gate.
