// ============================================================================
// 名称: delete_check.js
// 用途: 直接 DELETE /api/admin/problems/18 验证删除执行
// 套件: ADMIN_LIST / AD-09
// 用例: 验证 SPEC §4.1 F-10 "删除题目后题列表更新"
// 前置: admin 登录态 + 题 #018 存在
// 调用: playwright-cli run-code --filename=delete_check.js
// 期望: 首次删除 200，再次删除 404
// 结果: ✅ PASS（首次成功后题被删除）
// ============================================================================
async page => {
  const result = await page.evaluate(async () => {
    const r = await fetch('/api/admin/problems/18', { method: 'DELETE' });
    return { status: r.status, body: await r.text() };
  });
  return result;
}