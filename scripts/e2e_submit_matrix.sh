#!/usr/bin/env bash
# SPEC §3 Phase 6 — end-to-end submission matrix
# Exercises AC / WA / CE / TLE / MLE / RE at least once across the 3 seed problems.
#
# Usage:
#   bash scripts/e2e_submit_matrix.sh [BASE_URL]   # default http://localhost:8088
#
# Exit code is 0 iff every observed status matches its expected label.
set -u
BASE="${1:-${OJ_BASE:-http://localhost:8088}}"
COOKIES="$(mktemp)"
trap 'rm -f "$COOKIES"' EXIT

PASS=0
FAIL=0
FAILED_TESTS=()

note()  { printf "  • %s\n" "$*"; }
ok()    { printf "  \033[32m✓\033[0m %s\n" "$*"; PASS=$((PASS+1)); }
bad()   { printf "  \033[31m✗\033[0m %s\n" "$*"; FAIL=$((FAIL+1)); FAILED_TESTS+=("$*"); }

submit() {
    # submit <problem_id> <code_body> -> echoes 3 tab-separated fields:
    #   status<TAB>message<TAB>compile_error_summary
    # The body is wrapped in the SPEC §2.6 default template so the user only
    # writes the inside of main().
    local pid="$1" body="$2"
    local code
    code=$(cat <<EOF
#include <bits/stdc++.h>
using namespace std;

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
${body}
    return 0;
}
EOF
)
    local payload
    payload=$(python3 -c '
import json, sys
print(json.dumps({"problem_id": int(sys.argv[1]), "code": sys.argv[2]}))
' "$pid" "$code")
    curl -s -X POST "$BASE/api/submit" \
        -H "Content-Type: application/json" \
        -d "$payload" \
        -c "$COOKIES" -b "$COOKIES" \
    | python3 -c '
import json, sys
try:
    j = json.load(sys.stdin)
except Exception:
    j = {}
status  = j.get("status", "")
message = j.get("message", "")
cerr    = j.get("compile_error", "").splitlines()[0][:60] if j.get("compile_error") else ""
# Use a sentinel that will not appear in compile_error / message strings.
print(f"{status}<<<>>>{message}<<<>>>{cerr}")
'
}

expect() {
    # expect <label> <actual_status> <actual_message_substring>
    local label="$1" got_status="$2" got_msg="$3"
    if [[ "$got_status" == "AC" ]]; then
        [[ "$label" == "AC" ]] && ok "AC" || bad "expected $label got AC ($got_msg)"
    else
        # WA bucket covers CE/TLE/MLE/RE per SPEC §1.5
        if [[ "$label" == "$got_msg" || ( "$label" != "AC" && "$got_status" == "WA" && "$got_msg" == *"$label"* ) ]]; then
            ok "$label"
        else
            bad "expected $label, got status=$got_status msg=$got_msg"
        fi
    fi
}

echo "=== Phase 6 E2E submission matrix against $BASE ==="
echo

# Pull the 3 seed problem ids by title.
P_AB=$(curl -s "$BASE/api/problems" | python3 -c '
import json, sys
for p in json.load(sys.stdin):
    if p["title"] == "A+B":          print(p["id"]); break
')
P_SUM=$(curl -s "$BASE/api/problems" | python3 -c '
import json, sys
for p in json.load(sys.stdin):
    if p["title"] == "Sum of N":     print(p["id"]); break
')
P_MAX=$(curl -s "$BASE/api/problems" | python3 -c '
import json, sys
for p in json.load(sys.stdin):
    if p["title"] == "Maximum of N": print(p["id"]); break
')
echo "A+B=$P_AB  Sum of N=$P_SUM  Maximum of N=$P_MAX"
echo

# Helper that calls submit and unpacks the sentinel-separated output.
run_case() {
    # run_case <note> <expected_label> <problem_id> <code_body>
    local note_text="$1" label="$2" pid="$3" body="$4"
    note "$note_text"
    local raw
    raw="$(submit "$pid" "$body")"
    local s="${raw%%<<<>>>*}"
    local rest="${raw#*<<<>>>}"
    local m="${rest%%<<<>>>*}"
    local c="${rest#*<<<>>>}"
    expect "$label" "$s" "$m"
    if [[ -n "$c" ]]; then printf "      └─ compile_error: %s\n" "$c"; fi
}

# --- AC: correct A+B ---
run_case "AC: correct A+B solution" AC "$P_AB" \
    'int a, b; std::cin >> a >> b; std::cout << a + b;'

# --- WA: A+B but prints a*b instead ---
run_case "WA: prints a*b on A+B" "Wrong Answer" "$P_AB" \
    'int a, b; std::cin >> a >> b; std::cout << a * b;'

# --- CE: syntax error ---
run_case "CE: missing semicolon" "Compile Error" "$P_AB" \
    'std::cout << 1 return 0;'

# --- RE: division by zero on A+B ---
run_case "RE: SIGFPE (division by zero) on A+B" "Runtime Error" "$P_AB" \
    'volatile int x = 0; return 1 / x;'

# --- AC: streaming Sum of N (linear scan) ---
run_case "AC: linear Sum of N" AC "$P_SUM" \
    'long long n; std::cin >> n;
     long long s = 0;
     for (long long i = 0; i < n; ++i) { long long x; std::cin >> x; s += x; }
     std::cout << s;'

# --- AC: streaming Maximum of N ---
run_case "AC: streaming Maximum of N" AC "$P_MAX" \
    'long long n; std::cin >> n;
     long long best = LLONG_MIN;
     for (long long i = 0; i < n; ++i) {
         long long x; std::cin >> x;
         if (x > best) best = x;
     }
     std::cout << best;'

# --- TLE: O(N^2) pairwise compare on Maximum of N, N=200000 ---
# Reads all numbers into a vector first (so the answer is still correct), then
# does a quadratic pairwise comparison for the max. Both phases are cheap,
# but the O(N^2) comparison (≈ 2*10^10 ops) blows past the 1 s time limit.
run_case "TLE: O(N^2) pairwise compare on Maximum of N (N=200000)" \
    "Time Limit Exceeded" "$P_MAX" \
    'long long n; std::cin >> n;
     std::vector<long long> v(n);
     for (long long i = 0; i < n; ++i) std::cin >> v[i];
     long long best = v[0];
     for (long long i = 0; i < n; ++i)
         for (long long j = i + 1; j < n; ++j)
             if (v[j] > best) best = v[j];
     std::cout << best;'

# --- MLE: vector<int>(N) on Sum of N with N=8_000_000 and 16MB cap ---
run_case "MLE: vector<int>(8000000) on 16MB cap" \
    "Memory Limit Exceeded" "$P_SUM" \
    'long long n; std::cin >> n;
     std::vector<int> v(n);
     for (long long i = 0; i < n; ++i) std::cin >> v[i];
     long long s = 0;
     for (long long i = 0; i < n; ++i) s += v[i];
     std::cout << s;'

# --- WA on Sum of N (off-by-one: skips last element) ---
run_case "WA: skips last element on Sum of N" "Wrong Answer" "$P_SUM" \
    'long long n; std::cin >> n;
     long long s = 0;
     for (long long i = 0; i < n - 1; ++i) { long long x; std::cin >> x; s += x; }
     long long last; std::cin >> last;
     std::cout << s;'

echo
echo "=== summary: passed=$PASS failed=$FAIL ==="
if [[ $FAIL -gt 0 ]]; then
    printf 'failed cases:\n'
    for t in "${FAILED_TESTS[@]}"; do printf '  - %s\n' "$t"; done
    exit 1
fi
exit 0
