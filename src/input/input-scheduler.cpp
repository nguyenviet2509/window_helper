#include "input-scheduler.h"
#include "../core/logger.h"

using clk = std::chrono::steady_clock;

InputScheduler::InputScheduler(IInputBackend& b, Humanizer& h, OutputGate& g)
    : backend_(b), human_(h), gate_(g) {}

InputScheduler::~InputScheduler() { stop(); }

void InputScheduler::schedule(InputCmd cmd) {
    cmd.seq = ++seqGen_;
    // Apply humanizer jitter for non-emergency priorities.
    auto jitter = human_.gaussianJitter(cmd.priority);
    if (jitter.count() > 0) cmd.fireAt += jitter;
    {
        std::lock_guard<std::mutex> g(mu_);
        q_.push(std::move(cmd));
    }
    cv_.notify_one();
}

void InputScheduler::start() {
    if (running_.exchange(true)) return;
    th_ = std::thread([this] { runLoop(); });
}

void InputScheduler::stop() {
    if (!running_.exchange(false)) return;
    cv_.notify_all();
    if (th_.joinable()) th_.join();
}

void InputScheduler::runLoop() {
    while (running_.load()) {
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait_for(lk, std::chrono::milliseconds(50),
                     [this] { return !running_.load() || !q_.empty(); });
        if (!running_.load()) return;
        if (q_.empty()) continue;

        auto now = clk::now();
        const InputCmd& top = q_.top();
        if (top.fireAt > now) {
            cv_.wait_until(lk, top.fireAt);
            continue;
        }

        InputCmd cmd = q_.top();
        q_.pop();
        lk.unlock();

        // Drop non-critical commands during break / session pause.
        if (cmd.priority > 2 && !cmd.bypassRefillGate) {
            if (human_.inBreakWindow() || human_.inSessionPause()) {
                gatedDrops_.fetch_add(1);
                continue;
            }
        }

        // Refill pause: drop mọi cmd bình thường khi gate.refillActive(),
        // cho phép cmd của PotRefillScheduler (bypassRefillGate=true) đi qua.
        if (gate_.refillActive() && !cmd.bypassRefillGate) {
            gatedDrops_.fetch_add(1);
            Logger::instance().logf(LogLevel::Debug,
                "[scheduler] DROPPED prio=P%d seq=%llu (refill active)",
                cmd.priority, (unsigned long long)cmd.seq);
            continue;
        }

        if (!gate_.allowInput()) {
            gatedDrops_.fetch_add(1);
            Logger::instance().logf(LogLevel::Info,
                "[scheduler] DROPPED prio=P%d seq=%llu (gate denied)",
                cmd.priority, (unsigned long long)cmd.seq);
            continue;
        }

        Logger::instance().logf(LogLevel::Info,
            "[scheduler] FIRING prio=P%d seq=%llu",
            cmd.priority, (unsigned long long)cmd.seq);
        if (cmd.action) cmd.action(backend_);
        fired_.fetch_add(1);
        human_.notifyActionFired(cmd.priority);
    }
}
