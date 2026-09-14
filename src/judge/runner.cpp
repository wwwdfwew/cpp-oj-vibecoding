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
        // 子进程
        // 设置资源限制
        rlim_t cpu_seconds = static_cast<rlim_t>(limits.time_limit_ms / 1000) + 1;
        set_limit(RLIMIT_CPU, cpu_seconds);
        // 仅限制堆内存。RLIMIT_AS 同时还会约束二进制和共享库的映射
        // (glibc 系统上 libc/libstdc++/libm 通常 > 16 MB),即便一个完全
        // 不碰堆的流式解法也会因此无法启动。
        set_limit(RLIMIT_DATA,
                  static_cast<rlim_t>(limits.memory_limit_mb) * 1024 * 1024);
        // 作为兜底,再把 RLIMIT_AS 设为堆预算的若干倍,这样基于 mmap
        // 的分配器(大块分配走 mmap 而不是 brk)也会在把主机换出去
        // 之前先被触发。
        set_limit(RLIMIT_AS,
                  static_cast<rlim_t>(limits.memory_limit_mb) * 8 * 1024 * 1024);
        set_limit(RLIMIT_FSIZE,
                  static_cast<rlim_t>(limits.output_limit_mb) * 1024 * 1024);
        set_limit(RLIMIT_NPROC, 1);

        if (dup2(in_fd, STDIN_FILENO) < 0) _exit(127);
        if (dup2(out_fd, STDOUT_FILENO) < 0) _exit(127);
        // 静默子进程的 stderr,避免 std::bad_alloc / SIGABRT 等信息
        // (由 MLE / RE 用例刻意产生)泄漏到服务端日志。
        // Runner 仍然会上报运行时状态。
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }
        close(in_fd);
        close(out_fd);

        // 新建会话,确保任何游离信号都留在本进程组内
        setsid();

        char* const argv[] = {
            const_cast<char*>("main.bin"),
            nullptr
        };
        execve(bin_path.c_str(), argv, nullptr);
        _exit(127);
    }

    // 父进程
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
                // 已被回收(例如 SIGCHLD 竞争),继续轮询。
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

    // 内存统计。把 MLE 解释为:"进程被杀掉,是因为它试图使用的内存远远
    // 超出了预算"。正常退出(WIFEXITED && exit_code == 0)始终视为 Ok,
    // 即便 ru_maxrss 略超预算 —— 现代 glibc 运行时 + libstdc++ 仅靠
    // text/rodata 就会轻松触及 16-30 MB,因此仅看 RSS 容易误判。
    // 只有当进程因信号被杀(这是 RLIMIT_DATA / RLIMIT_AS 触发的表现),
    // 同时确实超出预算,并且还没消耗掉相当一部分 CPU 时间时,
    // 才将其归为 MLE;这条经验规则可以把真正的 OOM 崩溃和因加载库
    // 而恰好 RSS 偏高的 TLE 杀掉区分开。
    long mem_limit_kb = static_cast<long>(limits.memory_limit_mb) * 1024L;
    bool rss_over = (mem_limit_kb > 0) && (result.mem_kb > mem_limit_kb);
    bool cpu_quick_kill = (result.time_ms <
                            static_cast<long>(limits.time_limit_ms) / 2);

    if (WIFSIGNALED(status)) {
        result.signal = WTERMSIG(status);
        if (result.signal == SIGXCPU || result.signal == SIGKILL) {
            // 可能是超时或墙钟超时触发的杀进程
            struct stat st{};
            if (stat(stdout_path.c_str(), &st) == 0) {
                result.output_bytes = static_cast<uint64_t>(st.st_size);
            }
            // 在 SIGKILL 情形下区分 MLE 与 TLE:
            //   - TLE:被 SIGKILL 时已接近 CPU 预算
            //   - MLE:被 SIGKILL 时进程很快被杀,且 RSS 超额
            // SIGXCPU 则一律视为 TLE(先触发了 CPU 上限)。
            if (result.signal == SIGKILL && rss_over && cpu_quick_kill) {
                result.status = RunStatus::MemoryLimitExceeded;
            } else {
                result.status = RunStatus::TimeLimitExceeded;
            }
            return result;
        }
        if ((result.signal == SIGABRT || result.signal == SIGSEGV ||
             result.signal == SIGBUS) && rss_over && cpu_quick_kill) {
            // malloc 失败 → std::bad_alloc → 未捕获 → terminate → SIGABRT
            // (或 vector<T>(n) 解引用空指针的 UB → SIGSEGV)。
            // 总之从 rusage 可以看出确实超额了。
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
        // exit 0 的正常退出不受 RSS 影响 —— RSS 度量的是常驻集,
        // 其中包含用户无法控制的库代码 text/rodata。
        // 完全不碰堆的流式解法,其 RSS 与"重型"解法也基本相当。
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
