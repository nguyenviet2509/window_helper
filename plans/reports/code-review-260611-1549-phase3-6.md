# Code Review — Phase 3 (C++ client) + Phase 6 (Discord bot)

**Verdict:** APPROVE_WITH_CHANGES
**Score:** 7.5 / 10
**Scope:** ~900 LOC C++ (license-client, license-cache, ed25519-verify, activation-dialog), ~470 LOC Node (license command, notifier, db methods, poller).

Overall solid. Crypto primitives (libsodium init, AES-GCM, HKDF) correctly used. Worker thread lifecycle clean. Main concern: canonical message format mismatch between client and the documented server format, plus a couple of WinHTTP correctness issues.

---

## CRITICAL (must-fix)

### C1. Canonical signing message mismatch — likely breaks verification
**File:** `src/license/license-client.cpp:181-196`
The task brief specifies the canonical format as `token_hash|machine_id|expires_at||0|issued_at` where `expires_at` becomes `""` (empty) for null and the separator after `expires_at` distinguishes null vs zero. Current code emits `token_hash|machine_id|0|issued_at` for permanent licenses (because `std::to_string(0)` produces `"0"`). If the server signs `"...|<machine>||<issued>"` (empty between pipes for null expires), every permanent-license signature will fail verification → SignatureInvalid.

**Fix:** Match server exactly. If `sp.expires_at == 0` emit empty string, else emit the number:
```cpp
std::string expField = (sp.expires_at == 0) ? "" : std::to_string(sp.expires_at);
std::string canonical = sp.token_hash + "|" + sp.machine_id + "|" + expField + "|" + std::to_string(sp.issued_at);
```
**Action:** Confirm exact format from phase-02 server signing code (`server/.../sign.*`) and align. This is the highest-risk bug — phase-02 review or an end-to-end test will catch it but worth fixing first.

### C2. WinHTTP server cert validation — relies on default but no defense-in-depth
**File:** `src/license/license-client.cpp:118-129`
HTTPS is enforced (good) and `WINHTTP_FLAG_SECURE` set. However, no explicit reject of weak cert errors. Default WinHTTP DOES validate certs, but if any future maintainer adds `WINHTTP_OPTION_SECURITY_FLAGS` they could accidentally disable it. Currently safe but brittle.

**Fix:** Add explicit security flag set to **0** (no overrides) to make the security posture explicit:
```cpp
DWORD secFlags = 0;
WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
```
Severity bumped to CRITICAL because cert pinning is the whole trust anchor — but signature verify (Ed25519 with pinned key) provides defense-in-depth so the actual exposure is limited. Treat as HIGH if signature pin is considered sufficient.

---

## HIGH (should-fix)

### H1. WinHTTP errors silently swallowed
**File:** `src/license/license-client.cpp:132-157`
Return value of `WinHttpSendRequest`, `WinHttpReceiveResponse`, `WinHttpQueryHeaders`, `WinHttpReadData` not checked individually. If `sent=FALSE`, code still tries to read status code / body → reads garbage / status=0. The current path returns `NetworkError` for status 0 which is acceptable, but if `WinHttpReceiveResponse` fails the `WinHttpQueryHeaders` call may still return a stale value. Add `if (!sent) goto cleanup;` early-out for clarity.

### H2. WinHTTP request handle / connect handle not closed on `WinHttpOpenRequest` failure
**File:** `src/license/license-client.cpp:121-125`
Already handled — false alarm. (kept for completeness; verified the path closes `hConnect` and `hSession`.) — **No fix needed.**

### H3. `pollWorkerResult` clears `resultReady_` before reading — race with worker
**File:** `src/ui/activation-dialog.cpp:81-89`
`resultReady_.store(false)` is called *before* taking the mutex to copy `workerResult_`. In practice the worker has already set the result and stored true, so no observed bug. However, the load->store->lock sequence has no acquire/release semantics between the flag and the data. Use `resultReady_.load(std::memory_order_acquire)` and rely on the mutex for the data, OR set the flag to true *after* storing the result under lock (current code already does this — `workerResult_=result;` then `resultReady_.store(true)`). With default `seq_cst` ordering this is fine. **Confirmed safe but document the ordering invariant in a comment.**

