# SPEC.md — cpp-oj-vibecoding

> 一个仿 LeetCode 的轻量 OJ（在线判题系统）。单用户自用场景，支持管理员后台维护题库。
> 后端 C++ + cpp-httplib + MySQL；前端原生 HTML/CSS/JS + CodeMirror 6。

---

## 1. 需求说明（Requirements）

### 1.1 业务目标与成功标准

- **目标用户**：个人学习/求职刷题，单机部署，单用户自用。
- **成功标准**：
  - 用户能在浏览器中查看题目、编写 C++ 代码、提交并立即看到 AC / WA 结果。
  - 管理员能通过后台新增和删除题目，题目立即对用户可见。
  - 单次提交（编译+运行所有用例）端到端 P95 延迟 ≤ 5 秒（取决于题设 time_limit 总和）。
  - 编译错误能完整展示 g++ 输出，WA 仅提示"答案错误"。
  - 关闭/刷新浏览器，已写代码不丢失（localStorage）。

### 1.2 用户角色与权限

| 角色 | 权限 |
|---|---|
| 普通用户（匿名） | 浏览题目列表、查看题目详情、提交代码、查看判题结果 |
| 管理员 | 普通用户全部权限 + 登录后台 + 新增题目 + 删除题目 |

> 注：MVP 不开放普通用户注册。管理员账号通过首次启动种子脚本生成。

### 1.3 核心功能

| 模块 | 功能点 |
|---|---|
| 题库浏览 | 题列表（id/标题/时间限制/内存限制）、单题详情（描述/输入输出格式/样例） |
| 在线编码 | CodeMirror 6 编辑器、全局默认模板、localStorage 草稿 |
| 提交判题 | 提交 C++ 代码 → 同步阻塞 → 返回 AC/WA + 可选编译错误详情 |
| 后台登录 | 账号密码登录 → Session/Cookie → 注销 |
| 后台题库管理 | 表单新增题目（动态增删测试用例）、删除题目（带二次确认） |

### 1.4 非功能需求

| 维度 | 要求 |
|---|---|
| 性能 | 提交后端到端 P95 ≤ 5s（题设 time_limit 总和 ≤ 3s 前提下） |
| 并发 | 单用户场景，后端开 4~8 个工作线程，判题串行（全局 mutex） |
| 安全 | 子进程资源限制（CPU/内存/输出大小/文件大小/进程数）；密码哈希存储；Session 过期机制 |
| 可扩展 | 模块分层清晰；判题子系统可未来替换为 Docker/isolate；支持后续接入更多语言 |
| 部署 | Linux 物理机；Makefile 或 CMake 构建；README 含 MySQL 初始化步骤 |
| 成本 | 零依赖商业组件；仅需 g++、MySQL、cpp-httplib（header-only） |

### 1.5 判题异常状态

| 状态 | 含义 | 前端展示 |
|---|---|---|
| `AC` | 所有用例通过 | "通过" + 绿色徽标 |
| `WA` | 任意用例未通过 | "答案错误" + 灰色徽标；若是编译失败则附带完整 g++ 报错 |
| 超时 | time_limit 耗尽 | 归为 WA，message 包含 "Time Limit Exceeded" |
| 超内存 | memory_limit 超限 | 归为 WA，message 包含 "Memory Limit Exceeded" |
| RE | 非零退出码 | 归为 WA，message 包含 "Runtime Error (exit=N)" |
| 系统错误 | 服务端异常 | 返回 HTTP 500 + 错误信息 |

> 不区分 CE/TLE/MLE/RE 为独立状态，全部折叠为 WA，仅在 message 字段里区分，简化前端。

---

## 2. 架构设计（Architecture）

### 2.1 系统架构图

