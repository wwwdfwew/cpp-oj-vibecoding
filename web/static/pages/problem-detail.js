// =============================================================
// pages/problem-detail.js —— 题目详情 + CodeMirror 6 编辑器 + 提交
// =============================================================

const TEMPLATE = `#include <bits/stdc++.h>
using namespace std;

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    // 在这里编写你的代码
    return 0;
}
`;

let cmView = null;
let suppressDraft = false;

export async function renderProblemDetail(root, id) {
  if (!id || !/^\d+$/.test(id)) {
    root.innerHTML = `<div class="empty-state">无效的题目编号</div>`;
    return;
  }

  root.innerHTML = `
    <div class="page">
      <div class="page-header">
        <div>
          <a class="btn btn-ghost btn-sm" href="/" data-route style="margin-bottom:12px">${Icons.back} 返回题库</a>
          <h1 class="page-title" id="pd-title"><span class="accent">/</span> <span id="pd-title-text">加载中…</span></h1>
          <div class="problem-meta" id="pd-meta"></div>
        </div>
      </div>

      <div class="detail-layout">
        <section class="detail-card" id="pd-desc-card">
          <h2><span class="dot"></span> 题目描述</h2>
          <div id="pd-body"><div class="spinner"></div></div>
        </section>

        <section class="editor-card">
          <div class="editor-bar">
            <span class="filename">main.cpp</span>
            <div class="actions">
              <button class="btn btn-sm" id="pd-reset" title="恢复默认模板">${Icons.reset} 重置</button>
              <button class="btn btn-sm" id="pd-clear-draft" title="清除已保存的草稿">${Icons.trash} 清空草稿</button>
              <button class="btn btn-primary btn-sm" id="pd-submit"${Auth.isLoggedIn() ? '' : ' disabled title="请先登录"'}>${Icons.play} 提交</button>
            </div>
          </div>
          ${!Auth.isLoggedIn() ? `
            <div class="login-prompt">
              <div class="lp-text">
                <strong>登录后即可提交代码</strong>
                <span>提交会编译并运行你的代码,匿名访问无法使用此功能。</span>
              </div>
              <button class="btn btn-primary btn-sm" id="pd-login-btn">${Icons.user} 立即登录 / 注册</button>
            </div>` : ''}
          <div class="editor-host" id="pd-editor-host">
            <div class="editor-empty">编辑器加载中…</div>
          </div>
          <div class="draft-status" id="pd-draft-status">
            <span id="pd-draft-info">草稿:—</span>
            <span id="pd-saved-info"></span>
          </div>
        </section>
      </div>

      <section class="result-panel" id="pd-result" style="display:none">
        <div class="result-head">
          <span class="label">判题结果</span>
          <span id="pd-result-badge"></span>
        </div>
        <p class="result-message" id="pd-result-message"></p>
        <div class="compile-error" id="pd-compile-error" style="display:none">
          <div class="head">编译错误 (g++)</div>
          <pre id="pd-compile-error-text"></pre>
        </div>
      </section>
    </div>
  `;

  let problem;
  try {
    problem = await api.get(`/api/problems/${id}`);
  } catch (e) {
    root.innerHTML = `
      <div class="page">
        <div class="page-header">
          <a class="btn btn-ghost btn-sm" href="/" data-route>${Icons.back} 返回题库</a>
        </div>
        <div class="empty-state" style="color:var(--danger)">${esc(e.message || '题目不存在')}</div>
      </div>`;
    return;
  }

  document.getElementById('pd-title-text').textContent = problem.title;
  const meta = document.getElementById('pd-meta');
  meta.innerHTML = `
    <span class="meta-chip">编号 <strong>#${esc(String(problem.id).padStart(3, '0'))}</strong></span>
    <span class="meta-chip">时间 <strong>${esc(problem.time_limit_ms)} ms</strong></span>
    <span class="meta-chip">内存 <strong>${esc(problem.memory_limit_mb)} MB</strong></span>
  `;

  const samples = problem.samples || [];
  document.getElementById('pd-body').innerHTML = `
    <div class="section-block">
      <h3>题目描述</h3>
      <p>${esc(problem.description)}</p>
    </div>
    <div class="section-block">
      <h3>输入格式</h3>
      <p>${esc(problem.input_format || '—')}</p>
    </div>
    <div class="section-block">
      <h3>输出格式</h3>
      <p>${esc(problem.output_format || '—')}</p>
    </div>
    ${samples.length ? `
      <div class="section-block">
        <h3>样例</h3>
        <div class="sample-list">
          ${samples.map((s, i) => `
            <div class="sample-item">
              <div class="sample-box">
                <div class="label">输入 · 样例 ${i + 1}</div>
                <pre>${esc(s.input)}</pre>
              </div>
              <div class="sample-box">
                <div class="label">期望输出 · 样例 ${i + 1}</div>
                <pre>${esc(s.expected_output)}</pre>
              </div>
            </div>
          `).join('')}
        </div>
      </div>
    ` : ''}
  `;

  await bootEditor(problem.id);

  document.getElementById('pd-reset').addEventListener('click', () => {
    if (!cmView) return;
    suppressDraft = true;
    cmView.dispatch({ changes: { from: 0, to: cmView.state.doc.length, insert: TEMPLATE } });
    suppressDraft = false;
    updateDraftInfo(problem.id);
  });

  document.getElementById('pd-clear-draft').addEventListener('click', () => {
    if (!cmView) return;
    Drafts.clear(problem.id);
    updateDraftInfo(problem.id);
  });

  document.getElementById('pd-login-btn')?.addEventListener('click', () => openAuthModal('login'));

  document.getElementById('pd-submit').addEventListener('click', () => {
    if (!Auth.isLoggedIn()) {
      openAuthModal('login');
      return;
    }
    onSubmit(problem);
  });
}

