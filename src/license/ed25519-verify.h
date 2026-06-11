#pragma once
// Ed25519 signature verification wrapper around libsodium.
// Uses the pinned server public key from server-public-key.h.

#include <cstdint>
#include <cstddef>

namespace License {

// Verify a detached Ed25519 signature.
// msg      — message bytes
// msg_len  — message length
// sig      — 64-byte signature
// pk       — 32-byte Ed25519 public key
// Returns true if signature is valid.
bool VerifySignature(const uint8_t* msg, size_t msg_len,
                     const uint8_t sig[64], const uint8_t pk[32]);

} // namespace License
