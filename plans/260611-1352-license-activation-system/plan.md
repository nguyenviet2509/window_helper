---
title: License / Activation System
slug: license-activation-system
created: 2026-06-11
status: completed
priority: high
brainstorm: ../reports/brainstorm-260611-1352-license-activation-system.md
blockedBy: []
blocks: []
---

# License / Activation System

Gate WindowHelper.exe behind per-machine token; manage from existing Node/Express dashboard.

## Goal
- Bind 1 token ↔ 1 machine (HWID)
- Admin CRUD tokens from dashboard (Alpine.js tab)
- Modal activation gate in ImGui giống mockup
- Grace offline 48h, periodic re-verify
- Ed25519-signed responses + AES-GCM cache

## Phases

| # | Phase | Status | File |
|---|---|---|---|
| 1 | HWID collector + ImGui activation dialog skeleton | completed | [phase-01-hwid-and-dialog.md](phase-01-hwid-and-dialog.md) |
| 2 | Server endpoints + DB schema + admin UI tab | completed | [phase-02-server-and-admin.md](phase-02-server-and-admin.md) |
| 3 | WinHTTP client + Ed25519 verify + AES cache | completed | [phase-03-client-network-crypto.md](phase-03-client-network-crypto.md) |
| 4 | Grace period + periodic verify + revoke handling | completed | [phase-04-grace-and-revoke.md](phase-04-grace-and-revoke.md) |
| 5 | Integrate main.cpp gate + tray status + polish | completed | [phase-05-integration-polish.md](phase-05-integration-polish.md) |
| 6 | Discord bot integration (/license slash commands + DM + audit log) | completed | [phase-06-discord-bot-integration.md](phase-06-discord-bot-integration.md) |

## Key Dependencies
- vcpkg: `libsodium` (Ed25519) — or embed Monocypher header-only
- Windows: WinHTTP, bcrypt (CNG AES-GCM) — built-in
- Server: **reuse existing `bot_discord_app` (Node 18 + Express 4 + better-sqlite3 + JWT)** ; add `tweetnacl` + `express-rate-limit`

## Server Project Context (CONFIRMED via scout)
- **Repo location (ABSOLUTE):** `D:/Vietnt/Project/bot_discord_app/` — edit trực tiếp ở đây, KHÔNG ở window_helper
- Node workspaces `bot/` + `dashboard/`, shared `shared/`
- DB: SQLite via `shared/db.js` `initDb()` — add license tables inline (project không dùng migration file)
- Auth: single admin JWT (`DASHBOARD_SECRET` env, 7d)
- Routes pattern: `dashboard/routes/{feature}.js` + `shared/db-{feature}.js`
- Frontend: `dashboard/public/index.html` SPA Alpine.js, mỗi tab 1 `x-data` + JS file riêng trong `public/js/`
- **Quan trọng:** mọi `/api/*` route hiện đang sau `auth` middleware → public license endpoints phải mount RIÊNG trước auth chain

## Open Questions
1. ~~DB stack~~ → **RESOLVED: SQLite via shared/db.js**
2. Server URL production hardcode vào C++? → **RESOLVED: https://vietnt.io.vn hardcoded**
3. ~~Token format~~ → **RESOLVED: 32-hex used**
4. ~~Hard exit vs demo-mode~~ → **RESOLVED: hard exit**
5. ~~Crypto lib~~ → **RESOLVED: libsodium (libsodium via tweetnacl client-side phase 3)**
6. ~~1 Ed25519 key~~ → **RESOLVED: 1 key in .env**

## Success Criteria
- Run mà không có token → modal block, không vào main UI
- Activate máy A → activate máy B same token → 409 reject + event log
- Admin revoke → user kế tiếp restart bị block ≤6h online / ≤48h offline
- Cache file copy chéo máy → giải mã fail → force re-activate
- Mất mạng 24h sau activate → vẫn chạy bình thường
- Tampered server response → client reject (Ed25519 sig fail)

## Final Status

**Completion Date:** 2026-06-11

**All 6 phases completed successfully:**

1. ✓ Phase 1: HWID + ImGui dialog skeleton
2. ✓ Phase 2: Server endpoints + DB schema + admin UI
3. ✓ Phase 3: WinHTTP client + Ed25519 + AES cache
4. ✓ Phase 4: Grace 48h + periodic verify 6h + revoke detection
5. ✓ Phase 5: main.cpp gate + tray "License Info" + UX polish
6. ✓ Phase 6: Discord bot integration (/license + audit log)

**Implementation Summary:**
- **LicenseManager** orchestrator: Bootstrap branching (ENTER_MAIN | SHOW_DIALOG | EXIT), atomic grace state, Snapshot() for race-safe reads
- **Periodic verify** thread: 6h interval, network-safe (no retry kill), async detection of revoke/expiry
- **Main integration:** License gate inserted after window.init() + ImGui init, before capture/vision spawn
- **Client UX:** Activation dialog with Copy Machine ID, Vietnamese error messages, tray "License Info" menu
- **Graceful revoke:** 30s countdown toast + clean shutdown
- **Admin dashboard:** Alpine.js tab for token CRUD, event log with user HWID + timestamp + action

**Code Repositories:**
- `D:\Vietnt\Game\window_helper` — 4 commits (hwid, activation-dialog, license-manager, integration)
- `D:\Vietnt\Project\bot_discord_app` — distributed license routes, DB tables, dashboard admin UI

**Testing:** All phases validated by tester-260611-1630, code review passed (race conditions verified), ready for end-to-end deployment with real users/tokens.
