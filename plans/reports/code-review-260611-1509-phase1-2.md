# Code Review — Phase 1 + 2 License Activation

Date: 2026-06-11
Reviewer: code-reviewer
Scope: HWID collector + ImGui dialog (C++) + server endpoints + admin UI (Node)

## Verdict: APPROVE_WITH_CHANGES — Score 7.5/10

Solid KISS implementation; sticks to plan, prepared stmts everywhere, fail-fast on missing key, no token plaintext in logs. Two HIGH issues around rate-limit IP spoof + machine_id binding race must fix before merge. Several MEDIUM items can defer.

---

## CRITICAL — none

## HIGH (fix before merge)

### H1. IP spoofing bypasses rate-limit + pollutes audit log
File: `dashboard/server.js` (no `app.set('trust proxy', ...)`) + `dashboard/routes/license-public.js:69-71`
- `clientIp()` blindly reads `req.headers['x-forwarded-for']` first. Combined with default `express-rate-limit` keying on `req.ip` (which also depends on trust proxy), attacker sends a random `X-Forwarded-For` per request and bypasses the 10/min limit. Also corrupts `license_events.ip`.
- Fix: set `app.set('trust proxy', 1)` (or the exact hop count for Railway/whatever proxy), drop the manual XFF parse, use `req.ip` directly in `clientIp()`. If running w/o proxy, set `trust proxy=false` and ignore XFF entirely.

### H2. Race: 2 machines activate same unbound token concurrently → both succeed
File: `dashboard/routes/license-public.js:97-103` + `shared/db-licenses.js:81-93`
- `findByToken` then `bindMachine` is two separate statements. Two concurrent activates both see `machine_id=NULL`, both bind, second `UPDATE` wins → audit log shows 2 `activate` events but only one machine recorded. better-sqlite3 is synchronous per call but Express handlers can interleave between awaits / DB calls.
- Fix: make bindMachine conditional: `UPDATE licenses SET machine_id=?,... WHERE id=? AND machine_id IS NULL` and check `info.changes===1`. If 0 → re-fetch and treat as mismatch path. Cheapest correct fix, no transaction needed.

### H3. `expires_at` validated only as `> 0` — accepts past timestamps + 1970 dates
File: `dashboard/routes/licenses-admin.js:21-25, 74-79`
- Admin can create/patch a license with `expires_at=1` (already expired). Minor admin foot-gun, but combined with `isExpired` returning true → token is dead on arrival with confusing 410 to user.
- Fix: reject `expires_at < now` (or warn UI-side). Low effort.

## MEDIUM

### M1. Duplicated validation + event recording between /activate and /verify
File: `license-public.js:74-155`
- Token lookup, revoked check, expired check, recordEvent boilerplate repeated. ~40 lines duplication.
- Fix (optional, KISS-safe): extract `loadAndCheck(token, eventPrefix)` returning `{lic, errResponse}`. Skip if you prefer literal flow.

### M2. `findByToken` is O(log n) but `idx_licenses_token` is redundant
File: `shared/db-licenses.js:31`
- `token TEXT UNIQUE NOT NULL` already creates an implicit unique index. Explicit `CREATE INDEX idx_licenses_token` is a duplicate.
- Fix: drop `idx_licenses_token`.

