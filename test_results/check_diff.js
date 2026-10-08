async page => {
  const result = await page.evaluate(() => {
    const el = document.querySelector('[data-diff="all"]');
    return { className: el?.className, ariaPressed: el?.getAttribute('aria-pressed'), text: el?.textContent };
  });
  return result;
}