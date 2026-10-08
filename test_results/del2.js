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