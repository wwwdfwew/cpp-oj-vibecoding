# cpp-oj-vibecoding

一个仿 LeetCode 的轻量级在线判题系统(单用户自用)。后端使用 C++17
(`cpp-httplib` + MySQL),前端使用原生 JS + CodeMirror 6。

完整设计见 [SPEC.md](./SPEC.md)。

## 进度(Phase 1–6 已完成)

| 阶段 | 内容 | 状态 |
|---|---|---|
| 1 | 项目脚手架 + 数据库 schema + 初始化脚本 | [x] 完成 |
| 2 | 认证、会话、登录 / 注销 | [x] 完成 |
| 3 | 题目 CRUD(创建 / 列表 / 删除) | [x] 完成 |
| 4 | 判题(编译 + 运行 + diff,AC/WA/CE/TLE/MLE/RE) | [x] 完成 |
| 5 | 前端 SPA + CodeMirror 6 + 草稿持久化 | [x] 完成 |
| 6 | 端到端验证、种子题目、安全与性能测试 | [x] 完成 |

服务已**端到端可用**:可服务静态 SPA、所有 JSON 接口、管理员认证,
以及可工作的判题(g++ `fork`/`exec` + `rlimits`,`mkdtemp` 清理,
互斥锁串行化)。下面的冒烟 / E2E / 性能测试结果均为实测。

## 依赖

| 组件 | 版本 | 来源 |
|---|---|---|
| g++ | ≥ 7(本机 11.4) | 系统 |
| cmake | ≥ 3.16(本机 3.22) | 系统 |
| MySQL Server | 8.0 | `apt install mysql-server` |
| libmysqlclient-dev | 8.0 | `apt install libmysqlclient-dev` |
| libssl-dev | 3.0 | `apt install libssl-dev` |
| cpp-httplib | 0.54.1 | `/usr/include/httplib.h` |
| pthread / libmysqlclient / OpenSSL | 运行时 | 由 CMake 链接 |
| (可选,E2E 测试用) python3 | 3.10+ | `apt install python3` |
| (可选,E2E 测试用) playwright + chromium | 1.x | `pip install playwright && playwright install chromium` |

## 快速开始

```bash
# 1. 初始化数据库(创建 oj 库、4 张表,并种子管理员账号)
bash scripts/init_db.sh
# 默认管理员账号:admin / admin123

# 2. 编译(默认 Release)
cmake -S . -B build
cmake --build build -j$(nproc)

# 3. 种子 3 道示例题(A+B、N 个数的和、N 个数中的最大值)
python3 scripts/seed_problems.py

# 4. 运行
OJ_DB_USER=oj OJ_DB_PASS=oj_dev_pw ./build/oj_server
# 浏览器访问 http://localhost:8088/
```

> **端口**:SPEC 默认端口 8080 在本机已被占用,因此这里默认使用 **8088**。
> 可通过 `OJ_HTTP_PORT` 覆盖。

## MySQL 用户配置

服务端不使用 MySQL 的 `root`/`auth_socket`,因为 C++ 客户端走 TCP
连接,需要密码认证。请先创建一个专用用户:

```bash
sudo mysql <<'EOF'
CREATE USER 'oj'@'localhost' IDENTIFIED BY 'oj_dev_pw';
GRANT ALL PRIVILEGES ON oj.* TO 'oj'@'localhost';
FLUSH PRIVILEGES;
EOF
```

之后通过环境变量(`OJ_DB_USER=oj`、`OJ_DB_PASS=oj_dev_pw`)传给服务。

`scripts/init_db.sh` 使用 `sudo mysql` 来应用 schema 并插入管理员行,
因此无需提前创建 `oj` 用户也能跑通。

## 配置

所有设置均来自环境变量:

| 变量 | 默认值 | 说明 |
|---|---|---|
| `OJ_HTTP_HOST` | `0.0.0.0` | 监听地址 |
| `OJ_HTTP_PORT` | `8088` | 监听端口 |
| `OJ_DB_HOST` | `localhost` | MySQL 主机 |
| `OJ_DB_PORT` | `3306` | MySQL 端口 |
| `OJ_DB_USER` | `root` | MySQL 用户 |
| `OJ_DB_PASS` | (空) | MySQL 密码 |
| `OJ_DB_NAME` | `oj` | 数据库名 |
| `OJ_WEB_ROOT` | `./web` | 前端静态资源目录 |
| `OJ_GXX_PATH` | `/usr/bin/g++` | 判题使用的 C++ 编译器 |
| `OJ_LOG` | `INFO` | 取值 `DEBUG`/`INFO`/`WARN`/`ERROR` |

## API 概览

| 方法 | 路径 | 鉴权 |
|---|---|---|
| `POST` | `/api/register` | — |
| `POST` | `/api/login` | — |
| `POST` | `/api/admin/login` | —(要求角色为 admin) |
| `POST` | `/api/logout` | session |
| `GET`  | `/api/me` | session(可选) |
| `GET`  | `/api/problems` | — |
| `GET`  | `/api/problems/:id` | — |
| `POST` | `/api/submit` | session |
| `POST` | `/api/admin/problems` | admin |
| `DELETE` | `/api/admin/problems/:id` | admin |

