// ============================================================================
// 名称: check_pat.js
// 用途: 检查用户名输入框的 pattern 属性（HTML5 正则约束）
// 套件: AUTH / AU-05
// 用例: 验证非法字符 "bad name!" 触发的错误提示
// 调用: playwright-cli run-code --filename=check_pat.js
// 说明: 本系统未使用 HTML5 pattern 属性，由后端正则校验
//       "仅允许字母/数字/下划线/连字符"，见报告 §4.3 AUTH-05
// ============================================================================
async page => {
  const result = await page.evaluate(() => {
    const username = document.querySelector('#auth-username');
    return {
      value: username?.value,
      validity: username?.validationMessage,
      pattern: username?.pattern
    };
  });
  return result;
}