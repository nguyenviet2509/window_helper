#pragma once
// LicenseManager — orchestrates bootstrap, grace-period logic, and periodic verify.
// Usage:
//   LicenseManager mgr;
//   BootstrapResult bs = mgr.Bootstrap();      // synchronous
//   if (bs == BootstrapResult::ENTER_MAIN)      // proceed
//       mgr.StartPeriodicVerify();
//   // each frame:
//   if (mgr.LicenseLost()) { /* show toast, schedule shutdown */ }

#include "license-types.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace License {

enum class BootstrapResult {
    ENTER_MAIN,   // cache valid and within grace (or online verify passed)
    SHOW_DIALOG,  // no cache, expired, revoked, or grace-expired + offline
    EXIT,         // reserved (fatal internal error)
};

class LicenseManager {
public:
    LicenseManager() = default;
    ~LicenseManager();                       // stops worker thread

    // Synchronous. Loads cache, verifies if grace expired.
    // Sets current_ on ENTER_MAIN. Sets lost_reason_ on SHOW_DIALOG.
    BootstrapResult Bootstrap();

    // Spawns daemon thread that re-verifies every 6 hours. Interruptible.
    void StartPeriodicVerify();

    // Signals stop and joins thread. Idempotent.
    void Stop();

    // Poll each frame from main loop.
    bool        LicenseLost()       const { return license_lost_.load(); }
    std::string LicenseLostReason() const;

    // Valid after Bootstrap() == ENTER_MAIN.
    const CachedLicense& Current() const { return current_; }

    // Thread-safe snapshot copy — use this from UI/render thread.
    CachedLicense Snapshot() const;

    // Called by main.cpp after user activates via dialog.
    void AdoptFromDialog(const CachedLicense& fresh);

private:
    void verifyLoop();       // periodic thread body
    void runOneVerify();     // single verify + cache update or flag set

    CachedLicense           current_;
    std::atomic<bool>       license_lost_{ false };
    std::string             lost_reason_;
    std::atomic<bool>       stop_flag_{ false };
    std::thread             verify_thread_;
    std::condition_variable cv_;
    mutable std::mutex      mtx_;  // mutable: locked in const methods
};

} // namespace License
