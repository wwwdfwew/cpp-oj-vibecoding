#!/usr/bin/env python3
"""
SPEC §3 Phase 6.7 — performance / latency check.

Submits a correct solution to the "Sum of N" problem (id=8) repeatedly and
measures end-to-end latency (HTTP request + compile + run all 3 cases). Reports
P50 / P95 / P99 and asserts P95 <= 5 s, per SPEC §1.4 / §4.2 N-01.

Usage:
  python3 scripts/perf_latency.py [--base URL] [--runs N] [--problem-id N]
                                  [--time-limit-ms N]
"""
import argparse
import json
import os
import statistics
import sys
import time
import urllib.error
import urllib.request


# Same correct Sum-of-N code we use in the smoke tests.
CODE = """\
#include <bits/stdc++.h>
using namespace std;

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    long long n; std::cin >> n;
    long long s = 0;
    for (long long i = 0; i < n; ++i) {
        long long x; std::cin >> x;
        s += x;
    }
    std::cout << s;
    return 0;
}
"""


def submit(base, problem_id, code):
    body = json.dumps({"problem_id": problem_id, "code": code}).encode("utf-8")
    req = urllib.request.Request(
        f"{base}/api/submit",
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    t0 = time.perf_counter()
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            payload = json.loads(r.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        payload = json.loads(e.read().decode("utf-8") or "null")
    elapsed = time.perf_counter() - t0
    return elapsed, payload


def percentile(values, p):
    """Return the p-th percentile (0..100) using linear interpolation."""
    if not values:
        return 0.0
    s = sorted(values)
    k = (len(s) - 1) * (p / 100.0)
    f = int(k)
    c = min(f + 1, len(s) - 1)
    if f == c:
        return s[f]
    return s[f] + (s[c] - s[f]) * (k - f)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default=os.environ.get("OJ_BASE", "http://localhost:8088"))
    ap.add_argument("--runs", type=int, default=20)
    ap.add_argument("--warmup", type=int, default=2)
    ap.add_argument("--problem-id", type=int, default=8)
    args = ap.parse_args()

    base = args.base.rstrip("/")

    # Warmup: prime the page cache + MySQL connection pool.
    for i in range(args.warmup):
        ms, payload = submit(base, args.problem_id, CODE)
        if payload.get("status") != "AC":
            print(f"warmup #{i}: not AC — payload={payload}", file=sys.stderr)
            sys.exit(2)

    samples_ms = []
    statuses = {"AC": 0, "WA": 0, "ERROR": 0}
    for i in range(args.runs):
        ms, payload = submit(base, args.problem_id, CODE)
        status = payload.get("status", "ERROR")
        statuses[status] = statuses.get(status, 0) + 1
        samples_ms.append(ms * 1000.0)
        print(f"  run {i + 1:>3}/{args.runs}: {ms * 1000:7.1f} ms  status={status}")

    samples_ms.sort()
    p50 = percentile(samples_ms, 50)
    p95 = percentile(samples_ms, 95)
    p99 = percentile(samples_ms, 99)
    avg = statistics.mean(samples_ms)
    mx  = max(samples_ms)

    print()
    print(f"=== Phase 6.7 latency over {args.runs} runs ===")
    print(f"  status counts: {statuses}")
    print(f"  avg = {avg:7.1f} ms")
    print(f"  P50 = {p50:7.1f} ms")
    print(f"  P95 = {p95:7.1f} ms")
    print(f"  P99 = {p99:7.1f} ms")
    print(f"  max = {mx:7.1f} ms")

    p95_limit_ms = 5000.0
    ok = (p95 <= p95_limit_ms) and (statuses.get("AC", 0) == args.runs)
    if ok:
        print(f"  -> PASS (P95 {p95:.1f} ms <= {p95_limit_ms:.0f} ms)")
        sys.exit(0)
    else:
        print(f"  -> FAIL (P95 {p95:.1f} ms > {p95_limit_ms:.0f} ms "
              f"or non-AC submissions)")
        sys.exit(1)


if __name__ == "__main__":
    main()
