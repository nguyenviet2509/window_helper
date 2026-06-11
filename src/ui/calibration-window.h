#pragma once
// Calibration UI: numeric ROI inputs + hue auto-sampler + preset save/load cho
// VisionConfig. Mở qua button trong main window. Edit trực tiếp draft_.vision;
// onChanged() fire để main-window debounce-flush + bus.publish.
#include <functional>
#include <string>
#include <vector>
#include "../state/game-state.h"

class CalibrationWindow {
public:
    void setLatestVision(double hp, double mp, double sp) {
        liveHp_ = hp; liveMp_ = mp; liveSp_ = sp;
    }
    // Tên preset đang dùng (sau khi user load hoặc save). Rỗng nếu chưa chọn —
    // tức là tool dùng giá trị thẳng trong config.json (không có preset).
    const std::string& currentPresetName() const { return currentPresetName_; }

    // Cho phép main window cũng nạp preset (dropdown gọn ngoài), không cần mở
    // calibration window. Cập nhật currentPresetName_ + return true nếu OK.
    const std::vector<std::string>& presets() const { return presetFiles_; }
    void refreshPresets() { refreshPresetList(); }
    bool loadPresetByName(const std::string& name, VisionConfig& v);
    void open();
    void close() { open_ = false; }
    bool isOpen() const { return open_; }

    // Vẽ window (gọi từ MainWindow::renderFrame). Modifies vision in-place.
    // onChanged: gọi sau mỗi edit để main-window mark dirty + flush sau debounce.
    void draw(VisionConfig& vision, std::function<void()> onChanged);

private:
    void drawBarSection(const char* label, VisionBarCfg& bar, double livePct,
                        std::function<void()> onChanged);
    void refreshPresetList();
    bool savePreset(const std::string& name, const VisionConfig& v) const;
    bool loadPreset(const std::string& name, VisionConfig& v) const;
    bool deletePreset(const std::string& name) const;

    bool open_ = false;
    double liveHp_ = 0, liveMp_ = 0, liveSp_ = 0;

    char presetName_[64] = "server-x";
    std::vector<std::string> presetFiles_;
    int presetSelected_ = -1;
    std::string lastStatus_;
    std::string currentPresetName_;
    bool confirmingDelete_ = false;
};
