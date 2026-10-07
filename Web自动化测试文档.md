# cpp-oj-vibecoding —— Web 自动化测试用例文档

> 文档版本：v1.1
> 适用系统版本：SPEC.md v1.4
> 测试对象：`http://193.112.29.233:8088`（生产部署）
> 管理员账号：`admin / admin123`（种子脚本生成，详见 SPEC §2.3）
> 最后更新：2026-10-08

本文档面向 **Web UI / 浏览器端** 自动化测试，覆盖 SPEC 文档中所有用户可见的功能点。测试以浏览器驱动（推荐 Playwright / Selenium WebDriver + Chromium）模拟真人操作，并对后端响应进行断言。
**区别于** `TEST_CASES.md`（侧重服务端 API + 兼容性矩阵）以及 `TEST_REPORT.md`（侧重后端判题 / 沙箱 / 性能数据），本文档**只关注浏览器层**的真实交互与渲染。

> **v1.1 变更**：测试用例**按页面 / 功能模块**作为一级分类（与 `test.md` 对齐），测试类型（SMK/PL/SR/...）作为二级分类。所有用例 ID（如 `SMK-01`、`SB-03`）保持不变，便于跨版本追溯。

---

## 0. 测试环境与前置条件

### 0.1 测试环境

| 项 | 值 |
|---|---|
| 服务地址 | `http://193.112.29.233:8088` |
| 浏览器 | Chromium 120+ / Chrome 120+ / Edge 120+（推荐） |
| 浏览器驱动 | Playwright ≥ 1.40 或 Selenium 4.x + WebDriver |
| 操作系统 | 任意（Linux 容器内 Chromium 也可） |
| 网络 | 可访问 `https://esm.sh`（CodeMirror 6 通过 esm.sh CDN 加载） |
| 屏幕分辨率 | 1440×900（桌面端，SPEC §5 已声明不做移动端适配） |

### 0.2 前置数据假设

- 数据库已通过 `scripts/init_db.sh` 初始化（admin 种子已写入）。
- 题库至少存在 **1 道 easy 题、1 道 medium 题、1 道 hard 题**，建议各 ≥ 1；命名为可识别字符串（例如"两数之和" / "A+B"、"求和"、"字符串反转"），便于按标题搜索。
- 不存在以 "AUTOTEST_DELETE_ME" 开头的题目（测试结束后会清理新建的题）。
- 默认浏览器启动参数：`--no-sandbox --disable-dev-shm-usage --lang=zh-CN`，时区 `Asia/Shanghai`。

### 0.3 测试账号

> **设计原则**：所有自动生成的账号**必须全局唯一**，避免并发跑、跨日重跑、同一会话内多 case 复用同一账号造成撞名。
> 推荐命名：`auto_<role>_<RUN_ID>_<SUFFIX>`，其中 `RUN_ID` = `pytest` 会话级别的 UUID，`SUFFIX` 用于子用例。

| 角色 | 用户名 | 密码 | 说明 |
|---|---|---|---|
| 管理员 | `admin` | `admin123` | 登录后台用，全程固定使用；**已存在于数据库** |
| 普通用户 A | `auto_user_<RUN_ID>` | `Passw0rd!` | 由 fixture 在 session 级注册；AU-01..09 共享此账号 |
| 普通用户 B | `auto_user_<RUN_ID>_perm` | `Passw0rd!` | 仅用于"普通用户调 admin API" 等权限否定场景 |
| 冲突测试专用 | `auto_user_<RUN_ID>_dup` | `Passw0rd!` | AU-02 专用：先注册一次，然后在 AU-02 中再次注册同名账号触发 409 |

**`RUN_ID` 生成示例（Python）**：

```python
import uuid
RUN_ID = uuid.uuid4().hex[:8]            # 例: "3f7a91c2"
# → auto_user_3f7a91c2 / auto_user_3f7a91c2_perm / auto_user_3f7a91c2_dup
```

> **绝对禁止**：
> 1. 使用 `int(time.time())` —— 秒级精度，并发或同秒重跑必撞名；AU-02 会假阳性失败。
> 2. 使用 `auto_user_1`、`auto_user_2` 等短序号 —— 跨会话必撞名。
> 3. 在多个用例里**各自独立生成**账号 —— AU-02 冲突测试将失去"已知存在的目标"。
>
> **session 级 fixture 模式（推荐）**：在 `conftest.py` 用 `autouse=True & scope="session"` 注册一次 `RUN_ID` 派生账号并 yield，整套件内 `AU-*` 共享同一身份，避免重复注册和顺序耦合。

> 普通用户账号在测试结束后**通过清理脚本删除**（见 §10.3），不留垃圾数据。
> 严禁用 admin 账号执行"普通用户权限不足"场景，避免污染 admin session。

### 0.4 关键定位器约定

