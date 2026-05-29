#pragma once
// Global hotkey wrapper around RegisterHotKey. One handler per ID.
// Owner window must call onMessage(wp) inside WM_HOTKEY.
#include <windows.h>
#include <functional>
#include <unordered_map>

class HotkeyManager {
public:
    bool registerKey(HWND owner, int id, UINT mod, UINT vk, std::function<void()> handler) {
        if (!RegisterHotKey(owner, id, mod, vk)) return false;
        owners_[id] = owner;
        handlers_[id] = std::move(handler);
        return true;
    }
    void unregisterAll() {
        for (auto& kv : owners_) UnregisterHotKey(kv.second, kv.first);
        owners_.clear();
        handlers_.clear();
    }
    void onMessage(WPARAM wp) {
        auto it = handlers_.find(static_cast<int>(wp));
        if (it != handlers_.end() && it->second) it->second();
    }
private:
    std::unordered_map<int, HWND> owners_;
    std::unordered_map<int, std::function<void()>> handlers_;
};
