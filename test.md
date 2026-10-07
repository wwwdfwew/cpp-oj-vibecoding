# cpp-oj-vibecoding Web自动化测试用例

## 首页 / 题库列表（Home / Problem List）

### 冒烟与基础可用性（SMK）
- 入口页加载
  - 浏览器打开 http://193.112.29.233:8088/
    - HTTP 200；title 含 CPP-OJ；main#app 渲染 .problem-card；无 console error
- 静态资源 200
  - 并行请求 /static/style.css、/static/app.js 等静态资源
    - 全部 200，Content-Type 正确
- API 健康检查
  - GET /api/problems
    - 200；JSON 数组；元素含 id/title/difficulty/time_limit_ms/memory_limit_mb
- 不存在的 API 路径
  - GET /api/nope
    - 返回 404 或 200 + JSON，但不能 500

### 题库浏览（PL）
- 题列表默认加载
  - 打开 /，等待 .problem-card 出现
    - 每张卡片显示编号、标题、difficulty 徽标、时间、内存；无白屏
- 题列表计数与 hero stats 一致
  - 读 #stat-problems 文本，与 GET /api/problems 数组长度对比
    - 一致
- 题列表为空时占位
  - 题库 0 题
    - 打开 /
      - 显示还没有题目 + 提示文案；不抛错
- 题列表卡片点击进入
  - 点任一 .problem-card
    - URL 切到 /problems/<id>；无白屏过渡

### 题目搜索（SR）
- 输入关键词命中
  - 题库含 A+B
    - 在 #pl-search 输入 A+B
      - 等待 200ms；列表只剩标题含 A+B 的题
- 关键词清空恢复
  - SR-01 后
    - 点 #pl-search-clear
      - 列表恢复全部
- 大小写不敏感
  - 题库含 BFS 模板
    - 输入 bfs
      - 列表命中
- 中文关键词
  - 题库含两数之和
    - 输入两数
      - 列表命中
- 无命中结果
  - 题库 4 题
    - 输入 ZZZZ_NO_HIT
      - 显示没有匹配的题目
- 防抖与即时过滤
  - 监听 /api/problems 请求；连续输入 AB、ABC、ABCD
    - 1.5s 内只触发 0 次网络请求

### 难度筛选（DF）
- 全部 chip 默认激活
  - 打开 /
    - [data-diff=all] 有 .active 类
- 切到简单
  - 题库有 easy
    - 点 [data-diff=easy]
      - 该 chip 激活；列表只剩 difficulty=easy
- 切到中等
  - 点 [data-diff=medium]
    - 列表只剩 medium
- 切到困难
  - 题库有 hard
    - 点 [data-diff=hard]
      - 列表只剩 hard
- 搜索 + 难度叠加
  - 题库既有 easy A+B，又有 medium BFS
    - 切到 easy；再搜索 BFS
      - 结果 0；搜索 A+B → 命中 1

### 非功能（NF）
- 首屏可交互时间
  - 用 Playwright performance.timing / LargestContentfulPaint
    - LCP < 2.5s

## 单题详情页（Problem Detail）

### 冒烟与基础可用性（SMK）
- 直链刷新单题
  - 题库存在任意题 id=X
    - 直接打开 http://193.112.29.233:8088/problems/X
      - 200；SPA fallback 返回 index.html；前端按路由渲染单题详情

### 题库浏览（PL）
- 单题详情加载
  - 题 id=X
    - 点卡片 /problems/X
      - 标题渲染到 #pd-title-text；元数据完整；题干、样例全部渲染
- 单题默认模板填充
  - 题 id=X，未登录
    - 进入 /problems/X
      - CodeMirror 初始化；内容为 TEMPLATE
- 单题样例渲染多组
  - 题有 ≥2 个 samples
    - 进入 /problems/X
      - .sample-item 数 ≥ 2
- 单题 XSS 转义
  - 题干含 script 或 img onerror
    - 进入 /problems/X
      - 文本原样显示，未执行脚本
- 不存在题目
  - 题 id=999999
    - 进入 /problems/999999
      - 渲染题目不存在；不白屏
- 无效路径
  - 进入 /problems/abc
    - 显示无效的题目编号
- 单题 meta 与 difficulty 一致
  - 后台设 difficulty=hard
    - 进入 /problems/X
      - .diff-badge class 含 diff-hard，文字困难
- 单题导航返回
  - 题 id=X
    - 点返回题库按钮
      - URL 变 /；题列表正常渲染

### 在线编辑 / CodeMirror（ED）
- 编辑器初始化
  - 进入单题
    - 等 2s
      - .cm-editor 出现；行号、高亮、语法着色存在
- 输入字符可写
  - 在编辑器敲 cout << 1;
    - 文档变更；字符数 +N；行号更新
- Tab 缩进
  - 光标置于行首，按 Tab
    - 插入 2/4 空格缩进，未跳出编辑器
