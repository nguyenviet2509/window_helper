// license-cache.cpp — AES-256-GCM encrypted license cache via bcrypt CNG.
// Key = HKDF-SHA256(HWID bytes, salt="WindowHelper.v1", info="license-cache.v1")
// File format: [12 IV][16 GCM-tag][ciphertext]
// Atomic write: write to .tmp then MoveFileExW REPLACE_EXISTING.

#include "license-cache.h"

#include <windows.h>
#include <bcrypt.h>
#include <shlobj.h>     // SHGetKnownFolderPath
#include <winerror.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <fstream>
#include <sstream>

#pragma comment(lib, "bcrypt.lib")

namespace License {

namespace {

static constexpr char kSalt[] = "WindowHelper.v1";
static constexpr char kInfo[] = "license-cache.v1";
static constexpr DWORD kKeyBytes = 32; // AES-256
static constexpr DWORD kIvBytes  = 12; // GCM nonce
static constexpr DWORD kTagBytes = 16; // GCM auth tag

// ---- HMAC-SHA256 (bcrypt) ---------------------------------------------------

static std::vector<uint8_t> hmacSha256(const std::vector<uint8_t>& key,
                                        const std::vector<uint8_t>& data)
{
    BCRYPT_ALG_HANDLE  hAlg  = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::vector<uint8_t> result;

    auto cleanup = [&]() {
        if (hHash) BCryptDestroyHash(hHash);
        if (hAlg)  BCryptCloseAlgorithmProvider(hAlg, 0);
    };

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM,
            nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG))) {
        return result;
    }

    if (!BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0,
            const_cast<PUCHAR>(key.data()), static_cast<ULONG>(key.size()), 0))) {
        cleanup(); return result;
    }

    if (!BCRYPT_SUCCESS(BCryptHashData(hHash,
            const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), 0))) {
        cleanup(); return result;
    }

    DWORD hashLen = 32; // SHA-256 output is always 32 bytes
    result.resize(hashLen);
    if (!BCRYPT_SUCCESS(BCryptFinishHash(hHash, result.data(), hashLen, 0))) {
        result.clear();
    }

    cleanup();
    return result;
}

// ---- HKDF-SHA256 (RFC 5869) — manual implementation (30 LOC) ---------------
// Required because BCryptKeyDerivation HKDF is Win10 1709+ only.

static std::vector<uint8_t> hkdfSha256(const std::vector<uint8_t>& ikm,
                                         const std::string& salt,
                                         const std::string& info,
                                         DWORD outLen)
{
    // Step 1: Extract — PRK = HMAC-SHA256(salt, IKM)
    std::vector<uint8_t> saltBytes(salt.begin(), salt.end());
    std::vector<uint8_t> prk = hmacSha256(saltBytes, ikm);
    if (prk.empty()) return {};

    // Step 2: Expand — T(1) = HMAC-SHA256(PRK, info || 0x01)
    // For 32 bytes output we only need T(1) since SHA-256 output is 32 bytes.
    std::vector<uint8_t> infoBytes(info.begin(), info.end());
    infoBytes.push_back(0x01);
    std::vector<uint8_t> t1 = hmacSha256(prk, infoBytes);
    // Wipe PRK intermediate — no longer needed after expand
    SecureZeroMemory(prk.data(), prk.size());
    if (t1.size() < outLen) return {};

    t1.resize(outLen);
    return t1;
}

// ---- Key derivation from HWID hex string ------------------------------------

static std::vector<uint8_t> deriveKeyFromHwid(const std::string& hwid_full) {
    // HWID is already 64-hex; use raw bytes as IKM
    std::vector<uint8_t> ikm(hwid_full.begin(), hwid_full.end());
    return hkdfSha256(ikm, kSalt, kInfo, kKeyBytes);
}

// ---- Get %APPDATA%\WindowHelper\ path as wide string ------------------------

static std::wstring getAppDataDir() {
    PWSTR pPath = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &pPath)))
        return {};
    std::wstring dir(pPath);
    CoTaskMemFree(pPath);
    dir += L"\\WindowHelper\\";
    return dir;
}

// ---- AES-256-GCM encrypt ----------------------------------------------------
// Output: [12 IV][16 tag][ciphertext]

