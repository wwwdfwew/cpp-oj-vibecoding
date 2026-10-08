async page => {
  const result = await page.evaluate(async () => {
    const r = await fetch('/api/admin/problems/18', { method: 'DELETE' });
    return { status: r.status, body: await r.text() };
  });
  return result;
}