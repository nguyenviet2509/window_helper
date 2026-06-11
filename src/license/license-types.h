#pragma once
// Shared types for license activation system.
// Used by license-client, license-cache, and activation-dialog.

#include <cstdint>
#include <string>
#include <optional>

namespace License {

// Error codes returned from server or local validation.
enum class ErrorCode {
    None,
    InvalidToken,       // 404 — token không tồn tại
    MachineMismatch,    // 409 — token đã bind máy khác
    Revoked,            // 410 REVOKED
    Expired,            // 410 EXPIRED
    RateLimited,        // 429
    NetworkError,       // WinHTTP / connect fail
    SignatureInvalid,   // Ed25519 verify fail
    ParseError,         // JSON / schema error
};

// Result returned from LicenseClient::Activate / Verify.
struct ActivationResult {
    bool      ok         = false;
    int64_t   expires_at = 0;    // 0 = permanent
    int32_t   grace_hours = 48;
    ErrorCode error      = ErrorCode::None;
};

// Payload stored in %APPDATA%\WindowHelper\license.dat (AES-GCM encrypted).
struct CachedLicense {
    std::string token;
    std::string machine_id;   // 64-hex full HWID
    int64_t     expires_at    = 0;
    int64_t     last_verified = 0;
    int32_t     grace_hours   = 48;
};

// Signed payload from server response.
struct SignedPayload {
    std::string token_hash;  // SHA-256(token) lowercase hex
    std::string machine_id;
    int64_t     expires_at = 0;
    int64_t     issued_at  = 0;
};

} // namespace License
