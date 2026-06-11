// ed25519-verify.cpp — libsodium wrapper for Ed25519 signature verification.
// sodium_init() is called once via std::call_once (thread-safe).

#include "ed25519-verify.h"
#include <sodium.h>
#include <mutex>

namespace License {

namespace {

// Thread-safe one-time init
static std::once_flag g_sodiumInitFlag;
static bool           g_sodiumOk = false;

static void initSodium() {
    g_sodiumOk = (sodium_init() >= 0); // returns 0 first call, 1 if already init, -1 on fail
}

} // anonymous namespace

bool VerifySignature(const uint8_t* msg, size_t msg_len,
                     const uint8_t sig[64], const uint8_t pk[32])
{
    std::call_once(g_sodiumInitFlag, initSodium);
    if (!g_sodiumOk) return false;

    return crypto_sign_verify_detached(sig, msg, msg_len, pk) == 0;
}

} // namespace License
