// ============================================================================
// 名称: check_pwd.js
// 用途: 检查用户名/密码框的当前值和 validity 状态
// 套件: AUTH / AU-03 / AU-04
// 用例: 验证密码长度校验提示（输入 "123" 时显示 6字符 提示）
// 调用: playwright-cli run-code --filename=check_pwd.js
// 说明: 配合 fill 操作后调用，用于调试 HTML5 校验消息
// ============================================================================
async page => {
  const result = await page.evaluate(() => {
    const username = document.querySelector('#auth-username');
    const password = document.querySelector('#auth-password');
    return {
      username: { value: username?.value, validity: username?.validationMessage },
      password: { value: password?.value, validity: password?.validationMessage }
    };
  });
  return result;
}