| 元素 | 推荐 selector | 说明 |
|---|---|---|
| 题列表卡片 | `.problem-card` | 点击进入单题 |
| 题列表空状态 | `#problem-grid-host .empty-state` | 无题 / 无搜索结果 |
| 搜索框 | `#pl-search` | 题列表页 |
| 搜索清空按钮 | `#pl-search-clear` | 仅在有输入时显示 |
| 难度 chip | `#diff-chips .chip[data-diff="easy\|medium\|hard\|all"]` | 4 个 |
| 单题标题 | `#pd-title-text` | 单题详情页 |
| 单题元数据 | `#pd-meta` | 编号 / 难度 / 时间 / 内存 |
| 编辑器宿主 | `#pd-editor-host .cm-editor` | CodeMirror 6 |
| 提交按钮 | `#pd-submit` | 未登录时被 `disabled` |
| 判题结果面板 | `#pd-result` | 隐藏时 `style.display == 'none'` |
| 状态徽标 | `#pd-result-badge .badge` | `.badge-ac` / `.badge-wa` |
| 编译错误框 | `#pd-compile-error` | 仅 CE 时显示 |
| 草稿状态 | `#pd-draft-info` | 显示已保存字节数 |
| 后台题列表 | `.admin-table tbody tr` | |
| 删除按钮 | `button[data-del]` | |
| 删除确认模态框 | `.modal-backdrop` | `确认删除题目?` |
| 新建题表单 | `#problem-form` | 后台 `/admin/problems/new` |
| 登录/注册模态框 | `#auth-modal` | 任意页弹出 |
| 导航用户菜单 | `#user-menu` | 已登录显示 |

---

## 1. 测试套件总览

### 1.1 按页面 / 功能模块分类（一级）

| 套件 ID | 名称 | 用例数 | 优先级 | 关联 SPEC |
|---|---|---|---|---|
| **HOME** | 首页 / 题库列表 | 20 | P0 | §1.3, §4.1, §8 |
| **DETAIL** | 单题详情页 | 40 | P0 | §1.5, §2.6, §4.1 |
| **AUTH** | 登录注册 | 9 | P0 | §1.2 |
| **ADMIN_LOGIN** | 管理员登录页 | 2 | P0 | §1.3 |
| **ADMIN_LIST** | 管理员后台 - 题库列表 | 9 | P0 | §1.3, §4.1 F-08..10 |
| **ADMIN_CREATE** | 管理员后台 - 新建题目 | 6 | P0 | §1.3, §4.1 F-15..16 |
| **ROUTING** | 路由切换与 SPA 行为 | 6 | P1 | §2.6, §4.1 F-11 |
| **SESSION** | 会话管理 | 1 | P1 | §4.2 N-08 |
| **合计** |  | **93** |  |  |

### 1.2 按测试类型分类（二级，交叉矩阵）

| 测试类型 | HOME | DETAIL | AUTH | ADMIN_LOGIN | ADMIN_LIST | ADMIN_CREATE | ROUTING | SESSION | 合计 |
|---|---|---|---|---|---|---|---|---|---|
| **SMK** 冒烟与基础可用性 | 4 | 1 | 0 | 1 | 1 | 1 | 0 | 0 | **8** |
| **PL** 题库浏览 | 4 | 8 | 0 | 0 | 0 | 0 | 0 | 0 | **12** |
| **SR** 题目搜索 | 6 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | **6** |
| **DF** 难度筛选 | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | **5** |
| **ED** 在线编辑 | 0 | 6 | 0 | 0 | 0 | 0 | 0 | 0 | **6** |
| **DR** 草稿持久化 | 0 | 5 | 0 | 0 | 0 | 0 | 0 | 0 | **5** |
| **SB** 提交判题 | 0 | 9 | 0 | 0 | 0 | 0 | 0 | 0 | **9** |
| **AU** 普通用户认证 | 0 | 0 | 9 | 0 | 0 | 0 | 0 | 0 | **9** |
| **AD** 管理员后台 | 0 | 0 | 0 | 1 | 5 | 4 | 0 | 0 | **10** |
| **RT** 路由切换 | 0 | 0 | 0 | 0 | 0 | 0 | 6 | 0 | **6** |
| **NF** 非功能 | 1 | 5 | 0 | 0 | 0 | 0 | 0 | 1 | **7** |
| **EX** 边界与异常 | 0 | 6 | 0 | 0 | 3 | 1 | 0 | 0 | **10** |
| **合计** | **20** | **40** | **9** | **2** | **9** | **6** | **6** | **1** | **93** |

### 1.3 测试类型代号说明

| 代号 | 全称 | 用途 |
|---|---|---|
| **SMK** | Smoke | 冒烟与基础可用性（进站、静态资源、SPA fallback） |
| **PL** | Problem List | 题库浏览（题列表 + 单题详情元数据） |
| **SR** | Search | 题目搜索 |
| **DF** | Difficulty Filter | 难度筛选 |
| **ED** | Editor | 在线编辑 / CodeMirror |
| **DR** | Draft | 草稿持久化（localStorage） |
| **SB** | Submit | 提交判题（AC / WA / CE / TLE / MLE / RE） |
| **AU** | Auth (User) | 普通用户认证（登录 / 注册 / 注销） |
| **AD** | Admin | 管理员后台（登录 / CRUD） |
| **RT** | Routing | 路由切换与 SPA 行为 |
| **NF** | Non-Functional | 非功能（性能 / 资源 / 安全） |
| **EX** | Exception | 边界与异常 |

---

## 2. 首页 / 题库列表（Home / Problem List）

> 对应路由：`/`（SPA fallback 后 `index.html` 渲染题列表）
> 套件 ID：**HOME**（20 用例）

### 2.1 冒烟与基础可用性（SMK）

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| SMK-01 | 入口页加载 | — | 浏览器打开 `http://193.112.29.233:8088/` | HTTP 200；`<title>` 含 "CPP-OJ"；`<main id="app">` 渲染题列表卡片 `.problem-card`；底部加载 CodeMirror 模块无 console error |
| SMK-04 | 静态资源 200 | — | 并行请求 `/static/style.css`、`/static/app.js`、`/static/pages/problem-list.js`、`/static/pages/problem-detail.js`、`/static/pages/admin/dashboard.js`、`/static/pages/admin/problem-form.js` | 全部 200，Content-Type 正确 |
| SMK-05 | API 健康检查 | — | `GET /api/problems` | 200；JSON 数组；元素含 `id/title/difficulty/time_limit_ms/memory_limit_mb` |
| SMK-06 | 不存在的 API 路径 | — | `GET /api/nope` | 返回 404 或 200 + JSON（不限具体 body 形态，但不能 500） |

