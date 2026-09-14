#include "judge/judge.h"

#include <chrono>
#include <iostream>
#include <utility>

#include "db/mysql_client.h"
#include "judge/compiler.h"
#include "judge/diff.h"
#include "judge/runner.h"
#include "judge/tempdir.h"
#include "util/log.h"
#include "util/strutil.h"

namespace oj::judge {

std::mutex g_judge_mu;

Judge::Judge(db::MySQLClient& db, std::string gxx_path)
    : db_(db), gxx_path_(std::move(gxx_path)) {}

JudgeResult Judge::run(int problem_id, const std::string& code) {
    std::lock_guard<std::mutex> lk(g_judge_mu);
    JudgeResult jr;

    // 1. fetch problem + test cases
    auto prows = db_.query(
        "SELECT id, title, time_limit_ms, memory_limit_mb FROM problems "
        "WHERE id=" + std::to_string(problem_id));
    if (prows.empty()) {
        jr.status = JudgeStatus::WA;
        jr.message = "Problem not found";
        return jr;
    }
    Problem p;
    p.id = std::stoi(prows[0][0]);
    p.title = prows[0][1];
    p.time_limit_ms = std::stoi(prows[0][2]);
    p.memory_limit_mb = std::stoi(prows[0][3]);

    auto crows = db_.query(
        "SELECT ord, input, expected_output FROM test_cases "
        "WHERE problem_id=" + std::to_string(problem_id) +
        " ORDER BY ord ASC, id ASC");
    for (auto& r : crows) {
        TestCase tc;
        tc.ord = std::stoi(r[0]);
        tc.input = r[1];
        tc.expected_output = r[2];
        p.test_cases.push_back(std::move(tc));
    }
    if (p.test_cases.empty()) {
        jr.status = JudgeStatus::WA;
        jr.message = "No test cases";
        return jr;
    }

    // 2. write source
    TempDir tmp;
    const std::string src = tmp.path() + "/main.cpp";
    const std::string bin = tmp.path() + "/main.bin";
    TempDir::write_file(src, code);

    // 3. compile
    Compiler compiler(gxx_path_);
    auto cr = compiler.compile(src, bin);
    if (!cr.success) {
        jr.status = JudgeStatus::WA;
        jr.message = "Compile Error";
        jr.compile_error = cr.stderr_output;
        return jr;
    }

    // 4. run each test case
    for (const auto& tc : p.test_cases) {
        std::string in_path  = tmp.path() + "/in_"  + std::to_string(tc.ord) + ".txt";
        std::string out_path = tmp.path() + "/out_" + std::to_string(tc.ord) + ".txt";
        TempDir::write_file(in_path, tc.input);

        RunLimits lim;
        lim.time_limit_ms   = p.time_limit_ms;
        lim.memory_limit_mb = p.memory_limit_mb;
        lim.wall_timeout_ms = p.time_limit_ms + 3000;

        RunResult rr = Runner::run(bin, in_path, out_path, lim);
        if (rr.status == RunStatus::TimeLimitExceeded) {
            jr.status = JudgeStatus::WA;
            jr.message = "Time Limit Exceeded";
            jr.failed_case = tc.ord;
            return jr;
        }
        if (rr.status == RunStatus::MemoryLimitExceeded) {
            jr.status = JudgeStatus::WA;
            jr.message = "Memory Limit Exceeded";
            jr.failed_case = tc.ord;
            return jr;
        }
        if (rr.status == RunStatus::OutputLimitExceeded) {
            jr.status = JudgeStatus::WA;
            jr.message = "Output Limit Exceeded";
            jr.failed_case = tc.ord;
            return jr;
        }
        if (rr.status != RunStatus::Ok) {
            jr.status = JudgeStatus::WA;
            jr.message = "Runtime Error (exit=" + std::to_string(rr.exit_code) + ")";
            jr.failed_case = tc.ord;
            return jr;
        }

        std::string actual = TempDir::read_file(out_path);
        if (!Diff::compare(tc.expected_output, actual)) {
            jr.status = JudgeStatus::WA;
            jr.message = "Wrong Answer";
            jr.failed_case = tc.ord;
            return jr;
        }
    }

    jr.status = JudgeStatus::AC;
    jr.message = "Accepted";
    return jr;
}

}  // namespace oj::judge
