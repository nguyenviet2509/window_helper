#pragma once
// InputScheduler: priority queue of input commands, single execution slot.
// Applies Humanizer jitter and OutputGate before dispatching to IInputBackend.
// Lower priority number = higher urgency (P0 = emergency HP, P4 = buff).
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include "i-input-backend.h"
#include "../core/humanizer.h"
#include "../core/output-gate.h"

struct InputCmd {
    int priority = 4;
    std::chrono::steady_clock::time_point fireAt{};
    std::function<void(IInputBackend&)> action;
    uint64_t seq = 0;

    bool operator<(const InputCmd& o) const {
        // priority_queue is max-heap; lower priority value or earlier time wins.
        if (priority != o.priority) return priority > o.priority;
        if (fireAt != o.fireAt) return fireAt > o.fireAt;
        return seq > o.seq;
    }
};

class InputScheduler {
public:
    InputScheduler(IInputBackend& backend, Humanizer& human, OutputGate& gate);
    ~InputScheduler();

    void schedule(InputCmd cmd);
    void start();
    void stop();

    // Stats / debug.
    int gatedDropCount() const { return gatedDrops_.load(); }
    int firedCount() const     { return fired_.load(); }

private:
    void runLoop();

    IInputBackend& backend_;
    Humanizer& human_;
    OutputGate& gate_;

    std::priority_queue<InputCmd> q_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> seqGen_{0};
    std::atomic<int> gatedDrops_{0};
    std::atomic<int> fired_{0};
    std::thread th_;
};
