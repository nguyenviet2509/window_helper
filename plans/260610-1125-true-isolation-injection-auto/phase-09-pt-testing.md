---
phase: 9
title: Testing trên PT thật (ALT ACCOUNT + ALT MACHINE)
status: pending
priority: P0
effort: 3d
---

# Phase 9 — PT testing

## ⚠️ CRITICAL SAFETY PRE-CONDITIONS

**MUST satisfy trước khi bắt đầu phase này:**
1. Alt PT account (account phụ, không value cao) — KHÔNG main.
2. Alt machine (laptop khác / VM khác / spare desktop) — KHÔNG main workstation.
3. Backup HWID profile (optional: HWID spoof tool ready nếu HW ban).
4. Network: alt machine không share VPN/IP với main (XC3 có thể IP track).

**User confirm 4 conditions trước khi proceed. KHÔNG skip.**

## Test scenarios

### Smoke (alt machine + alt account)
1. Build `WindowHelperInjection.exe` + `pt_input_proxy.dll`.
2. Run PT.exe alt account. Wait login screen.
3. Launch WindowHelperInjection (KHÔNG khởi động trước PT).
4. Verify: tab xuất hiện với "INJECTED ✓" trong 3-5s.
5. Login PT. Verify: KHÔNG XC3 kick / popup error.
6. Manual cast 1 skill via auto → verify character casts.

**Accept**: smoke pass = 4/4 step OK.
**Fail**: log inject error / XC3 detect → revisit Phase 5.

### Mouse isolation verification
7. Auto ON 1 window. User di chuột bình thường trên desktop.
8. Run external cursor tracker logging `GetCursorPos` mỗi 10ms 5 phút.
9. Verify: cursor log = chỉ user moves; KHÔNG có jump từ auto.

**Accept**: 0 unexpected cursor moves trong log.

### Multi-window
10. Mở 2 PT alt accounts. Wait both inject. Auto ON cả 2.
11. 15 phút stress: combat + refill cả 2 PT đồng thời.
12. Cursor tracker: vẫn 0 disturbance.

### N=3
13. 3 PT đồng thời. 30 phút stress.
14. Metric: combat miss rate, refill success rate, FPS PT per window.

### Hot-plug
15. Đóng 1 PT giữa chừng → tab remove ≤ 5s. Mở lại PT → tab + inject mới ≤ 5s.

### 4h soak
16. 3 PT + AUTO ON 4 tiếng overnight. Log handle count + memory mỗi 5 phút.
17. Verify: no PT crash, no DLL self-unload, no XC3 detect.

### XC3 detect scenarios (proactive test)
18. Attach debugger to host process during inject → DLL should evade (Phase 5).
19. Attach debugger to PT.exe after inject → DLL should unload graceful.
20. Manual VirtualProtect+memcpy unhook → watchdog should restore.

## Metrics acceptance

| Metric | Target |
|---|---|
| Inject success rate | > 95% / 50 inject |
| System cursor movement | 0 pixel / 30 phút stress |
| FPS PT impact | < 5% degradation |
| Combat miss | < 1% / 1000 actions |
| 4h soak crash | 0 |
| XC3 detect signal | 0 trong 4h |

## Bug response protocol

| Bug type | Action |
|---|---|
| PT crash on inject | Stop testing, revisit Phase 2/3, fix, retry |
| XC3 popup detect | Phase 10 arms-race, identify vector |
| Combat miss > 1% | Tune action timing in Phase 8 |
| Inject success < 95% | Improve injector retry logic |
| DLL self-unload spurious | Tune anti-debug check (false positives) |

## Todo
- [ ] User confirm 4 pre-conditions
- [ ] Setup alt machine + alt account
- [ ] Run scenarios 1-20 sequentially
- [ ] Record metrics in `phase-09-test-report.md`
- [ ] If pass → Phase 10 optional buffer (proactive hardening)
- [ ] If fail critical → bug fix loop

## Success criteria
- 20/20 scenario PASS.
- Metrics all green.
- 4h soak clean.
- User confidence high enough to potentially try main account (USER DECISION, not plan recommendation).

## Risks
- Test on alt → still some chance XC3 ban alt account (acceptable loss).
- HW ban on alt machine — extreme worst case. Mitigation: VM as alt.
- PT update during testing → XC3 update → all bypasses broken → Phase 10.
