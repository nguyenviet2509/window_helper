// activation-dialog.cpp — ImGui modal + worker-thread activation (phase 3).
// Activation flow: UI thread → spawns std::thread → LicenseClient::Activate
// → result posted back via atomic flag → UI thread polls and handles result.

#include "activation-dialog.h"
#include "../license/hwid-collector.h"
#include "../license/license-client.h"
#include "../license/license-cache.h"

#include <windows.h>
#include "imgui.h"
#include <string>
#include <algorithm>
#include <ctime>

// ---- Vietnamese error messages ----------------------------------------------

/*static*/ std::string ActivationDialog::errorMessage(License::ErrorCode code) {
    using EC = License::ErrorCode;
    switch (code) {
        case EC::None:            return "Không có lỗi cụ thể (None).";
        case EC::InvalidToken:    return "Mã không hợp lệ.";
        case EC::MachineMismatch: return "Mã đã được dùng cho máy khác. Liên hệ admin để reset.";
        case EC::Revoked:         return "Mã đã bị thu hồi.";
        case EC::Expired:         return "Mã đã hết hạn.";
        case EC::NetworkError:    return "Không kết nối được server. Kiểm tra mạng.";
        case EC::SignatureInvalid: return "Phản hồi server không hợp lệ (ParseError).";
        case EC::RateLimited:     return "Quá nhiều yêu cầu. Thử lại sau ít phút.";
        case EC::ParseError:      return "Phản hồi server không đọc được (ParseError).";
        default:                  return "Lỗi không xác định.";
    }
}

// ---- clipboard helper -------------------------------------------------------

void ActivationDialog::copyFullIdToClipboard() {
    std::string full = License::HwidFull();
    if (full.empty()) return;

    std::wstring wfull(full.begin(), full.end());
    size_t byteLen = (wfull.size() + 1) * sizeof(wchar_t);

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, byteLen);
    if (!hMem) return;

    void* ptr = GlobalLock(hMem);
    if (!ptr) { GlobalFree(hMem); return; }
    memcpy(ptr, wfull.c_str(), byteLen);
    GlobalUnlock(hMem);

    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        SetClipboardData(CF_UNICODETEXT, hMem);
        CloseClipboard();
    } else {
        GlobalFree(hMem);
    }
}

// ---- worker thread ----------------------------------------------------------

void ActivationDialog::startActivationWorker(const std::string& token) {
    // Join any previous thread before spawning new one
    if (workerThread_.joinable()) workerThread_.join();

    resultReady_.store(false);
    workerToken_ = token;

    workerThread_ = std::thread([this, token]() {
        std::string full  = License::HwidFull();
        std::string short_ = License::HwidShort();
        License::ActivationResult result =
            License::LicenseClient::Activate(token, full, short_);
        {
            std::lock_guard<std::mutex> lk(resultMutex_);
            workerResult_ = result;
        }
        resultReady_.store(true);
    });
}

// Gọi từ Render() mỗi frame — nếu worker xong thì xử lý kết quả
void ActivationDialog::pollWorkerResult() {
    if (!resultReady_.load()) return;
    resultReady_.store(false); // consume

    License::ActivationResult result;
    {
        std::lock_guard<std::mutex> lk(resultMutex_);
        result = workerResult_;
    }

    SetBusy(false);

    if (result.ok) {
        // Build cache entry
        License::CachedLicense cached;
        cached.token         = workerToken_;
        cached.machine_id    = License::HwidFull();
        cached.expires_at    = result.expires_at;
        cached.last_verified = static_cast<int64_t>(std::time(nullptr));
        cached.grace_hours   = result.grace_hours;

        // Persist to disk
        License::LicenseCache::Save(cached, cached.machine_id);

        // Notify caller → close dialog
        ImGui::CloseCurrentPopup();
        open_ = false;
        if (onActivated_) onActivated_(cached);
    } else {
        SetStatus(errorMessage(result.error), true);
    }
}

