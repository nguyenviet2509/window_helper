// license-client.cpp — WinHTTP POST + Ed25519 verify for license activation.
// Server: https://vietnt.io.vn
// Uses nlohmann/json (in vcpkg.json) for JSON parsing.
// Uses bcrypt CNG for SHA-256(token) hex and base64 decode.

#include "license-client.h"
#include "ed25519-verify.h"
#include "server-public-key.h"

#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <wincrypt.h>   // CryptStringToBinaryA, CRYPT_STRING_BASE64
#include <winerror.h>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <stdexcept>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

namespace License {

namespace {

// ---- SHA-256 hex (bcrypt CNG) ------------------------------------------------

static std::string sha256Hex(const std::string& input) {
    BCRYPT_ALG_HANDLE  hAlg  = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;

    auto cleanup = [&]() {
        if (hHash) BCryptDestroyHash(hHash);
        if (hAlg)  BCryptCloseAlgorithmProvider(hAlg, 0);
    };

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
        return "";

    DWORD hashLen = 0, cbResult = 0;
    if (!BCRYPT_SUCCESS(BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(&hashLen), sizeof(hashLen), &cbResult, 0))) {
        cleanup(); return "";
    }

    if (!BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0))) {
        cleanup(); return "";
    }

    auto* dataPtr = reinterpret_cast<PUCHAR>(const_cast<char*>(input.data()));
    if (!BCRYPT_SUCCESS(BCryptHashData(hHash, dataPtr, static_cast<ULONG>(input.size()), 0))) {
        cleanup(); return "";
    }

    std::vector<uint8_t> digest(hashLen);
    if (!BCRYPT_SUCCESS(BCryptFinishHash(hHash, digest.data(), hashLen, 0))) {
        cleanup(); return "";
    }

    cleanup();

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto b : digest) oss << std::setw(2) << static_cast<unsigned>(b);
    return oss.str();
}

// ---- Base64 decode (Windows CryptStringToBinaryA) ----------------------------

static std::vector<uint8_t> base64Decode(const std::string& b64) {
    DWORD needed = 0;
    // Lần 1: tính kích thước output
    if (!CryptStringToBinaryA(b64.c_str(), static_cast<DWORD>(b64.size()),
            CRYPT_STRING_BASE64, nullptr, &needed, nullptr, nullptr))
        return {};

    std::vector<uint8_t> out(needed);
    if (!CryptStringToBinaryA(b64.c_str(), static_cast<DWORD>(b64.size()),
            CRYPT_STRING_BASE64, out.data(), &needed, nullptr, nullptr))
        return {};

    out.resize(needed);
    return out;
}

// ---- WinHTTP request helper --------------------------------------------------

struct HttpResponse {
    int         status = 0;
    std::string body;
};

