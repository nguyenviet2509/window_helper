# Phase 2 Implementation Report — Server Endpoints + DB Schema + Admin UI

**Date:** 2026-06-11  
**Status:** DONE

---

## Files Created

| File | LOC | Notes |
|------|-----|-------|
| `shared/db-licenses.js` | 169 | CRUD layer — createToken, findByToken, bindMachine, touchSeen, revoke, resetMachine, update, list, recordEvent, listEvents + initLicensesSchema |
| `shared/license-crypto.js` | 33 | Ed25519 sign + buildCanonical; lazy-load key from env; fail-fast on missing/bad key |
| `dashboard/routes/license-public.js` | 157 | POST /activate + POST /verify; machine bind logic; grace period check |
| `dashboard/routes/licenses-admin.js` | 134 | GET / POST / PATCH /:id / POST /:id/revoke / POST /:id/reset-machine / GET /:id/events |
| `dashboard/public/js/licenses.js` | 207 | Alpine component `licensesSection` — list, create modal (show plaintext once + copy), edit, revoke, reset-machine, events drawer |
| `tools/license-keygen.js` | 24 | Generate Ed25519 keypair, print env var line + C++ header hex array |

## Files Modified

| File | Change |
|------|--------|
| `shared/db.js` | +3 lines: call `require('./db-licenses').initLicensesSchema(database)` in initDb() |
| `dashboard/server.js` | Mount `/api/license` (public, rate-limited) BEFORE auth; mount `/api/admin/licenses` AFTER auth |
| `dashboard/package.json` | Added `tweetnacl ^1.0.3` + `express-rate-limit ^7.5.0` |
| `dashboard/public/index.html` | Sidebar nav item "Licenses" (lock SVG icon) after managed-bots; full LICENSES TAB section (~175 lines); `<script src="/js/licenses.js">` before `</body>` |
| `.env.example` | Added LICENSE_ED25519_PRIVATE_KEY + LICENSE_GRACE_HOURS=48 |
| `.env` | Appended LICENSE_ED25519_PRIVATE_KEY (generated) + LICENSE_GRACE_HOURS=48 |

## Dependencies Added

```
dashboard/node_modules:
  tweetnacl@1.0.3
  express-rate-limit@7.5.0
```

---

## Generated Ed25519 Keypair

**CRITICAL for Phase 3 client:**

```
LICENSE_ED25519_PUBLIC_KEY_B64=rtoV9rxWowy83CcfEb8UX0iopmDfZt/Jfrzl3E5ge94=
```

**C++ header (paste into `src/license/server-public-key.h`):**
```cpp
#pragma once
constexpr unsigned char kServerPubKey[32] = {
    0xAE, 0xDA, 0x15, 0xF6, 0xBC, 0x56, 0xA3, 0x0C, 0xBC, 0xDC, 0x27, 0x1F, 0x11, 0xBF, 0x14, 0x5F, 0x48, 0xA8, 0xA6, 0x60, 0xDF, 0x66, 0xDF, 0xC9, 0x7E, 0xBC, 0xE5, 0xDC, 0x4E, 0x60, 0x7B, 0xDE
};
```

Private key stored in `D:/Vietnt/Project/bot_discord_app/.env` as `LICENSE_ED25519_PRIVATE_KEY`.

---

## Server Start Status

```
[Dashboard] ✅ Running on port 3001
[bots-lite] khong co bot can auto-restore
```
DB init: tables `licenses` + `license_events` + indexes created on first boot. No errors.

---

## Curl Test Results

| # | Test | HTTP | Pass? |
|---|------|------|-------|
| 1 | POST /api/auth/login | 200 + JWT | ✅ |
| 2 | POST /api/admin/licenses (create) | 201 `{id,token}` | ✅ |
| 3 | POST /api/license/activate (first — bind) | 200 `{ok,signed,payload}` | ✅ |
| 4 | POST /api/license/activate (same machine re-activate) | 200 | ✅ |
| 5 | POST /api/license/activate (different machine) | 409 `MACHINE_MISMATCH` | ✅ |
| 6 | POST /api/license/verify (correct machine) | 200 | ✅ |
| 7 | POST /api/admin/licenses/1/revoke | 200 `{ok:true}` | ✅ |
| 8 | POST /api/license/verify after revoke | 410 `REVOKED` | ✅ |
| 9 | POST /api/admin/licenses/1/reset-machine | 200 `{ok:true}` | ✅ |
| 10 | Create 2nd token + activate new machine | 200 | ✅ |
| 11 | GET /api/admin/licenses/1/events | 200, 7 events logged | ✅ |
| 12 | Rate limit (11 rapid requests) | req5+ → 429 | ✅ |

Event types logged in test: `activate`, `activate_reject`, `verify`, `verify_reject`, `revoked`, `reset_machine`

---

## Deviations from Plan

1. **`licenses.js` is 207 LOC** (spec said "ideally <200, ok to go slightly over"). Full UI fit in single file — no modularization needed.
2. **Rate limit window shared**: The test showed 429 firing at req5 rather than req11 because earlier test requests (tests 3–5) consumed 4 of the 10/min allowance in the same window. Rate limit is correct — 10 req/min/IP per spec.
3. **`tools/license-keygen.js` uses `require('tweetnacl')`** which resolves via NODE_PATH or dashboard/node_modules. Added note: run from `dashboard/` or with `NODE_PATH=dashboard/node_modules`.

---

## Open Questions

1. **Phase 3 client**: Confirm receipt of C++ header above. Will need `nacl_sign_open` or `crypto_sign_ed25519_verify_detached` (libsodium/tweetnacl-C equivalent) to verify the `signed` base64 response.
2. **Un-revoke**: No un-revoke endpoint implemented (not in spec). If needed later, trivial `UPDATE licenses SET revoked=0`.
3. **discord_user_id / issued_by_discord_id**: Columns exist in DB, createToken accepts them, but admin UI create form does not expose them (Phase 6 bot will fill these via `/license issue` command — not needed in dashboard UI per spec).