### 2.2 题库浏览（PL）

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| PL-01 | 题列表默认加载 | — | 打开 `/`，等待 `.problem-card` 出现 | 每张卡片显示：`#NNN` 编号、标题、`difficulty` 徽标、时间 ms、内存 MB；无白屏 |
| PL-02 | 题列表计数与 hero stats 一致 | — | 读 `#stat-problems` 文本，与 `GET /api/problems` 数组长度对比 | 一致 |
| PL-03 | 题列表为空时占位 | 题库 0 题（管理员先全删） | 打开 `/` | 显示 "还没有题目" + 提示文案；不抛错 |
| PL-12 | 题列表卡片点击进入 | — | 点任一 `.problem-card` | URL 切到 `/problems/<id>`；无白屏过渡 |

### 2.3 题目搜索（SR）

> 关联 SPEC §8.2、F-12：120ms 防抖，对**已拉取的题列表**做本地过滤（不再发请求）。

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| SR-01 | 输入关键词命中 | 题库含 "A+B" | 在 `#pl-search` 输入 `A+B` | 等待 200ms；列表只剩标题含 "A+B" 的题；其它隐藏 |
| SR-02 | 关键词清空恢复 | SR-01 后 | 点 `#pl-search-clear` | 列表恢复全部 |
| SR-03 | 大小写不敏感 | 题库含 "BFS 模板" | 输入 `bfs` | 列表命中 |
| SR-04 | 中文关键词 | 题库含 "两数之和" | 输入 `两数` | 列表命中 |
| SR-05 | 无命中结果 | 题库 4 题 | 输入 `ZZZZ_NO_HIT` | 显示 "没有匹配的题目" |
| SR-06 | 防抖与即时过滤 | — | 监听 `/api/problems` 请求；连续输入 "AB"、"ABC"、"ABCD" | 1.5s 内只触发 0 次网络请求（前端纯前端过滤） |

### 2.4 难度筛选（DF）

> 关联 SPEC §8.1、F-13..16。

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| DF-01 | 全部 chip 默认激活 | — | 打开 `/` | `#diff-chips .chip[data-diff="all"]` 有 `.active` 类 |
| DF-02 | 切到简单 | 题库有 easy | 点 `[data-diff="easy"]` | 该 chip 激活；列表只剩 `difficulty=easy`；计数显示 N |
| DF-03 | 切到中等 | — | 点 `[data-diff="medium"]` | 列表只剩 medium；计数正确 |
| DF-04 | 切到困难 | 题库有 hard | 点 `[data-diff="hard"]` | 列表只剩 hard；计数正确 |
| DF-05 | 搜索 + 难度叠加 | 题库既有 easy "A+B"，又有 medium "BFS" | 切到 easy；再在搜索框输入 "BFS" | 结果 0；切回 easy+搜索 "A+B" → 命中 1 |

### 2.5 非功能（NF）

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| NF-01 | 首屏可交互时间 | — | 用 Playwright `performance.timing` / `LargestContentfulPaint` | LCP < 2.5s（服务端与本地同网段） |

---

## 3. 单题详情页（Problem Detail）

> 对应路由：`/problems/<id>`（SPA fallback 后由前端按路由渲染）
> 套件 ID：**DETAIL**（40 用例）

### 3.1 冒烟与基础可用性（SMK）

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| SMK-02 | 直链刷新单题 | 题库存在任意题 id=X | 直接打开 `http://193.112.29.233:8088/problems/X` | 200；SPA fallback 把任意 path 都返回 `index.html`；前端按 `/problems/X` 路由渲染单题详情，元数据 chip 完整 |

### 3.2 题库浏览（PL）

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| PL-04 | 单题详情加载 | 题 id=X | 点卡片 `/problems/X` | 标题渲染到 `#pd-title-text`；元数据含 `#NNN / 难度 / 时间 / 内存`；题干、输入输出格式、样例至少一组全部渲染 |
| PL-05 | 单题默认模板填充 | 题 id=X，未登录 | 进入 `/problems/X` | CodeMirror 编辑器初始化；内容为 `TEMPLATE`（含 `bits/stdc++.h` + `int main(){...}`） |
| PL-06 | 单题样例渲染多组 | 题有 ≥2 个 samples | 进入 `/problems/X` | `.sample-item` 数 ≥ 2；每个含 "输入·样例 N" 与 "期望输出·样例 N" 标签 |
| PL-07 | 单题 XSS 转义 | 题干含 `<script>alert(1)</script>` 或 `<img onerror>` | 进入 `/problems/X` | 文本原样显示，未执行脚本；DevTools 检查 `<script>` 标签未插入 |
| PL-08 | 不存在题目 | 题 id=999999 | 进入 `/problems/999999` | 渲染 "题目不存在"（来自 `GET /api/problems/999999` 抛 404）；不白屏 |
| PL-09 | 无效路径 | — | 进入 `/problems/abc` | 显示 "无效的题目编号" |
| PL-10 | 单题 meta 与 difficulty 一致 | 后台设 difficulty=hard | 进入 `/problems/X`，观察 `.diff-badge` | class 含 `diff-hard`，文字 "困难" |
| PL-11 | 单题导航返回 | 题 id=X | 点 `返回题库` 按钮 | URL 变 `/`；题列表正常渲染 |