static std::vector<uint8_t> aesGcmEncrypt(const std::vector<uint8_t>& key,
                                            const std::vector<uint8_t>& plaintext)
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    std::vector<uint8_t> result;

    auto cleanup = [&]() {
        if (hKey) BCryptDestroyKey(hKey);
        if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    };

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0))) {
        return result;
    }

    // Đặt GCM mode
    if (!BCRYPT_SUCCESS(BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
            reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
            sizeof(BCRYPT_CHAIN_MODE_GCM), 0))) {
        cleanup(); return result;
    }

    if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
            const_cast<PUCHAR>(key.data()), static_cast<ULONG>(key.size()), 0))) {
        cleanup(); return result;
    }

    // Random 12-byte IV
    std::vector<uint8_t> iv(kIvBytes);
    BCryptGenRandom(nullptr, iv.data(), kIvBytes, BCRYPT_USE_SYSTEM_PREFERRED_RNG);

    // GCM auth info
    std::vector<uint8_t> tag(kTagBytes, 0);
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce     = iv.data();
    authInfo.cbNonce     = kIvBytes;
    authInfo.pbTag       = tag.data();
    authInfo.cbTag       = kTagBytes;
    authInfo.pbAuthData  = nullptr;
    authInfo.cbAuthData  = 0;

    DWORD cipherLen = 0;
    if (!BCRYPT_SUCCESS(BCryptEncrypt(hKey,
            const_cast<PUCHAR>(plaintext.data()), static_cast<ULONG>(plaintext.size()),
            &authInfo, nullptr, 0, nullptr, 0, &cipherLen, 0))) {
        cleanup(); return result;
    }

    std::vector<uint8_t> cipher(cipherLen);
    if (!BCRYPT_SUCCESS(BCryptEncrypt(hKey,
            const_cast<PUCHAR>(plaintext.data()), static_cast<ULONG>(plaintext.size()),
            &authInfo, nullptr, 0, cipher.data(), cipherLen, &cipherLen, 0))) {
        cleanup(); return result;
    }
    cipher.resize(cipherLen);

    cleanup();

    // Assemble: IV + tag + ciphertext
    result.reserve(kIvBytes + kTagBytes + cipher.size());
    result.insert(result.end(), iv.begin(), iv.end());
    result.insert(result.end(), tag.begin(), tag.end());
    result.insert(result.end(), cipher.begin(), cipher.end());

    // Wipe sensitive stack data
    SecureZeroMemory(iv.data(), iv.size());
    return result;
}

// ---- AES-256-GCM decrypt ----------------------------------------------------

static std::optional<std::vector<uint8_t>> aesGcmDecrypt(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& blob)   // [12 IV][16 tag][ciphertext]
{
    if (blob.size() < kIvBytes + kTagBytes) return std::nullopt;

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;

    auto cleanup = [&]() {
        if (hKey) BCryptDestroyKey(hKey);
        if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    };

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0))) {
        return std::nullopt;
    }

    if (!BCRYPT_SUCCESS(BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
            reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
            sizeof(BCRYPT_CHAIN_MODE_GCM), 0))) {
        cleanup(); return std::nullopt;
    }

    if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
            const_cast<PUCHAR>(key.data()), static_cast<ULONG>(key.size()), 0))) {
        cleanup(); return std::nullopt;
    }

    // Parse blob
    std::vector<uint8_t> iv(blob.begin(), blob.begin() + kIvBytes);
    std::vector<uint8_t> tag(blob.begin() + kIvBytes, blob.begin() + kIvBytes + kTagBytes);
    const DWORD cipherLen = static_cast<DWORD>(blob.size() - kIvBytes - kTagBytes);
    const uint8_t* cipherPtr = blob.data() + kIvBytes + kTagBytes;

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce     = iv.data();
    authInfo.cbNonce     = kIvBytes;
    authInfo.pbTag       = tag.data();
    authInfo.cbTag       = kTagBytes;
    authInfo.pbAuthData  = nullptr;
    authInfo.cbAuthData  = 0;

    DWORD plainLen = 0;
    if (!BCRYPT_SUCCESS(BCryptDecrypt(hKey,
            const_cast<PUCHAR>(cipherPtr), cipherLen,
            &authInfo, nullptr, 0, nullptr, 0, &plainLen, 0))) {
        cleanup(); return std::nullopt;
    }

    std::vector<uint8_t> plain(plainLen);
    NTSTATUS st = BCryptDecrypt(hKey,
        const_cast<PUCHAR>(cipherPtr), cipherLen,
        &authInfo, nullptr, 0, plain.data(), plainLen, &plainLen, 0);

    cleanup();
    SecureZeroMemory(iv.data(), iv.size());

    // AUTH_TAG_MISMATCH (0xC000A002) means tampered data
    if (!BCRYPT_SUCCESS(st)) return std::nullopt;

    plain.resize(plainLen);
    return plain;
}

} // anonymous namespace

