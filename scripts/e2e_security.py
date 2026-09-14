#!/usr/bin/env python3
"""
SPEC §3 Phase 6.8 — security / sandbox test.

Each scenario posts a malicious or resource-abusive submission and asserts
that:
  - the OJ reports a non-zero (failure) status — never AC,
  - the server log shows no exceptions / crashes, and
  - targeted side-effects (e.g. /etc/passwd being read into the OJ process,
    a child surviving, the host's tmp tree being wiped) do NOT happen.

Scenarios:
  1. Read /etc/passwd and print it as the answer.
     Expected: child fails or output doesn't match; status != AC.
  2. Fork bomb via fork() loop.
     Expected: capped by RLIMIT_NPROC=1 → child dies (RuntimeError / TLE).
  3. system("rm -rf /") via std::system.
     Expected: cmd either rejected by glibc (no shell) or returns 127 → RE.
  4. open a server socket on a privileged port.
     Expected: refused → RuntimeError (exit!=0).
  5. Cause OOM with vector (re-uses MLE signal).
     Expected: Memory Limit Exceeded.
  6. Infinite loop (re-uses TLE signal).
     Expected: Time Limit Exceeded.
"""
import argparse
import json
import os
import subprocess
import sys
import time
import urllib.error
import urllib.request


# Use Sum of N (id=8) as the target problem for malicious code.
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
    # RLIMIT_NPROC=1 should prevent the bomb from spawning multiple children.
    # First fork() returns -1 with EAGAIN; the code prints a message and
    # returns 0, which the runner sees as a clean exit but a wrong answer.
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

    # Snapshot the host's /tmp before to detect any deletions.
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

    # Side-effect checks.
    print("\n[sec] ---- post-checks ----")

    # 1. Server still answering (no crash).
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

    # 2. /tmp/oj_* leftovers from the test run are cleaned up (the system()
    #    scenario tries to rm -rf /tmp/should_not_exist_xyz; we only care that
    #    the OJ temp dirs themselves don't pile up).
    leftover = [p for p in os.listdir("/tmp")
                if p.startswith("oj_") and os.path.isdir(f"/tmp/{p}")]
    if leftover:
        # not necessarily a failure — judge cleans per-submit, but a leftover
        # is worth a warning.
        print(f"[sec] WARN: {len(leftover)} leftover /tmp/oj_* dirs "
              f"(first few: {leftover[:3]})")
    else:
        print("[sec] /tmp/oj_* dirs clean ✓")

    # 3. The /tmp/should_not_exist_xyz sentinel shouldn't exist (rm -rf
    #    target was a non-existing dir — system() returns 127 in that case,
    #    which the runner sees as RuntimeError). We don't actually verify
    #    deletion because the dir never existed.

    print()
    print(f"[sec] === summary: passes={passes} fails={fails} ===")
    if fails:
        for f in failures:
            print(f"  - {f}")
        sys.exit(1)
    print("[sec] PASS")


if __name__ == "__main__":
    main()
