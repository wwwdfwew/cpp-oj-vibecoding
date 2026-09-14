#ifndef CPP_OJ_VIBECODING_JUDGE_JUDGE_H
#define CPP_OJ_VIBECODING_JUDGE_JUDGE_H

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace oj::db { class MySQLClient; }

namespace oj::judge {

struct TestCase {
    int         ord = 0;
    std::string input;
    std::string expected_output;
};

struct Problem {
    int         id = 0;
    std::string title;
    int         time_limit_ms = 1000;
    int         memory_limit_mb = 128;
    std::vector<TestCase> test_cases;
};

enum class JudgeStatus { AC, WA };

struct JudgeResult {
    JudgeStatus status = JudgeStatus::WA;
    std::string message;          // "Wrong Answer" | "Time Limit Exceeded" | ...
    std::string compile_error;    // populated on compile failure
    int         failed_case = -1; // -1 if all passed
};

// Global mutex — serializes judge to keep sandbox resource stable (SPEC §2.5).
extern std::mutex g_judge_mu;

class Judge {
public:
    explicit Judge(db::MySQLClient& db, std::string gxx_path = "/usr/bin/g++");

    JudgeResult run(int problem_id, const std::string& code);

private:
    db::MySQLClient& db_;
    std::string gxx_path_;
};

}  // namespace oj::judge

#endif