```
┌───────────────────────────────────────────────────────────────┐
│  Browser                                                      │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │  原生 HTML + CSS + JS（单页 + history.pushState 路由） │  │
│  │  ┌──────────────────────────┐ ┌──────────────────────┐  │  │
│  │  │ CodeMirror 6（CDN 引入） │ │ localStorage 草稿    │  │  │
│  │  └──────────────────────────┘ └──────────────────────┘  │  │
│  └─────────────────────────────────────────────────────────┘  │
└───────────────────────────┬───────────────────────────────────┘
                            │  HTTP / JSON（Cookie: SESSION）
                            ▼
┌───────────────────────────────────────────────────────────────┐
│  cpp-httplib HTTP Server（C++ 多线程）                       │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ Middleware: AuthGuard（session 校验、admin 角色校验） │  │
│  └─────────────────────────────────────────────────────────┘  │
│  ┌────────────────────── Routes ───────────────────────────┐  │
│  │  POST   /api/login                                      │  │
│  │  POST   /api/logout                                     │  │
│  │  GET    /api/problems                                   │  │
│  │  GET    /api/problems/:id                               │  │
│  │  POST   /api/submit                                     │  │
│  │  POST   /api/admin/login                                │  │
│  │  POST   /api/admin/problems        (admin only)         │  │
│  │  DELETE /api/admin/problems/:id    (admin only)         │  │
│  │  GET    /static/*                                       │  │
│  └─────────────────────────────────────────────────────────┘  │
│  ┌──────────────── Judge Orchestrator ─────────────────────┐  │
│  │  std::mutex g_judge_mu  ← 判题串行化                     │  │
│  │  Judge::run(code, problem) → JudgeResult                │  │
│  │   ├─ compile(): popen("g++ -O2 -std=c++17 ...")         │  │
│  │   └─ execute(): fork+exec + setrlimit + 逐用例运行      │  │
│  └─────────────────────────────────────────────────────────┘  │
│  ┌──────────────── Persistence ────────────────────────────┐  │
│  │  MySQL 连接（单连接 + mutex，或简单连接池）              │  │
│  └─────────────────────────────────────────────────────────┘  │
└────────────┬──────────────────────────────────┬───────────────┘
             ▼                                  ▼
     ┌────────────────────┐             ┌──────────────────────┐
     │ MySQL              │             │ /tmp/oj_<rand>/       │
     │  - users           │             │  ├─ main.cpp          │
     │  - sessions        │             │  ├─ main.bin          │
     │  - problems        │             │  ├─ in_0.txt .. in_N  │
     │  - test_cases      │             │  └─ out_0.txt .. out_N│
     └────────────────────┘             │  （每次提交独立目录） │
                                        └──────────────────────┘
```

### 2.2 关键模块职责

| 模块 | 文件（建议） | 职责 |
|---|---|---|
| `main.cpp` | `src/main.cpp` | 启动 httplib server，注册路由 |
| `routes/` | `src/routes/*.cpp` | 各 HTTP 端点处理函数 |
| `auth/` | `src/auth/session.cpp` | Session 创建、校验、销毁；密码哈希 |
| `db/` | `src/db/mysql_client.cpp` | MySQL 连接、查询封装 |
| `judge/` | `src/judge/judge.cpp` | 编译 + 运行 + 比对流程编排 |
| `judge/` | `src/judge/compiler.cpp` | g++ 子进程调用 |
| `judge/` | `src/judge/runner.cpp` | fork+exec + setrlimit + IO 重定向 |
| `util/` | `src/util/*.cpp` | 临时目录、字符串、JSON 构造 |
| `web/` | `web/index.html` `web/app.js` `web/style.css` `web/admin/*` | 前端资源 |

### 2.3 数据模型（MySQL Schema）

```sql
CREATE DATABASE IF NOT EXISTS oj DEFAULT CHARACTER SET utf8mb4;
USE oj;

CREATE TABLE users (
  id            INT PRIMARY KEY AUTO_INCREMENT,
  username      VARCHAR(64)  NOT NULL UNIQUE,
  password_hash VARCHAR(128) NOT NULL,         -- sha256(salt || password)
  salt          VARCHAR(32)  NOT NULL,
  role          ENUM('user','admin') NOT NULL DEFAULT 'user',
  created_at    TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB;

CREATE TABLE sessions (
  token       CHAR(64)    PRIMARY KEY,        -- 随机 64 字符 hex
  user_id     INT         NOT NULL,
  expires_at  TIMESTAMP   NOT NULL,
  created_at  TIMESTAMP   NOT NULL DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_expires (expires_at),
  FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE problems (
  id               INT PRIMARY KEY AUTO_INCREMENT,
  title            VARCHAR(255) NOT NULL,
  description      MEDIUMTEXT   NOT NULL,     -- 题干（Markdown 或纯文本）
  input_format     TEXT,                      -- 输入格式说明
  output_format    TEXT,                      -- 输出格式说明
  time_limit_ms    INT          NOT NULL DEFAULT 1000,
  memory_limit_mb  INT          NOT NULL DEFAULT 128,
  created_at       TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at       TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB;

CREATE TABLE test_cases (
  id               INT PRIMARY KEY AUTO_INCREMENT,
  problem_id       INT          NOT NULL,
  input            MEDIUMTEXT   NOT NULL,
  expected_output  MEDIUMTEXT   NOT NULL,
  ord              INT          NOT NULL DEFAULT 0,
  FOREIGN KEY (problem_id) REFERENCES problems(id) ON DELETE CASCADE,
  INDEX idx_problem (problem_id, ord)
) ENGINE=InnoDB;
```

