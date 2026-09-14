#include "util/log.h"

#include <chrono>
#include <ctime>
#include <iomanip>

namespace oj::util {

namespace {
std::mutex g_log_mu;
LogLevel g_log_level = LogLevel::Info;

const char* level_str(LogLevel l) {
    switch (l) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
    }
    return "?";
}
}  // namespace

LogStream::LogStream(LogLevel level, const char* file, int line)
    : level_(level), file_(file), line_(line) {}

LogStream::~LogStream() {
    if (level_ < g_log_level) return;
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    char ts[32];
    std::tm tm{};
    localtime_r(&t, &tm);
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

    std::lock_guard<std::mutex> lk(g_log_mu);
    std::cerr << '[' << ts << "] [" << level_str(level_) << "] "
              << '(' << file_ << ':' << line_ << ") " << buf_.str() << '\n';
}

void log_set_level(LogLevel level) { g_log_level = level; }
LogLevel log_get_level() { return g_log_level; }
const char* level_name(LogLevel l) { return level_str(l); }

}  // namespace oj::util
