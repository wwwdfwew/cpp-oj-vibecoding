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