### 3.3 在线编辑 / CodeMirror（ED）

> 关联 SPEC §2.6、F-03。

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| ED-01 | 编辑器初始化 | 进入单题 | 等 2s | `.cm-editor` 出现；行号、高亮、语法着色存在 |
| ED-02 | 输入字符可写 | — | 在编辑器敲 `cout << 1;` | 文档变更；字符数 +N；行号更新 |
| ED-03 | Tab 缩进 | — | 光标置于行首，按 Tab | 插入 2/4 空格缩进，未跳出编辑器 |
| ED-04 | Undo / Redo | — | 输入后 Ctrl+Z | 内容回退；Ctrl+Shift+Z 前进 |
| ED-05 | 重置回默认 | ED-02 后 | 点 `#pd-reset` | 编辑器内容 == `TEMPLATE`；`#pd-draft-info` 重新显示 |
| ED-06 | 清空草稿 | 已保存草稿 | 点 `#pd-clear-draft` | localStorage 中 `draft:problem:<id>` 被移除；`#pd-draft-info` 显示 "草稿:—" |

### 3.4 草稿持久化（DR）

> 关联 SPEC §1.1、F-04。

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| DR-01 | 刷新仍在 | 题 id=X，已在编辑器输入 `xxx` | F5 刷新 `/problems/X` | 编辑器内容 == `xxx`；`#pd-draft-info` 显示 "草稿已保存 · N 字节" |
| DR-02 | 关闭浏览器仍在 | DR-01 | `context.close()` 后重新 `browser.new_context()`，打开 `/problems/X` | 编辑器内容仍是 `xxx`（localStorage 是 origin 级） |
| DR-03 | 草稿隔离 | 题 A、B | 在 A 输入 `aaa`，在 B 输入 `bbb` | 互不污染 |
| DR-04 | 草稿空则默认模板 | 题 id=X | 清空 localStorage 中 `draft:problem:X`，刷新 | 编辑器 == `TEMPLATE` |
| DR-05 | 草稿超限降级 | — | 用 JS 注入一段 > 5MB 字符串到 `draft:problem:X` | 刷新页面不抛 fatal；编辑框显示该内容；如超 localStorage 配额应降级而非崩溃 |

### 3.5 提交判题（SB）

> 关联 SPEC §1.5、F-05..07、N-03..05。
> **前置**：先以普通用户 `auto_user_*` 登录；选一道 **A+B** 类的简单题，预期用例：输入 `1 2` → 输出 `3`。
> 提交按钮：`#pd-submit`，点击后变 `判题中…`，结果在 `#pd-result`。

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| SB-01 | 正确解 → AC | A+B 题 + `USER_MAIN` 已登录 | 提交 `int a,b;cin>>a>>b;cout<<a+b;` | `#pd-result` 显示；徽标 `.badge-ac`，文字 "全部样例通过!"；无 `#pd-compile-error` |
| SB-02 | 输出错误 → WA Wrong Answer | A+B 题 + `USER_MAIN` 已登录 | 提交 `cout<<0;`（强制输出与样例不符） | 徽标 `.badge-wa`；`#pd-result-message` 文本含 "Wrong Answer" |
| SB-03 | 编译错误 CE | A+B 题 | 提交缺分号 `int a,b;cin>>a>>b;cout<<a+b` | 徽标 WA；message 含 "Compile Error"；`#pd-compile-error` 显示且包含 g++ stderr（含 `error:` 行） |
| SB-04 | 超时 TLE | 题 N 大且 time_limit=1s | 提交 O(N²)：双层 for 循环遍历 N=50000 | 徽标 WA；message 含 "Time Limit Exceeded" |
| SB-05 | 超内存 MLE | 题 memory_limit=16 MB | 提交 `vector<int> v; while(1) v.emplace_back(1);` | 徽标 WA；message 含 "Memory Limit Exceeded" |
| SB-06 | 运行时错误 RE | — | 提交 `int x = 1/0;` | 徽标 WA；message 含 "Runtime Error"（不强制等于 exit 数值，前缀匹配即可） |
| SB-07 | 空代码 → WA | — | 清空编辑器后点提交 | 提示 "代码为空"（前端拦截）；不发起网络请求 |
| SB-08 | 未登录提交 | 干净 context | 打开单题 → 点 `#pd-submit`（应被 `disabled`） | 按钮不可点；或被前端拦截弹出 `#auth-modal` |
| SB-09 | 提交中按钮禁用 | — | 提交后立刻重试点 `#pd-submit` | 按钮变 `判题中…` 且 `disabled=true`；判题完成恢复 |

### 3.6 边界与异常（EX）

> 关联 SPEC §4.3 E-01..06。

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| EX-01 | 提交空 code | — | `POST /api/submit {problem_id, code:""}` | 400 |
| EX-02 | 提交超长 code | — | `POST /api/submit {problem_id, code: <70KB>}` | 413 |
| EX-03 | 提交不存在 problem_id | — | `POST /api/submit {problem_id: 999999, code}` | 404 |
| EX-04 | system("rm -rf /") | — | 提交 `if (system("rm -rf /tmp/oj_test 2>/dev/null")) {}` | 服务侧 tmp 残留无变化；UI 拿到 WA |
| EX-05 | 二进制 NUL | 题用例含 NUL 字节 | 提交正确解 | AC（文本模式读写不挂） |
| EX-06 | 末尾多空行 | 题期望 `3\n\n\n` | 提交输出 `3\n\n` | AC |

