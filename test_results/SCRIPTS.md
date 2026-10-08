# Test Results 目录脚本说明

本目录包含 Web 自动化测试执行过程中使用的所有辅助脚本。

所有 `.js` 文件都是 **Playwright `run-code` 命令**的输入（`playwright-cli run-code --filename=xxx.js`），它们在浏览器上下文中执行 `fetch()` 调用或读取 DOM 状态，结果以 JSON 形式返回。

## 1. 状态检查类（DOM 探针）

| 文件 | 用途 | 调用方式 | 输出 |
|---|---|---|---|
| `check_diff.js` | 读取难度 chip `[data-diff="all"]` 的 className/aria-pressed/textContent，用于 **DF-01** 验证默认激活 | `playwright-cli run-code --filename=check_diff.js` | `{className:"chip active", ariaPressed:null, text:"..."}` |
| `check_form.js` | 读取注册表单 `#auth-username` 和 `#auth-password` 的 minLength/pattern/validity，用于 **AU-03/04** 验证 HTML5 校验属性 | `playwright-cli run-code --filename=check_form.js` | `{username:{minLength:3,validity:"..."}, password:{minLength:6}}` |
| `check_me.js` | 调用 `GET /api/me`，验证登录态，用于 **AU-01** 角色检查 | `playwright-cli run-code --filename=check_me.js` | `{status:200, body:"{\"role\":\"user\",...}"}` |
| `check_pat.js` | 读取 `#auth-username` 的 pattern 属性，用于 **AU-05** 验证用户名正则约束 | `playwright-cli run-code --filename=check_pat.js` | `{value:"...", validity:"...", pattern:"..."}` |
| `check_pwd.js` | 读取用户名/密码框的当前值和 validity 状态，用于 **AU-03/04** 长度校验调试 | `playwright-cli run-code --filename=check_pwd.js` | `{username:{...}, password:{...}}` |

## 2. 边界异常类（API 探针）

| 文件 | 用途 | 调用方式 | 期望输出 |
|---|---|---|---|
| `empty_code.js` | 提交空代码 → `/api/submit`，用于 **EX-01** 验证空代码被拒绝 | `playwright-cli run-code --filename=empty_code.js` | `400 + {"error":"代码为空"}` |
| `long_code.js` | 提交 70KB xxxxxxx → `/api/submit`，用于 **EX-02** 验证代码长度限制（**BUG：实际返回 200**） | `playwright-cli run-code --filename=long_code.js` | 应 `413`，实际 `200 + Compile Error` |
| `notexist_problem.js` | 提交 `problem_id=999999` → `/api/submit`，用于 **EX-03** 验证不存在题目（**BUG：实际返回 200**） | `playwright-cli run-code --filename=notexist_problem.js` | 应 `404`，实际 `200 + Problem not found` |
| `tl0.js` | admin 创建题目带 `time_limit_ms=0`，用于 **EX-07** 验证时间范围校验 | `playwright-cli run-code --filename=tl0.js` | `400 + "time_limit_ms must be in [100, 10000]"` |

## 3. 权限验证类（多步流程）

| 文件 | 用途 | 调用方式 | 期望输出 |
|---|---|---|---|
| `admin_api_user.js` | 1) 登出 admin → 2) 登录普通用户 → 3) 尝试 `DELETE /api/admin/problems/1`，用于 **EX-09** 验证普通用户调 admin API 被拒 | `playwright-cli run-code --filename=admin_api_user.js` | `del: {status:403, body:"{\"error\":\"admin only\"}"}` |
| `del2.js` | 1) admin 登录 → 2) 删除已不存在的 #18，用于 **AD-10 / EX-10** 验证二次删除 | `playwright-cli run-code --filename=del2.js` | `del: {status:404, body:"{\"error\":\"not found\"}"}` |
| `delete_check.js` | 直接删除 #18（admin 登录态），用于 **AD-09** 验证删除执行 | `playwright-cli run-code --filename=delete_check.js` | `404`（首次成功 200，再次 404） |
| `second_delete.js` | 完整流程：登出→admin 登录→创建临时题→第一次删→第二次删，用于 **EX-10** 端到端验证 | `playwright-cli run-code --filename=second_delete.js` | `{create:200, del1:200, del2:404}` |

## 4. 测试编排脚本

| 文件 | 用途 |
|---|---|
| `run_tests.ps1` | PowerShell 测试编排脚本（v1.0 旧版本，未完全跑通，保留作为参考）。当前测试已改用交互式 `playwright-cli` 直接执行。 |
| `console_output.txt` | 旧版 PowerShell 脚本运行时的 console 输出日志（v1.0 阶段产物，仅供参考）。 |

## 5. 使用示例

```bash
# 1. 启动浏览器
playwright-cli open --headed http://193.112.29.233:8088/

# 2. 在浏览器中导航到目标
playwright-cli goto http://193.112.29.233:8088/

# 3. 执行 JS 探针脚本
playwright-cli run-code --filename=test_results/check_diff.js
# → {"className":"chip active","ariaPressed":null,"text":"..."}

# 4. 关闭浏览器
playwright-cli close
```

## 6. 脚本与测试用例映射

| 脚本 | 对应用例 | 套件 | 期望结果 | 实际结果 |
|---|---|---|---|---|
| `check_diff.js` | DF-01 | HOME | `chip active` | ✅ PASS |
| `check_form.js` | AU-03/04 | AUTH | minLength=3/6 + 校验提示 | ✅ PASS |
| `check_me.js` | AU-01 | AUTH | 200 + role=user | ✅ PASS |
| `check_pat.js` | AU-05 | AUTH | pattern 属性 | ✅ PASS |
| `check_pwd.js` | AU-03/04 | AUTH | 校验提示 | ✅ PASS |
| `empty_code.js` | EX-01 | DETAIL | 400 | ✅ PASS |
| `long_code.js` | EX-02 | DETAIL | 413（实际 200）| ❌ BUG-EX-02 |
| `notexist_problem.js` | EX-03 | DETAIL | 404（实际 200）| ❌ BUG-EX-03 |
| `admin_api_user.js` | EX-09 | ADMIN_LIST | 403 | ✅ PASS |
| `del2.js` | AD-10 / EX-10 | ADMIN_LIST | 404 | ✅ PASS |
| `delete_check.js` | AD-09 | ADMIN_LIST | 200/404 | ✅ PASS |
| `second_delete.js` | EX-10 | ADMIN_LIST | del2=404 | ✅ PASS |
| `tl0.js` | EX-07 | ADMIN_CREATE | 400 | ✅ PASS |

详细测试结果见 [`Web自动化测试-playwright.md`](./Web自动化测试-playwright.md)。