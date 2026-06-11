#pragma once
// ActivationDialog — ImGui modal that gates the app until a valid license code
// is entered. Phase 3: wires LicenseClient::Activate on a worker thread.
//
// Usage:
//   ActivationDialog dlg;
//   dlg.SetOnActivated([](CachedLicense) { /* proceed to main UI */ });
//   dlg.SetOnExit([]() { PostQuitMessage(0); });
//   // inside render loop:
//   dlg.Open();   // call once to trigger the popup
//   dlg.Render(); // call every frame

#include "../license/license-types.h"

#include <functional>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <optional>

class ActivationDialog {
public:
    ActivationDialog();
    ~ActivationDialog();

    // Open the modal popup. Safe to call every frame — ImGui deduplicates.
    void Open();

    // Draw the modal. Call from within an active ImGui frame.
    // Polls worker thread result each frame.
    void Render();

    // Called on successful activation — receives the CachedLicense to store/use.
    void SetOnActivated(std::function<void(License::CachedLicense)> cb);

    // Called when user clicks [Exit].
    void SetOnExit(std::function<void()> cb);

    // Display a status line at the bottom of the dialog.
    // is_error=true → red text; false → green text.
    void SetStatus(const std::string& msg, bool is_error = false);

    // Disable buttons and show "Đang kiểm tra..." while network call is in-flight.
    void SetBusy(bool busy);

    bool IsOpen() const { return open_; }

private:
    void copyFullIdToClipboard();
    void startActivationWorker(const std::string& token);
    void pollWorkerResult();
    static std::string errorMessage(License::ErrorCode code);

    std::function<void(License::CachedLicense)> onActivated_;
    std::function<void()> onExit_;

    char  codeBuffer_[65] = {};   // up to 64 chars + null
    std::string statusMsg_;
    bool  statusIsError_ = false;
    bool  busy_ = false;
    bool  open_ = false;
    bool  pendingOpen_ = false;   // set by Open(), consumed first frame

    // Worker thread state — written by worker, read by Render() (UI thread).
    std::thread                            workerThread_;
    std::atomic<bool>                      resultReady_{ false };
    std::mutex                             resultMutex_;
    License::ActivationResult              workerResult_;
    std::string                            workerToken_;  // token used for cache
};
