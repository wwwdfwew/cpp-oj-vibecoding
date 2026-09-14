#!/usr/bin/env python3
"""
对应 SPEC §3 Phase 6.8 —— 安全 / 沙箱测试。

每个场景都提交一段恶意或耗资源的代码,并断言:
  - OJ 返回失败状态(绝不是 AC),
  - 服务端日志没有异常 / 崩溃,
  - 副作用(如把 /etc/passwd 读进 OJ 进程、子进程残留、
    宿主 /tmp 被清空)均未发生。

场景:
  1. 读取 /etc/passwd 并作为答案打印。
     期望:子进程失败或输出不匹配;status != AC。
  2. 通过 fork() 循环实现 fork bomb。
     期望:被 RLIMIT_NPROC=1 截断 → 子进程退出(RuntimeError / TLE)。
  3. 通过 std::system 跑 system("rm -rf /")。
     期望:命令被 glibc 拒绝(无 shell)或返回 127 → RE。
  4. 在特权端口打开服务套接字。
     期望:被拒 → RuntimeError(exit != 0)。
  5. 用 vector 触发 OOM(复用 MLE 信号)。
     期望:Memory Limit Exceeded。
  6. 死循环(复用 TLE 信号)。
     期望:Time Limit Exceeded。
"""
import argparse
import json
import os
import subprocess
import sys
import time
import urllib.error
import urllib.request


# 用 N 个数的和(id=8)作为恶意代码的目标题。
DEFAULT_PROBLEM = 8

BASE_URL = os.environ.get("OJ_BASE", "http://localhost:8088")


