#ifndef CPP_OJ_VIBECODING_JUDGE_DIFF_H
#define CPP_OJ_VIBECODING_JUDGE_DIFF_H

#include <string>

namespace oj::judge {

class Diff {
public:
    // 比较期望输出与实际输出。
// 规则:去除 '\r',忽略末尾空行,逐行精确比较。
    static bool compare(const std::string& expected,
                        const std::string& actual);

    // 规范化:去除 '\r',并去掉末尾的空行。
    static std::string normalize(const std::string& s);
};

}  // namespace oj::judge

#endif
