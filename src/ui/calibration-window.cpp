#include "calibration-window.h"
#include "imgui.h"
#include "../core/logger.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <windows.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

static fs::path presetsDir() {
    // Resolve cạnh exe để consistent với config.json (xem main.cpp:96-105).
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return fs::path(buf).parent_path() / "presets";
}

static json barToJson(const VisionBarCfg& b) {
    json hues = json::array();
    for (const auto& h : b.hues) hues.push_back(json{ {"lo", h.hMin}, {"hi", h.hMax} });
    return json{
        {"region", { {"x", b.region.x}, {"y", b.region.y},
                     {"w", b.region.w}, {"h", b.region.h},
                     {"shape", b.region.shape}, {"radius", b.region.radius} }},
        {"hues", hues},
    };
}

static void jsonToBar(const json& j, VisionBarCfg& b) {
    if (j.contains("region")) {
        const auto& r = j["region"];
        if (r.contains("x")) b.region.x = r["x"];
        if (r.contains("y")) b.region.y = r["y"];
        if (r.contains("w")) b.region.w = r["w"];
        if (r.contains("h")) b.region.h = r["h"];
        if (r.contains("shape")) b.region.shape = r["shape"].get<std::string>();
        if (r.contains("radius")) b.region.radius = r["radius"];
    }
    if (j.contains("hues") && j["hues"].is_array()) {
        b.hues.clear();
        for (const auto& hj : j["hues"]) {
            HueRange h{};
            if (hj.contains("lo")) h.hMin = hj["lo"];
            if (hj.contains("hi")) h.hMax = hj["hi"];
            b.hues.push_back(h);
        }
    }
}

void CalibrationWindow::open() {
    open_ = true;
    // Auto-load danh sách preset mỗi lần mở. Tránh để dropdown rỗng cho user
    // lần đầu vào — bug đã gặp: tưởng preset mất sau khi restart tool.
    refreshPresetList();
}

bool CalibrationWindow::loadPresetByName(const std::string& name, VisionConfig& v) {
    if (!loadPreset(name, v)) {
        lastStatus_ = "Nạp cấu hình thất bại: " + name;
        return false;
    }
    currentPresetName_ = name;
    lastStatus_ = "Đã nạp cấu hình: " + name;
    // Sync presetSelected_ cho dropdown bên trong calibration cũng phản ánh.
    for (size_t i = 0; i < presetFiles_.size(); ++i) {
        if (presetFiles_[i] == name) { presetSelected_ = static_cast<int>(i); break; }
    }
    return true;
}

void CalibrationWindow::refreshPresetList() {
    presetFiles_.clear();
    presetSelected_ = -1;
    auto dir = presetsDir();
    std::error_code ec;
    if (!fs::exists(dir, ec)) return;
    for (auto& e : fs::directory_iterator(dir, ec)) {
        if (e.is_regular_file() && e.path().extension() == ".json") {
            presetFiles_.push_back(e.path().stem().string());
        }
    }
    std::sort(presetFiles_.begin(), presetFiles_.end());
}

bool CalibrationWindow::savePreset(const std::string& name, const VisionConfig& v) const {
    auto dir = presetsDir();
    std::error_code ec;
    fs::create_directories(dir, ec);
    json j;
    j["frameWidth"]  = v.frameWidth;
    j["frameHeight"] = v.frameHeight;
    j["hp"] = barToJson(v.hp);
    j["sp"] = barToJson(v.sp);
    j["mp"] = barToJson(v.mp);
    auto path = dir / (name + ".json");
    std::ofstream f(path);
    if (!f.good()) return false;
    f << j.dump(2);
    return true;
}

bool CalibrationWindow::deletePreset(const std::string& name) const {
    auto path = presetsDir() / (name + ".json");
    std::error_code ec;
    return fs::remove(path, ec) && !ec;
}

bool CalibrationWindow::loadPreset(const std::string& name, VisionConfig& v) const {
    auto path = presetsDir() / (name + ".json");
    std::ifstream f(path);
    if (!f.good()) return false;
    json j;
    try { f >> j; } catch (...) { return false; }
    if (j.contains("frameWidth"))  v.frameWidth  = j["frameWidth"];
    if (j.contains("frameHeight")) v.frameHeight = j["frameHeight"];
    if (j.contains("hp")) jsonToBar(j["hp"], v.hp);
    if (j.contains("sp")) jsonToBar(j["sp"], v.sp);
    if (j.contains("mp")) jsonToBar(j["mp"], v.mp);
    return true;
}

void CalibrationWindow::drawBarSection(const char* label, VisionBarCfg& bar,
                                       double livePct, std::function<void()> onChanged) {
    ImGui::PushID(label);
    if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextColored(ImVec4(0.1f, 0.7f, 0.1f, 1.0f),
                           "Nhận diện hiện tại: %.1f%%", livePct * 100.0);
        ImGui::TextDisabled("Vùng quét trên ảnh game (px):");
        bool dirty = false;
        dirty |= ImGui::DragInt("Tọa độ X", &bar.region.x, 1.0f, 0, 8192);
        dirty |= ImGui::DragInt("Tọa độ Y", &bar.region.y, 1.0f, 0, 8192);
        dirty |= ImGui::DragInt("Chiều rộng", &bar.region.w, 1.0f, 1, 8192);
        dirty |= ImGui::DragInt("Chiều cao",  &bar.region.h, 1.0f, 1, 8192);
        if (dirty && onChanged) onChanged();
    }
    ImGui::PopID();
}

