#pragma once
// LicenseClient — WinHTTP-based client for license activation/verification.
// Calls POST https://vietnt.io.vn/api/license/activate
// Verifies Ed25519 signature over canonical payload before trusting response.

#include "license-types.h"
#include <string>

namespace License {

class LicenseClient {
public:
    // POST /api/license/activate — binds token to hwid on server.
    // hwid_full: 64-hex HWID from HwidFull()
    // hwid_short: 8-hex prefix from HwidShort()
    // Blocks for up to 10s (run on worker thread).
    static ActivationResult Activate(const std::string& token,
                                     const std::string& hwid_full,
                                     const std::string& hwid_short);

    // POST /api/license/verify — re-validates an already-activated token.
    // Same signature as Activate.
    static ActivationResult Verify(const std::string& token,
                                   const std::string& hwid_full,
                                   const std::string& hwid_short);

private:
    static ActivationResult PostActivate(const std::string& token,
                                         const std::string& hwid_full,
                                         const std::string& hwid_short,
                                         const std::string& path);
};

} // namespace License