### 2.4 REST API 约定

| Method | Path | 鉴权 | 入参 | 出参 |
|---|---|---|---|---|
| POST | `/api/login` | 否 | `{username, password}` | `{ok, role}` + Set-Cookie |
| POST | `/api/logout` | session | — | `{ok}` |
| GET | `/api/problems` | 否 | — | `[{id, title, time_limit_ms, memory_limit_mb}]` |
| GET | `/api/problems/:id` | 否 | — | `{id, title, description, input_format, output_format, time_limit_ms, memory_limit_mb, samples:[{input, expected_output}]}` |
| POST | `/api/submit` | 否 | `{problem_id, code}` | `{status: "AC"\|"WA", message?: string, compile_error?: string}` |
| POST | `/api/admin/login` | 否 | 同 `/api/login`，要求 role=admin | `{ok, role}` |
| POST | `/api/admin/problems` | admin | `{title, description, input_format, output_format, time_limit_ms, memory_limit_mb, test_cases:[{input, expected_output}]}` | `{ok, id}` |
| DELETE | `/api/admin/problems/:id` | admin | — | `{ok}` |

> `/api/submit` 与 `/api/login` 同名便于单页应用，但 `/api/admin/*` 与 `/api/*` 解耦，普通用户登录走 `/api/login`（默认 user 角色），后台入口走 `/admin/login` 页面调 `/api/admin/login`。

### 2.5 判题子系统设计

**核心流程**（`Judge::run`）：

```
1. mkdtemp("/tmp/oj_XXXXXX") -> tmp_dir
2. write(tmp_dir/main.cpp, code)
3. compile_result = compile(tmp_dir/main.cpp, tmp_dir/main.bin)
   - 命令: g++ -O2 -std=c++17 -DONLINE_JUDGE -o main.bin main.cpp
   - 捕获 stderr, 最多 64KB
   - 返回 {success, stderr}
4. 若 compile 失败:
     - 清理 tmp_dir
     - 返回 {status: WA, message: "Compile Error", compile_error: stderr}
5. 对每个 test_case:
   a. write(tmp_dir/in_i.txt, test_case.input)
   b. run_result = execute(tmp_dir/main.bin, in_i.txt, out_i.txt, time_limit_ms, memory_limit_mb)
      - fork()
      - 子进程: setrlimit(RLIMIT_CPU, time_limit_ms/1000 + 1)
                setrlimit(RLIMIT_AS, memory_limit_mb * 1024 * 1024)
                setrlimit(RLIMIT_FSIZE, 64 * 1024 * 1024)   // 输出限 64MB
                setrlimit(RLIMIT_NPROC, 1)                 // 禁 fork bomb
                freopen(in_i.txt, "r", stdin)
                freopen(out_i.txt, "w", stdout)
                execve(main.bin)
      - 父进程: wait4() with WNOHANG 超时检查（轮询 or alarm）
                读取 out_i.txt 长度（防止超 RLIMIT_FSIZE 截断异常）
   c. 判定:
        - 超时          → WA, message="Time Limit Exceeded", break
        - 超内存        → WA, message="Memory Limit Exceeded", break
        - exit != 0     → WA, message="Runtime Error (exit=N)", break
        - 否则 diff(out_i.txt, expected_output)
            - 一致 → 继续下一个用例
            - 不一致 → WA, message="Wrong Answer", break
6. 全部用例通过 → AC
7. 清理 tmp_dir（rm -rf）
8. 返回 JudgeResult
```

