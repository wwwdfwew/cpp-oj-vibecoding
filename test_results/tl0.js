async page => {
  const result = await page.evaluate(async () => {
    const r = await fetch('/api/admin/problems', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        title: 'AUTOTEST_TL0_' + Date.now(),
        difficulty: 'easy',
        time_limit_ms: 0,
        memory_limit_mb: 128,
        description: 'x',
        input_format: 'x',
        output_format: 'x',
        samples: [{ input: '1', output: '1' }]
      })
    });
    return { status: r.status, body: await r.text() };
  });
  return result;
}