#ifndef CPP_OJ_VIBECODING_UTIL_LOG_H
#define CPP_OJ_VIBECODING_UTIL_LOG_H

#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace oj::util {

enum class LogLevel { Debug, Info, Warn, Error };

class LogStream {
public:
    LogStream(LogLevel level, const char* file, int line);
    ~LogStream();

    template <typename T>
    LogStream& operator<<(const T& v) {
        buf_ << v;
        return *this;
    }

private:
    LogLevel level_;
    const char* file_;
    int line_;
    std::ostringstream buf_;
};

void log_set_level(LogLevel level);
LogLevel log_get_level();
const char* level_name(LogLevel l);

#define OJ_LOG(level, ...) ::oj::util::LogStream(::oj::util::LogLevel::level, __FILE__, __LINE__) << __VA_ARGS__

#define OJ_DEBUG(...) OJ_LOG(Debug, __VA_ARGS__)
#define OJ_INFO(...)  OJ_LOG(Info,  __VA_ARGS__)
#define OJ_WARN(...)  OJ_LOG(Warn,  __VA_ARGS__)
#define OJ_ERROR(...) OJ_LOG(Error, __VA_ARGS__)

}  // namespace oj::util

#endif