**关键设计要点**：
- 用 `std::mutex g_judge_mu` 串行化判题，避免单机器资源打满。
- 编译/运行用绝对路径，tmp_dir 权限 `0700`。
- 每次判题有 wall-clock 上限保护（即使 ulimit 失效也兜底 SIGKILL）。
- diff 实现：去掉行尾 `\r`，逐行比较；末尾允许多余空行。

### 2.6 前端结构

```
web/
├── index.html              # 入口（包含 SPA 容器）
├── style.css               # 全局样式
├── app.js                  # 路由 + 主控制器
├── pages/
│   ├── problem-list.js     # 题目列表视图
│   ├── problem-detail.js   # 单题视图（编辑器 + 提交 + 结果）
│   └── admin/
│       ├── login.js        # 管理员登录
│       ├── dashboard.js    # 管理员题列表
│       └── problem-form.js # 新增/编辑题目表单
└── vendor/
    └── codemirror.min.js   # （可选：本地化 CodeMirror；推荐 CDN）
```

**路由约定**（hash 或 pushState）：
- `/` → 题列表
- `/problems/:id` → 单题
- `/admin/login` → 后台登录
- `/admin` → 后台题列表
- `/admin/problems/new` → 新增题目

**CodeMirror 集成**：
- CDN：`https://cdn.jsdelivr.net/npm/codemirror@6.x/dist/...` 或 ESM `esm.sh/codemirror`。
- 语言模式：`@codemirror/lang-cpp`。
- 主题：`oneDark` 或 `default`。

**默认模板**：
```cpp
#include <bits/stdc++.h>
using namespace std;

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    // your code here
    return 0;
}
```

---

## 3. TODO 清单（按阶段）

### Phase 1 — 项目脚手架

- [x] 1.1 创建目录结构：`src/{routes,auth,db,judge,util}`、`web/{pages/admin,vendor}`、`sql/`、`scripts/`、`build/`
- [x] 1.2 编写 `CMakeLists.txt`（或 `Makefile`），集成 cpp-httplib、libmysqlclient（或 mysql-connector-cpp）、pthread
- [x] 1.3 编写 `sql/schema.sql`（含建库 + 4 张表）
- [x] 1.4 编写 `scripts/init_db.sh`（执行 schema.sql 并种子 admin 账号）
- [x] 1.5 编写 `scripts/seed_admin.cpp`（参数化用户名/密码，生成 sha256+盐）
- [x] 1.6 编写 `.gitignore`（`build/`、`*.o`、`web/vendor/...` 视情况）
- [x] 1.7 编写 `README.md`（依赖、构建、运行步骤）

### Phase 2 — 后端基础

- [x] 2.1 实现 `MySQLClient` 单例/连接封装（连接重试、错误日志）
- [x] 2.2 实现 `SessionManager`：生成 token、写入 sessions 表、过期清理
- [x] 2.3 实现密码哈希工具 `sha256(salt || password)`
- [x] 2.4 实现 `AuthGuard` 中间件：解析 Cookie → 查 session → 注入 user 上下文
- [x] 2.5 实现 HTTP server 主入口，注册所有路由
- [x] 2.6 实现 `POST /api/login`、`POST /api/logout`
- [x] 2.7 实现静态资源服务（`/static/*` → `web/`）

### Phase 3 — 题目 CRUD

- [x] 3.1 实现 `GET /api/problems`（列表，时间/内存限制一并返回）
- [x] 3.2 实现 `GET /api/problems/:id`（含 samples 测试用例数组）
- [x] 3.3 实现管理员角色校验中间件
- [x] 3.4 实现 `POST /api/admin/problems`（含 test_cases 批量插入）
- [x] 3.5 实现 `DELETE /api/admin/problems/:id`（级联删除 test_cases）
- [x] 3.6 实现 `POST /api/admin/login`（与 /api/login 区分路径）

### Phase 4 — 判题子系统

- [x] 4.1 实现 `Compiler::compile`：popen g++、捕获 stderr、超时保护
- [x] 4.2 实现 `Runner::execute`：fork、setrlimit、IO 重定向、waitpid
- [x] 4.3 实现 wall-clock 超时兜底（alarm / pthread_cond_timedwait）
- [x] 4.4 实现 `Diff::compare`：规范化行尾与尾随空行
- [x] 4.5 实现 `TempDir`：mkdtemp + 析构清理
- [x] 4.6 实现 `Judge::run` 编排逻辑
- [x] 4.7 实现 `std::mutex g_judge_mu` 串行化
- [x] 4.8 实现 `POST /api/submit`，串接 Judge

