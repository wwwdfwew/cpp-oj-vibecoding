// ============================================================================
// 名称: del2.js
// 用途: 验证已删除题二次删除返回 404
// 套件: ADMIN_LIST / AD-10 + EX-10
// 用例: 验证 SPEC §4.1 F-10 / §4.3 E-10 "同一 ID 二次删除"
// 流程: 1) admin 登录 → 2) DELETE 已不存在的 #18
// 调用: playwright-cli run-code --filename=del2.js
// 期望: del: { status: 404, body: "{\"error\":\"not found\"}" }
// 结果: ✅ PASS
// ============================================================================
async page => {
  // 先登录 admin (通过 admin login, 这个接口正常)
  const adminLogin = await page.evaluate(async () => {
    const r = await fetch('/api/admin/login', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username: 'admin', password: 'admin123' })
    });
    return { status: r.status, body: await r.text() };
  });
  // 删除已不存在的 18
  const del = await page.evaluate(async () => {
    const r = await fetch('/api/admin/problems/18', { method: 'DELETE' });
    return { status: r.status, body: await r.text() };
  });
  return { adminLogin, del };
}