### 3.7 非功能（NF）

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| NF-02 | 单次提交 P95 | — | 提交正确解 20 次，记录端到端耗时 | P95 ≤ 5000ms（SPEC §4.2 N-01） |
| NF-03 | 并发提交串行化 | — | 打开两个标签，都点提交 | 服务端日志显示第二个等第一个完成；浏览器侧均得到正确结果 |
| NF-04 | while(1) 不会卡死服务 | — | 提交死循环代码 | 在 TL 内被 kill；UI 拿到 WA "Time Limit Exceeded"；服务仍可响应其它请求 |
| NF-05 | 输出超 64MB → 截断不崩 | 题 special：期望输出固定 32MB | 提交代码打印 100MB | 服务不挂；UI 拿到结果（WA 或按预期） |
| NF-07 | 草稿写入频率 | — | 1s 内连续输入 50 字符 | localStorage 写入节流（每次 docChanged 写一次，可接受） |

---

## 4. 登录注册（Login / Register）

> 对应：导航栏 `#nav-login` / `#nav-register` 弹出的 `#auth-modal`；提交走 `/api/register` / `/api/login`。
> 套件 ID：**AUTH**（9 用例）

### 4.1 普通用户认证（AU）

> **账号管理约定**：本套件所有用例共用一组由 session fixture 提前注册的账号（见 §0.3）：
> - `USER_MAIN` = `auto_user_<RUN_ID>`（用于 AU-01 注册成功 / AU-06..09 登录登出）
> - `USER_DUP`  = `auto_user_<RUN_ID>_dup`（**先注册一次**再在 AU-02 中再次提交触发冲突）
> - `USER_PERM` = `auto_user_<RUN_ID>_perm`（用于 §6 AD-02 "普通用户访问后台被拒"）
>
> **禁止**每个用例自己即时注册——会导致：① 并发跑时撞名；② AU-02 失去"目标账号"；③ 失败用例重跑时残留账号影响断言。

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| AU-01 | 注册成功 | 干净 context | 打开 `/`，点右上"注册" → `#auth-modal` 出现 → 填 `USER_MAIN` / `Passw0rd!` → 提交 | 模态框关闭；导航右上显示 `USER_MAIN`；`/api/me` 返回 200 且 `role=user` |
| AU-02 | 注册失败：用户名冲突 | `USER_DUP` **已在 fixture 中注册成功** | 再次填写 `USER_DUP` 提交注册 | 模态框底部 `.form-error` 显示"该用户名已被占用"（或同义错误文案，**HTTP 409**）；不跳路由；`USER_DUP` 的旧 session 不被踢出 |
| AU-03 | 注册失败：用户名长度 | — | 用户名 `ab`（<3） | `.form-error` 显示用户名长度提示 |
| AU-04 | 注册失败：密码长度 | — | 密码 `123`（<6） | `.form-error` 显示密码长度提示 |
| AU-05 | 注册失败：非法字符 | — | 用户名 `bad name!` | `.form-error` 显示 "仅允许字母/数字/下划线/连字符" |
| AU-06 | 登录成功 | 干净 context（清 cookie） | 右上"登录" → 填 `USER_MAIN` / `Passw0rd!` → 提交 | 右上变 `USER_MAIN`；`/api/me` 200；导航不出现 "管理后台"（角色 user） |
| AU-07 | 登录失败：错密码 | — | 密码错 1 位 | `.form-error` 显示 "登录失败"（不应透露 "用户不存在" vs "密码错误"，避免账号枚举） |
| AU-08 | 退出登录 | AU-06 已登录 | 右上点用户菜单 → "退出登录" | `POST /api/logout` 200；sessionStorage 清空；右上变 "登录 / 注册" |
| AU-09 | 会话刷新保持 | AU-06 已登录 | F5 刷新 | `Auth.hydrate()` 自动从 `/api/me` 恢复登录态；导航不闪；编辑器内容仍在 |

> **AU-02 排错指南**：
> - 若 `.form-error` 显示"用户名长度"等其它错误 → 说明 fixture 没有把 `USER_DUP` 注册成功（**前置失败**），而非被测系统错误。
> - 若登录后 `USER_DUP` 的旧 session 仍可访问 → 后端有"重复登录挤下线"或"不踢旧 session"问题，提交开发修。
> - 若该用例因 fixture 注册失败而失败 → 标记为 **fixture error**，不要计入"被测系统 bug"。

---

## 5. 管理员登录页（Admin Login）

> 对应路由：`/admin/login`
> 套件 ID：**ADMIN_LOGIN**（2 用例）

### 5.1 冒烟与基础可用性（SMK）

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| SMK-08 | 错误码：admin 错密码 | — | `POST /api/admin/login {username:"admin",password:"wrong"}` | 401；body 含 `error` |

### 5.2 管理员后台（AD）

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| AD-01 | 管理员登录成功 | — | `/admin/login` → 模态框填 `admin/admin123` | 登录后自动跳 `/admin`，渲染 `.admin-table`；导航出现 "管理后台" 链接 |

---

## 6. 管理员后台 - 题库列表（Admin Panel - List）

> 对应路由：`/admin`（列表页）
> 套件 ID：**ADMIN_LIST**（9 用例）

### 6.1 冒烟与基础可用性（SMK）

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| SMK-03 | 直链刷新后台列表 | 已登录 admin | 浏览器打开 `/admin` | 渲染 `.admin-table`，行数 == `/api/problems` 返回条数 |