### Phase 5 — 前端

- [x] 5.1 引入 CodeMirror 6（CDN 或本地化）
- [x] 5.2 实现 SPA 路由（`history.pushState` + popstate）
- [x] 5.3 实现 `pages/problem-list.js`：拉题列表、卡片点击进详情
- [x] 5.4 实现 `pages/problem-detail.js`：题干 + 样例 + 编辑器 + 提交按钮 + 结果面板
- [x] 5.5 实现 localStorage 草稿读写（key=`draft:problem:<id>`）
- [x] 5.6 实现 `pages/admin/login.js`：账号密码表单 + 跳后台
- [x] 5.7 实现 `pages/admin/dashboard.js`：管理员题列表 + 删除按钮
- [x] 5.8 实现 `pages/admin/problem-form.js`：表单字段 + 动态增删用例 + 提交
- [x] 5.9 全局样式：基础布局、卡片、按钮、代码块、结果徽标配色

### Phase 6 — 验证与交付

- [x] 6.1 编写 3 道种子题目（含 AC、WA、CE、TLE、MLE、RE 各场景至少 1）
- [x] 6.2 端到端测试：普通用户完整流程（打开 → 看题 → 写代码 → 提交 → 看结果）
- [x] 6.3 端到端测试：管理员流程（登录 → 新增题 → 用户侧可见 → 删除题 → 用户侧不可见）
- [x] 6.4 边界测试：编译错误提交、段错误提交、无限循环、O(n²) 在大数据下超时
- [x] 6.5 边界测试：localStorage 草稿持久化（刷新/重启浏览器）
- [x] 6.6 边界测试：管理员未登录访问 `/api/admin/*` 返回 401
- [x] 6.7 性能测试：单次提交 P95 ≤ 5s（按题设 time_limit）
- [x] 6.8 安全测试：恶意代码读取 `/etc/passwd`、fork bomb、rm -rf /（应被 ulimit/隔离阻止）
- [x] 6.9 README 完善：启动步骤、依赖、默认账号、FAQ
- [ ] 6.10 提交 git 初始版本（README + SPEC + 骨架代码） — 待用户确认后再提交

---

## 4. 验收标准（Acceptance Criteria）

### 4.1 功能验收

| ID | 验收项 | 通过条件 |
|---|---|---|
| F-01 | 题列表展示 | 打开 `/` 看到所有题目，含 id/标题/时间/内存限制 |
| F-02 | 单题详情 | 点击进入 `/problems/:id`，题干、输入输出格式、样例全部渲染 |
| F-03 | 编辑器加载 | CodeMirror 6 正常初始化，默认模板填充，语法高亮可用 |
| F-04 | 草稿持久化 | 输入代码 → 刷新 → 代码仍在 |
| F-05 | 提交 AC | 正确代码 → 返回 `{status:"AC"}` |
| F-06 | 提交 WA | 错误代码 → 返回 `{status:"WA", message:"Wrong Answer"}` |
| F-07 | 编译错误展示 | 含语法错误的代码 → 返回 WA + 完整 g++ 报错在 `compile_error` 字段 |
| F-08 | 管理员登录 | 输入正确 admin 账号密码 → 后台可访问；错则 401 |
| F-09 | 新增题目 | 填写表单提交 → 题目立即出现在普通用户列表 |
| F-10 | 删除题目 | 后台点删除 → 题列表中消失；普通用户访问 `/problems/:id` 返回 404 |
| F-11 | 路由切换 | 题列表 ↔ 单题 ↔ 后台页面间切换无白屏、状态保留（草稿不丢） |

### 4.2 非功能验收

| ID | 验收项 | 通过条件 |
|---|---|---|
| N-01 | 同步判题延迟 | time_limit=1s、3 个用例，单次提交 P95 ≤ 5s |
| N-02 | 并发判题串行 | 同时打开两个浏览器标签提交，后台日志显示第二个等第一个完成才执行 |
| N-03 | 资源限制生效 | 提交 `while(1){}` 不会卡死服务，time_limit 后返回 WA |
| N-04 | 资源限制生效 | 提交 `vector<int> v; while(1) v.push_back(1);` 不会 OOM 杀进程 |
| N-05 | 资源限制生效 | 提交读取大文件循环输出，输出超 64MB 被截断 |
| N-06 | Session 过期 | 设置 session 1 分钟后过期，过期后调 `/api/admin/*` 返回 401 |
| N-07 | 密码安全 | DB 中存的是 sha256(salt+password)，不是明文 |
| N-08 | 临时目录清理 | 提交完成后 `/tmp/oj_*` 不残留 |

