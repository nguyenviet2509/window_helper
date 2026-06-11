// LicenseManager — bootstrap, grace-period, periodic verify, revoke detection.
// Grace window: 48h from last_verified (configurable via grace_hours in cache).
// Periodic verify: every 6h while app is running; network fail → silent retry.
// Revoke / expire detected → sets atomic flag; main loop shows toast + shutdown.

#include "license-manager.h"
#include "hwid-collector.h"
#include "license-cache.h"
#include "license-client.h"

#include <ctime>
#include <chrono>

namespace License {

namespace {
    // Interval between periodic verify ticks.
    constexpr auto kVerifyInterval = std::chrono::hours(6);

    // Returns current Unix timestamp (seconds).
    int64_t Now() {
        return static_cast<int64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }
} // namespace

// ---------------------------------------------------------------------------
// Destructor — ensures thread is stopped before destruction.
// ---------------------------------------------------------------------------
LicenseManager::~LicenseManager() {
    Stop();
}

// ---------------------------------------------------------------------------
// Bootstrap — synchronous, called once before heavy init.
// ---------------------------------------------------------------------------
BootstrapResult LicenseManager::Bootstrap() {
    const std::string hwid = HwidFull();
    const std::string hwidShort = HwidShort();

    // 1. Load cache from encrypted file.
    auto cache = LicenseCache::Load(hwid);
    if (!cache) return BootstrapResult::SHOW_DIALOG;

    int64_t now = Now();

    // 2. Immediate rejection: revoked or expired.
    if (cache->expires_at != 0 && now >= cache->expires_at) {
        lost_reason_ = "Mã đã hết hạn";
        return BootstrapResult::SHOW_DIALOG;
    }

    // 3. Grace window check.
    int64_t graceSeconds = static_cast<int64_t>(cache->grace_hours) * 3600;
    bool graceExpired = (now - cache->last_verified) >= graceSeconds;

    if (graceExpired) {
        // Grace expired → must verify online now (synchronous, blocks briefly).
        auto result = LicenseClient::Verify(cache->token, hwid, hwidShort);
        if (!result.ok) {
            // Map error to Vietnamese reason.
            switch (result.error) {
            case ErrorCode::Revoked:
                lost_reason_ = "Mã đã bị thu hồi";
                break;
            case ErrorCode::Expired:
                lost_reason_ = "Mã đã hết hạn";
                break;
            case ErrorCode::MachineMismatch:
                lost_reason_ = "Máy không khớp với mã kích hoạt";
                break;
            case ErrorCode::NetworkError:
                // Network down + grace expired → block (need online confirmation).
                lost_reason_ = "Không kết nối được server. Kiểm tra mạng.";
                break;
            default:
                lost_reason_ = "Xác thực thất bại";
                break;
            }
            return BootstrapResult::SHOW_DIALOG;
        }
        // Update last_verified and persist.
        cache->last_verified = now;
        if (result.expires_at != 0) cache->expires_at = result.expires_at;
        LicenseCache::Save(*cache, hwid);
    }
    // Within grace: no background refresh here — periodic verify (6h) covers it.
    // Dropping the detached thread avoids: untracked lifetime, stale in-memory
    // current_, and redundant I/O duplicating the periodic verify.

    current_ = *cache;
    return BootstrapResult::ENTER_MAIN;
}

// ---------------------------------------------------------------------------
// AdoptFromDialog — called after user activates successfully via dialog.
// ---------------------------------------------------------------------------
void LicenseManager::AdoptFromDialog(const CachedLicense& fresh) {
    std::lock_guard<std::mutex> lock(mtx_);
    current_ = fresh;
    license_lost_.store(false);
    lost_reason_.clear();
}

// ---------------------------------------------------------------------------
// StartPeriodicVerify — spawns background daemon thread.
// ---------------------------------------------------------------------------
void LicenseManager::StartPeriodicVerify() {
    stop_flag_.store(false);
    verify_thread_ = std::thread(&LicenseManager::verifyLoop, this);
}

// ---------------------------------------------------------------------------
// Stop — signals thread to exit and joins. Idempotent.
// ---------------------------------------------------------------------------
void LicenseManager::Stop() {
    stop_flag_.store(true);
    cv_.notify_all();
    if (verify_thread_.joinable()) {
        verify_thread_.join();
    }
}

// ---------------------------------------------------------------------------
// Snapshot — returns a thread-safe copy of current_ under lock.
// ---------------------------------------------------------------------------
CachedLicense LicenseManager::Snapshot() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return current_;
}

// ---------------------------------------------------------------------------
// LicenseLostReason — thread-safe read (H1: lock before reading lost_reason_).
// ---------------------------------------------------------------------------
std::string LicenseManager::LicenseLostReason() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return lost_reason_;
}

// ---------------------------------------------------------------------------
// verifyLoop — daemon thread: sleep 6h, verify, repeat.
// ---------------------------------------------------------------------------
void LicenseManager::verifyLoop() {
    while (true) {
        // Sleep for kVerifyInterval, interruptible by stop signal.
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait_for(lock, kVerifyInterval, [this] { return stop_flag_.load(); });
        if (stop_flag_.load()) return;

        // Release lock before network call (don't block main thread operations).
        lock.unlock();
        runOneVerify();
    }
}

// ---------------------------------------------------------------------------
// runOneVerify — single verify tick; sets lost flag on definitive failure.
// ---------------------------------------------------------------------------
void LicenseManager::runOneVerify() {
    const std::string hwid = HwidFull();
    const std::string hwidShort = HwidShort();

    std::string token;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        token = current_.token;
    }

    if (token.empty()) return;

    auto result = LicenseClient::Verify(token, hwid, hwidShort);

    if (result.ok) {
        // Refresh last_verified in memory; copy under lock then save outside (C2).
        int64_t now = Now();
        CachedLicense to_save;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            current_.last_verified = now;
            if (result.expires_at != 0) current_.expires_at = result.expires_at;
            to_save = current_;  // copy while holding lock
        }
        LicenseCache::Save(to_save, hwid);  // file I/O outside lock
        return;
    }

    // Network error → silent retry next tick; grace window handles offline periods.
    if (result.error == ErrorCode::NetworkError) return;

    // Definitive server rejection → set lost flag.
    std::string reason;
    switch (result.error) {
    case ErrorCode::Revoked:        reason = "Mã đã bị thu hồi";             break;
    case ErrorCode::Expired:        reason = "Mã đã hết hạn";                break;
    case ErrorCode::MachineMismatch:reason = "Máy không khớp";               break;
    default:                        reason = "Xác thực thất bại";            break;
    }

    {
        std::lock_guard<std::mutex> lock(mtx_);
        lost_reason_ = reason;
    }
    license_lost_.store(true);
    // Clear cache to force re-activation on next start.
    LicenseCache::Clear();
}

} // namespace License
