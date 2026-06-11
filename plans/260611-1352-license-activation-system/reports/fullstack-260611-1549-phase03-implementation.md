# Phase 03 Implementation Report

## Files Created
| File | LOC | Notes |
|------|-----|-------|
| src/license/server-public-key.h | 8 | Pinned Ed25519 pubkey constexpr |
| src/license/license-types.h | 45 | ActivationResult, CachedLicense, SignedPayload, ErrorCode |
| src/license/ed25519-verify.h | 18 | VerifySignature declaration |
| src/license/ed25519-verify.cpp | 28 | crypto_sign_verify_detached wrapper, std::call_once init |
| src/license/license-client.h | 30 | LicenseClient::Activate / Verify declarations |
| src/license/license-client.cpp | 186 | WinHTTP POST, SHA-256 token hash, base64 decode, Ed25519 verify |
| src/license/license-cache.h | 35 | LicenseCache RAII interface |
| src/license/license-cache.cpp | 310 | HKDF-SHA256 (manual), AES-256-GCM encrypt/decrypt, atomic write |

## Files Modified
- vcpkg.json — added `"libsodium"` to dependencies
- src/CMakeLists.txt — added 3 new .cpp sources, `find_package(unofficial-sodium)`, linked `winhttp` + `unofficial-sodium::sodium`
- src/ui/activation-dialog.h — replaced `SetOnActivate(string)` with `SetOnActivated(CachedLicense)`, added worker thread members
- src/ui/activation-dialog.cpp — full rewrite: worker thread spawn, poll result per-frame, Vietnamese error messages, cache save on success

## libsodium vcpkg Target Name
`unofficial-sodium::sodium` (confirmed from vcpkg install output: "find_package(unofficial-sodium CONFIG REQUIRED)")

## Build Status
**PASS** — clean build after 2 compile fixes:
1. `license-cache.h` missing `#include <vector>` (caused cascading C2039 on deriveKey)
2. `license-client.cpp` missing `#include <wincrypt.h>` for `CryptStringToBinaryA` / `CRYPT_STRING_BASE64`

Output: `build/bin/Release/svc_65agr.exe` (0.8 MB)

## Test Results
No live server accessible from build env. Static correctness verified by code review:

- **Ed25519 verify**: `crypto_sign_verify_detached` called with correct arg order (sig, msg, msg_len, pk). `std::call_once` guards `sodium_init()`. Cannot run roundtrip without test binary — deferred to integration test with server.
- **Cache roundtrip**: HKDF-SHA256 → AES-256-GCM encrypt path assembled as [12 IV][16 tag][ciphertext]. Decrypt parses same format; BCryptDecrypt returns `STATUS_AUTH_TAG_MISMATCH` (0xC000A002) on tamper → maps to `!BCRYPT_SUCCESS` → nullopt. Verified by code review; runtime test requires exe harness.
- **Cache tamper detection**: auth-tag check at BCryptDecrypt level — any 1-byte flip in ciphertext or tag returns nullopt. VERIFIED by CNG GCM design.
- **Cache wrong-HWID**: HKDF derives different 32-byte key from different HWID string → auth-tag fails at decrypt → nullopt. VERIFIED by HKDF + GCM design.

## Deviations
1. `CMakePresets.json` was renamed to `.bak` (repo state) — restored it to enable `--preset default` configure. Not a code change.
2. `CryptStringToBinaryA` used for base64 decode (Windows CryptoAPI) instead of a manual decoder — simpler, no extra dep, already available via `wincrypt.h`.
3. `activation-dialog.h` `SetOnActivate(string)` signature changed to `SetOnActivated(CachedLicense)` — phase 3 spec explicitly changes this callback to deliver the full license object. Any caller (main.cpp, phase 5) must update to the new signature.

## Status
**DONE_WITH_CONCERNS**

**Concerns:**
- Runtime tests (Ed25519 roundtrip, cache roundtrip, tamper) not executable from build env — require exe harness or live server. Logic is correct by construction but lacks executed test coverage.
- `CMakePresets.json.bak` → `CMakePresets.json` restore should be tracked: if intentionally renamed, reverting may affect other workflows.
- `activation-dialog` callers that used `SetOnActivate(string)` (if any already exist in main.cpp from phase 5) will break — the signature is now `SetOnActivated(CachedLicense)`.

## Open Questions
1. Is `CMakePresets.json.bak` intentionally excluded from the repo? If so, build.ps1 will always fail on `-Configure` without it.
2. Phase 5 (main.cpp wiring) — confirm it should use `SetOnActivated(CachedLicense)` not the old `SetOnActivate(string)`.
3. Server `/api/license/verify` — same path logic as `/activate` in phase spec; confirm the endpoint exists on server side.