判题响应格式(`POST /api/submit`):

```json
{ "status": "AC", "message": "Accepted" }
{ "status": "WA", "message": "Wrong Answer", "failed_case": 0 }
{ "status": "WA", "message": "Compile Error", "compile_error": "..." }
{ "status": "WA", "message": "Time Limit Exceeded", "failed_case": 0 }
{ "status": "WA", "message": "Memory Limit Exceeded", "failed_case": 0 }
{ "status": "WA", "message": "Runtime Error (exit=1)", "failed_case": 0 }
```

其余请求 / 响应细节见 [SPEC.md §2.4](./SPEC.md)。

## 项目结构

```
.
├── CMakeLists.txt
├── SPEC.md
├── README.md
├── sql/
│   └── schema.sql
├── scripts/
│   ├── init_db.sh                  # 应用 schema.sql + 种子管理员
│   ├── seed_admin.cpp              # C++ 辅助:输出 sha256+盐 的 INSERT
│   ├── seed_problems.py            # Phase 6.1:创建 3 道种子题
│   ├── e2e_submit_matrix.sh        # Phase 6.x:AC/WA/CE/RE/TLE/MLE 冒烟
│   ├── e2e_draft_persistence.py    # Phase 6.5:localStorage 草稿测试
│   ├── e2e_security.py             # Phase 6.8:恶意代码探测
│   └── perf_latency.py             # Phase 6.7:P95 延迟检查
├── src/
│   ├── main.cpp                    # httplib 服务入口、信号处理
│   ├── util/                       # strutil、json、log、config
│   ├── auth/                       # password(sha256+盐)、session
│   ├── db/                         # mysql_client(互斥保护)
│   ├── judge/                      # tempdir、compiler、runner、diff、judge
│   └── routes/                     # auth、problems、submit、admin、static_files
└── web/
    ├── index.html
    └── static/
        ├── style.css
        ├── app.js                  # SPA 路由 + 框架
        └── pages/                  # problem-list、problem-detail、admin/*
```

## 冒烟测试(结果)

按快速启动步骤起服务后执行:

```
GET /                          -> 200
GET /api/problems              -> 200  (种子后为 3 道题)
GET /api/problems/8            -> 200
POST /api/admin/login (good)   -> 200  {"ok":true,"role":"admin"}
POST /api/admin/login (bad)    -> 401  {"error":"invalid credentials"}
POST /api/submit (AC)          -> 200  {"status":"AC","message":"Accepted"}
POST /api/submit (WA)          -> 200  {"status":"WA","message":"Wrong Answer","failed_case":0}
POST /api/submit (CE)          -> 200  {"status":"WA","message":"Compile Error","compile_error":"..."}
POST /api/submit (TLE)         -> 200  {"status":"WA","message":"Time Limit Exceeded","failed_case":0}
POST /api/submit (MLE)         -> 200  {"status":"WA","message":"Memory Limit Exceeded","failed_case":0}
POST /api/submit (RE exit=1)   -> 200  {"status":"WA","message":"Runtime Error (exit=1)","failed_case":0}
POST /api/submit (empty code)  -> 400  {"error":"empty code"}
GET  /static/style.css         -> 200
GET  /problems/8 (SPA fb)      -> 200  (返回 index.html)
POST /api/logout               -> 200
DELETE /api/admin/problems/8   -> 200  {"ok":true}
DELETE /api/admin/problems/8   -> 404  {"error":"not found"}  (幂等)
POST /api/admin/problems (no auth)        -> 401  {"error":"unauthenticated"}
POST /api/admin/problems (non-admin user) -> 403  {"error":"admin only"}
```

## 端到端 / 性能测试结果

Phase 6 提供了一整套自动化测试,均在本机通过。

### `bash scripts/e2e_submit_matrix.sh` —— AC/WA/CE/TLE/MLE/RE 矩阵

针对运行中的服务,9 / 9 场景通过:

| # | 场景 | 题目 | 期望 | 实际 |
|---|---|---|---|---|
| 1 | 正确 A+B | A+B | AC | AC |
| 2 | 输出 `a*b` | A+B | Wrong Answer | Wrong Answer |
| 3 | 缺少分号 | A+B | Compile Error | Compile Error |
| 4 | `return 1/0` | A+B | Runtime Error | Runtime Error |
| 5 | 线性求和 | N 个数的和 | AC | AC |
| 6 | 流式最大 | N 个数中的最大值 | AC | AC |
| 7 | O(N²) 两两比较 | N 个数中的最大值 | Time Limit Exceeded | Time Limit Exceeded |
| 8 | `vector<int>(8e6)`(16 MB 上限) | N 个数的和 | Memory Limit Exceeded | Memory Limit Exceeded |
| 9 | 跳过最后一个元素 | N 个数的和 | Wrong Answer | Wrong Answer |

### `python3 scripts/perf_latency.py --runs 20` —— P95 延迟

`time_limit_ms = 1000`,每个提交 3 个用例,端到端 20 次:

