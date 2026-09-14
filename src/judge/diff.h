#ifndef CPP_OJ_VIBECODING_JUDGE_DIFF_H
#define CPP_OJ_VIBECODING_JUDGE_DIFF_H

#include <string>

namespace oj::judge {

class Diff {
public:
    // Compare expected vs actual.
    // Rules: strip '\r', ignore trailing blank lines, exact compare per line.
    static bool compare(const std::string& expected,
                        const std::string& actual);

    // Normalize: strip '\r', trim trailing blank lines.
    static std::string normalize(const std::string& s);
};

}  // namespace oj::judge

#endif
