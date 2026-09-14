#!/usr/bin/env python3
"""
SPEC §3 Phase 6.1 — seed 3 OJ problems that exercise AC / WA / CE / TLE / MLE / RE.

Each problem is designed so that with appropriate user submissions we can observe
each judge status at least once:
  - A+B           → AC, WA, CE, RE  (warm-up)
  - Sum of N      → AC, TLE         (large N; O(n^2) times out)
  - Maximum of N  → AC, MLE, RE     (tight memory; vector<int>(N) OOMs)

Usage:
  python3 scripts/seed_problems.py [--base http://localhost:8088]
                                  [--admin admin] [--password admin123]
"""
import argparse
import json
import os
import sys
import urllib.error
import urllib.request
from http.cookiejar import CookieJar


def make_opener():
    cj = CookieJar()
    return urllib.request.build_opener(urllib.request.HTTPCookieProcessor(cj))


def request(opener, method, url, body=None):
    data = None
    headers = {}
    if body is not None:
        data = json.dumps(body).encode("utf-8")
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with opener.open(req, timeout=15) as r:
            return r.status, json.loads(r.read().decode("utf-8") or "null")
    except urllib.error.HTTPError as e:
        try:
            payload = json.loads(e.read().decode("utf-8") or "null")
        except Exception:
            payload = None
        return e.code, payload


def login(opener, base, username, password):
    code, body = request(opener, "POST", f"{base}/api/admin/login",
                         {"username": username, "password": password})
    if code != 200 or not body or not body.get("ok"):
        print(f"[seed] admin login failed: HTTP {code} {body}", file=sys.stderr)
        sys.exit(1)
    print(f"[seed] admin login ok ({username})")


def list_problems(opener, base):
    code, body = request(opener, "GET", f"{base}/api/problems")
    if code != 200:
        print(f"[seed] list problems failed: HTTP {code}", file=sys.stderr)
        sys.exit(1)
    return body


def delete_problem(opener, base, pid):
    code, body = request(opener, "DELETE", f"{base}/api/admin/problems/{pid}")
    if code not in (200, 404):
        print(f"[seed] delete {pid} failed: HTTP {code} {body}", file=sys.stderr)
        sys.exit(1)
    return code


def create_problem(opener, base, problem):
    code, body = request(opener, "POST", f"{base}/api/admin/problems", problem)
    if code != 200 or not body or not body.get("ok"):
        print(f"[seed] create '{problem['title']}' failed: HTTP {code} {body}",
              file=sys.stderr)
        sys.exit(1)
    return body["id"]


def gen_sum_n_large_input(n):
    """Single-line space-separated string of n copies of 1."""
    return " ".join(["1"] * n) + "\n"


def gen_max_n_large_input(n):
    """Single-line space-separated string of n copies of 7 with one 999 at end."""
    parts = ["7"] * (n - 1) + ["999"]
    return " ".join(parts) + "\n"


def problems():
    # ----- Problem 1: A+B (warm-up) -----
    p1 = {
        "title": "A+B",
        "description": (
            "从标准输入读取两个整数 a 和 b,输出它们的和。\n"
            "\n"
            "本题为热身题,用于验证系统的 AC / WA / CE / RE 各个判题路径。"
        ),
        "input_format": "一行,包含两个用空格分隔的整数 a 和 b。",
        "output_format": "一个整数,即 a + b 的值,末尾换行。",
        "time_limit_ms": 1000,
        "memory_limit_mb": 64,
        "test_cases": [
            {"input": "1 2\n",          "expected_output": "3\n"},
            {"input": "10 20\n",        "expected_output": "30\n"},
            {"input": "-5 5\n",         "expected_output": "0\n"},
            {"input": "1000000 2000000\n", "expected_output": "3000000\n"},
        ],
    }

    # ----- Problem 2: Maximum of N (TLE trigger) -----
    # Includes a large N case where an O(N^2) pairwise comparison finishes but
    # past the time limit; the intended O(N) scan stays well under budget.
    p2 = {
        "title": "N 个数中的最大值",
        "description": (
            "读取一个整数 N,接着在同一行读入 N 个整数。\n"
            "输出这 N 个数中的最大值。\n"
            "\n"
            "推荐 O(N) 的线性扫描解法。若使用 O(N²) 的两两比较,"
            "在最大数据点会超出 1 秒时间限制。"
        ),
        "input_format": "第 1 行:整数 N。第 2 行:N 个用空格分隔的整数。",
        "output_format": "一个整数(最大值),末尾换行。",
        "time_limit_ms": 1000,
        "memory_limit_mb": 128,
        "test_cases": [
            {"input": "5\n3 1 4 1 5\n",      "expected_output": "5\n"},
            {"input": "3\n-1 -2 -3\n",       "expected_output": "-1\n"},
            # N=200000 时,O(N²) 两两比较(≈ 2*10^10 次操作)会爆 1s,
            # 而 O(N) 的单次扫描在微秒级即可完成。
            {"input": "200000\n" + gen_max_n_large_input(200000),
             "expected_output": "999\n"},
        ],
    }

    # ----- Problem 3: Sum of N (MLE trigger) -----
    # 内存上限较紧(16 MB);逐个累加的流式解法可以通过,
    # 但一次性把所有数塞进 vector<int> 会在最大数据点触发 MLE。
    p3 = {
        "title": "N 个数的和",
        "description": (
            "读取一个整数 N,接着在同一行读入 N 个整数。\n"
            "输出这 N 个整数的和。\n"
            "\n"
            "本题内存限制较紧(16 MB)。若把全部 N 个数先存到 vector<int> 再求和,"
            "可能在最大数据点触发 MLE(内存超限)。请改用边读边累加的方式。"
        ),
        "input_format": "第 1 行:整数 N。第 2 行:N 个用空格分隔的整数。",
        "output_format": "一个整数(N 个数的和),末尾换行。",
        "time_limit_ms": 1000,
        "memory_limit_mb": 16,
        "test_cases": [
            {"input": "5\n1 2 3 4 5\n",     "expected_output": "15\n"},
            {"input": "1\n42\n",            "expected_output": "42\n"},
            # 800 万 int * 4 字节 = 32 MB > 16 MB 上限 → vector<int>(N) 会 MLE;
            # 流式累加方案则远远低于上限。
            {"input": "8000000\n" + gen_sum_n_large_input(8000000),
             "expected_output": "8000000\n"},
        ],
    }

    return [p1, p2, p3]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default=os.environ.get("OJ_BASE", "http://localhost:8088"))
    ap.add_argument("--admin", default=os.environ.get("OJ_ADMIN_USER", "admin"))
    ap.add_argument("--password", default=os.environ.get("OJ_ADMIN_PASS", "admin123"))
    args = ap.parse_args()

    opener = make_opener()
    login(opener, args.base, args.admin, args.password)

    seeds = problems()
    seed_titles = {p["title"] for p in seeds}

    # Wipe any existing seed problems so the script is idempotent.
    print("[seed] removing existing seed problems (if any)…")
    for existing in list_problems(opener, args.base):
        if existing["title"] in seed_titles:
            code = delete_problem(opener, args.base, existing["id"])
            print(f"[seed]   - {existing['title']} (id={existing['id']}): "
                  f"{'deleted' if code == 200 else 'was already gone'}")

    print("[seed] creating problems:")
    for p in seeds:
        pid = create_problem(opener, args.base, p)
        print(f"[seed]   + {p['title']} id={pid} "
              f"tl={p['time_limit_ms']}ms ml={p['memory_limit_mb']}MB "
              f"cases={len(p['test_cases'])}")

    print("[seed] done.")


if __name__ == "__main__":
    main()
