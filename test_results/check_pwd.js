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