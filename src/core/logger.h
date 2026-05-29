#pragma once
// Minimal thread-safe rolling logger. Format:
//   [yyyy-MM-dd HH:mm:ss.SSS][tid][LEVEL] message
// Rotates when file exceeds maxBytes_ (default 5 MB), keeps maxFiles_ backups.
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

enum class LogLevel { Trace, Debug, Info, Warn, Error };

class Logger {
public:
    static Logger& instance();

    void open(const std::string& path, size_t maxBytes = 5 * 1024 * 1024, int maxFiles = 5);
    void close();
    void setMinLevel(LogLevel l) { minLevel_ = l; }

    void log(LogLevel lvl, std::string_view msg);
    void logf(LogLevel lvl, const char* fmt, ...);

private:
    Logger() = default;
    void rotateIfNeeded();
    static const char* levelStr(LogLevel l);

    std::mutex mu_;
    std::ofstream file_;
    std::string path_;
    size_t maxBytes_ = 5 * 1024 * 1024;
    int maxFiles_ = 5;
    LogLevel minLevel_ = LogLevel::Info;
};

#define LOG_INFO(msg)  Logger::instance().log(LogLevel::Info,  (msg))
#define LOG_WARN(msg)  Logger::instance().log(LogLevel::Warn,  (msg))
#define LOG_ERROR(msg) Logger::instance().log(LogLevel::Error, (msg))
