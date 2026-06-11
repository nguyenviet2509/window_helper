#pragma once
// ActivationDialog — ImGui modal that gates the app until a valid license code
// is entered. Callback-based; no network logic here (wired in phase 3).
//
// Usage:
//   ActivationDialog dlg;
//   dlg.SetOnActivate([](std::string code) { /* POST /activate */ });
//   dlg.SetOnExit([]() { PostQuitMessage(0); });
//   // inside render loop:
//   dlg.Open();   // call once to trigger the popup
//   dlg.Render(); // call every frame

#include <functional>
#include <string>

class ActivationDialog {
public:
    ActivationDialog();

    // Open the modal popup. Safe to call every frame — ImGui deduplicates.
    void Open();

    // Draw the modal. Call from within an active ImGui frame.
    void Render();

    // Called when user clicks [Activate] — receives the trimmed code string.
    void SetOnActivate(std::function<void(std::string)> cb);

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

    std::function<void(std::string)> onActivate_;
    std::function<void()> onExit_;

    char  codeBuffer_[33] = {};   // 32 chars + null
    std::string statusMsg_;
    bool  statusIsError_ = false;
    bool  busy_ = false;
    bool  open_ = false;
    bool  pendingOpen_ = false;   // set by Open(), consumed first frame
};
