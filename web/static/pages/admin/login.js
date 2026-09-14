// =============================================================
// pages/admin/login.js — 管理员登录(用通用模态框)
// =============================================================

export async function renderAdminLogin(root) {
  // 未登录:打开登录模态框
  if (!Auth.isLoggedIn()) {
    openAuthModal('login');
    root.innerHTML = `
      <div class="empty-state">
        <p>请在弹窗中登录管理员账号。</p>
        <button class="btn btn-primary" id="reopen-login">重新打开登录框</button>
      </div>`;
    document.getElementById('reopen-login')?.addEventListener('click', () => openAuthModal('login'));
    return;
  }

  // 已登录但不是管理员:提示
  if (!Auth.isAdmin()) {
    root.innerHTML = `
      <div class="empty-state" style="color:var(--danger)">
        <p>当前账号不是管理员,无法进入后台。</p>
        <button class="btn" id="go-home" style="margin-top:12px">返回首页</button>
      </div>`;
    document.getElementById('go-home')?.addEventListener('click', () => {
      history.pushState({}, '', '/');
      App.handleRoute();
    });
    return;
  }

  // 已是管理员:跳到后台
  history.replaceState({}, '', '/admin');
  const { renderAdminDashboard } = await import('/static/pages/admin/dashboard.js');
  await renderAdminDashboard(root);
}
