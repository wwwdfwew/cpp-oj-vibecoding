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