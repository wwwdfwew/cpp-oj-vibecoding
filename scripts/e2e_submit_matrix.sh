#!/usr/bin/env bash
# 对应 SPEC §3 Phase 6 —— 端到端提交矩阵
# 在 3 道种子题上,至少各触发一次 AC / WA / CE / TLE / MLE / RE。
#
# 用法:
#   bash scripts/e2e_submit_matrix.sh [BASE_URL]   # 默认 http://localhost:8088
#
# 当且仅当每个观察到的状态都匹配其期望标签时,退出码为 0。
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
    # submit <problem_id> <code_body> -> 输出以制表符分隔的 3 个字段:
    #   status<TAB>message<TAB>compile_error_summary
    # code_body 会被套用 SPEC §2.6 的默认模板,使用者只需写 main() 内部。
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
# 使用一个不会出现在 compile_error / message 中的哨兵字符串。
            print(f"{status}<<<>>>{message}<<<>>>{cerr}")
'
}

expect() {
    # expect <label> <actual_status> <actual_message_substring>
    local label="$1" got_status="$2" got_msg="$3"
    if [[ "$got_status" == "AC" ]]; then
        [[ "$label" == "AC" ]] && ok "AC" || bad "expected $label got AC ($got_msg)"
    else
        # 按 SPEC §1.5,WA 桶涵盖 CE/TLE/MLE/RE
        if [[ "$label" == "$got_msg" || ( "$label" != "AC" && "$got_status" == "WA" && "$got_msg" == *"$label"* ) ]]; then
            ok "$label"
        else
            bad "expected $label, got status=$got_status msg=$got_msg"
        fi
    fi
}

echo "=== Phase 6 E2E submission matrix against $BASE ==="
echo

# 按标题拉出 3 道种子题的 id。
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

# 辅助函数:调用 submit 并解析以哨兵分隔的输出。
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

# --- TLE:N 个数中的最大值,O(N²) 两两比较,N=200000 ---
# 先把全部数读入 vector(这样答案仍然正确),再用 O(N²) 两两比较求最大。
# 两阶段单独看都不算贵,但 O(N²) 比较(≈ 2×10^10 次操作)会爆掉 1 秒时限。
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

# --- MLE:N 个数的和用 vector<int>(N),N=8_000_000 且 16MB 上限 ---
run_case "MLE: vector<int>(8000000) on 16MB cap" \
    "Memory Limit Exceeded" "$P_SUM" \
    'long long n; std::cin >> n;
     std::vector<int> v(n);
     for (long long i = 0; i < n; ++i) std::cin >> v[i];
     long long s = 0;
     for (long long i = 0; i < n; ++i) s += v[i];
     std::cout << s;'

# --- WA:N 个数的和(off-by-one:跳过最后一个元素) ---
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