// ---- ctor / dtor ------------------------------------------------------------

ActivationDialog::ActivationDialog() {
    memset(codeBuffer_, 0, sizeof(codeBuffer_));
}

ActivationDialog::~ActivationDialog() {
    // Must not destroy while worker is live — join it
    if (workerThread_.joinable()) workerThread_.join();
}

// ---- public API -------------------------------------------------------------

void ActivationDialog::Open() {
    pendingOpen_ = true;
    open_ = true;
    focusInputNextFrame_ = true;
}

void ActivationDialog::SetOnActivated(std::function<void(License::CachedLicense)> cb) {
    onActivated_ = std::move(cb);
}

void ActivationDialog::SetOnExit(std::function<void()> cb) {
    onExit_ = std::move(cb);
}

void ActivationDialog::SetStatus(const std::string& msg, bool is_error) {
    statusMsg_      = msg;
    statusIsError_  = is_error;
    busy_           = false;
}

void ActivationDialog::SetBusy(bool busy) {
    busy_ = busy;
    if (busy) statusMsg_ = "Đang kiểm tra...";
}

// ---- Render -----------------------------------------------------------------

void ActivationDialog::Render() {
    if (!open_) return;

    // Poll worker result before drawing this frame's UI
    pollWorkerResult();

    if (pendingOpen_) {
        ImGui::OpenPopup("Activation Required");
        pendingOpen_ = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    constexpr ImGuiWindowFlags kModalFlags =
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove           |
        ImGuiWindowFlags_NoCollapse;

    bool modalOpen = true;
    if (!ImGui::BeginPopupModal("Activation Required", &modalOpen, kModalFlags)) {
        pendingOpen_ = true;
        return;
    }

    const std::string shortId = License::HwidShort();

    // --- Machine ID row
    ImGui::TextUnformatted("Machine ID:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "%s", shortId.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy Full ID")) {
        copyFullIdToClipboard();
        SetStatus("Copied to clipboard.", false);
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
    ImGui::TextWrapped("Please send this ID to the author to get an activation code.");
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::Spacing();

    // --- Code input
    ImGui::TextUnformatted("Enter code:");

    if (busy_) ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(320.0f);
    // Auto-focus the input the first frame it appears (after popup opens).
    if (focusInputNextFrame_) {
        ImGui::SetKeyboardFocusHere();
        focusInputNextFrame_ = false;
    }
    bool enterPressed = ImGui::InputText("##code", codeBuffer_, sizeof(codeBuffer_),
        ImGuiInputTextFlags_EnterReturnsTrue);
    if (busy_) ImGui::EndDisabled();

    ImGui::Spacing();

    // --- Buttons
    if (busy_) ImGui::BeginDisabled();

    bool doActivate = false;
    if (ImGui::Button("Activate", ImVec2(100, 0)) || enterPressed)
        doActivate = true;

    ImGui::SameLine();

    if (ImGui::Button("Exit", ImVec2(60, 0))) {
        ImGui::CloseCurrentPopup();
        open_ = false;
        if (onExit_) onExit_();
    }

    if (busy_) ImGui::EndDisabled();

    // Xử lý activate sau render buttons
    if (doActivate && !busy_) {
        std::string code(codeBuffer_);
        auto first = code.find_first_not_of(" \t\r\n");
        auto last  = code.find_last_not_of(" \t\r\n");
        if (first != std::string::npos)
            code = code.substr(first, last - first + 1);

        if (code.empty()) {
            SetStatus("Please enter a code.", true);
        } else {
            SetBusy(true);
            startActivationWorker(code);
        }
    }

    // --- Status line
    if (!statusMsg_.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        if (statusIsError_)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
        else
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
        ImGui::TextWrapped("%s", statusMsg_.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::EndPopup();

    if (!modalOpen) {
        open_ = false;
        if (onExit_) onExit_();
    }
}
