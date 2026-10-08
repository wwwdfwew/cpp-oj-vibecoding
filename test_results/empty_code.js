// ============================================================================
// 名称: empty_code.js
// 用途: 提交空代码到 /api/submit 验证服务端拒绝
// 套件: DETAIL / EX-01
// 用例: 验证 SPEC §4.3 E-01 "提交空 code" 返回 400
// 前置: 需要处于登录态（admin 或 user）
// 调用: playwright-cli run-code --filename=empty_code.js
// 期望: { status: 400, body: "{\"error\":\"代码为空\"}" }
// 结果: ✅ PASS
// ============================================================================
async page => {
  const result = await page.evaluate(async () => {
    const r = await fetch('/api/submit', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ problem_id: 12, code: '' })
    });
    return { status: r.status, body: await r.text() };
  });
  return result;
}