### H4. Worker thread captures `this` — destruction during in-flight worker
**File:** `src/ui/activation-dialog.cpp:67-78, 120-123`
Dtor joins the thread (good). But if `~ActivationDialog` runs while the worker is mid-`Activate` (10s blocking call), the UI thread blocks up to 10s on join. Acceptable for shutdown but should be documented. **Add comment** noting dtor may block up to network timeout.

### H5. `aesGcmDecrypt` does not wipe key on early-error paths
**File:** `src/license/license-cache.cpp:207-220, 316-339`
`deriveKey` returns a vector containing the AES key; `SecureZeroMemory` is called on `key.data()` in `Save`/`Load` after decrypt completes. But if `aesGcmEncrypt` returns empty (BCrypt failure), the wipe in `Save` (line 286) still executes — OK. In `Load`, the wipe at line 339 always runs — OK. **No fix.** (kept for completeness)

However: `std::vector<uint8_t>` for the key means the underlying buffer is heap-allocated and may have been reallocated during `hkdfSha256` chain. The `SecureZeroMemory` only zeroes the final vector, not intermediate PRK / T1 buffers in HKDF. Plaintext key material may linger in the heap after free. **Fix:** Add `SecureZeroMemory(prk.data(), prk.size())` in `hkdfSha256` before return, and same for `t1` not returned. Low-likelihood exploit but cheap to add.

