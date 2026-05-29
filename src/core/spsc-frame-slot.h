#pragma once
// SPSC 1-slot overwrite buffer. Producer always overwrites; consumer reads latest.
// Used to bridge WGC frame-arrived callback thread -> vision worker.
#include <condition_variable>
#include <mutex>
#include <optional>
#include <utility>
#include <chrono>

template <class T>
class SpscFrameSlot {
public:
    void push(T v) {
        {
            std::lock_guard<std::mutex> g(m_);
            slot_ = std::move(v);
        }
        cv_.notify_one();
    }

    bool pop(T& out, int timeoutMs) {
        std::unique_lock<std::mutex> g(m_);
        if (!cv_.wait_for(g, std::chrono::milliseconds(timeoutMs),
                          [&] { return slot_.has_value() || stopped_; }))
            return false;
        if (!slot_.has_value()) return false;
        out = std::move(*slot_);
        slot_.reset();
        return true;
    }

    void stop() {
        {
            std::lock_guard<std::mutex> g(m_);
            stopped_ = true;
        }
        cv_.notify_all();
    }

private:
    std::mutex m_;
    std::condition_variable cv_;
    std::optional<T> slot_;
    bool stopped_ = false;
};