### 6.2 管理员后台（AD）

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| AD-02 | 普通用户登录后台被拒 | `USER_PERM` 已登录 | 访问 `/admin` | 跳 `/admin/login`；显示 "当前账号不是管理员"；不渲染 `.admin-table` |
| AD-03 | 后台列表含难度列 | 题库有不同难度 | 进 `/admin` | 表格列含 `编号 / 标题 / 难度 / 时间 / 内存 / 操作`；难度列展示对应 badge |
| AD-08 | 删除题目：确认弹窗 | 选中 AD-04 新建的题 | 点该行 `#data-del` | 弹出 "确认删除题目?" 模态框；点 "取消" → 关闭且未删 |
| AD-09 | 删除题目：确认执行 | AD-08 | 点 "删除" | 请求 200；表格行消失；普通用户侧题列表与 `/problems/<id>` 404 |
| AD-10 | 两次删除同题 | AD-09 后 | 再点同 id 删除 | 后端 404；`alert('删除失败:…')` 触发 |

### 6.3 边界与异常（EX）

> 关联 SPEC §4.3 E-08..10。

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| EX-08 | admin API 未登录 | 干净 context | `DELETE /api/admin/problems/1` | 401 |
| EX-09 | admin API 普通用户 | 普通用户登录 | 同上 | 403 |
| EX-10 | 同一 ID 二次删除 | 题已删 | 再发 `DELETE /api/admin/problems/<id>` | 404 |

---

## 7. 管理员后台 - 新建题目（Admin Panel - Create）

> 对应路由：`/admin/problems/new`
> 套件 ID：**ADMIN_CREATE**（6 用例）

### 7.1 冒烟与基础可用性（SMK）

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| SMK-07 | 错误码：未登录访问 admin API | 干净 context | `POST /api/admin/problems` 不带 cookie | 401；body 含 `error` 字段 |

### 7.2 管理员后台（AD）

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| AD-04 | 新建题目成功 | admin | 点"新建题目" → 填标题 "AUTOTEST_<ts>" / 难度=easy / TL=1000 / ML=128 / 题干 / 输入输出 / 1 组用例 `1 2\n3` → 提交 | 创建成功；自动跳 `/problems/<new_id>`；单题详情正常渲染；题列表能搜到 |
| AD-05 | 新建题目：缺标题 | — | 留空标题 | 不发请求，前端 `.form-error` 显示 "标题和题干不能为空" |
| AD-06 | 新建题目：动态增删用例 | — | 点 `#add-tc` 多次；删到 1 个 | 删除按钮禁用；继续点 `add-tc` 可恢复 |
| AD-07 | 新建题目：默认难度 medium | — | 打开 `/admin/problems/new` | `#f-diff` 默认值 `medium` |

### 7.3 边界与异常（EX）

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| EX-07 | time_limit=0 拒绝 | admin | `POST /api/admin/problems` 带 `time_limit_ms:0` | 400 |

---

## 8. 路由切换与 SPA 行为（Routing）

> 覆盖所有页面之间的导航、刷新、前进/后退、直链、深链接。
> 套件 ID：**ROUTING**（6 用例）

### 8.1 路由切换与 SPA 行为（RT）

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| RT-01 | 列表 ↔ 单题 | — | 列表 → 单题 → 返回 | 无白屏；无整页 reload（DevTools Network 不应出现 `document` 类型请求） |
| RT-02 | 单题 ↔ 后台 | admin | 单题 → 顶部"管理后台" → 单题 | 切换无白屏；编辑器内容保留（草稿不丢） |
| RT-03 | 浏览器后退 / 前进 | — | `/` → `/problems/X` → 后退 → 前进 | popstate 正确触发 `App.handleRoute()`；视图对应 |
| RT-04 | 直链打开未登录 `/admin` | 未登录 | 直接访问 `/admin` | 跳 `/admin/login`；显示提示 |
| RT-05 | 直链打开已登录非管理员 `/admin` | 普通用户登录态 | 直接访问 `/admin` | 跳 `/admin/login` 并显示 "当前账号不是管理员" |
| RT-06 | 直链打开 `/admin/problems/new` 已登录 admin | — | 打开 → 渲染表单 | 路由正确 |

---

## 9. 会话管理（Session）

> 覆盖 session 生命周期中的边界情况（过期、清理）。
> 套件 ID：**SESSION**（1 用例）

### 9.1 非功能（NF）

| ID | 用例 | 前置 | 步骤 | 期望 |
|---|---|---|---|---|
| NF-06 | Session 过期 | — | 把 session `expires_at` 改为过去时刻；admin 调 `/api/admin/problems` | 401；前端跳 `/admin/login` |

---

## 10. 测试数据准备与清理

### 10.1 测试前

```bash
# 1) 确认服务可达
curl -fsS http://193.112.29.233:8088/api/problems | jq 'length'

# 2) 准备 3 道不同难度的种子题(若题库不足)
# 建议:
#   - 标题:"两数之和 / A+B"  difficulty=easy  TL=1000  ML=128
#                          input:  "1 2"  output: "3"
#                          input:  "10 20" output:"30"
#   - 标题:"求和 N 个整数" difficulty=medium TL=1000 ML=128
#                          input:  "3\n1 2 3" output: "6"
#   - 标题:"反转字符串"  difficulty=hard   TL=1000 ML=128
#                          input:  "abc" output: "cba"
```

### 10.2 测试中

