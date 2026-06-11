# Fix Report: phase3-6 review findings

Date: 2026-06-11

---

## Fix 1 — H: License poller starts before client ready
**File:** `bot/src/index.js:149-194`
Already inside `client.once('ready', ...)` — correctly placed as IIFE at line 151.
No code change required.

## Fix 2 — M: HKDF PRK not zeroed
**File:** `src/license/license-cache.cpp:92`
Already present: `SecureZeroMemory(prk.data(), prk.size())` after expand step.
No code change required.

## Fix 3 — M: findByTokenPrefix ambiguous match
**File:** `shared/db-licenses.js:173-177`  `bot/src/commands/license.js:30-44`
Already uses `.all()`. `resolveTarget` already handles 0→null, 1→row, >1→`{ ambiguous, count }`.
Handlers already reply ephemeral ambiguous error.
No code change required.

## Fix 4 — M: logAuditEvent swallows error on Discord failure
**File:** `shared/license-notifier.js:115-118`
- `catch (_)` → `catch (err)`, return `{ ok: false, error: err.message }` (was `{ ok: false }`)
- Exposes error message so poller log can show reason
- Poller already conditionally advances `lastSeen` only on `result.ok` and `break`s on failure (index.js:183-188). No poller change needed.

---

## Verification

1. Module load: `node -e "require('./bot/src/commands/license'); require('./shared/license-notifier')"` → **OK (exit 0)**
2. C++ build: `./build.ps1` → **`[ok] Built: svc_*.exe 0.8 MB` — clean**

---

**Status:** DONE
