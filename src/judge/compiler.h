#ifndef CPP_OJ_VIBECODING_JUDGE_COMPILER_H
#define CPP_OJ_VIBECODING_JUDGE_COMPILER_H

#include <string>

namespace oj::judge {

struct CompileResult {
    bool        success = false;
    int         exit_code = -1;
    std::string stderr_output;
};

class Compiler {
public:
    explicit Compiler(std::string gxx_path = "/usr/bin/g++");

    CompileResult compile(const std::string& src_path,
                          const std::string& out_path) const;

private:
    std::string gxx_path_;
};

}  // namespace oj::judge

#endif
