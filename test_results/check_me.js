// ============================================================================
// 名称: check_me.js
// 用途: 调用 GET /api/me 验证当前登录态
// 套件: AUTH / AU-01 / AU-06
// 用例: 验证注册/登录成功后 /api/me 返回 200 且 role=user
// 调用: playwright-cli run-code --filename=check_me.js
// 期望: { status: 200, body: "{\"ok\":true,\"role\":\"user\",\"username\":\"...\"}" }
// ============================================================================
async page => {
  const resp = await page.evaluate(async () => {
    const r = await fetch('/api/me');
    return { status: r.status, body: await r.text() };
  });
  return resp;
}