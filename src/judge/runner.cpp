#include "judge/runner.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <stdexcept>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util/log.h"

namespace oj::judge {

namespace {

void set_limit(int resource, rlim_t value) {
    struct rlimit rl{};
    rl.rlim_cur = value;
    rl.rlim_max = value;
    if (setrlimit(resource, &rl) != 0) {
        OJ_WARN("setrlimit " << resource << "=" << value
                 << " failed: " << std::strerror(errno));
    }
}

}  // namespace

RunResult Runner::run(const std::string& bin_path,
                      const std::string& stdin_path,
                      const std::string& stdout_path,
                      const RunLimits& limits) {
    RunResult result;

    int in_fd = open(stdin_path.c_str(), O_RDONLY);
    if (in_fd < 0) {
        result.status = RunStatus::InternalError;
        return result;
    }
    int out_fd = open(stdout_path.c_str(),
                      O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (out_fd < 0) {
        close(in_fd);
        result.status = RunStatus::InternalError;
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(in_fd);
        close(out_fd);
        result.status = RunStatus::InternalError;
        return result;
    }

    if (pid == 0) {
        // child
        // apply resource limits
        rlim_t cpu_seconds = static_cast<rlim_t>(limits.time_limit_ms / 1000) + 1;
        set_limit(RLIMIT_CPU, cpu_seconds);
        // Heap-only memory cap. RLIMIT_AS would also constrain the binary +
        // shared library mappings (libc/libstdc++/libm are typically > 16 MB
        // on a glibc system) and a streaming solution that doesn't touch the
        // heap would still fail to start.
        set_limit(RLIMIT_DATA,
                  static_cast<rlim_t>(limits.memory_limit_mb) * 1024 * 1024);
        // As a backstop, also cap RLIMIT_AS at a generous multiple of the
        // heap budget so that an mmap-based allocator (large allocations use
        // mmap, not brk) still trips before it can swap the host to death.
        set_limit(RLIMIT_AS,
                  static_cast<rlim_t>(limits.memory_limit_mb) * 8 * 1024 * 1024);
        set_limit(RLIMIT_FSIZE,
                  static_cast<rlim_t>(limits.output_limit_mb) * 1024 * 1024);
        set_limit(RLIMIT_NPROC, 1);

        if (dup2(in_fd, STDIN_FILENO) < 0) _exit(127);
        if (dup2(out_fd, STDOUT_FILENO) < 0) _exit(127);
        // Silence the child's stderr so std::bad_alloc / SIGABRT messages
        // (deliberately triggered by MLE / RE test cases) don't leak into
        // the server log. The runner still surfaces the runtime status.
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }
        close(in_fd);
        close(out_fd);

        // new session so any stray signals stay in this group
        setsid();

        char* const argv[] = {
            const_cast<char*>("main.bin"),
            nullptr
        };
        execve(bin_path.c_str(), argv, nullptr);
        _exit(127);
    }

    // parent
    close(in_fd);
    close(out_fd);

    int wall_ms = limits.wall_timeout_ms > 0
                    ? limits.wall_timeout_ms
                    : limits.time_limit_ms + 2000;

    struct timespec start_ts{};
    clock_gettime(CLOCK_MONOTONIC, &start_ts);

    int status = 0;
    struct rusage ru{};
    bool got_status = false;
    while (true) {
        struct timespec now_ts{};
        clock_gettime(CLOCK_MONOTONIC, &now_ts);
        long elapsed_ms = (now_ts.tv_sec - start_ts.tv_sec) * 1000L +
                          (now_ts.tv_nsec - start_ts.tv_nsec) / 1000000L;
        if (elapsed_ms > wall_ms) {
            kill(pid, SIGKILL);
            wait4(pid, &status, 0, &ru);
            got_status = true;
            result.status = RunStatus::TimeLimitExceeded;
            break;
        }
        pid_t r = wait4(pid, &status, WNOHANG, &ru);
        if (r == pid) { got_status = true; break; }
        if (r < 0) {
            if (errno == ECHILD) {
                // already reaped (e.g. SIGCHLD race); continue polling.
                usleep(5000);
                continue;
            }
            result.status = RunStatus::InternalError;
            return result;
        }
        usleep(5000);
    }

    if (got_status) {
        result.time_ms = (ru.ru_utime.tv_sec * 1000L) +
                         (ru.ru_utime.tv_usec / 1000L) +
                         (ru.ru_stime.tv_sec * 1000L) +
                         (ru.ru_stime.tv_usec / 1000L);
        result.mem_kb = ru.ru_maxrss;  // KB on Linux
    }

    // Memory accounting. We treat MLE as "the process was killed because it
    // tried to use far more memory than its budget allowed". A clean exit
    // (WIFEXITED && exit_code == 0) is always Ok, even if ru_maxrss is
    // slightly over the budget — modern glibc runtime + libstdc++ can easily
    // touch 16-30 MB just from text/rodata, so a tight RSS budget on its own
    // is a false positive. We only call MLE when the process died from a
    // signal (which is how RLIMIT_DATA / RLIMIT_AS excess surfaces) AND it
    // was over budget AND it had not yet burned a significant chunk of its
    // CPU budget — the latter is the heuristic that separates a true OOM
    // crash from a TLE-kill that happens to report high RSS because the
    // process had time to fault in libraries.
    long mem_limit_kb = static_cast<long>(limits.memory_limit_mb) * 1024L;
    bool rss_over = (mem_limit_kb > 0) && (result.mem_kb > mem_limit_kb);
    bool cpu_quick_kill = (result.time_ms <
                            static_cast<long>(limits.time_limit_ms) / 2);

    if (WIFSIGNALED(status)) {
        result.signal = WTERMSIG(status);
        if (result.signal == SIGXCPU || result.signal == SIGKILL) {
            // could be time-limit or wall-timeout kill
            struct stat st{};
            if (stat(stdout_path.c_str(), &st) == 0) {
                result.output_bytes = static_cast<uint64_t>(st.st_size);
            }
            // Distinguish MLE from TLE when SIGKILLed:
            //   - TLE: process ran close to its CPU budget before SIGKILL
            //   - MLE: process was killed quickly despite over-budget RSS
            // For SIGXCPU it's always TLE (CPU limit tripped first).
            if (result.signal == SIGKILL && rss_over && cpu_quick_kill) {
                result.status = RunStatus::MemoryLimitExceeded;
            } else {
                result.status = RunStatus::TimeLimitExceeded;
            }
            return result;
        }
        if ((result.signal == SIGABRT || result.signal == SIGSEGV ||
             result.signal == SIGBUS) && rss_over && cpu_quick_kill) {
            // malloc failed → std::bad_alloc → uncaught → terminate → SIGABRT
            // (or vector<T>(n) with null pointer UB → SIGSEGV). Either way,
            // the rusage tells us we were over budget.
            result.status = RunStatus::MemoryLimitExceeded;
            return result;
        }
        result.status = RunStatus::RuntimeError;
        return result;
    }

    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
        if (result.exit_code != 0) {
            result.status = RunStatus::RuntimeError;
            return result;
        }
        // Clean exit with exit 0 is accepted regardless of RSS — RSS measures
        // resident set, which includes library text/rodata that is not under
        // the user's control. A streaming solution that uses no heap will
        // still report RSS in the same ballpark as a heavy solution.
        result.status = RunStatus::Ok;
        return result;
    }

    struct stat st{};
    if (stat(stdout_path.c_str(), &st) == 0) {
        result.output_bytes = static_cast<uint64_t>(st.st_size);
        if (result.output_bytes >
            static_cast<uint64_t>(limits.output_limit_mb) * 1024 * 1024) {
            result.status = RunStatus::OutputLimitExceeded;
            return result;
        }
    }

    result.status = RunStatus::Ok;
    return result;
}

}  // namespace oj::judge