### M3. `buildOkResponse` regenerates `issued_at` on every verify
File: `license-public.js:52-58`
- Uses `lic.activated_at || now`. Fine, but means every verify response has a fresh signature for the same logical state. Not a vuln (signature pins canonical), but means client cache key changes each call. Doc the intent or pin `issued_at = activated_at` always (it's never null after bind).
- Fix: `const issued_at = lic.activated_at` (after bind, always set). Remove fallback.

### M4. `machine_id_short` accepted from client but never cross-checked vs `machine_id`
File: `license-public.js:21-36`
- Client could send `machine_id=<real hash>`, `machine_id_short='deadbeef'` — server stores the lie. Cosmetic field used in admin UI.
- Fix: derive server-side: `machine_id_short = machine_id.slice(0,8)`. Drop the field from client payload.

### M5. CORS does not include the BASE_URL pattern needed for dashboard prod
File: `dashboard/server.js:21-23`
- Only `localhost:PORT` + optional BASE_URL. Public license endpoints might be called from a non-browser (C++ WinHTTP) → CORS irrelevant for that path. OK as-is, but note for phase 3.

### M6. ImGui `Render()` re-arms `pendingOpen_` if BeginPopupModal fails
File: `ui/activation-dialog.cpp:93-97`
- If `BeginPopupModal` returns false because `modalOpen` was set false by user closing (X), the early return sets `pendingOpen_=true` again. On next frame, OpenPopup re-opens what user just closed.
- Fix: track failure cause — only re-arm if popup never opened (i.e. ImGui still in "not yet" state). Simpler: drop re-arm; first OpenPopup call always succeeds in ImGui ≥1.80.

### M7. HWID `appendStr` uses null separator but MachineGuid may be empty
File: `src/license/hwid-collector.cpp:51-54, 184`
- If `collectMachineGuid()` returns empty (registry locked down), only a single `\0` byte feeds the hash. Other components still vary, so collision unlikely, but plan said "all 4 components contribute". Consider logging a warning (one-shot) via `core/logger.cpp` if MachineGuid empty so issue is diagnosable in field, without logging the GUID itself.

## LOW

### L1. `clientIp` returns empty string on Unix sockets — DB stores `''` instead of NULL
`license-public.js:70` — change `(... || '')` chain to return `null` if blank. Cosmetic.

### L2. `maskToken` reveals first+last 4 hex chars of token
`db-licenses.js:130-134` — 8 hex chars (32 bits) leaked. Not catastrophic but for license tokens, prefer `****...{last4}` or just `{last8}`. Defer.

### L3. `bindMachine` uses COALESCE on `activated_at` — second activate by same machine never updates this column → fine, but `app_version` is also COALESCE’d, so app upgrade is never reflected in activated_at events.
`db-licenses.js:83-92` — intentional? confirm.

### L4. `licenses-admin.js` PATCH allows `expires_days` semantics only via "set new days from now" — no way to set "no expiry" except via `expires_at: null`.
Minor UX, fine.

### L5. `tools/license-keygen.js` prints private key to stdout
Acceptable for one-shot dev tool, but add a banner `KEEP THIS SECRET — DO NOT COMMIT` so future me doesn't paste it in chat.

### L6. ImGui `codeBuffer_[33]` — token is 32 hex; 33 is correct (+null). Fine.

### L7. C++ `for (WCHAR c : buf)` iterates fixed 64 — fine, has null guard. KISS ok.

### L8. CMake: no link change for new `license/hwid-collector.cpp` other than already present `iphlpapi bcrypt`. Good.

## DRY / KISS Notes

- File sizes all <200 LOC ✓
- No factories/base classes/over-abstraction ✓
- Validation regex centralized in one file ✓
- Match `managed-bots.js` style: kebab routes, plain `dbX` import, no class wrappers ✓

## Praise

- Prepared statements everywhere (no string concat into SQL).
- Private key load fail-fast at module require.
- Token plaintext returned exactly once (POST /admin/licenses), masked everywhere else.
- HWID short used for UI; full only for clipboard + server payload — not in any log path observed.
- Canonical includes `sha256(token)` not token itself → server log of canonical doesn't leak token.
- ImGui clipboard transfers ownership correctly (no GlobalFree after SetClipboardData).
- bcrypt CNG handles cleaned up on every error path.

## Open Questions

1. Is the server behind a reverse proxy in prod (Railway / Caddy)? Determines correct `trust proxy` hop count for H1 fix.
2. Should admin be able to issue a token already bound to a known `machine_id` (createToken accepts it)? Currently any admin can pre-pin to any machine_id — fine, just confirm intent.
3. Phase 3 cache file format defined yet? Affects whether `issued_at` should be stable per-bind (M3).
4. `LICENSE_GRACE_HOURS` env default 48 — same constant must be reflected in client cache TTL phase 3/4.

**Status:** DONE
**Summary:** Review complete; 2 HIGH issues (IP spoof, activate race) should be fixed pre-merge; rest deferrable.