async function bootEditor(problemId) {
  const host = document.getElementById('pd-editor-host');
  if (cmView) {
    try { cmView.destroy(); } catch {}
    cmView = null;
  }
  host.innerHTML = '';
  try {
    const [
      { EditorState },
      { EditorView, lineNumbers, highlightActiveLineGutter, highlightActiveLine, keymap },
      cmdsMod,
      { cpp },
      { oneDark },
    ] = await Promise.all([
      import('https://esm.sh/@codemirror/state@6'),
      import('https://esm.sh/@codemirror/view@6'),
      import('https://esm.sh/@codemirror/commands@6'),
      import('https://esm.sh/@codemirror/lang-cpp@6'),
      import('https://esm.sh/@codemirror/theme-one-dark@6'),
    ]);
    const { defaultKeymap, history, historyKeymap, indentWithTab } = cmdsMod;

    const initial = Drafts.get(problemId) || TEMPLATE;
    suppressDraft = true;
    cmView = new EditorView({
      state: EditorState.create({
        doc: initial,
        extensions: [
          lineNumbers(),
          highlightActiveLineGutter(),
          highlightActiveLine(),
          history(),
          keymap.of([...defaultKeymap, ...historyKeymap, indentWithTab]),
          cpp(),
          oneDark,
          EditorView.lineWrapping,
          EditorView.updateListener.of((u) => {
            if (suppressDraft) return;
            if (u.docChanged) {
              Drafts.set(problemId, u.state.doc.toString());
              updateDraftInfo(problemId);
            }
          }),
        ],
      }),
      parent: host,
    });
    suppressDraft = false;
    updateDraftInfo(problemId);
  } catch (e) {
    host.innerHTML = `<div class="editor-empty">编辑器加载失败:${esc(e.message)}<br><small>请检查网络或 CDN 是否可访问。</small></div>`;
  }
}

function updateDraftInfo(problemId) {
  const info = document.getElementById('pd-draft-info');
  const saved = document.getElementById('pd-saved-info');
  if (!info) return;
  const draft = Drafts.get(problemId);
  const bytes = new Blob([draft]).size;
  info.textContent = draft ? `草稿已保存 · ${bytes} 字节` : '草稿:—(使用默认模板)';
  if (saved) saved.textContent = '';
}

async function onSubmit(problem) {
  const btn = document.getElementById('pd-submit');
  const code = cmView ? cmView.state.doc.toString() : '';
  if (!code.trim()) {
    showResult({ status: 'WA', message: '代码为空,请先编写代码再提交。' });
    return;
  }
  btn.disabled = true;
  btn.innerHTML = `<span class="spinner"></span> 判题中…`;

  try {
    const out = await api.post('/api/submit', { problem_id: problem.id, code });
    showResult(out, problem);
  } catch (e) {
    if (e.status === 401) {
      showResult({ status: 'WA', message: e.message || '请先登录后再提交代码' });
      Auth.clear();
      App.renderNav();
    } else {
      showResult({ status: 'WA', message: `请求失败:${e.message}` });
    }
  } finally {
    btn.disabled = false;
    btn.innerHTML = `${Icons.play} 提交`;
  }
}

function showResult(r) {
  const panel = document.getElementById('pd-result');
  const badge = document.getElementById('pd-result-badge');
  const msg = document.getElementById('pd-result-message');
  const ceBox = document.getElementById('pd-compile-error');
  const ceTxt = document.getElementById('pd-compile-error-text');
  panel.style.display = '';

  badge.innerHTML = '';
  msg.classList.remove('is-error', 'is-success');

  if (r.status === 'AC') {
    badge.innerHTML = `<span class="badge badge-ac">AC</span>`;
    msg.textContent = '全部样例通过!';
    msg.classList.add('is-success');
    ceBox.style.display = 'none';
  } else {
    badge.innerHTML = `<span class="badge badge-wa">WA</span>`;
    let m = r.message || '答案错误';
    if (r.failed_case != null) m += ` · 样例 #${r.failed_case + 1}`;
    msg.textContent = m;
    msg.classList.add('is-error');
    if (r.compile_error) {
      ceBox.style.display = '';
      ceTxt.textContent = r.compile_error;
    } else {
      ceBox.style.display = 'none';
    }
  }
  panel.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
}
