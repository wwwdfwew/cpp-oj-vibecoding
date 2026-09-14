#ifndef CPP_OJ_VIBECODING_JUDGE_RUNNER_H
#define CPP_OJ_VIBECODING_JUDGE_RUNNER_H

#include <cstdint>
#include <string>

namespace oj::judge {

struct RunLimits {
    int time_limit_ms    = 1000;
    int memory_limit_mb  = 128;
    int output_limit_mb  = 64;
    int wall_timeout_ms  = 0;  // 0 means auto = time_limit_ms * testcases + buffer
};

enum class RunStatus {
    Ok,
    TimeLimitExceeded,
    MemoryLimitExceeded,
    RuntimeError,
    OutputLimitExceeded,
    InternalError,
};

struct RunResult {
    RunStatus  status      = RunStatus::InternalError;
    int        exit_code   = -1;
    int        signal      = 0;
    long       time_ms     = 0;       // approximate CPU time used (rusage)
    long       mem_kb      = 0;       // peak RSS
    uint64_t   output_bytes = 0;      // size of stdout file
    std::string stdout_text;          // contents if not too large
};

class Runner {
public:
    static RunResult run(const std::string& bin_path,
                         const std::string& stdin_path,
                         const std::string& stdout_path,
                         const RunLimits& limits);
};

}  // namespace oj::judge

#endif