- **每个测试文件用独立的 `browser.new_context()`** 隔离 cookie / localStorage。
- **不要每个用例单独注册账号**——所有 `auto_*` 账号**只在 session 开始时由 fixture 一次性注册**，整会话复用。
- 普通用户账号命名：**必须包含 `RUN_ID`（UUID hex 前 8 位）**，禁止 `int(time.time())` 等秒级精度拼接；推荐 `auto_<role>_<RUN_ID>_<SUFFIX>`。
- 同一进程内并发跑多个 worker（`pytest -n 4`）时，每个 worker 应有**自己的 `RUN_ID`**（从 `os.getpid()` 或 worker id 派生），否则仍可能撞名。
- 新建的题标题前缀统一为 `AUTOTEST_<RUN_ID>_<SEQ>`，便于清理。
- `pytest -n 4` 并发跑时，每个 worker 应有**自己的 `RUN_ID`**（从 worker id 派生，如 `f"{os.getpid()}_{uuid4().hex[:6]}"`），避免 worker 间撞名。

### 10.3 测试后

> **必须同时清理题与用户**，否则 `auto_user_*` 长期残留会污染用户表与 `/api/me` 行为。

```bash
RUN_ID="<本次会话 UUID 前 8 位>"   # 与 §0.3 一致

# 1) admin 登录拿 cookie
curl -s -c jar -b jar -X POST http://193.112.29.233:8088/api/admin/login \
  -H 'Content-Type: application/json' \
  -d '{"username":"admin","password":"admin123"}'

# 2) 删除本次会话新建的题
for id in $(curl -s -b jar http://193.112.29.233:8088/api/problems | \
  jq -r --arg r "AUTOTEST_${RUN_ID}" \
         '.[] | select(.title | startswith($r)).id'); do
  curl -s -b jar -X DELETE "http://193.112.29.233:8088/api/admin/problems/$id"
done

# 3) 删除本次会话注册的普通用户
#    注意:服务端没提供 DELETE /api/admin/users 接口,
#    现阶段只能直接清 DB (需要 mysql 客户端);或保留观察。
#    后续可在 /api/admin/users 上加 DELETE 后改为:
#   for uname in $(mysql -h<host> oj -N -e \
#     "SELECT username FROM users WHERE username LIKE 'auto_%_${RUN_ID}%'"); do
#     curl -s -b jar -X DELETE "http://193.112.29.233:8088/api/admin/users/$uname"
#   done
```

> **fallback**：若不能直接清 DB，建议在 CI 中用单独 `oj_test` 数据库跑自动化，避免污染生产库。

---

## 11. 框架与代码示例

### 11.1 推荐栈

| 选项 | 优点 |
|---|---|
| **Playwright (Python / Node)** | 自带等待、自动重试、原生多 context 隔离、trace viewer 调试；首选 |
| Selenium 4 + pytest | 团队熟悉、生态成熟；等待需用 `expected_conditions` |

### 11.2 Playwright (Python) 示例

```python
import os, uuid, requests
import pytest
from playwright.sync_api import expect, Page

BASE = "http://193.112.29.233:8088"
ADMIN = ("admin", "admin123")
PWD   = "Passw0rd!"

# === 会话级唯一种子(避免跨 worker / 跨重跑撞名)===
RUN_ID = f"{os.getpid()}_{uuid.uuid4().hex[:8]}"          # 例: "14213_3f7a91c2"
USER_MAIN = f"auto_user_{RUN_ID}"                         # AU-01..09 主体
USER_DUP  = f"auto_user_{RUN_ID}_dup"                     # AU-02 冲突目标
USER_PERM = f"auto_user_{RUN_ID}_perm"                    # AD-02 权限否定


def _reg(username, password=PWD):
    """直接调用注册接口,失败时直接抛,确保 fixture 失败能被 pytest 看到。"""
    r = requests.post(f"{BASE}/api/register",
                      json={"username": username, "password": password},
                      timeout=5)
    # 200=新注册成功;409=已存在(允许并发重跑时幂等)
    assert r.status_code in (200, 409), f"register {username}: {r.status_code} {r.text}"
    return r


@pytest.fixture(scope="session", autouse=True)
def seed_users():
    """session 首轮一次性注册所有 auto_* 账号;失败则整套件 fail-fast。"""
    for u in (USER_MAIN, USER_DUP, USER_PERM):
        _reg(u)
    yield
    # teardown:测试结束后可在此处发起清理(若 §10.3 提供了 admin 删除用户接口)
    # for u in (USER_MAIN, USER_DUP, USER_PERM): delete_user_as_admin(u)


@pytest.fixture(scope="session")
def admin_ctx(browser):
    """管理员登录后的 browser context,各用例共享。"""
    ctx = browser.new_context()
    page = ctx.new_page()
    page.goto(f"{BASE}/admin/login")
    page.locator("#auth-username").fill(ADMIN[0])
    page.locator("#auth-password").fill(ADMIN[1])
    page.locator("#auth-submit").click()
    page.wait_for_url(f"{BASE}/admin")
    yield ctx
    ctx.close()


@pytest.fixture
def user_ctx(browser):
    """普通用户登录的 browser context,每个用例独立,但账号复用 USER_MAIN。"""
    ctx = browser.new_context()
    page = ctx.new_page()
    page.goto(f"{BASE}/")
    page.locator("#nav-login").click()
    page.locator("#auth-username").fill(USER_MAIN)
    page.locator("#auth-password").fill(PWD)
    page.locator("#auth-submit").click()
    # 等右上变用户名 → 模态框关闭 + Auth.current() 已写入
    page.wait_for_selector("#user-menu")
    yield ctx
    ctx.close()


def test_smk_01_home_loads(page: Page):
    page.goto(BASE)
    expect(page).to_have_title("CPP-OJ 在线判题")
    expect(page.locator(".problem-card").first).to_be_visible(timeout=10_000)


def test_pl_04_problem_detail(page: Page):
    page.goto(BASE)
    page.locator(".problem-card").first.click()
    expect(page.locator("#pd-title-text")).not_to_have_text("加载中…")
    expect(page.locator("#pd-meta .diff-badge")).to_be_visible()
    expect(page.locator(".cm-editor")).to_be_visible(timeout=10_000)


def test_au_02_duplicate_username(page: Page):
    """AU-02:注册冲突。USER_DUP 已在 seed_users 中注册,这里再注册一次触发 409。"""
    page.goto(BASE)
    page.locator("#nav-register").click()
    page.locator("#auth-username").fill(USER_DUP)        # 同一会话已存在的账号
    page.locator("#auth-password").fill(PWD)
    page.locator("#auth-submit").click()
    err = page.locator("#auth-err")
    expect(err).to_be_visible()
    expect(err).to_contain_text("已被占用")
    # 不应跳路由
    assert page.url.endswith("/"), page.url


def test_sb_01_ac(user_ctx):
    """SB-01:用 USER_MAIN 提交正确解 → AC。"""
    page = user_ctx.new_page()
    page.goto(f"{BASE}/problems/1")  # 假设 A+B 题
    page.wait_for_selector(".cm-editor")
    page.keyboard.press("Control+A")
    page.keyboard.press("Delete")
    page.keyboard.type("int a,b;cin>>a>>b;cout<<a+b;")
    page.locator("#pd-submit").click()
    expect(page.locator("#pd-result-badge .badge-ac")).to_be_visible(timeout=15_000)
```

