async page => {
  const longCode = 'x'.repeat(70000);
  const result = await page.evaluate(async (code) => {
    const r = await fetch('/api/submit', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ problem_id: 12, code: code })
    });
    return { status: r.status, body: await r.text() };
  }, longCode);
  return result;
}