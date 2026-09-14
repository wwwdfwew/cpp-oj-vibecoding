#include "judge/diff.h"

#include <algorithm>
#include <sstream>
#include <vector>

namespace oj::judge {

namespace {

std::vector<std::string> split_lines_keep(const std::string& s) {
    std::vector<std::string> lines;
    std::string cur;
    for (char c : s) {
        if (c == '\n') {
            lines.push_back(std::move(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) lines.push_back(std::move(cur));
    return lines;
}

}  // namespace

std::string Diff::normalize(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\r') continue;
        out.push_back(c);
    }
    auto lines = split_lines_keep(out);
    while (!lines.empty() && lines.back().empty()) lines.pop_back();
    std::string joined;
    for (size_t i = 0; i < lines.size(); ++i) {
        joined.append(lines[i]);
        joined.push_back('\n');
    }
    return joined;
}

bool Diff::compare(const std::string& expected, const std::string& actual) {
    return normalize(expected) == normalize(actual);
}

}  // namespace oj::judge
