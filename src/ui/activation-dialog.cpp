// activation-dialog.cpp — ImGui modal gate for license activation.
// No network logic — callbacks wired externally (phase 3).

#include "activation-dialog.h"
#include "../license/hwid-collector.h"

#include <windows.h>
#include "imgui.h"
#include <string>
#include <algorithm>

// ---- clipboard helper -------------------------------------------------------

void ActivationDialog::copyFullIdToClipboard() {
    std::string full = License::HwidFull();
    if (full.empty()) return;

    // Chuyển sang wstring để dùng CF_UNICODETEXT
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
        // hMem is now owned by the clipboard — do NOT free
    } else {
        GlobalFree(hMem);
    }
}

// ---- ctor -------------------------------------------------------------------

ActivationDialog::ActivationDialog() {
    memset(codeBuffer_, 0, sizeof(codeBuffer_));
}

// ---- public API -------------------------------------------------------------

void ActivationDialog::Open() {
    pendingOpen_ = true;
    open_ = true;
}

void ActivationDialog::SetOnActivate(std::function<void(std::string)> cb) {
    onActivate_ = std::move(cb);
}

void ActivationDialog::SetOnExit(std::function<void()> cb) {
    onExit_ = std::move(cb);
}

void ActivationDialog::SetStatus(const std::string& msg, bool is_error) {
    statusMsg_ = msg;
    statusIsError_ = is_error;
    busy_ = false; // clear busy when status arrives
}

void ActivationDialog::SetBusy(bool busy) {
    busy_ = busy;
    if (busy) statusMsg_ = "Đang kiểm tra...";
}

// ---- Render -----------------------------------------------------------------

void ActivationDialog::Render() {
    if (!open_) return;

    // OpenPopup phải được gọi cùng frame với BeginPopupModal để ImGui nhận ra.
    if (pendingOpen_) {
        ImGui::OpenPopup("Activation Required");
        pendingOpen_ = false;
    }

    // Căn giữa modal trên viewport
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    constexpr ImGuiWindowFlags kModalFlags =
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove           |
        ImGuiWindowFlags_NoCollapse;

    bool modalOpen = true;
    if (!ImGui::BeginPopupModal("Activation Required", &modalOpen, kModalFlags)) {
        // Popup chưa hiện (first frame race) — thử lại frame sau
        pendingOpen_ = true;
        return;
    }

    // Lấy short + full HWID một lần — hàm đã cache nội bộ
    const std::string shortId = License::HwidShort();

    // --- Machine ID row ------------------------------------------------------
    ImGui::TextUnformatted("Machine ID:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "%s", shortId.c_str());
    ImGui::SameLine();

    // Nút Copy Full ID — copy 64-hex vào clipboard để user gửi cho admin
    if (ImGui::SmallButton("Copy Full ID")) {
        copyFullIdToClipboard();
        SetStatus("Copied to clipboard.", false);
    }

    // --- Instruction text ----------------------------------------------------
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
    ImGui::TextWrapped("Please send this ID to the author to get an activation code.");
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::Spacing();

    // --- Code input ----------------------------------------------------------
    ImGui::TextUnformatted("Enter code:");
    ImGui::SameLine();

    // Disable input khi đang busy
    if (busy_) ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(220.0f);
    // EnterReturnsTrue: nhấn Enter cũng trigger activate
    bool enterPressed = ImGui::InputText("##code", codeBuffer_, sizeof(codeBuffer_),
        ImGuiInputTextFlags_EnterReturnsTrue);
    if (busy_) ImGui::EndDisabled();

    ImGui::Spacing();

    // --- Buttons -------------------------------------------------------------
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

    // Xử lý activate sau khi đã render hết buttons (tránh mutate state giữa frame)
    if (doActivate && !busy_) {
        // Trim leading/trailing spaces
        std::string code(codeBuffer_);
        auto first = code.find_first_not_of(" \t\r\n");
        auto last  = code.find_last_not_of(" \t\r\n");
        if (first != std::string::npos)
            code = code.substr(first, last - first + 1);

        if (code.empty()) {
            SetStatus("Please enter a code.", true);
        } else {
            SetBusy(true);
            if (onActivate_) onActivate_(code);
        }
    }

    // --- Status line ---------------------------------------------------------
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

    // Nếu user đóng popup bằng nút X (modalOpen menjadi false)
    if (!modalOpen) {
        open_ = false;
        if (onExit_) onExit_();
    }
}
