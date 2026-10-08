// ============================================================================
// 名称: long_code.js
// 用途: 提交 70KB 超长代码到 /api/submit 验证服务端长度限制
// 套件: DETAIL / EX-02
// 用例: 验证 SPEC §4.3 E-02 "提交超长 code" 应返回 413
// 前置: 需要处于登录态（admin 或 user）
// 调用: playwright-cli run-code --filename=long_code.js
// 期望: { status: 413, body: "{\"error\":\"代码长度超过限制(64KB)\"}" }
// 实际: { status: 200, body: "{\"status\":\"WA\",\"message\":\"Compile Error\",...}" }
// 结果: ❌ FAIL —— 触发 BUG-EX-02（服务端未做长度限制）
// 修复: 在 src/handlers/submit.cpp 中校验 code.size() <= 64*1024
// ============================================================================
async page => {
  const longCode = 'x'.repeat(70000);
  const result = await page.evaluate(async (code) => {
    const r = await fetch('/api/submit', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ problem_id: 12, code: code })
    });
    return { status: r.status, body: await r.text() };
  }, longCode);
  return result;
}