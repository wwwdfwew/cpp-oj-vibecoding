// =============================================================
// CPP-OJ Vibecoding — SPA bootstrap & router
// =============================================================

const api = {
  async _fetch(path, opts = {}) {
    const res = await fetch(path, {
      credentials: 'same-origin',
      headers: { 'Content-Type': 'application/json' },
      ...opts,
    });
    let body = null;
    const text = await res.text();
    if (text) {
      try { body = JSON.parse(text); } catch { body = { raw: text }; }
    }
    if (!res.ok) {
      const msg = (body && body.error) || `HTTP ${res.status}`;
      const err = new Error(msg);
      err.status = res.status;
      err.body = body;
      throw err;
    }
    return body;
  },
  get(path)              { return this._fetch(path); },
  post(path, data)       { return this._fetch(path, { method: 'POST', body: JSON.stringify(data) }); },
  del(path)              { return this._fetch(path, { method: 'DELETE' }); },
};

const Drafts = {
  key(problemId) { return `draft:problem:${problemId}`; },
  get(problemId) {
    try { return localStorage.getItem(this.key(problemId)) || ''; }
    catch { return ''; }
  },
  set(problemId, code) {
    try { localStorage.setItem(this.key(problemId), code); return true; }
    catch { return false; }
  },
  clear(problemId) {
    try { localStorage.removeItem(this.key(problemId)); } catch {}
  },
};

// =============================================================
// 登录状态
// =============================================================
const Auth = {
  // sessionStorage: { username, role: 'user' | 'admin' | null }
  current() {
    const username = sessionStorage.getItem('oj_username') || '';
    const role = sessionStorage.getItem('oj_role') || '';
    if (!username) return null;
    return { username, role };
  },
  isLoggedIn() { return !!this.current(); },
  isAdmin() { const u = this.current(); return u && u.role === 'admin'; },

  set(user) {
    sessionStorage.setItem('oj_username', user.username);
    sessionStorage.setItem('oj_role', user.role);
  },
  clear() {
    sessionStorage.removeItem('oj_username');
    sessionStorage.removeItem('oj_role');
  },

  // 启动时调用一次,从服务器 cookie 同步登录态
  async hydrate() {
    try {
      const me = await api.get('/api/me');
      this.set(me);
      return me;
    } catch {
      this.clear();
      return null;
    }
  },

  async login(username, password) {
    const out = await api.post('/api/login', { username, password });
    this.set({ username: out.username || username, role: out.role || 'user' });
    return out;
  },

  async register(username, password) {
    const out = await api.post('/api/register', { username, password });
    this.set({ username: out.username || username, role: out.role || 'user' });
    return out;
  },

  async logout() {
    try { await api.post('/api/logout', {}); } catch {}
    this.clear();
  },
};

// SVG icons (Heroicons-style, 24x24)
const Icons = {
  list: `<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M8 6h13M8 12h13M8 18h13M3 6h.01M3 12h.01M3 18h.01"/></svg>`,
  clock: `<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><path d="M12 6v6l4 2"/></svg>`,
  memory: `<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="6" width="20" height="12" rx="2"/><path d="M6 10v4M10 10v4M14 10v4M18 10v4"/></svg>`,
  shield: `<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/></svg>`,
  user: `<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M20 21v-2a4 4 0 00-4-4H8a4 4 0 00-4 4v2"/><circle cx="12" cy="7" r="4"/></svg>`,
  logout: `<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M9 21H5a2 2 0 01-2-2V5a2 2 0 012-2h4M16 17l5-5-5-5M21 12H9"/></svg>`,
  plus: `<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 5v14M5 12h14"/></svg>`,
  trash: `<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M3 6h18M8 6V4a2 2 0 012-2h4a2 2 0 012 2v2M19 6l-1 14a2 2 0 01-2 2H8a2 2 0 01-2-2L5 6"/></svg>`,
  play: `<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M5 3l14 9-14 9V3z"/></svg>`,
  back: `<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M19 12H5M12 19l-7-7 7-7"/></svg>`,
  reset: `<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M1 4v6h6M23 20v-6h-6"/><path d="M20.49 9A9 9 0 005.64 5.64L1 10m22 4l-4.64 4.36A9 9 0 013.51 15"/></svg>`,
  chevron: `<svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M6 9l6 6 6-6"/></svg>`,
};

