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