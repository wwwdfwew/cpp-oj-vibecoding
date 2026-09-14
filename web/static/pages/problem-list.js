// =============================================================
// pages/problem-list.js —— 首页 / 题目列表
// =============================================================

export async function renderProblemList(root) {
  root.innerHTML = `
    <div class="page">
      <section class="hero">
        <div class="hero-inner">
          <h1 class="hero-title">在线刷题,即写即判</h1>
          <p class="hero-sub">简洁的 C++ 在线判题系统。挑一道题,写代码,提交,瞬间看到 AC / WA。</p>
          <div class="hero-cta">
            <a class="btn btn-primary btn-lg" href="#problems" id="hero-start">${Icons.play} 开始刷题</a>
            ${!Auth.isLoggedIn()
              ? `<button class="btn btn-ghost btn-lg" id="hero-register">注册账号</button>`
              : `<a class="btn btn-ghost btn-lg" href="/admin/problems/new" data-route>${Icons.plus} 我要出题</a>`}
          </div>
          <ul class="hero-stats" id="hero-stats">
            <li><strong id="stat-problems">—</strong><span>题目</span></li>
            <li><strong>C++17</strong><span>评测语言</span></li>
            <li><strong>≤ 5s</strong><span>P95 延迟</span></li>
            <li><strong>本地</strong><span>单用户</span></li>
          </ul>
        </div>
      </section>

      <div class="page" id="problems">
        <div class="page-header">
          <div>
            <h2 class="page-title"><span class="accent">/</span> 题目列表</h2>
            <p class="page-subtitle">点击任意题目进入编辑器,支持 C++17 评测与本地草稿。</p>
          </div>
        </div>
        <div id="problem-grid-host">
          <div class="empty-state"><div class="spinner"></div>&nbsp;&nbsp;正在加载…</div>
        </div>
      </div>
    </div>
  `;
  document.getElementById('hero-start')?.addEventListener('click', (e) => {
    e.preventDefault();
    document.getElementById('problems')?.scrollIntoView({ behavior: 'smooth' });
  });
  document.getElementById('hero-register')?.addEventListener('click', () => openAuthModal('register'));

  const host = document.getElementById('problem-grid-host');
  try {
    const list = await api.get('/api/problems');
    document.getElementById('stat-problems').textContent = list.length;
    if (!list.length) {
      host.innerHTML = `
        <div class="empty-state">
          <p style="margin:0 0 8px;color:var(--text);font-weight:600">还没有题目</p>
          <p style="margin:0;font-size:13px">请联系管理员添加第一道题。</p>
        </div>`;
      return;
    }
    host.className = 'problem-grid';
    host.innerHTML = list.map(p => `
      <a class="problem-card" href="/problems/${esc(p.id)}" data-route>
        <div class="id">#${esc(String(p.id).padStart(3, '0'))}</div>
        <div class="title">${esc(p.title)}</div>
        <div class="meta">
          <span title="时间限制">${Icons.clock} ${esc(p.time_limit_ms)} ms</span>
          <span title="内存限制">${Icons.memory} ${esc(p.memory_limit_mb)} MB</span>
        </div>
      </a>
    `).join('');
  } catch (e) {
    host.innerHTML = `<div class="empty-state" style="color:var(--danger)">加载失败:${esc(e.message)}</div>`;
  }
}
