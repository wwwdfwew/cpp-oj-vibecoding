// ============================================================================
// 名称: check_diff.js
// 用途: 读取难度 chip 的激活状态（DOM 探针）
// 套件: HOME / DF-01
// 用例: 验证"全部"chip 默认有 `.active` 类
// 调用: playwright-cli run-code --filename=check_diff.js
// 期望: { className: "chip active", ariaPressed: null }
// ============================================================================
async page => {
  const result = await page.evaluate(() => {
    const el = document.querySelector('[data-diff="all"]');
    return { className: el?.className, ariaPressed: el?.getAttribute('aria-pressed'), text: el?.textContent };
  });
  return result;
}