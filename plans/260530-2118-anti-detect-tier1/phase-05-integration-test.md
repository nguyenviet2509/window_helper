# Phase 05 — Integration Test E2E

## Context Links
- All previous phases.
- Brainstorm "Success criteria" section.

## Overview
- Priority: P1
- Status: pending (depends 01,02,03,04)
- Verify từng success criteria tier 1 end-to-end với live game.

## Key Insights
- Test sau khi merge 4 phase, không edit code (chỉ regression fix).
- Cần log click coordinates để analyze cluster offline.

## Requirements
**Functional**
- 1h combat session với config default.
- Capture: click coord log, screen recording (cursor trajectory), task manager screenshot, Spy++ window title.
- Optional: histogram script (Python/PowerShell) cho click coord.

**Non-functional**
- Test plan reproducible: documented steps trong `plans/reports/integration-test-260530.md`.

## Architecture (test matrix)

| Tiêu chí | Verify cách |
|---|---|
| Cursor smooth (no teleport) | Screen record 30s, eyeball + frame-by-frame |
| Click cluster Gaussian | Log 1h, dump coord, plot scatter; expect dense center, sparse edge |
| Title không "WindowHelper" | Spy++, GetWindowText programmatically |
| Process name random | Task Manager + dir listing build/ |
| PostMessage backend works (nếu verdict OK) | 1h gameplay, no missed input |
| Pot click latency unchanged | Manual: HP drop → measure response qua replay |

## Related Code Files
- Chỉ tạo report, không edit code (trừ regression fix nhỏ).

## Implementation Steps
1. Build clean.
2. Launch game + tool; record screen 30s combat → verify cursor trajectory.
3. Enable click-coord logging (temp dev flag nếu chưa có; OK to add 1 file log line).
4. Run 1h combat; export log.
5. Plot scatter (Python matplotlib hoặc PowerShell + Excel CSV).
6. Capture: title via Spy++; exe name via dir.
7. Verify pot priority: trigger low HP scenario, measure key tap latency.
8. Write `integration-test-260530.md` với per-criterion verdict + screenshots.

## Todo List
- [ ] Clean build all phases merged
- [ ] Screen record cursor trajectory
- [ ] Enable click coord log (add 1 line to combat-fsm.cpp if needed)
- [ ] Run 1h session
- [ ] Plot click distribution
- [ ] Verify title + exe name
- [ ] Verify pot latency unchanged
- [ ] Write integration report
- [ ] If any criterion FAIL → file follow-up tasks

## Success Criteria
- All 6 verify rows = PASS hoặc documented LIMITATION với mitigation.
- Report committed at `plans/reports/integration-test-260530.md`.

## Risk Assessment
| Risk | L | I | Mitigation |
|---|---|---|---|
| Cursor smooth visual subjective | M | L | Frame-by-frame check + path log dump |
| 1h test bị ngắt do game crash | M | M | Restart, accept partial data, document |
| Click log spam log file | M | L | Sample 1/10 hoặc rotate log |

## Next Steps
- Tier 2 planning sau khi tier 1 verified.
- Nếu integration fail → root cause + plan fix.
