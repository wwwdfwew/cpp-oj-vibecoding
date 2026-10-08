// ============================================================================
// 名称: second_delete.js
// 用途: 端到端验证"创建→删除→二次删除"完整流程
// 套件: ADMIN_LIST / EX-10
// 用例: 验证 SPEC §4.3 E-10 "同一 ID 二次删除"
// 流程: 1) 登出→admin 登录 → 2) 创建临时题 → 3) 第一次 DELETE (200)
//        4) 第二次 DELETE (404)
// 调用: playwright-cli run-code --filename=second_delete.js
// 期望: { create: 200, del1: {status:200}, del2: {status:404} }
// 结果: ✅ PASS（验证服务端正确区分"删除成功" vs "资源不存在"）
// ============================================================================
async page => {
  // 重新登录 admin
  await page.evaluate(async () => {
    await fetch('/api/logout', { method: 'POST' });
    await fetch('/api/admin/login', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username: 'admin', password: 'admin123' })
    });
  });
  // 创建并删除一个临时题
  const create = await page.evaluate(async () => {
    const r = await fetch('/api/admin/problems', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        title: 'AUTOTEST_temp_' + Date.now(),
        difficulty: 'easy',
        time_limit_ms: 1000,
        memory_limit_mb: 128,
        description: 'temp',
        input_format: 'x',
        output_format: 'y',
        samples: [{ input: '1', output: '2' }]
      })
    });
    return { status: r.status, body: await r.text() };
  });
  const id = JSON.parse(create.body).id;
  // 第一次删除
  const del1 = await page.evaluate(async (pid) => {
    const r = await fetch('/api/admin/problems/' + pid, { method: 'DELETE' });
    return { status: r.status, body: await r.text() };
  }, id);
  // 第二次删除
  const del2 = await page.evaluate(async (pid) => {
    const r = await fetch('/api/admin/problems/' + pid, { method: 'DELETE' });
    return { status: r.status, body: await r.text() };
  }, id);
  return { create: create.status, del1, del2 };
}