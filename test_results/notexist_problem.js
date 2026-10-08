// ============================================================================
// 名称: notexist_problem.js
// 用途: 提交不存在的 problem_id=999999 验证服务端返回 404
// 套件: DETAIL / EX-03
// 用例: 验证 SPEC §4.3 E-03 "提交不存在 problem_id" 应返回 404
// 前置: 需要处于登录态（admin 或 user）
// 调用: playwright-cli run-code --filename=notexist_problem.js
// 期望: { status: 404, body: "{\"error\":\"题目不存在\"}" }
// 实际: { status: 200, body: "{\"status\":\"WA\",\"message\":\"Problem not found\"}" }
// 结果: ❌ FAIL —— 触发 BUG-EX-03（API 语义错误：4xx vs 200）
// 修复: 在 src/handlers/submit.cpp 中 ProblemStore::get() == null 时返回 404
// ============================================================================
async page => {
  const result = await page.evaluate(async () => {
    const r = await fetch('/api/submit', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ problem_id: 999999, code: 'int main(){return 0;}' })
    });
    return { status: r.status, body: await r.text() };
  });
  return result;
}