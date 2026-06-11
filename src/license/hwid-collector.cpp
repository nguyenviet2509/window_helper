// hwid-collector.cpp — Machine fingerprint collector.
// Sources: VolumeSerial(C:) + cpuid(EAX=1) EAX+EBX + MachineGuid + primary MAC
// Hashed via Windows CNG SHA-256 (bcrypt.lib). Result cached after first call.
//
// IMPORTANT: winsock2.h MUST come before windows.h to get AF_UNSPEC and
// IP_ADAPTER_ADDRESSES; iphlpapi.h depends on ws2def.h via winsock2.h.

#include "hwid-collector.h"

// clang-format off
#include <winsock2.h>      // AF_UNSPEC, SOCKADDR — must precede windows.h
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>      // GetAdaptersAddresses (needs winsock2.h first)
#include <intrin.h>        // __cpuid
#include <bcrypt.h>        // BCryptOpenAlgorithmProvider, BCryptHash
#include <winerror.h>
// clang-format on
#include <cstdint>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "bcrypt.lib")

namespace License {

namespace {

// --- helpers -----------------------------------------------------------------

static std::string toHex(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i)
        oss << std::setw(2) << static_cast<unsigned>(data[i]);
    return oss.str();
}

// Append little-endian bytes of v to buf
static void appendU32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>(v));
    buf.push_back(static_cast<uint8_t>(v >> 8));
    buf.push_back(static_cast<uint8_t>(v >> 16));
    buf.push_back(static_cast<uint8_t>(v >> 24));
}

static void appendStr(std::vector<uint8_t>& buf, const std::string& s) {
    buf.insert(buf.end(), s.begin(), s.end());
    buf.push_back(0); // null separator
}

// --- component collectors ----------------------------------------------------

static uint32_t collectVolumeSerial() {
    DWORD serial = 0;
    // Bỏ qua lỗi — nếu fail thì serial = 0 (vẫn đưa vào hash)
    GetVolumeInformationW(L"C:\\", nullptr, 0, &serial, nullptr, nullptr, nullptr, 0);
    return static_cast<uint32_t>(serial);
}

static void collectCpuid(uint32_t& outEax, uint32_t& outEbx) {
    int info[4] = {};
    __cpuid(info, 1);
    outEax = static_cast<uint32_t>(info[0]);
    outEbx = static_cast<uint32_t>(info[1]);
}

// Đọc MachineGuid từ registry (64-bit hive, tránh WOW64 redirect).
static std::string collectMachineGuid() {
    WCHAR buf[64] = {};
    DWORD bufSize = sizeof(buf);
    LSTATUS st = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Cryptography",
        L"MachineGuid",
        RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY,
        nullptr, buf, &bufSize);
    if (st != ERROR_SUCCESS) return "";
    // Convert wide → narrow (GUID chỉ chứa ASCII hex + dấu -)
    std::string out;
    for (WCHAR c : buf) {
        if (c == L'\0') break;
        out += static_cast<char>(c);
    }
    return out;
}

// First non-loopback, non-tunnel physical adapter MAC (6 bytes).
static std::vector<uint8_t> collectPrimaryMac() {
    ULONG bufLen = 0;
    // Lần 1: lấy kích thước cần thiết
    GetAdaptersAddresses(AF_UNSPEC,
        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        nullptr, nullptr, &bufLen);

    std::vector<uint8_t> rawBuf(bufLen);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(rawBuf.data());

    ULONG ret = GetAdaptersAddresses(AF_UNSPEC,
        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        nullptr, adapters, &bufLen);
    if (ret != ERROR_SUCCESS) return {};

    for (auto* a = adapters; a != nullptr; a = a->Next) {
        // Bỏ qua loopback, tunnel, và adapter không có MAC
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        if (a->IfType == IF_TYPE_TUNNEL) continue;
        if (a->PhysicalAddressLength != 6) continue;
        // Bỏ qua MAC toàn 0 (virtual / disabled)
        bool allZero = true;
        for (ULONG i = 0; i < 6; ++i)
            if (a->PhysicalAddress[i] != 0) { allZero = false; break; }
        if (allZero) continue;

        return std::vector<uint8_t>(a->PhysicalAddress, a->PhysicalAddress + 6);
    }
    return {};
}

// SHA-256 via Windows CNG
static std::string sha256Hex(const std::vector<uint8_t>& data) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::string result;

    auto cleanup = [&]() {
        if (hHash) BCryptDestroyHash(hHash);
        if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
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

    if (!BCRYPT_SUCCESS(BCryptHashData(hHash,
            const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), 0))) {
        cleanup(); return "";
    }

    std::vector<uint8_t> digest(hashLen);
    if (!BCRYPT_SUCCESS(BCryptFinishHash(hHash, digest.data(), hashLen, 0))) {
        cleanup(); return "";
    }

    cleanup();
    return toHex(digest.data(), digest.size());
}

// --- cache -------------------------------------------------------------------

static std::mutex g_mutex;
static std::string g_cached; // empty = not yet computed

static const std::string& ensureComputed() {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_cached.empty()) return g_cached;

    std::vector<uint8_t> buf;
    buf.reserve(64);

    // 1. Volume serial
    appendU32(buf, collectVolumeSerial());

    // 2. CPUID leaf 1: EAX (stepping/model/family) + EBX (brand index/CLFLUSH/APIC ID)
    uint32_t cpuEax = 0, cpuEbx = 0;
    collectCpuid(cpuEax, cpuEbx);
    appendU32(buf, cpuEax);
    appendU32(buf, cpuEbx);

    // 3. MachineGuid
    appendStr(buf, collectMachineGuid());

    // 4. Primary MAC
    auto mac = collectPrimaryMac();
    if (!mac.empty())
        buf.insert(buf.end(), mac.begin(), mac.end());

    g_cached = sha256Hex(buf);
    if (g_cached.empty()) g_cached = "0000000000000000000000000000000000000000000000000000000000000000";
    return g_cached;
}

} // anonymous namespace

// --- public API --------------------------------------------------------------

std::string HwidFull() {
    return ensureComputed();
}

std::string HwidShort() {
    const std::string& full = ensureComputed();
    return full.substr(0, 8);
}

} // namespace License
