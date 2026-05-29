#pragma once
// ConfigBus: lock-free snapshot via std::atomic<std::shared_ptr<const AppConfig>>.
// Producer (UI thread) publishes; consumers (dispatcher, FSM) call snapshot() each tick.
#include <atomic>
#include <memory>
#include "../state/game-state.h"

class ConfigBus {
public:
    ConfigBus() : current_(std::make_shared<const AppConfig>()) {}

    void publish(std::shared_ptr<const AppConfig> c) {
        std::atomic_store(&current_, c);
    }
    std::shared_ptr<const AppConfig> snapshot() const {
        return std::atomic_load(&current_);
    }

private:
    std::shared_ptr<const AppConfig> current_;
};