### H6. License poller — `meta` DB handle reuse + missing started-event guard
**File:** `bot/src/index.js:180-217`
- IIFE runs at module load time, **before** `client.login`. `client.channels.fetch` inside `logAuditEvent` will throw if cache not ready. The `try/catch` in `logAuditEvent` swallows it silently — events will be **lost** (last_event_id still advances). **Fix:** Wrap interval body with `if (!client.isReady()) return;` so events are deferred until the bot is ready.
- `setLastSeen(ev.id)` runs inside the for-loop *after* awaiting the audit post. If the audit post throws (silently swallowed in `logAuditEvent`'s catch), we still advance — so the event isn't retried. Currently acceptable because audit is non-critical, but worth a comment.

---

## MEDIUM

### M1. WinHTTP user-agent leaks app name
`L"WindowHelper/1.0"` is fine for honesty but conflicts with the Xingcode3-evasion theme elsewhere (randomized output name). Consider a neutral UA like `L"Mozilla/5.0"` or read from a CMake-defined constant.

### M2. `license-client.cpp` — `tlsFlags` set after `WinHttpOpenRequest` but BEFORE send
**Line 128-129.** Per MSDN, `WINHTTP_OPTION_SECURE_PROTOCOLS` must be set on the **session** handle, not the request handle. Setting on request handle is a no-op on older Windows builds. **Fix:** Move the `WinHttpSetOption` call to immediately after `WinHttpOpen(hSession,...)` and target `hSession`.

### M3. `aesGcmEncrypt` — `SecureZeroMemory` on IV is wrong
**Line 187.** IV is *prepended to ciphertext*; it's already in `result` and is **not** secret (GCM IVs are public). The wipe is harmless but misleading. Remove it or replace with wiping the symmetric key buffer.

### M4. `base64Decode` accepts any base64 variant
**Line 75-89.** `CRYPT_STRING_BASE64` doesn't accept URL-safe base64 (`-_`). If server emits URL-safe signatures, decode fails → SignatureInvalid. Confirm server emits standard base64 (`+/` with `=` padding). The task brief implies standard b64; document expectation.

### M5. `license.js handleList` — `db.list()` may load all rows; missing limit at SQL layer
**Line 224.** UI caps at 25 displayed but query loads everything. With thousands of licenses this is O(N) memory each call. Add `LIMIT 100` to `db.list()` or accept page param.

### M6. `tokenMask` — 8 of 32 chars leaked (25%)
**license.js:11-14, license-notifier.js, index.js:202.** Acceptable for ops, but consider 3+3 (19% leak) for slightly safer audit logs. **Defer.**

### M7. Token plaintext logged on DM-fail fallback
**license.js:92-95.** When DM fails, full token is shown in ephemeral admin reply. Ephemeral so only admin sees it — acceptable, but document that this reply must NOT be re-posted to a channel. Add a `⚠️ Ephemeral only — do not forward in plaintext` note in the message.

### M8. `resolveTarget` — ambiguous prefix matching
**license.js:30-42.** `findByTokenPrefix` returns first match for 4-char prefix. Two licenses with the same 4-char prefix → admin revokes wrong one. Probability with 16^4 space is low (~1/65536 per pair) but real.

**Fix:** Return all matches; if >1, error out asking admin to use @user or longer prefix.

### M9. `handleIssue` — no check that user doesn't already have an active license
**license.js:62-78.** Admin can accidentally double-issue. Add a check + override flag (e.g., `--force` or just warn).

### M10. License `note` field never sanitized before audit embed
**license.js + notifier.** Markdown/special chars in `note`/`label`/`reason` are passed to embed `value` fields. Discord renders markdown in embeds; an admin issuing a license with `label = "@everyone"` won't cause a ping in an embed field but will if a future change moves it to message content. **Defer**, but worth a comment.

---

## LOW

- **L1.** `license-client.cpp:117` — `std::wstring wpath(path.begin(), path.end())` is technically UB for non-ASCII. Path is ASCII-only here so OK.
- **L2.** `license-client.cpp:283` — `j.value("grace_hours", 48)` — magic number. Define `constexpr int kDefaultGraceHours = 48;` shared with cache.
- **L3.** `license-cache.cpp:73-95` — comment says "for 32 bytes only need T(1)"; assertion would be nice: `static_assert(kKeyBytes <= 32);`.
- **L4.** `activation-dialog.cpp:69` — `short_` underscore suffix is fine; reads as `_short` reversed in some fonts. Minor.
- **L5.** `license.js` 300 LOC — at the limit per dev-rules guideline; subcommand handlers could split into `commands/license/{issue,revoke,reset,info,list}.js`. **Defer** — keeping atomic helps reviewability.
- **L6.** `license-notifier.js` — three near-identical DM helpers; could be one `sendLicenseDM(client, userId, embed)`. Minor DRY win.
- **L7.** `license.js` audit-log build duplicated 3× across handlers — extract `buildAuditEvent(type, row, interaction, extras)`.
- **L8.** `license-client.cpp` — `Activate` / `Verify` only differ by path; current factoring via `PostActivate` is clean. Good.

---

## Praise
- Ed25519 init via `std::call_once` — textbook correct.
- AES-GCM: 12-byte random IV, separate tag, tag verified by BCrypt → tampering rejected. Atomic file write via `.tmp` + `MoveFileExW(REPLACE_EXISTING|WRITE_THROUGH)`. Clean.
- Worker thread: explicit `joinable()`+`join()` in dtor and before re-spawning. No detach. No use-after-free.
- HWID full never logged (verified by grep over all phase-3 files).
- Pinned pubkey at compile time + cross-check `sha256(token) == token_hash` before sig verify — belt + suspenders.
- Audit channel errors swallowed correctly so main flow never breaks.
- `bot_meta.license_last_event_id` persisted → no double-post on restart. Good.
- Prepared statements throughout `db-licenses.js` — no SQL injection.
- Slash command uses `setDefaultMemberPermissions(Administrator)` at registration — enforced at Discord level. Good.

---

## Open Questions
1. **Canonical message format (C1)** — what exactly does phase-02 server emit when `expires_at` is null? Code currently emits `"0"`. Needs end-to-end test or phase-02 inspection.
2. Is base64 signature standard (`+/=`) or URL-safe (`-_`)? Decode assumes standard.
3. Should `handleIssue` block on existing active license for same user, or allow multi-license per user? Plan doc unclear.
4. License cache file path includes `WindowHelper` — does the randomized exe name strategy require cache path randomization too? If anti-cheat scans appdata for known folders, this leaks the app identity.
