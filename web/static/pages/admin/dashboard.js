// =============================================================
// pages/admin/dashboard.js — 管理后台 / 题目列表 + 删除
// =============================================================

let cachedList = null;

export async function renderAdminDashboard(root) {
  // 守卫(双保险,App 路由层已经过滤)
  if (!Auth.isAdmin()) {
    history.pushState({}, '', '/admin/login');
    return App.handleRoute();
  }

  root.innerHTML = `
    <div class="page">
      <div class="page-header">
        <div>
          <h1 class="page-title"><span class="accent">/</span> 管理后台 · 题目列表</h1>
          <p class="page-subtitle">创建、查看、删除题目。</p>
        </div>
        <a class="btn btn-primary" href="/admin/problems/new" data-route>${Icons.plus} 新建题目</a>
      </div>
      <div id="adm-host">
        <div class="empty-state"><div class="spinner"></div>&nbsp;&nbsp;加载中…</div>
      </div>
    </div>
  `;

  try {
    const list = await api.get('/api/problems');
    cachedList = list;
    renderTable(root, list);
  } catch (e) {
    if (e.status === 401 || e.status === 403) {
      await Auth.logout();
      history.pushState({}, '', '/admin/login');
      return App.handleRoute();
    }
    document.getElementById('adm-host').innerHTML =
      `<div class="empty-state" style="color:var(--danger)">${esc(e.message)}</div>`;
  }
}

function renderTable(root, list) {
  const host = document.getElementById('adm-host');
  if (!list.length) {
    host.innerHTML = `
      <div class="empty-state">
        <p style="margin:0 0 12px;color:var(--text);font-weight:600">还没有题目</p>
        <a class="btn btn-primary" href="/admin/problems/new" data-route>${Icons.plus} 创建第一道题</a>
      </div>`;
    return;
  }
  host.innerHTML = `
    <table class="admin-table">
      <thead>
        <tr>
          <th style="width:80px">编号</th>
          <th>标题</th>
          <th style="width:120px">时间</th>
          <th style="width:120px">内存</th>
          <th style="width:200px;text-align:right">操作</th>
        </tr>
      </thead>
      <tbody>
        ${list.map(p => `
          <tr>
            <td class="id-cell">#${esc(String(p.id).padStart(3, '0'))}</td>
            <td><strong>${esc(p.title)}</strong></td>
            <td><span class="meta-chip">${esc(p.time_limit_ms)} ms</span></td>
            <td><span class="meta-chip">${esc(p.memory_limit_mb)} MB</span></td>
            <td>
              <div class="row-actions">
                <a class="btn btn-sm" href="/problems/${esc(p.id)}" data-route>${Icons.list} 查看</a>
                <button class="btn btn-sm btn-danger" data-del="${esc(p.id)}" data-title="${esc(p.title)}">${Icons.trash} 删除</button>
              </div>
            </td>
          </tr>
        `).join('')}
      </tbody>
    </table>
  `;
  host.querySelectorAll('button[data-del]').forEach(b => {
    b.addEventListener('click', () => confirmDelete(b.dataset.del, b.dataset.title));
  });
}

function confirmDelete(id, title) {
  const modal = document.createElement('div');
  modal.className = 'modal-backdrop';
  modal.innerHTML = `
    <div class="modal">
      <h3>确认删除题目?</h3>
      <p>「${esc(title)}」及其全部测试用例将被永久删除,且无法恢复。</p>
      <div class="actions">
        <button class="btn" id="cancel">取消</button>
        <button class="btn btn-danger" id="confirm">${Icons.trash} 删除</button>
      </div>
    </div>
  `;
  document.body.appendChild(modal);
  const close = () => modal.remove();
  modal.querySelector('#cancel').addEventListener('click', close);
  modal.addEventListener('click', (e) => { if (e.target === modal) close(); });
  modal.querySelector('#confirm').addEventListener('click', async () => {
    const btn = modal.querySelector('#confirm');
    btn.disabled = true;
    btn.innerHTML = `<span class="spinner"></span> 删除中…`;
    try {
      await api.del(`/api/admin/problems/${id}`);
      cachedList = (cachedList || []).filter(p => String(p.id) !== String(id));
      close();
      renderTable(document, cachedList);
    } catch (e) {
      btn.disabled = false;
      btn.innerHTML = `${Icons.trash} 删除`;
      alert(`删除失败:${e.message}`);
    }
  });
}
