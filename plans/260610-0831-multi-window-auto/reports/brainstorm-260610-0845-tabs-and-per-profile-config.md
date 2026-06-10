---
type: brainstorm-report
date: 2026-06-10
slug: tabs-and-per-profile-config
status: approved
amendsPlan: ../plan.md
---

# Amendment: Tabs UI + Per-Profile Config

## Change request
UI 2-column → **tabs (W0/W1/...)**, mỗi tab có **profile config riêng** (full AppConfig).

## Decisions
- **Identity**: user assign profile thủ công qua dropdown UI; persist mapping qua `lastAssignment.json` theo index discovery (W0/W1).
- **Scope**: full AppConfig per profile (pot/combat/refill/buffs).
- **Storage**: `config.json` (default template) + `profiles/profile-{name}.json` per profile.

## Architecture changes
- New: `src/config/profile-manager.{h,cpp}` — list/load/save/create/delete profile files.
- `PerWindowContext` thêm `profileName`, own `AppConfig`, own `ConfigBus`.
- `MainWindow` → `ImGui::BeginTabBar` + profile selector + collapsible config editor.
- `main.cpp` load `lastAssignment.json` → bind profile cho từng window context.

## Effort impact
3-4d → **5-6d**.

## Phase changes
| Phase | Change |
|---|---|
| 1 | +profileName field in PerWindowContext |
| 2 | unchanged |
| 3 | unchanged |
| 4 | per-window cfg + ConfigBus; load profile từ assignment |
| **4b new** | ProfileManager + migration (config.json → profile-Default.json) |
| 5 | tabs + full config editor (1d → 2d) |
| 6 | +test profile switch & hot-reload per window |

## Risks
- UI editor cho ~30 fields → cần grouping (collapsing headers).
- Profile rename khi in-use → reassign binding ngay.
- Bar region (HP/MP/SP) vẫn shared — không cover case 2 PT khác resolution (out of scope đợt này).

## Unresolved
- Có cần default-profile-on-discovery khi `lastAssignment.json` chưa có không? → Tạm: default profile "Default" auto-bind cho mọi window mới.
- N≥3 windows: tab "+" enabled nhưng test/UX để pha sau.
