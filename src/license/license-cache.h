#pragma once
// LicenseCache — AES-256-GCM encrypted cache for license data.
// File: %APPDATA%\WindowHelper\license.dat
// Key derived from HWID via HKDF-SHA256 (bcrypt CNG).
// Format: [12 bytes IV][16 bytes GCM tag][N bytes ciphertext]

#include "license-types.h"
#include <optional>
#include <string>
#include <vector>

namespace License {

class LicenseCache {
public:
    // Encrypt and save CachedLicense to disk.
    // hwid_full: 64-hex HWID used as key derivation input.
    // Returns false on any error.
    static bool Save(const CachedLicense& lic, const std::string& hwid_full);

    // Load and decrypt from disk.
    // hwid_full must match the one used during Save (wrong HWID → nullopt).
    // Returns nullopt on missing file, decrypt failure, or auth-tag mismatch.
    static std::optional<CachedLicense> Load(const std::string& hwid_full);

    // Delete the cache file (e.g., after revocation).
    static void Clear();

private:
    static std::wstring getCachePath();
    static std::vector<uint8_t> deriveKey(const std::string& hwid_full);
};

} // namespace License