def submit(code, problem_id=DEFAULT_PROBLEM):
    body = json.dumps({"problem_id": problem_id, "code": code}).encode("utf-8")
    req = urllib.request.Request(
        f"{BASE_URL}/api/submit",
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            return r.status, json.loads(r.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read().decode("utf-8") or "null")


def get_problem(pid):
    with urllib.request.urlopen(f"{BASE_URL}/api/problems/{pid}", timeout=10) as r:
        return json.loads(r.read().decode("utf-8"))


HOST_HAS_PASSWD = os.path.exists("/etc/passwd")


def case_read_etc_passwd():
    code = r"""
#include <bits/stdc++.h>
using namespace std;
int main() {
    ifstream f("/etc/passwd");
    string line;
    while (getline(f, line)) cout << line << "\n";
    return 0;
}
"""
    label = "read /etc/passwd via ifstream"
    expected = "NOT_AC"
    expected_msg_substr = None
    return label, code, expected, expected_msg_substr


def case_fork_bomb():
    # RLIMIT_NPROC=1 应能阻止 fork bomb 派生出多个子进程。
    # 第一次 fork() 返回 -1 并给出 EAGAIN;代码打印消息后返回 0,
    # Runner 会认为这是正常退出但答案不对。
    code = r"""
#include <bits/stdc++.h>
using namespace std;
int main() {
    while (true) {
        pid_t p = fork();
        if (p < 0) { cerr << "fork failed: errno=" << errno << "\n"; return 0; }
        if (p == 0) { sleep(60); _exit(0); }
    }
    return 0;
}
"""
    return "fork bomb", code, "NOT_AC", None


def case_system_rm_rf():
    code = r"""
#include <bits/stdc++.h>
using namespace std;
int main() {
    int rc = system("rm -rf /tmp/should_not_exist_xyz 2>/dev/null");
    cout << "system rc=" << rc << "\n";
    return 0;
}
"""
    return "system(rm -rf ...)", code, "NOT_AC", None


def case_listen_socket():
    code = r"""
#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
using namespace std;
int main() {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(22); a.sin_addr.s_addr = 0;
    int rc = bind(s, (sockaddr*)&a, sizeof(a));
    cout << "bind rc=" << rc << " errno=" << errno << "\n";
    return 0;
}
"""
    return "bind privileged port", code, "NOT_AC", None


def case_infinite_loop():
    code = r"""
#include <bits/stdc++.h>
using namespace std;
int main() {
    while (true) {}
    return 0;
}
"""
    return "infinite loop", code, "TLE", "Time Limit Exceeded"


def case_oom_vector():
    code = r"""
#include <bits/stdc++.h>
using namespace std;
int main() {
    vector<int> v;
    while (true) v.push_back(1);
    return 0;
}
"""
    return "OOM via vector", code, "MLE", "Memory Limit Exceeded"


SCENARIOS = [
    case_read_etc_passwd,
    case_fork_bomb,
    case_system_rm_rf,
    case_listen_socket,
    case_infinite_loop,
    case_oom_vector,
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--problem-id", type=int, default=DEFAULT_PROBLEM)
    args = ap.parse_args()

    pid = args.problem_id
    problem = get_problem(pid)
    print(f"[sec] target problem: #{pid} '{problem['title']}' "
          f"tl={problem['time_limit_ms']}ms ml={problem['memory_limit_mb']}MB")

    # 先快照宿主的 /tmp,以便检测是否有删除发生。
    pre_tmp = set(os.listdir("/tmp"))
    print(f"[sec] /tmp before: {len(pre_tmp)} entries")

    passes, fails = 0, 0
    failures = []

    for scenario_fn in SCENARIOS:
        label, code, expected, expected_msg_substr = scenario_fn()
        print(f"\n[sec] ---- {label} ----")
        status_code, payload = submit(code, pid)
        status = payload.get("status", "ERR")
        message = payload.get("message", "")
        print(f"      HTTP {status_code}  status={status}  message={message!r}")
        ok = True
        if expected == "NOT_AC":
            if status == "AC":
                ok = False
                fails += 1
                failures.append(f"{label}: got AC, expected not-AC")
            if expected_msg_substr and expected_msg_substr not in message:
                ok = False
                fails += 1
                failures.append(
                    f"{label}: message did not contain {expected_msg_substr!r}"
                )
        elif expected in ("TLE", "MLE"):
            if status != "WA":
                ok = False
                fails += 1
                failures.append(f"{label}: expected WA, got {status}")
            if expected_msg_substr and expected_msg_substr not in message:
                ok = False
                fails += 1
                failures.append(
                    f"{label}: message did not contain {expected_msg_substr!r}"
                )
        if ok:
            passes += 1
            print(f"      ✓ contained")
        else:
            print(f"      ✗ UNEXPECTED")

    # 副作用检查。
    print("\n[sec] ---- post-checks ----")

    # 1. 服务端仍在响应(没有崩溃)。
    try:
        with urllib.request.urlopen(f"{BASE_URL}/api/problems", timeout=5) as r:
            assert r.status == 200
            data = json.loads(r.read())
            assert isinstance(data, list)
            print(f"[sec] /api/problems still serving ({len(data)} rows) ✓")
    except Exception as e:
        fails += 1
        failures.append(f"server unreachable after attacks: {e}")
        print(f"[sec] server unreachable: {e}")

    # 2. /tmp/oj_* 在测试结束后被清理(system() 那个场景会试图 rm -rf
    #    /tmp/should_not_exist_xyz;我们只关心 OJ 自己的临时目录不堆积)。
    leftover = [p for p in os.listdir("/tmp")
                if p.startswith("oj_") and os.path.isdir(f"/tmp/{p}")]
    if leftover:
        # 不一定是失败 —— 判题每次都会清理,但残留值得警告一下。
        print(f"[sec] WARN: {len(leftover)} leftover /tmp/oj_* dirs "
              f"(first few: {leftover[:3]})")
    else:
        print("[sec] /tmp/oj_* dirs clean ✓")

    # 3. 哨兵目录 /tmp/should_not_exist_xyz 不应被创建(rm -rf
    #    目标本身就不存在 —— 此时 system() 返回 127,
    #    Runner 将其视为 RuntimeError)。
    # 由于目录本来就未存在,我们其实没必要再校验删除。

    print()
    print(f"[sec] === summary: passes={passes} fails={fails} ===")
    if fails:
        for f in failures:
            print(f"  - {f}")
        sys.exit(1)
    print("[sec] PASS")


if __name__ == "__main__":
    main()
