#pragma once
// LicenseInfoDialog — read-only ImGui popup showing current license details.
// Call Open() to trigger the modal; Render() each frame.
// Takes a LicenseManager* and calls Snapshot() each Render() for thread-safety (C1).

#include <windows.h>
#include "../license/license-types.h"
#include "../license/license-manager.h"
#include <string>

class LicenseInfoDialog {
public:
    explicit LicenseInfoDialog(License::LicenseManager* mgr);

    // Trigger the modal — safe to call from any context before Render().
    void Open();

    // Draw the modal. Must be called within an active ImGui frame.
    void Render();

    bool IsOpen() const { return open_; }

private:
    static std::string maskToken(const std::string& token);
    static std::string formatDateVN(int64_t unixSec);
    static std::string formatGraceRemaining(int64_t lastVerified, int32_t graceHours);
    static void        copyToClipboard(const std::string& text);

    License::LicenseManager* mgr_;  // not owned; valid for dialog lifetime
    bool open_         = false;
    bool pendingOpen_  = false;
    bool copiedFlash_  = false;     // "Đã copy!" feedback timer
    float copiedTimer_ = 0.0f;
};
