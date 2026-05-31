# Phase 05 — Soak Test trên Throwaway Account

## Context
- Plan: [plan.md](plan.md)
- Phụ thuộc: Phase 01-04 đã merged.

## Overview
- Priority: P0 (gate trước khi dùng main acc)
- Status: pending
- Mục tiêu: chạy Virtual Desktop Mode liên tục 7 ngày trên throwaway account, monitor XGC reaction.

## Key Insights
- XGC behavior với non-default desktop là **biến số chưa rõ public**. Phải tự đo.
- Soak test phải mimic usage pattern thật: farm liên tục, có break, có relog.
- Throwaway acc = chấp nhận ban hoàn toàn.

## Requirements
- F1: Build release helper với Virtual Desktop Mode enabled.
- F2: Throwaway acc PT account riêng, không liên kết main.
- F3: Test plan covers ≥7 ngày, ≥40h farm net.
- F4: Daily check: disconnect, ban, warning, abnormal server response.

## Test Protocol

### Setup
- Helper config: `virtualDesktop.enabled=true`, anti-detect Tier 1 đã on.
- Map farm: chọn 1-2 map mob phù hợp (config sẵn).
- Logging: helper log + manual log (Google Sheet / markdown).

### Daily routine (7 days)
- Sáng: start helper, attach throwaway acc, farm 4-6h.
- Tối: kiểm tra status, ghi log:
  - Helper crash? Frequency?
  - Game disconnect? Bao nhiêu lần, thời gian.
  - Account locked? Warning popup?
  - Server behavior abnormal (delayed response, chat warning, GM contact)?
  - XP/loot rate có hợp lý không (hint behavioral detection).
- Logout đúng cách, không kill process.

### Stress scenarios
- Day 3: peek hotkey 50 lần/h (test SwitchDesktop intensity).
- Day 5: helper crash giữa session (kill process) → verify BotDesk cleanup.
- Day 6: relog 5 lần trong 1 session (test discover existing PT instance).

### Decision matrix sau 7 ngày
- 0 issues → **GO** cho main acc, document final.
- ≤2 disconnect không rõ nguyên nhân → **WATCH**, soak thêm 3 ngày.
- ≥1 ban/warning/account lock → **NO-GO**, kill plan, fallback Coexist Mode.

## Related Files
- Create: `docs/virtual-desktop-soak-results.md` — daily log + final verdict.
- Update: `plans/260531-0857-virtual-desktop-mode/plan.md` — mark status sau soak.

## Todo
- [ ] Build release helper từ master branch sau Phase 04 merge
- [ ] Setup throwaway acc + isolated PT install (để tránh ảnh hưởng main client install)
- [ ] Day 1-7 daily log
- [ ] Stress scenarios Day 3, 5, 6
- [ ] Final verdict + decision
- [ ] Document `docs/virtual-desktop-soak-results.md`

## Success Criteria
- 7 ngày, ≥40h farm net, 0 ban/warning.
- Helper stable: <1 crash/day, recovery hoạt động.
- Vision accuracy ≥95% (HP/MP đọc đúng).

## Risk Assessment
- Risk: XGC detect → throwaway bị ban, đây là kết quả expect-and-accept.
- Risk: PT account creation policy chặt → mitigation: chuẩn bị acc từ trước.
- Risk: Soak gián đoạn (mất điện, restart máy) → log gap, không invalidate test.

## Security Considerations
- Throwaway acc credentials KHÔNG lưu chung config với main.
- Không dùng IP/MAC giống main (nếu khả thi: VPN cho throwaway).

## Next Steps
- GO → update docs, communicate "Production ready" cho main acc usage.
- NO-GO → archive plan, brainstorm fallback (Coexist Mode hoặc máy 2 vật lý).
