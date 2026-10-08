// ============================================================================
// 名称: admin_api_user.js
// 用途: 验证普通用户调 admin API 被拒绝（403 admin only）
// 套件: ADMIN_LIST / EX-09
// 用例: 验证 SPEC §4.3 E-09 "admin API 普通用户"
// 流程: 1) 登出当前 admin → 2) 登录普通用户 → 3) DELETE /api/admin/problems/1
// 调用: playwright-cli run-code --filename=admin_api_user.js
// 期望: del: { status: 403, body: "{\"error\":\"admin only\"}" }
// 结果: ✅ PASS
// ============================================================================
async page => {
  // 先退出 admin
  const logoutResp = await page.evaluate(async () => {
    const r = await fetch('/api/logout', { method: 'POST' });
    return { status: r.status, body: await r.text() };
  });
  // 登录 user
  const loginResp = await page.evaluate(async () => {
    const r = await fetch('/api/login', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username: 'autotest_user_99', password: 'Passw0rd!' })
    });
    return { status: r.status, body: await r.text() };
  });
  // 尝试删除
  const delResp = await page.evaluate(async () => {
    const r = await fetch('/api/admin/problems/1', { method: 'DELETE' });
    return { status: r.status, body: await r.text() };
  });
  return { logout: logoutResp, login: loginResp, del: delResp };
}