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