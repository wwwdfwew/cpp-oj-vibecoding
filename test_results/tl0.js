// ============================================================================
// 名称: tl0.js
// 用途: 验证 admin 创建题目时 time_limit_ms=0 被拒绝
// 套件: ADMIN_CREATE / EX-07
// 用例: 验证 SPEC §4.3 E-07 "time_limit=0 拒绝"
// 前置: admin 登录态
// 调用: playwright-cli run-code --filename=tl0.js
// 期望: { status: 400, body: "{\"error\":\"time_limit_ms must be in [100, 10000]}\"}" }
// 结果: ✅ PASS —— 校验范围 [100, 10000] 合理
//       （防 0ms 立即超时，也防 100s+ 卡死服务）
// ============================================================================
async page => {
  const result = await page.evaluate(async () => {
    const r = await fetch('/api/admin/problems', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        title: 'AUTOTEST_TL0_' + Date.now(),
        difficulty: 'easy',
        time_limit_ms: 0,
        memory_limit_mb: 128,
        description: 'x',
        input_format: 'x',
        output_format: 'x',
        samples: [{ input: '1', output: '1' }]
      })
    });
    return { status: r.status, body: await r.text() };
  });
  return result;
}