### 4.3 边界 / 异常验收

| ID | 场景 | 期望行为 |
|---|---|---|
| E-01 | 提交空代码 | 后端拒绝，返回 400 |
| E-02 | 提交代码超过 64KB | 后端拒绝，返回 413 |
| E-03 | 提交不存在的 problem_id | 返回 404 |
| E-04 | 用户代码调用 `system("rm -rf /")` | 因 RLIMIT_NPROC/隔离，命令受限或失败；服务不受影响 |
| E-05 | 用例输入包含二进制 NUL | 用文本模式读写，不出问题 |
| E-06 | 期望输出末尾多个空行 | diff 容错，AC |
| E-07 | 题设 time_limit=0 | 后端拒绝创建（>= 100ms 校验） |
| E-08 | 管理员未登录调 admin API | 返回 401 |
| E-09 | 普通用户登录后调 admin API | 返回 403 |
| E-10 | 两次删除同一题 | 第二次返回 404 |

### 4.4 部署验收

| ID | 验收项 | 通过条件 |
|---|---|---|
| D-01 | 一键构建 | `make` 或 `cmake --build` 完成，无报错 |
| D-02 | 一键初始化 | `bash scripts/init_db.sh` 完成库表创建 + admin 种子 |
| D-03 | 一键启动 | `make run` 或 `./build/oj_server` 启动后浏览器可访问 |
| D-04 | 依赖清单 | README 列明 g++、MySQL、cpp-httplib、libmysqlclient |

---

## 5. 范围外（Out of Scope）

明确**不**在 MVP 范围，避免范围蔓延：

- ❌ 多语言支持（Python/Java/Go 等）
- ❌ 用户注册 / 找回密码 / 邮箱验证
- ❌ 提交历史持久化（题 1.5 已确认不做）
- ❌ 排行榜、比赛、讨论区
- ❌ 标签、难度分级、搜索
- ❌ Markdown 渲染（题干先用纯文本，预留升级）
- ❌ Docker / nsjail / isolate 等重量级沙箱（后续可选）
- ❌ HTTPS（部署在本地或内网，HTTP 即可）
- ❌ 移动端适配（仅桌面浏览器）
- ❌ 国际化（i18n）

---

## 6. 风险与权衡（Risks & Trade-offs）

| 风险 | 影响 | 当前缓解 |
|---|---|---|
| 单用户 + 管理员角色矛盾 | 增加 auth 复杂度，但保留未来拓展空间 | 仅种子 admin，登录接口兼容 |
| `fork+exec` 无 namespace 隔离 | 用户代码可读本地文件（虽然单用户场景） | 通过 RLIMIT_NPROC、RLIMIT_FSIZE 限制爆破；tmp_dir 权限 0700；只跑可信用户 |
| cpp-httplib 同步阻塞 | 长判题会占用工作线程 | 全局 mutex 串行判题，单用户下可接受；预留异步化接口 |
| `popen` vs `fork+exec` 选 compile | popen 简单但无法精细控制 | 用 popen 仅做编译，运行时用 fork+exec 精细控制 |
| 仅 AC/WA 状态 | 调试体验弱 | WA message 中带具体错误类别；编译错误带完整 stderr |
| 测试用例放 DB | 大数据用例不便 | 题设复杂度低，手写题够用；后续可加文件存储 |
| localStorage 容量 | 5~10MB 限制 | 单题代码一般 < 10KB，足够；超限则降级丢弃 |
| MySQL 必装 | 比 SQLite 重 | 已确认选 MySQL；README 给最小安装步骤 |

---

## 7. 后续迭代方向（非本次实现）

- v1.1：提交历史持久化（submissions 表 + 个人提交列表页）
- v1.2：Docker 沙箱化判题（替换 fork+exec）
- v1.3：多语言支持（Python/Java/Go）
- v1.4：题目标签/难度/搜索
- v1.5：比赛模式（限时赛 + 排行榜）

---

**文档版本**：v1.0
**最后更新**：2026-09-14