// Thực hiện HTTPS POST đến vietnt.io.vn với body là JSON string.
// Timeout connect + send + receive = 10 giây.
static HttpResponse httpsPost(const std::string& path, const std::string& jsonBody) {
    HttpResponse result;

    HINTERNET hSession = WinHttpOpen(
        L"WindowHelper/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return result;

    // Timeouts: resolve=10s, connect=10s, send=10s, receive=10s
    DWORD timeout = 10000;
    WinHttpSetTimeouts(hSession, timeout, timeout, timeout, timeout);

    HINTERNET hConnect = WinHttpConnect(hSession, L"vietnt.io.vn",
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return result; }

    std::wstring wpath(path.begin(), path.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
        wpath.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Yêu cầu TLS 1.2 trở lên
    DWORD tlsFlags = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURE_PROTOCOLS, &tlsFlags, sizeof(tlsFlags));

    // Content-Type header
    BOOL sent = WinHttpSendRequest(hRequest,
        L"Content-Type: application/json\r\n",
        static_cast<DWORD>(-1L),
        const_cast<void*>(static_cast<const void*>(jsonBody.data())),
        static_cast<DWORD>(jsonBody.size()),
        static_cast<DWORD>(jsonBody.size()),
        0);

    if (sent) WinHttpReceiveResponse(hRequest, nullptr);

    // Đọc HTTP status code
    DWORD statusCode = 0, statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
        WINHTTP_NO_HEADER_INDEX);
    result.status = static_cast<int>(statusCode);

    // Đọc response body
    DWORD bytesAvail = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvail) && bytesAvail > 0) {
        std::vector<char> buf(bytesAvail + 1, 0);
        DWORD bytesRead = 0;
        WinHttpReadData(hRequest, buf.data(), bytesAvail, &bytesRead);
        result.body.append(buf.data(), bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

// ---- Map HTTP status / error code string → ErrorCode ------------------------

static ErrorCode mapErrorCode(int httpStatus, const std::string& code) {
    if (httpStatus == 404) return ErrorCode::InvalidToken;
    if (httpStatus == 409) return ErrorCode::MachineMismatch;
    if (httpStatus == 429) return ErrorCode::RateLimited;
    if (httpStatus == 410) {
        if (code == "REVOKED") return ErrorCode::Revoked;
        return ErrorCode::Expired;
    }
    return ErrorCode::NetworkError;
}

// ---- Build canonical string and verify Ed25519 sig --------------------------
// Format: "${token_hash}|${machine_id}|${expires_at}|${issued_at}"

static bool verifySig(const SignedPayload& sp, const std::string& sigB64) {
    // Canonical message
    std::string canonical =
        sp.token_hash + "|" +
        sp.machine_id + "|" +
        std::to_string(sp.expires_at) + "|" +
        std::to_string(sp.issued_at);

    // Decode base64 signature
    auto sigBytes = base64Decode(sigB64);
    if (sigBytes.size() != 64) return false;

    auto* msg = reinterpret_cast<const uint8_t*>(canonical.data());
    return VerifySignature(msg, canonical.size(),
        sigBytes.data(), kServerPubKey);
}

} // anonymous namespace

// ---- LicenseClient::PostActivate -------------------------------------------

ActivationResult LicenseClient::PostActivate(const std::string& token,
                                              const std::string& hwid_full,
                                              const std::string& hwid_short,
                                              const std::string& path)
{
    ActivationResult res;

    // Build request JSON
    nlohmann::json req;
    req["token"]            = token;
    req["machine_id"]       = hwid_full;
    req["machine_id_short"] = hwid_short;
    req["app_version"]      = "1.0";
    std::string body = req.dump();

    HttpResponse http = httpsPost(path, body);

    if (http.status == 0) {
        res.error = ErrorCode::NetworkError;
        return res;
    }

    // Parse response JSON
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(http.body);
    } catch (...) {
        res.error = ErrorCode::ParseError;
        return res;
    }

    if (http.status != 200) {
        std::string code;
        try { code = j.value("code", ""); } catch (...) {}
        res.error = mapErrorCode(http.status, code);
        return res;
    }

    // Validate response schema
    if (!j.value("ok", false)) {
        res.error = ErrorCode::ParseError;
        return res;
    }

    std::string sigB64;
    nlohmann::json payload;
    try {
        sigB64  = j.at("signed").get<std::string>();
        payload = j.at("payload");
    } catch (...) {
        res.error = ErrorCode::ParseError;
        return res;
    }

    // Build SignedPayload for verification
    SignedPayload sp;
    try {
        sp.token_hash = payload.at("token_hash").get<std::string>();
        sp.machine_id = payload.at("machine_id").get<std::string>();
        sp.expires_at = payload.value("expires_at", int64_t(0));
        sp.issued_at  = payload.at("issued_at").get<int64_t>();
    } catch (...) {
        res.error = ErrorCode::ParseError;
        return res;
    }

    // Cross-check token_hash
    std::string expectedHash = sha256Hex(token);
    if (expectedHash.empty() || sp.token_hash != expectedHash) {
        res.error = ErrorCode::SignatureInvalid;
        return res;
    }

    // Verify Ed25519 signature — reject nếu fail
    if (!verifySig(sp, sigB64)) {
        res.error = ErrorCode::SignatureInvalid;
        return res;
    }

    res.ok          = true;
    res.expires_at  = sp.expires_at;
    res.grace_hours = j.value("grace_hours", 48);
    return res;
}

// ---- Public API -------------------------------------------------------------

ActivationResult LicenseClient::Activate(const std::string& token,
                                          const std::string& hwid_full,
                                          const std::string& hwid_short)
{
    return PostActivate(token, hwid_full, hwid_short, "/api/license/activate");
}

ActivationResult LicenseClient::Verify(const std::string& token,
                                        const std::string& hwid_full,
                                        const std::string& hwid_short)
{
    return PostActivate(token, hwid_full, hwid_short, "/api/license/verify");
}

} // namespace License
