// =============================================================
// pages/admin/problem-form.js — 创建题目
// =============================================================

export async function renderProblemForm(root) {
  if (!Auth.isAdmin()) {
    history.pushState({}, '', '/admin/login');
    return App.handleRoute();
  }

  root.innerHTML = `
    <div class="page">
      <div class="page-header">
        <div>
          <a class="btn btn-ghost btn-sm" href="/admin" data-route style="margin-bottom:12px">${Icons.back} 返回后台</a>
          <h1 class="page-title"><span class="accent">/</span> 新建题目</h1>
          <p class="page-subtitle">填写题目描述、时限/内存限制,以及至少一组测试用例。</p>
        </div>
        <div style="display:flex;gap:8px">
          <button class="btn" id="add-tc">${Icons.plus} 添加测试用例</button>
          <button class="btn btn-primary" id="submit-form">${Icons.play} 创建题目</button>
        </div>
      </div>

      <form id="problem-form" autocomplete="off">
        <section class="detail-card" style="margin-bottom:20px">
          <h2><span class="dot"></span> 基本信息</h2>
          <div style="display:grid;grid-template-columns:2fr 1fr 1fr;gap:14px">
            <div class="form-row" style="margin:0">
              <label for="f-title">题目标题</label>
              <input id="f-title" name="title" required placeholder="例如:两数之和">
            </div>
            <div class="form-row" style="margin:0">
              <label for="f-tl">时间限制 (ms)</label>
              <input id="f-tl" name="time_limit_ms" type="number" min="100" max="10000" step="100" value="1000" required>
            </div>
            <div class="form-row" style="margin:0">
              <label for="f-ml">内存限制 (MB)</label>
              <input id="f-ml" name="memory_limit_mb" type="number" min="16" max="1024" step="16" value="128" required>
            </div>
          </div>
        </section>

        <section class="detail-card" style="margin-bottom:20px">
          <h2><span class="dot"></span> 题目描述</h2>
          <div class="form-row">
            <label for="f-desc">题干</label>
            <textarea id="f-desc" name="description" required style="min-height:160px" placeholder="题目描述…"></textarea>
          </div>
          <div style="display:grid;grid-template-columns:1fr 1fr;gap:14px">
            <div class="form-row" style="margin:0">
              <label for="f-in">输入格式</label>
              <textarea id="f-in" name="input_format" style="min-height:120px" placeholder="描述输入格式…"></textarea>
            </div>
            <div class="form-row" style="margin:0">
              <label for="f-out">输出格式</label>
              <textarea id="f-out" name="output_format" style="min-height:120px" placeholder="描述期望输出…"></textarea>
            </div>
          </div>
        </section>

        <section class="detail-card">
          <h2><span class="dot"></span> 测试用例</h2>
          <div class="testcases" id="tc-host"></div>
        </section>

        <div class="form-error" id="form-err" style="display:none;margin-top:14px"></div>
      </form>
    </div>
  `;

  const host = document.getElementById('tc-host');
  let cases = [{ input: '', expected_output: '' }];

  const rerender = () => {
    host.innerHTML = cases.map((c, i) => `
      <div class="testcase-item" data-i="${i}">
        <span class="idx">#${i + 1}</span>
        <div class="form-row in" style="margin:0">
          <label>输入</label>
          <textarea data-field="input">${esc(c.input)}</textarea>
        </div>
        <div class="form-row out" style="margin:0">
          <label>期望输出</label>
          <textarea data-field="expected_output">${esc(c.expected_output)}</textarea>
        </div>
        <button class="btn btn-sm btn-danger remove" type="button" title="删除该用例">${Icons.trash}</button>
      </div>
    `).join('');
    host.querySelectorAll('.testcase-item').forEach(item => {
      const i = Number(item.dataset.i);
      item.querySelectorAll('textarea').forEach(t => {
        t.addEventListener('input', () => {
          cases[i][t.dataset.field] = t.value;
        });
      });
      const rm = item.querySelector('.remove');
      if (cases.length === 1) {
        rm.disabled = true;
        rm.style.opacity = '0.4';
      } else {
        rm.addEventListener('click', () => {
          cases.splice(i, 1);
          rerender();
        });
      }
    });
  };

  rerender();

  document.getElementById('add-tc').addEventListener('click', () => {
    cases.push({ input: '', expected_output: '' });
    rerender();
  });

  document.getElementById('submit-form').addEventListener('click', submit);
  document.getElementById('problem-form').addEventListener('submit', (e) => { e.preventDefault(); submit(); });

  async function submit() {
    const err = document.getElementById('form-err');
    err.style.display = 'none';

    const payload = {
      title: document.getElementById('f-title').value.trim(),
      description: document.getElementById('f-desc').value.trim(),
      input_format: document.getElementById('f-in').value.trim(),
      output_format: document.getElementById('f-out').value.trim(),
      time_limit_ms: Number(document.getElementById('f-tl').value),
      memory_limit_mb: Number(document.getElementById('f-ml').value),
      test_cases: cases,
    };

    if (!payload.title || !payload.description) {
      err.textContent = '标题和题干不能为空。';
      err.style.display = '';
      return;
    }
    if (!payload.test_cases.length) {
      err.textContent = '至少需要一组测试用例。';
      err.style.display = '';
      return;
    }

    const btn = document.getElementById('submit-form');
    btn.disabled = true;
    btn.innerHTML = `<span class="spinner"></span> 创建中…`;
    try {
      const out = await api.post('/api/admin/problems', payload);
      history.pushState({}, '', `/problems/${out.id}`);
      App.handleRoute();
    } catch (e) {
      err.textContent = e.message || '创建失败';
      err.style.display = '';
      btn.disabled = false;
      btn.innerHTML = `${Icons.play} 创建题目`;
    }
  }
}