void CalibrationWindow::draw(VisionConfig& vision, std::function<void()> onChanged) {
    if (!open_) return;
    // Mỗi lần mở: position (10,10), width = displayW-20, height = 80% displayH.
    // ImGuiCond_Appearing → reset về default mỗi lần becoming visible, nhưng
    // trong cùng session user vẫn drag/resize được (Appearing chỉ apply lần đầu
    // sau khi open() chuyển từ false → true).
    auto display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(
        ImVec2(std::max(200.0f, display.x - 20.0f), display.y * 0.80f),
        ImGuiCond_Appearing);
    if (!ImGui::Begin("Hiệu chỉnh nhận diện HP/MP/SP", &open_,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        ImGui::End();
        return;
    }

    // Đồng bộ kích thước ô nhập với main settings panel (110px) cho gọn.
    ImGui::PushItemWidth(110.0f);

    // Phần Cấu hình lưu sẵn (Preset).
    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "Cấu hình lưu sẵn");
    if (!lastStatus_.empty()) {
        ImGui::TextColored(ImVec4(0.1f, 0.6f, 0.9f, 1.0f),
                           "Trạng thái: %s", lastStatus_.c_str());
    }
    if (ImGui::Button("Tải lại danh sách")) refreshPresetList();
    ImGui::SameLine();
    const char* currentPreset = (presetSelected_ >= 0 &&
                                 presetSelected_ < (int)presetFiles_.size())
                                ? presetFiles_[presetSelected_].c_str() : "(chọn để nạp)";
    if (ImGui::BeginCombo("Chọn cấu hình", currentPreset)) {
        for (int i = 0; i < (int)presetFiles_.size(); ++i) {
            bool sel = (i == presetSelected_);
            if (ImGui::Selectable(presetFiles_[i].c_str(), sel)) {
                presetSelected_ = i;
                if (loadPreset(presetFiles_[i], vision)) {
                    currentPresetName_ = presetFiles_[i];
                    lastStatus_ = "Đã nạp cấu hình: " + presetFiles_[i];
                    if (onChanged) onChanged();
                } else {
                    lastStatus_ = "Nạp cấu hình thất bại: " + presetFiles_[i];
                }
            }
        }
        ImGui::EndCombo();
    }

    ImGui::InputText("Tên server / ghi chú", presetName_, sizeof(presetName_));
    if (ImGui::Button("Lưu thành cấu hình")) {
        if (savePreset(presetName_, vision)) {
            currentPresetName_ = presetName_;
            lastStatus_ = std::string("Đã lưu cấu hình: ") + presetName_;
            refreshPresetList();
        } else {
            lastStatus_ = std::string("Lưu cấu hình thất bại: ") + presetName_;
        }
        confirmingDelete_ = false;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
        "Lưu vùng quét hiện tại vào thư mục \"presets\\\" cạnh exe.\n"
        "Lần sau mở tool, chọn lại từ danh sách để dùng nhanh.");

    // Xóa cấu hình — 2 bước confirm để tránh lỡ tay.
    bool hasSelection = (presetSelected_ >= 0 &&
                        presetSelected_ < (int)presetFiles_.size());
    if (hasSelection) {
        ImGui::SameLine();
        const std::string& selName = presetFiles_[presetSelected_];
        if (!confirmingDelete_) {
            if (ImGui::Button("Xóa cấu hình đang chọn")) {
                confirmingDelete_ = true;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "Xóa file preset đã chọn ở dropdown \"Chọn cấu hình\".");
        } else {
            ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f),
                "Xóa \"%s\"?", selName.c_str());
            ImGui::SameLine();
            if (ImGui::Button("Xác nhận xóa")) {
                if (deletePreset(selName)) {
                    lastStatus_ = std::string("Đã xóa cấu hình: ") + selName;
                    if (currentPresetName_ == selName) currentPresetName_.clear();
                } else {
                    lastStatus_ = std::string("Xóa thất bại (file đang dùng?): ") + selName;
                }
                refreshPresetList();
                confirmingDelete_ = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Hủy")) {
                confirmingDelete_ = false;
            }
        }
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f),
                       "Kích thước ảnh game tool quét được");
    bool dirtyFrame = false;
    dirtyFrame |= ImGui::DragInt("Chiều rộng khung",  &vision.frameWidth,  1.0f, 1, 8192);
    dirtyFrame |= ImGui::DragInt("Chiều cao khung",   &vision.frameHeight, 1.0f, 1, 8192);
    if (dirtyFrame && onChanged) onChanged();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
        "Đây là kích thước ảnh tool capture được từ cửa sổ game (tham khảo). "
        "Tool tự lấy theo cửa sổ — chỉnh ở đây không thay đổi capture, chỉ ghi chú.");
    ImGui::Separator();

    drawBarSection("Thanh HP (máu)",   vision.hp, liveHp_, onChanged);
    drawBarSection("Thanh SP (thể lực)", vision.sp, liveSp_, onChanged);
    drawBarSection("Thanh MP (mana)",  vision.mp, liveMp_, onChanged);

    ImGui::PopItemWidth();
    ImGui::End();
}