function esc(s) {
  if (s == null) return '';
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

// =============================================================
// 登录 / 注册模态框
// =============================================================
function openAuthModal(mode /* 'login' | 'register' */) {
  // 关闭已存在的
  document.getElementById('auth-modal')?.remove();

  const isLogin = mode === 'login';
  const modal = document.createElement('div');
  modal.id = 'auth-modal';
  modal.className = 'modal-backdrop';
  modal.innerHTML = `
    <div class="modal auth-modal" role="dialog" aria-modal="true">
      <button class="modal-close" id="auth-close" aria-label="关闭">×</button>
      <div class="auth-tabs">
        <button class="auth-tab ${isLogin ? 'active' : ''}" data-tab="login">登录</button>
        <button class="auth-tab ${!isLogin ? 'active' : ''}" data-tab="register">注册</button>
      </div>
      <h3>${isLogin ? '欢迎回来' : '创建账号'}</h3>
      <p class="sub">${isLogin ? '登录后即可提交代码、保存草稿,并在管理后台维护题库。' : '注册一个新账号,即可开始刷题之旅。'}</p>
      <form id="auth-form" autocomplete="off">
        <div class="form-row">
          <label for="auth-username">用户名</label>
          <input id="auth-username" name="username" autocomplete="username" required minlength="3" maxlength="32" autofocus placeholder="字母/数字/下划线/连字符">
        </div>
        <div class="form-row">
          <label for="auth-password">密码</label>
          <input id="auth-password" name="password" type="password" autocomplete="${isLogin ? 'current-password' : 'new-password'}" required minlength="6" maxlength="128" placeholder="至少 6 位">
        </div>
        <button class="btn btn-primary btn-lg" id="auth-submit" type="submit" style="width:100%">
          ${isLogin ? '登录' : '注册并登录'}
        </button>
        <div class="form-error" id="auth-err" style="display:none"></div>
      </form>
      <div class="auth-foot">
        ${isLogin
          ? `还没有账号?<a href="#" data-switch="register">立即注册</a>`
          : `已有账号?<a href="#" data-switch="login">直接登录</a>`}
      </div>
    </div>
  `;
  document.body.appendChild(modal);

  const form = modal.querySelector('#auth-form');
  const errEl = modal.querySelector('#auth-err');
  const submitBtn = modal.querySelector('#auth-submit');
  const close = () => modal.remove();
  const switchTo = (m) => { openAuthModal(m); };

  modal.querySelector('#auth-close').addEventListener('click', close);
  modal.addEventListener('click', (e) => { if (e.target === modal) close(); });
  document.addEventListener('keydown', function esc(ev) {
    if (ev.key === 'Escape') { close(); document.removeEventListener('keydown', esc); }
  });
  modal.querySelectorAll('[data-tab]').forEach(t => {
    t.addEventListener('click', () => switchTo(t.dataset.tab));
  });
  modal.querySelector('[data-switch]')?.addEventListener('click', (e) => {
    e.preventDefault();
    switchTo(modal.querySelector('[data-switch]').dataset.switch);
  });

  form.addEventListener('submit', async (ev) => {
    ev.preventDefault();
    errEl.style.display = 'none';
    submitBtn.disabled = true;
    const orig = submitBtn.innerHTML;
    submitBtn.innerHTML = `<span class="spinner"></span> ${isLogin ? '登录中…' : '注册中…'}`;

    const username = modal.querySelector('#auth-username').value.trim();
    const password = modal.querySelector('#auth-password').value;

    try {
      const out = isLogin
        ? await Auth.login(username, password)
        : await Auth.register(username, password);
      close();
      App.renderNav();
      if (out.role === 'admin' && !location.pathname.startsWith('/admin')) {
        // 管理员登录:跳后台
        history.pushState({}, '', '/admin');
        App.handleRoute();
      } else if (location.pathname.startsWith('/admin')) {
        // 已在 /admin 区域:重新跑守卫(可能是登录后继续)
        App.handleRoute();
      } else {
        // 普通用户登录:刷新当前页(如题目详情页要把登录提示换成编辑器)
        App.refreshCurrent();
      }
    } catch (e) {
      errEl.textContent = e.message || (isLogin ? '登录失败' : '注册失败');
      errEl.style.display = '';
      submitBtn.disabled = false;
      submitBtn.innerHTML = orig;
    }
  });

  modal.querySelector('#auth-username').focus();
}

// =============================================================
// App shell + nav
// =============================================================
const App = {
  mount(headerEl, mainEl) {
    this.headerEl = headerEl;
    this.mainEl = mainEl;
    window.addEventListener('popstate', () => this.handleRoute());
    document.body.addEventListener('click', (e) => {
      const a = e.target.closest('a[data-route]');
      if (a) {
        e.preventDefault();
        const href = a.getAttribute('href');
        if (href && href !== location.pathname + location.search) {
          history.pushState({}, '', href);
          this.handleRoute();
        }
      }
    });
    // 启动时从服务器同步登录态
    Auth.hydrate().finally(() => this.handleRoute());
  },

  renderNav() {
    const path = location.pathname;
    const isAdminArea = path.startsWith('/admin');
    const me = Auth.current();

    this.headerEl.innerHTML = `
      <a class="brand" href="/" data-route>
        <span class="brand-mark"></span>
        <span class="brand-name">CPP-OJ <span style="color:var(--accent)">.</span></span>
      </a>
      <nav class="nav-links">
        <a href="/" data-route class="${path === '/' || path === '/index.html' ? 'active' : ''}">题库</a>
        ${me && me.role === 'admin'
          ? `<a href="/admin" data-route class="${path === '/admin' ? 'active' : ''}">管理后台</a>`
          : ''}
      </nav>
      <div class="nav-actions">
        ${me
          ? `<div class="user-menu" id="user-menu">
               <button class="user-btn" id="user-btn">
                 ${Icons.user}
                 <span>${esc(me.username)}</span>
                 ${me.role === 'admin' ? '<span class="role-tag">管理员</span>' : ''}
                 ${Icons.chevron}
               </button>
               <div class="user-dropdown" id="user-dropdown">
                  ${me.role === 'admin'
                    ? `<a href="/admin" data-route>管理后台</a>`
                    : ''}
                 <a href="/" data-route>题库</a>
                 <button id="user-logout">${Icons.logout} 退出登录</button>
               </div>
             </div>`
          : `<button class="btn btn-ghost btn-sm" id="nav-login">登录</button>
             <button class="btn btn-primary btn-sm" id="nav-register">注册</button>`}
      </div>
    `;

    if (me) {
      const userBtn = document.getElementById('user-btn');
      const dropdown = document.getElementById('user-dropdown');
      userBtn?.addEventListener('click', (e) => {
        e.stopPropagation();
        dropdown.classList.toggle('open');
      });
      document.addEventListener('click', () => dropdown?.classList.remove('open'));
      document.getElementById('user-logout')?.addEventListener('click', async (e) => {
        e.preventDefault();
        e.stopPropagation();
        await Auth.logout();
        App.renderNav();
        if (location.pathname.startsWith('/admin')) {
          history.pushState({}, '', '/');
        }
        App.refreshCurrent();
      });
    } else {
      document.getElementById('nav-login')?.addEventListener('click', () => openAuthModal('login'));
      document.getElementById('nav-register')?.addEventListener('click', () => openAuthModal('register'));
    }
  },

  // 仅重新渲染当前页(不重路由),登录/退出后用于刷新数据
  refreshCurrent() {
    return this.handleRoute();
  },

  async handleRoute() {
    const path = location.pathname || '/';
    const main = this.mainEl;
    main.innerHTML = '<div class="empty-state"><div class="spinner"></div></div>';
    this.renderNav();

    try {
      if (path === '/' || path === '/index.html') {
        const { renderProblemList } = await import('/static/pages/problem-list.js');
        await renderProblemList(main);
      } else if (path.startsWith('/problems/')) {
        const id = path.split('/')[2];
        const { renderProblemDetail } = await import('/static/pages/problem-detail.js');
        await renderProblemDetail(main, id);
      } else if (path === '/admin/login') {
        // 直接进 /admin/login:用模态框代替独立页,避免重复维护
        const { renderAdminLogin } = await import('/static/pages/admin/login.js');
        await renderAdminLogin(main);
      } else if (path === '/admin') {
        // 守卫:非管理员跳转到登录
        const me = Auth.current();
        if (!me || me.role !== 'admin') {
          history.pushState({}, '', '/admin/login');
          return this.handleRoute();
        }
        const { renderAdminDashboard } = await import('/static/pages/admin/dashboard.js');
        await renderAdminDashboard(main);
      } else if (path === '/admin/problems/new') {
        const me = Auth.current();
        if (!me || me.role !== 'admin') {
          history.pushState({}, '', '/admin/login');
          return this.handleRoute();
        }
        const { renderProblemForm } = await import('/static/pages/admin/problem-form.js');
        await renderProblemForm(main);
      } else {
        main.innerHTML = `<div class="empty-state">404 · 页面不存在</div>`;
      }
      this.renderNav(); // 渲染后再次刷新(标题/激活态)
    } catch (e) {
      console.error(e);
      main.innerHTML = `<div class="empty-state" style="color:var(--danger)">加载失败:${esc(e.message)}</div>`;
    }
  },
};

window.App = App;
window.api = api;
window.Drafts = Drafts;
window.Auth = Auth;
window.Icons = Icons;
window.esc = esc;
window.openAuthModal = openAuthModal;

document.addEventListener('DOMContentLoaded', () => {
  App.mount(document.getElementById('app-header'), document.getElementById('app'));
});
