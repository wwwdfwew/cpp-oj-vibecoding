async page => {
  const result = await page.evaluate(async () => {
    const r = await fetch('/api/submit', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ problem_id: 999999, code: 'int main(){return 0;}' })
    });
    return { status: r.status, body: await r.text() };
  });
  return result;
}