async page => {
  const resp = await page.evaluate(async () => {
    const r = await fetch('/api/me');
    return { status: r.status, body: await r.text() };
  });
  return resp;
}