- Undo / Redo
  - 输入后 Ctrl+Z
    - 内容回退；Ctrl+Shift+Z 前进
- 重置回默认
  - ED-02 后
    - 点 #pd-reset
      - 编辑器内容 == TEMPLATE；#pd-draft-info 重新显示
- 清空草稿
  - 已保存草稿
    - 点 #pd-clear-draft
      - localStorage 中 draft:problem:<id> 被移除；#pd-draft-info 显示草稿:—

### 草稿持久化（DR）
- 刷新仍在
  - 题 id=X，已在编辑器输入 xxx
    - F5 刷新 /problems/X
      - 编辑器内容 == xxx；#pd-draft-info 显示草稿已保存 · N 字节
- 关闭浏览器仍在
  - DR-01
    - context.close() 后重新 browser.new_context()，打开 /problems/X
      - 编辑器内容仍是 xxx
- 草稿隔离
  - 题 A、B
    - 在 A 输入 aaa，在 B 输入 bbb
      - 互不污染
- 草稿空则默认模板
  - 题 id=X
    - 清空 localStorage 中 draft:problem:X，刷新
      - 编辑器 == TEMPLATE
- 草稿超限降级
  - 用 JS 注入一段 > 5MB 字符串到 draft:problem:X
    - 刷新页面不抛 fatal；编辑框显示该内容；超配额应降级而非崩溃

### 提交判题（SB）
- 正确解 → AC
  - A+B 题 + USER_MAIN 已登录
    - 提交正确解
      - 徽标 .badge-ac，文字全部样例通过!；无 #pd-compile-error
- 输出错误 → WA
  - A+B 题 + USER_MAIN 已登录
    - 提交 cout<<0;
      - 徽标 .badge-wa；message 含 Wrong Answer
- 编译错误 CE
  - A+B 题
    - 提交缺分号代码
      - 徽标 WA；message 含 Compile Error；#pd-compile-error 显示 g++ stderr
- 超时 TLE
  - 题 N 大且 time_limit=1s
    - 提交 O(N²) 算法
      - 徽标 WA；message 含 Time Limit Exceeded
- 超内存 MLE
  - 题 memory_limit=16 MB
    - 提交死循环 vector push
      - 徽标 WA；message 含 Memory Limit Exceeded
- 运行时错误 RE
  - 提交 int x = 1/0;
    - 徽标 WA；message 含 Runtime Error
- 空代码 → WA
  - 清空编辑器后点提交
    - 提示代码为空（前端拦截）；不发起网络请求
- 未登录提交
  - 干净 context
    - 打开单题 → 点 #pd-submit
      - 按钮不可点；或弹出 #auth-modal
- 提交中按钮禁用
  - 提交后立刻重试点 #pd-submit
    - 按钮变判题中… 且 disabled=true；判题完成恢复

### 边界与异常（EX）
- 提交空 code
  - POST /api/submit {problem_id, code:""}
    - 400
- 提交超长 code
  - POST /api/submit {problem_id, code: <70KB>}
    - 413
- 提交不存在 problem_id
  - POST /api/submit {problem_id: 999999, code}
    - 404
- system("rm -rf /")
  - 提交 system(rm -rf /tmp/oj_test) 代码
    - 服务侧 tmp 残留无变化；UI 拿到 WA
- 二进制 NUL
  - 题用例含 NUL 字节
    - 提交正确解
      - AC
- 末尾多空行
  - 题期望 3\n\n\n
    - 提交输出 3\n\n
      - AC

### 非功能（NF）
- 单次提交 P95
  - 提交正确解 20 次，记录端到端耗时
    - P95 ≤ 5000ms
- 并发提交串行化
  - 打开两个标签，都点提交
    - 服务端日志显示第二个等第一个完成；浏览器侧均得到正确结果
- while(1) 不会卡死服务
  - 提交死循环代码
    - 在 TL 内被 kill；UI 拿到 WA Time Limit Exceeded；服务仍可响应其它请求
- 输出超 64MB → 截断不崩
  - 题 special：期望输出固定 32MB
    - 提交代码打印 100MB
      - 服务不挂；UI 拿到结果
- 草稿写入频率
  - 1s 内连续输入 50 字符
    - localStorage 写入节流

## 登录注册（Login / Register）

### 普通用户认证（AU）
- 注册成功
  - 干净 context
    - 打开 /，点右上注册 → #auth-modal 出现 → 填 USER_MAIN / Passw0rd! → 提交
      - 模态框关闭；导航右上显示 USER_MAIN；/api/me 返回 200 且 role=user
- 注册失败：用户名冲突
  - USER_DUP 已在 fixture 中注册成功
    - 再次填写 USER_DUP 提交注册
      - .form-error 显示该用户名已被占用（HTTP 409）；不跳路由；USER_DUP 的旧 session 不被踢出
- 注册失败：用户名长度
  - 用户名 ab（<3）
    - .form-error 显示用户名长度提示
