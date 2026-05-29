#include "logger.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <thread>
#include <sstream>

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

const char* Logger::levelStr(LogLevel l) {
    switch (l) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info:  return "INFO";
    case LogLevel::Warn:  return "WARN";
    case LogLevel::Error: return "ERROR";
    }
    return "?";
}

void Logger::open(const std::string& path, size_t maxBytes, int maxFiles) {
    std::lock_guard<std::mutex> g(mu_);
    path_ = path;
    maxBytes_ = maxBytes;
    maxFiles_ = maxFiles;
    std::filesystem::create_directories(std::filesystem::path(path_).parent_path());
    file_.open(path_, std::ios::app);
}

void Logger::close() {
    std::lock_guard<std::mutex> g(mu_);
    if (file_.is_open()) file_.close();
}

void Logger::rotateIfNeeded() {
    if (!file_.is_open()) return;
    file_.flush();
    std::error_code ec;
    auto sz = std::filesystem::file_size(path_, ec);
    if (ec || sz < maxBytes_) return;

    file_.close();
    for (int i = maxFiles_ - 1; i >= 1; --i) {
        auto src = path_ + "." + std::to_string(i);
        auto dst = path_ + "." + std::to_string(i + 1);
        std::filesystem::rename(src, dst, ec);
    }
    std::filesystem::rename(path_, path_ + ".1", ec);
    file_.open(path_, std::ios::app);
}

void Logger::log(LogLevel lvl, std::string_view msg) {
    if (static_cast<int>(lvl) < static_cast<int>(minLevel_)) return;
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::tm tm{};
    localtime_s(&tm, &t);

    char timeBuf[32];
    std::snprintf(timeBuf, sizeof(timeBuf),
                  "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec, (int)ms.count());

    std::ostringstream tidss; tidss << std::this_thread::get_id();

    std::lock_guard<std::mutex> g(mu_);
    if (!file_.is_open()) return;
    file_ << '[' << timeBuf << "][t" << tidss.str() << "][" << levelStr(lvl) << "] "
          << msg << '\n';
    rotateIfNeeded();
}

void Logger::logf(LogLevel lvl, const char* fmt, ...) {
    char buf[1024];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    log(lvl, buf);
}
