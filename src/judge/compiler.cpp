#include "judge/compiler.h"

#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>

namespace oj::judge {

namespace {

constexpr size_t kMaxStderrBytes = 64 * 1024;

}  // namespace

Compiler::Compiler(std::string gxx_path) : gxx_path_(std::move(gxx_path)) {}

CompileResult Compiler::compile(const std::string& src_path,
                                const std::string& out_path) const {
    CompileResult result;

    std::string cmd = gxx_path_ +
        " -O2 -std=c++17 -DONLINE_JUDGE -o " + out_path +
        " " + src_path + " 2>&1";

    std::unique_ptr<FILE, int(*)(FILE*)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) throw std::runtime_error("popen failed for compile");

    std::array<char, 4096> buf{};
    std::string out;
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe.get())) {
        out.append(buf.data());
        if (out.size() > kMaxStderrBytes) {
            out.resize(kMaxStderrBytes);
            out.append("\n[... truncated ...]\n");
            break;
        }
    }
    int status = pclose(pipe.release());
    if (status == -1) {
        throw std::runtime_error("pclose failed for compile");
    }
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    }
    result.stderr_output = std::move(out);
    result.success = (result.exit_code == 0);
    return result;
}

}  // namespace oj::judge