- 注册失败：密码长度
  - 密码 123（<6）
    - .form-error 显示密码长度提示
- 注册失败：非法字符
  - 用户名 bad name!
    - .form-error 显示仅允许字母/数字/下划线/连字符
- 登录成功
  - 干净 context（清 cookie）
    - 右上登录 → 填 USER_MAIN / Passw0rd! → 提交
      - 右上变 USER_MAIN；/api/me 200；导航不出现管理后台
- 登录失败：错密码
  - 密码错 1 位
    - .form-error 显示登录失败
- 退出登录
  - AU-06 已登录
    - 右上点用户菜单 → 退出登录
      - POST /api/logout 200；sessionStorage 清空；右上变登录 / 注册
- 会话刷新保持
  - AU-06 已登录
    - F5 刷新
      - Auth.hydrate() 自动从 /api/me 恢复登录态；导航不闪；编辑器内容仍在

## 管理员登录页（Admin Login）

### 冒烟与基础可用性（SMK）
- 错误码：admin 错密码
  - POST /api/admin/login {username:admin, password:wrong}
    - 401；body 含 error

### 管理员后台（AD）
- 管理员登录成功
  - /admin/login → 模态框填 admin/admin123
    - 登录后自动跳 /admin，渲染 .admin-table；导航出现管理后台链接

## 管理员后台 - 题库列表（Admin Panel - List）

### 冒烟与基础可用性（SMK）
- 直链刷新后台列表
  - 已登录 admin
    - 浏览器打开 /admin
      - 渲染 .admin-table，行数 == /api/problems 返回条数

### 管理员后台（AD）
- 普通用户登录后台被拒
  - USER_PERM 已登录
    - 访问 /admin
      - 跳 /admin/login；显示当前账号不是管理员；不渲染 .admin-table
- 后台列表含难度列
  - 题库有不同难度
    - 进 /admin
      - 表格列含编号 / 标题 / 难度 / 时间 / 内存 / 操作
- 删除题目：确认弹窗
  - 选中 AD-04 新建的题
    - 点该行 #data-del
      - 弹出确认删除题目? 模态框；点取消 → 关闭且未删
- 删除题目：确认执行
  - AD-08
    - 点删除
      - 请求 200；表格行消失；普通用户侧题列表与 /problems/<id> 404
- 两次删除同题
  - AD-09 后
    - 再点同 id 删除
      - 后端 404；alert(删除失败:…) 触发

### 边界与异常（EX）
- admin API 未登录
  - 干净 context
    - DELETE /api/admin/problems/1
      - 401
- admin API 普通用户
  - 普通用户登录
    - 同上
      - 403
- 同一 ID 二次删除
  - 题已删
    - 再发 DELETE /api/admin/problems/<id>
      - 404

## 管理员后台 - 新建题目（Admin Panel - Create）

### 冒烟与基础可用性（SMK）
- 错误码：未登录访问 admin API
  - 干净 context
    - POST /api/admin/problems 不带 cookie
      - 401；body 含 error 字段

### 管理员后台（AD）
- 新建题目成功
  - admin
    - 点新建题目 → 填标题 AUTOTEST_<ts> / 难度=easy / TL=1000 / ML=128 / 题干 / 输入输出 / 1 组用例 → 提交
      - 创建成功；自动跳 /problems/<new_id>；单题详情正常渲染
- 新建题目：缺标题
  - 留空标题
    - 不发请求，前端 .form-error 显示标题和题干不能为空
- 新建题目：动态增删用例
  - 点 #add-tc 多次；删到 1 个
    - 删除按钮禁用；继续点 add-tc 可恢复
- 新建题目：默认难度 medium
  - 打开 /admin/problems/new
    - #f-diff 默认值 medium

### 边界与异常（EX）
- time_limit=0 拒绝
  - admin
    - POST /api/admin/problems 带 time_limit_ms:0
      - 400

## 路由切换与 SPA 行为（Routing）

### 路由切换与 SPA 行为（RT）
- 列表 ↔ 单题
  - 列表 → 单题 → 返回
    - 无白屏；无整页 reload
- 单题 ↔ 后台
  - admin
    - 单题 → 顶部管理后台 → 单题
      - 切换无白屏；编辑器内容保留
- 浏览器后退 / 前进
  - / → /problems/X → 后退 → 前进
    - popstate 正确触发 App.handleRoute()；视图对应
- 直链打开未登录 /admin
  - 未登录
    - 直接访问 /admin
      - 跳 /admin/login；显示提示
- 直链打开已登录非管理员 /admin
  - 普通用户登录态
    - 直接访问 /admin
      - 跳 /admin/login 并显示当前账号不是管理员
- 直链打开 /admin/problems/new 已登录 admin
  - 打开 → 渲染表单
    - 路由正确

## 会话管理（Session）

### 非功能（NF）
- Session 过期
  - 把 session expires_at 改为过去时刻；admin 调 /api/admin/problems
    - 401；前端跳 /admin/login