> **代码关键点**：
> 1. `RUN_ID` 用 `pid + uuid4().hex[:8]` 双源,**彻底消除撞名风险**;不要用 `int(time.time())`。
> 2. `_reg` 容忍 200 与 409——支持 `pytest -n 4` 并发时不同 worker 各自注册不冲突,且重跑幂等。
> 3. `seed_users` 用 `autouse=True & scope="session"` ——保证 fixture 失败时 pytest 直接红色退出,而不是 12 个 AU 用例逐个报奇怪的错。
> 4. `user_ctx` 每个用例独立 browser context(隔离 cookie/localStorage),但账号复用 `USER_MAIN` —— 不再每个用例调一次 `/api/register`。
>
> 由于 CodeMirror 实例未挂到 `window`，上面清空逻辑用键盘 `Ctrl+A` + `Delete`，或生产代码里给 `cmView` 暴露一个 `window.__cm = cmView` 后用 `__cm.dispatch(...)`。

### 11.3 CI 集成建议

```yaml
# .github/workflows/web-e2e.yml
name: web-e2e
on: [push]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with: { python-version: "3.11" }
      - run: pip install playwright pytest requests
      - run: playwright install --with-deps chromium
      - run: pytest tests/web_e2e -q --junitxml=report.xml
        env:
          OJ_BASE: http://193.112.29.233:8088
      - uses: actions/upload-artifact@v4
        if: always()
        with: { name: report, path: report.xml }
```

---

## 12. 风险与注意事项

1. **账号撞名 → AU-02 假阳性**：见 §0.3，**严禁** `int(time.time())` 等秒级精度拼接；用 `pid + uuid4().hex[:8]`。所有 `auto_*` 账号必须在 session 级 fixture 一次性注册，整套件共享。
2. **CodeMirror CDN**：测试环境必须能 `fetch https://esm.sh/@codemirror/*`，否则编辑器永远停在"加载中…"，会拖累大量用例超时。必要时把 esm.sh 资源 mirror 到内网或本地化到 `/static/vendor/`。
3. **同会话串行判题**：SPEC §2.5 明确 `std::mutex g_judge_mu` 串行化，**多浏览器并发提交**会导致彼此阻塞。NF-02/NF-03 不要并行跑 4 个以上浏览器标签，避免服务卡死。
4. **/tmp 残留**：服务在每次提交后清理 `/tmp/oj_*`，但若服务异常退出可能残留。NF 套件跑完后建议 `ls /tmp/oj_*` 校验为空。
5. **并发测试数据清理**：所有 `AUTOTEST_*` 题清理动作**必须**用 admin 登录态执行；普通用户清理会被自家 pipeline 401。题与用户**都要清**，见 §10.3。
6. **sessionStorage 与 localStorage**：本系统两者职责不同——`Auth` 用 `sessionStorage` 缓存当前会话（关闭标签即丢），`Drafts` 用 `localStorage` 持久化草稿。AU-08 退出登录只清 sessionStorage，DR-02 验证草稿仍存——这恰好是两个存储的差异点。
7. **登录模态框 vs 独立登录页**：SPEC §2.6 提到 `/admin/login` 是独立路径，但实现上统一走 `#auth-modal`（详见 `web/static/pages/admin/login.js`）；UI 自动化直接监听模态框出现/关闭即可，不必校验 URL。
8. **SPA fallback**：服务把任何非 `/api/*`、`/static/*` 的 GET 都 fallback 到 `index.html`（详见 `src/routes/static_files.cpp:73`）。这是 RT 系列用例能正常直链打开 `/problems/X`、`/admin` 的前提。

---

## 13. 验收门槛

| 阶段 | 门槛 |
|---|---|
| **冒烟** | SMK 全部通过 → 才允许进入功能套件 |
| **功能套件** | PL/SR/DF/ED/DR/SB/AU/AD P0 用例 100% 通过；P1 允许 ≤ 2 条失败 |
| **非功能** | NF-02 P95 ≤ 5s；NF-04/05 沙箱无崩溃 |
| **边界** | EX 全部通过 → 视为满足 SPEC §4.3 |
| **整体** | 通过率 ≥ 95% 且无 P0 失败，视为 Web 自动化测试合格 |

---

**文档版本**：v1.1
**最后更新**：2026-10-08