```
=== Phase 6.7 latency over 20 runs ===
  status counts: {'AC': 20, 'WA': 0, 'ERROR': 0}
  avg = 3537.7 ms
  P50 = 3341.1 ms
  P95 = 4461.4 ms          # ≤ 5000 ms ✓
  P99 = 4488.6 ms
  max = 4495.4 ms
  -> PASS
```

### `python3 scripts/e2e_security.py` —— 沙箱探测

6 / 6 恶意负载被拦住,服务未宕:

| 负载 | 结果 |
|---|---|
| `ifstream("/etc/passwd")` 并打印 | WA(输出 ≠ 期望) |
| `while(1) fork()`(`RLIMIT_NPROC=1`) | WA(首次 fork 即 EAGAIN) |
| `system("rm -rf ...")` | WA(exit 127,输出 ≠ 期望) |
| `bind(22)` 特权端口 | WA(EACCES) |
| `while(1){}` 死循环 | TLE(CPU 上限触发) |
| `vector::push_back` 循环 | MLE(`RLIMIT_DATA` 触发) |

### `python3 scripts/e2e_draft_persistence.py` —— 前端草稿

通过 Playwright 驱动无头 Chromium:启动 SPA,输入自定义草稿,
然后验证它能(a) 完整刷新后存活;(b) 跳到 `/` 再回来后存活;
以及(c) 不会泄漏到其他题目 ID。

## 实现笔记

### Lambda 生命周期 Bug(已修复)

早期的冒烟测试中发现,`register_*_routes` 函数内部的 lambda 通过 `[&]`
捕获了局部的 `auto` lambda。当这些注册函数返回后,内层 lambda 已被
销毁,但已注册的 HTTP handler 还持有悬空引用。崩溃表现为
`MySQLClient::escape` 段错误,因为 `this` 指向的是注册函数已经失效
的栈帧。

修复:把辅助 lambda 提到 `std::function` 局部变量中,在 handler lambda
里**按值**捕获它们。静态文件路径和管理员守卫也使用同一模式。
参见 `src/routes/auth.cpp:30-50`、`src/routes/admin.cpp:65-72`、
`src/routes/static_files.cpp`。

### 判题安全

`Judge::run` 用全局互斥锁(`src/judge/judge.cpp:g_judge_mu`)串行化,
以保证沙箱资源可预期。每次提交都在自己的 `mkdtemp("/tmp/oj_XXXXXX")`
目录下运行,析构时清理。

`Runner::run` 设置:
- `RLIMIT_CPU` = `time_limit_ms / 1000 + 1` 秒
- `RLIMIT_DATA` = `memory_limit_mb` 字节(堆上限,不约束共享库映射)
- `RLIMIT_AS` = `8 × memory_limit_mb` 字节(给 `mmap` 式分配兜底)
- `RLIMIT_FSIZE` = 64 MB(输出上限)
- `RLIMIT_NPROC` = 1(防 fork bomb)

`time_limit_ms + 2000` 毫秒的墙钟杀进程是最后兜底。父进程每 5 ms
用 `wait4(WNOHANG)` 轮询并收集 `rusage`,用于上报 `time_ms` / `mem_kb`。

内存超限分类使用 CPU-vs-RSS 启发式:被杀进程已消耗的 CPU 时间不到
预算一半、且 RSS 超额,判定为 MLE;否则视为 TLE。这样可以避免把
"加载库导致 RSS 偏高"的紧 TLE 杀掉误判为 MLE。

子进程的 stderr 重定向到 `/dev/null`,避免 MLE 探测刻意触发的
`std::bad_alloc` / SIGABRT 信息污染服务端日志。

### 密码哈希

`sha256(salt_hex || password)`,每个用户带 16 字节的服务端生成盐
(见 `src/auth/password.cpp`、`scripts/seed_admin.cpp`)。盐和哈希
随用户行一起存储。

### 前端草稿持久化

`web/static/app.js` 中的 `Drafts` 读 / 写 `localStorage["draft:problem:<id>"]`。
`problem-detail.js` 在挂载时调用 `Drafts.get(id)` 为编辑器播种,
`EditorView.updateListener` 在每次文档变化时调用 `Drafts.set(id, ...)`。
端到端验证见 `scripts/e2e_draft_persistence.py`。

### CodeMirror 6 导入注意

`keymap` 导出位于 `@codemirror/view`,**而不是** `@codemirror/commands`
(`defaultKeymap`、`history`、`historyKeymap`、`indentWithTab` 在后者)。
从错误的模块拉 `keymap` 会导致 `Cannot read properties of undefined (reading 'of')`
并在编辑器启动时崩溃。

## 限制 / 尚未实现

- 提交历史持久化(见 SPEC §5 —— 不在 MVP 范围)。
- HTTPS(见 SPEC §5)。
- 重型沙箱(Docker / nsjail —— 见 SPEC §5)。
- 判题进程与服务器使用同一 UID。在每次提交独立的 `mkdtemp`(权限 `0700`)
  以及 4 个 rlimits 之内,坚决的攻击者仍可能读取宿主机文件。
  这在单用户 MVP 场景下可以接受(见 SPEC §6 "风险与权衡")。