#ifndef CPP_OJ_VIBECODING_UTIL_STRUTIL_H
#define CPP_OJ_VIBECODING_UTIL_STRUTIL_H

#include <string>
#include <string_view>
#include <vector>

namespace oj::util {

std::string trim(std::string_view s);
std::vector<std::string> split(std::string_view s, char delim);
std::string join(const std::vector<std::string>& parts, std::string_view sep);
bool starts_with(std::string_view s, std::string_view prefix);
bool ends_with(std::string_view s, std::string_view suffix);
std::string url_decode(std::string_view s);

}  // namespace oj::util

#endif