// ---- LicenseCache public methods --------------------------------------------

std::vector<uint8_t> LicenseCache::deriveKey(const std::string& hwid_full) {
    return deriveKeyFromHwid(hwid_full);
}

std::wstring LicenseCache::getCachePath() {
    return getAppDataDir() + L"license.dat";
}

bool LicenseCache::Save(const CachedLicense& lic, const std::string& hwid_full) {
    // Serialize to JSON plaintext
    nlohmann::json j;
    j["token"]         = lic.token;
    j["machine_id"]    = lic.machine_id;
    j["expires_at"]    = lic.expires_at;
    j["last_verified"] = lic.last_verified;
    j["grace_hours"]   = lic.grace_hours;
    std::string plain = j.dump();

    std::vector<uint8_t> key = deriveKey(hwid_full);
    if (key.empty()) return false;

    std::vector<uint8_t> plainBytes(plain.begin(), plain.end());
    std::vector<uint8_t> blob = aesGcmEncrypt(key, plainBytes);
    SecureZeroMemory(key.data(), key.size());

    if (blob.empty()) return false;

    // Ensure directory exists
    std::wstring dir = getAppDataDir();
    CreateDirectoryW(dir.c_str(), nullptr); // no-op if exists

    // Atomic write: write to .tmp then rename
    std::wstring path    = getCachePath();
    std::wstring tmpPath = path + L".tmp";

    HANDLE hFile = CreateFileW(tmpPath.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok = WriteFile(hFile, blob.data(), static_cast<DWORD>(blob.size()), &written, nullptr);
    CloseHandle(hFile);

    if (!ok || written != static_cast<DWORD>(blob.size())) {
        DeleteFileW(tmpPath.c_str());
        return false;
    }

    // Atomic rename
    return MoveFileExW(tmpPath.c_str(), path.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

std::optional<CachedLicense> LicenseCache::Load(const std::string& hwid_full) {
    std::wstring path = getCachePath();

    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return std::nullopt;

    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize == INVALID_FILE_SIZE || fileSize < kIvBytes + kTagBytes) {
        CloseHandle(hFile); return std::nullopt;
    }

    std::vector<uint8_t> blob(fileSize);
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hFile, blob.data(), fileSize, &bytesRead, nullptr);
    CloseHandle(hFile);

    if (!ok || bytesRead != fileSize) return std::nullopt;

    std::vector<uint8_t> key = deriveKey(hwid_full);
    if (key.empty()) return std::nullopt;

    auto plainOpt = aesGcmDecrypt(key, blob);
    SecureZeroMemory(key.data(), key.size());

    if (!plainOpt) return std::nullopt; // tampered or wrong HWID

    // Parse JSON
    try {
        std::string plainStr(plainOpt->begin(), plainOpt->end());
        nlohmann::json j = nlohmann::json::parse(plainStr);

        CachedLicense lic;
        lic.token         = j.at("token").get<std::string>();
        lic.machine_id    = j.at("machine_id").get<std::string>();
        lic.expires_at    = j.value("expires_at", int64_t(0));
        lic.last_verified = j.value("last_verified", int64_t(0));
        lic.grace_hours   = j.value("grace_hours", 48);
        return lic;
    } catch (...) {
        return std::nullopt;
    }
}

void LicenseCache::Clear() {
    DeleteFileW(getCachePath().c_str());
}

} // namespace License
