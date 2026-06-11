---
phase: 3
title: WinHTTP Client + Ed25519 Verify + AES-GCM Cache
status: pending
priority: high
effort: 1d
depends_on: [phase-01, phase-02]
---

# Phase 3 — Client Network + Crypto

## Context Links
- Brainstorm: [../reports/brainstorm-260611-1352-license-activation-system.md](../reports/brainstorm-260611-1352-license-activation-system.md)
- Public key from phase 2: `dashboard/public-key.hex`

## Overview
Wire activation dialog → WinHTTP POST → verify Ed25519 → save AES-GCM encrypted cache. End-to-end activate flow working.

## Key Insights
- WinHTTP synchronous OK cho activate (modal blocking anyway), nhưng dùng worker thread để UI không freeze
- libsodium qua vcpkg (đã có vcpkg.json) — ~30KB
- AES key derive từ HWID → ai copy cache file sang máy khác giải mã fail
- Pin public key as `constexpr unsigned char kServerPubKey[32]` trong header

## Requirements
**Functional:**
- `LicenseClient::Activate(token, hwid) → ActivationResult { ok, expires_at, error }`
- `LicenseClient::Verify(token, hwid) → similar`
- `LicenseCache::Save(payload)`, `LicenseCache::Load() → optional<CachedLicense>`
- Verify Ed25519 sig trước khi trust bất kỳ payload nào

**Non-functional:**
- Network timeout 10s
- Cache file location: `%APPDATA%\WindowHelper\license.dat`

## Files to Create
- `src/license/license-client.h` / `.cpp` — WinHTTP wrapper, JSON serialize, sig verify (~180 LOC)
- `src/license/license-cache.h` / `.cpp` — AES-GCM via bcrypt CNG (~150 LOC)
- `src/license/ed25519-verify.h` / `.cpp` — libsodium wrapper (~50 LOC)
- `src/license/server-public-key.h` — generated pinned key
- `src/license/license-types.h` — shared structs

## Files to Modify
- `vcpkg.json` — add `libsodium`
- `src/CMakeLists.txt` — link `unofficial::sodium::sodium`, `winhttp.lib`, `bcrypt.lib`
- `src/ui/activation-dialog.cpp` — wire callback to call LicenseClient on worker thread

## Implementation Steps

### 3.1 vcpkg + CMake
1. Add `"libsodium"` to `vcpkg.json` dependencies
2. CMakeLists: `find_package(unofficial-sodium CONFIG REQUIRED)` + link
3. Re-run cmake configure, verify build

### 3.2 Ed25519 verify
```cpp
// ed25519-verify.cpp
bool VerifySignature(span<const uint8_t> msg, span<const uint8_t,64> sig, span<const uint8_t,32> pubkey) {
    return crypto_sign_verify_detached(sig.data(), msg.data(), msg.size(), pubkey.data()) == 0;
}
```

### 3.3 license-client.cpp
- WinHTTP: `WinHttpOpen` → `WinHttpConnect` (host from hardcoded URL) → `WinHttpOpenRequest` (POST, HTTPS) → `WinHttpSetOption` (TLS 1.2+) → `WinHttpSendRequest` + body → read response
- JSON: dùng nlohmann/json (likely đã có trong project? check) hoặc minimal hand-roll
- Build canonical payload string: `token_hash|machine_id|expires_at|issued_at` — verify sig over that exact bytes
- Reject nếu sig fail, status != 200, or schema invalid

### 3.4 AES-GCM cache
- Key derive: `HKDF-SHA256(hwid_bytes, salt = "WindowHelper.v1", info = "license-cache")` → 32-byte key
- Encrypt:
  - Random 12-byte IV
  - BCryptEncrypt với `BCRYPT_CHAIN_MODE_GCM`
  - Output file format: `[12 IV][16 TAG][ciphertext]`
- Decrypt: parse format, BCryptDecrypt, fail → return nullopt
- Stored payload (JSON before encrypt): `{ token, machine_id, expires_at, last_verified, server_pub_payload_b64 }`

### 3.5 Wire activation dialog
- ActivationDialog::OnActivateClick: disable buttons, spawn `std::thread`
- Worker: call `LicenseClient::Activate`, post result back to UI thread via atomic + flag
- UI: pump result next frame → success → close modal + run continuation callback; fail → SetStatus(error)

## Todo
- [ ] vcpkg libsodium installed, CMake link OK
- [ ] ed25519-verify unit test (sign on server, verify on client)
- [ ] server-public-key.h generated from phase 2 output
- [ ] license-client Activate happy path → 200 + sig verify pass
- [ ] license-client Activate sad path → 409 mapped to error string
- [ ] license-cache encrypt → decrypt roundtrip
- [ ] license-cache copy file sang máy khác (simulate đổi HWID) → decrypt fail
- [ ] Wire activation-dialog → worker thread → success/fail UX
- [ ] Build clean, manual e2e test

## Success Criteria
- Activate qua dialog → server bind machine → cache file written
- Restart app → cache loaded, dialog không hiện (skip to main UI — handled in phase 4/5)
- Tampered server response (manual change 1 byte) → client reject
- Cache file copy sang máy khác → giải mã fail, force re-activate

## Risks
- WinHTTP cert pinning phức tạp → skip, rely on Ed25519 sig + HTTPS trust store (đủ)
- libsodium build trên Windows qua vcpkg ổn, nhưng tăng binary size ~200KB → acceptable
- HKDF không có sẵn trong CNG cũ → dùng `BCryptKeyDerivation` với BCRYPT_KDF_HKDF (Win10+) hoặc tự implement HMAC-based HKDF (~30 LOC)

## Security
- Server URL hardcode `https://...` (HTTPS only, refuse HTTP)
- Pubkey embedded const, không đọc từ file
- IV random per encrypt (CNG `BCryptGenRandom`)
- Wipe sensitive buffers sau dùng (`SecureZeroMemory`)

## Next Steps
→ Phase 4 grace + periodic verify
