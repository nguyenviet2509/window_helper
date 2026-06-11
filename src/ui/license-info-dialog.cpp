// LicenseInfoDialog — read-only license status popup (ImGui modal).
// Vietnamese labels, vi-VN date format, "Copy" Machine ID button.

#include "license-info-dialog.h"

#include <windows.h>
#include <chrono>
#include <ctime>
#include <cstring>
#include <sstream>
#include <iomanip>

#include "imgui.h"

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

std::string LicenseInfoDialog::maskToken(const std::string& token) {
    if (token.size() < 8) return "****";
    return token.substr(0, 4) + "****" + token.substr(token.size() - 4);
}

std::string LicenseInfoDialog::formatDateVN(int64_t unixSec) {
    if (unixSec == 0) return "Vĩnh viễn";
    std::time_t t = static_cast<std::time_t>(unixSec);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[64];
    // DD/MM/YYYY HH:MM — Vietnamese date convention
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d",
             tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
             tm.tm_hour, tm.tm_min);
    return buf;
}

std::string LicenseInfoDialog::formatGraceRemaining(int64_t lastVerified, int32_t graceHours) {
    if (lastVerified == 0) return "Chưa xác thực";
    int64_t now = static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    int64_t graceSeconds = static_cast<int64_t>(graceHours) * 3600;
    int64_t elapsed = now - lastVerified;
    int64_t remaining = graceSeconds - elapsed;
    if (remaining <= 0) return "Hết grace — cần online";
    int h = static_cast<int>(remaining / 3600);
    int m = static_cast<int>((remaining % 3600) / 60);
    char buf[32];
    snprintf(buf, sizeof(buf), "%dh %dm", h, m);
    return buf;
}

void LicenseInfoDialog::copyToClipboard(const std::string& text) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    // Convert UTF-8 to UTF-16 for CF_UNICODETEXT
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(wlen) * sizeof(wchar_t));
    if (hMem) {
        wchar_t* dst = static_cast<wchar_t*>(GlobalLock(hMem));
        if (dst) {
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, dst, wlen);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
    }
    CloseClipboard();
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

LicenseInfoDialog::LicenseInfoDialog(License::LicenseManager* mgr)
    : mgr_(mgr) {}

// ---------------------------------------------------------------------------
// Open — trigger modal on next Render() call
// ---------------------------------------------------------------------------

void LicenseInfoDialog::Open() {
    pendingOpen_ = true;
}

// ---------------------------------------------------------------------------
// Render — call every frame inside an ImGui frame
// ---------------------------------------------------------------------------

void LicenseInfoDialog::Render() {
    if (pendingOpen_) {
        ImGui::OpenPopup("Thông tin License");
        pendingOpen_ = false;
        open_ = true;
    }
    if (!open_) return;

    // Snapshot once per frame — thread-safe copy, no lock held during rendering (C1).
    const License::CachedLicense lic = mgr_->Snapshot();

    // Center the popup on first use
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

    bool modalOpen = true;
    if (ImGui::BeginPopupModal("Thông tin License", &modalOpen,
                               ImGuiWindowFlags_AlwaysAutoResize)) {

        ImGui::TextUnformatted("Chi tiết kích hoạt:");
        ImGui::Separator();

        // Token (masked)
        ImGui::TextUnformatted("Token:");
        ImGui::SameLine(130);
        ImGui::TextUnformatted(maskToken(lic.token).c_str());

        // Machine ID with Copy button
        // machine_id is the 64-hex full HWID; display first 8 chars (short form)
        std::string shortId = lic.machine_id.size() >= 8
                              ? lic.machine_id.substr(0, 8)
                              : lic.machine_id;
        ImGui::TextUnformatted("Machine ID:");
        ImGui::SameLine(130);
        ImGui::TextUnformatted(shortId.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Copy")) {
            copyToClipboard(shortId);
            copiedFlash_ = true;
            copiedTimer_ = 2.0f;  // show "Đã copy!" for 2 seconds
        }
        if (copiedFlash_) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.1f, 0.7f, 0.1f, 1.0f), "Da copy!");
        }

        // Expiry date
        ImGui::TextUnformatted("Hết hạn:");
        ImGui::SameLine(130);
        ImGui::TextUnformatted(formatDateVN(lic.expires_at).c_str());

        // Last verified
        ImGui::TextUnformatted("Kiểm tra cuối:");
        ImGui::SameLine(130);
        ImGui::TextUnformatted(formatDateVN(lic.last_verified).c_str());

        // Grace remaining
        ImGui::TextUnformatted("Grace còn lại:");
        ImGui::SameLine(130);
        std::string grace = formatGraceRemaining(lic.last_verified, lic.grace_hours);
        bool graceWarn = (grace == "Hết grace — cần online");
        if (graceWarn) {
            ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.1f, 1.0f), "%s", grace.c_str());
        } else {
            ImGui::TextUnformatted(grace.c_str());
        }

        ImGui::Separator();

        // Tick down copy flash timer
        if (copiedFlash_) {
            copiedTimer_ -= ImGui::GetIO().DeltaTime;
            if (copiedTimer_ <= 0.0f) {
                copiedFlash_ = false;
            }
        }

        // Close button
        float buttonW = 80.0f;
        float avail = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - buttonW) * 0.5f);
        if (ImGui::Button("Đóng", ImVec2(buttonW, 0))) {
            ImGui::CloseCurrentPopup();
            open_ = false;
        }

        ImGui::EndPopup();
    }

    // Modal closed via X button
    if (!modalOpen) open_ = false;
}
