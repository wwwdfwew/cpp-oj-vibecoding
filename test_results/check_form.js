// ============================================================================
// 名称: check_form.js
// 用途: 检查注册表单的 HTML5 校验属性（minLength / pattern）
// 套件: AUTH / AU-03 / AU-04
// 用例: 验证用户名 minLength=3、密码 minLength=6
// 调用: playwright-cli run-code --filename=check_form.js
// 期望: { username: { minLength: 3, validity: "请将该文本增加为 3 个字符或更多..." },
//         password: { minLength: 6, validity: "请将该文本增加为 6 个字符或更多..." } }
// ============================================================================
async page => {
  const result = await page.evaluate(() => {
    const username = document.querySelector('#auth-username');
    const password = document.querySelector('#auth-password');
    return {
      username: { value: username?.value, validity: username?.validationMessage, minLength: username?.minLength, pattern: username?.pattern },
      password: { value: password?.value, validity: password?.validationMessage, minLength: password?.minLength }
    };
  });
  return result;
}