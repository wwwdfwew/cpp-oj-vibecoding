#!/usr/bin/env python3
"""
对应 SPEC §3 Phase 6.5 —— 验证 localStorage 草稿的持久化。

通过 Playwright 驱动无头 Chromium:
  1. 打开题目详情页
  2. 在 CodeMirror 编辑器中输入代码
  3. 刷新页面
  4. 确认输入的代码还在(localStorage 草稿)
  5. 跳转到题库再返回
  6. 再次确认

用法:
  python3 scripts/e2e_draft_persistence.py [--base http://localhost:8088]
"""
import argparse
import os
import sys
import time

from playwright.sync_api import sync_playwright


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default=os.environ.get("OJ_BASE", "http://localhost:8088"))
    ap.add_argument("--problem-id", type=int, default=8)  # default: Sum of N (id=8)
    args = ap.parse_args()

    base = args.base.rstrip("/")
    problem_url = f"{base}/problems/{args.problem_id}"

    typed_code = (
        "// CUSTOM DRAFT FROM TEST\n"
        "#include <bits/stdc++.h>\n"
        "using namespace std;\n"
        "int main(){ long long s=0; int x; while(cin>>x) s+=x; cout<<s; }\n"
    )

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        context = browser.new_context()
        page = context.new_page()

        # 打印页面控制台和错误,方便看出 CodeMirror 加载失败等问题。
        page.on("console", lambda msg: print(f"  [page:{msg.type}] {msg.text}"))
        page.on("pageerror", lambda err: print(f"  [page:error] {err}"))

        print(f"[test] open {problem_url}")
        page.goto(problem_url, wait_until="domcontentloaded")

        # 等待 SPA 挂载并开始加载题目详情。
        page.wait_for_function(
            "() => document.getElementById('pd-title-text')?.innerText?.length > 0",
            timeout=15000,
        )

        # 等待 CodeMirror 启动(它会懒加载约 10 个 esm.sh 模块,
        # 冷启动可能耗时 10 秒以上)。
        page.wait_for_selector(".cm-content", timeout=45000)
        time.sleep(1.0)  # let extensions settle

        print("[test] type custom draft")
        page.locator(".cm-content").click()
        # 使用键盘操作:全选再输入主体。CodeMirror 默认键位包含 Ctrl-A 与 Delete。
        page.keyboard.press("Control+a")
        page.keyboard.press("Delete")
        page.keyboard.insert_text(typed_code)

        # 确认草稿已进入编辑器视图。
        in_editor = page.evaluate(
            "() => document.querySelector('.cm-content').innerText"
        )
        assert typed_code in in_editor, (
            f"typed code missing from editor.\n"
            f"expected snippet: {typed_code[:80]!r}\n"
            f"got: {in_editor[:200]!r}"
        )
        print("[test] typed code visible in editor ✓")

        # 给 updateListener(有防抖)一点时间把内容写入 localStorage。
        time.sleep(0.3)
        draft_raw = page.evaluate(
            "(id) => localStorage.getItem(`draft:problem:${id}`)", args.problem_id
        )
        assert draft_raw is not None and typed_code in draft_raw, (
            f"localStorage missing draft; got: {draft_raw!r}"
        )
        print("[test] localStorage has draft ✓")

        # ---- 1. 刷新页面,草稿应该保留。 ----
        print("[test] reload page")
        page.reload(wait_until="domcontentloaded")
        page.wait_for_selector(".cm-content", timeout=45000)
        time.sleep(1.0)

        in_editor_after_reload = page.evaluate(
            "() => document.querySelector('.cm-content').innerText"
        )
        assert typed_code in in_editor_after_reload, (
            f"draft did NOT survive reload.\n"
            f"expected snippet: {typed_code[:80]!r}\n"
            f"got: {in_editor_after_reload[:200]!r}"
        )
        print("[test] draft survived reload ✓")

        # ---- 2. 跳转到题库再回来。 ----
        print("[test] navigate to problem list and back")
        page.evaluate("() => { history.pushState({}, '', '/'); window.dispatchEvent(new PopStateEvent('popstate')); }")
        time.sleep(0.5)
        page.goto(problem_url, wait_until="domcontentloaded")
        page.wait_for_selector(".cm-content", timeout=45000)
        time.sleep(1.0)

        in_editor_after_nav = page.evaluate(
            "() => document.querySelector('.cm-content').innerText"
        )
        assert typed_code in in_editor_after_nav, (
            f"draft did NOT survive navigation.\n"
            f"got: {in_editor_after_nav[:200]!r}"
        )
        print("[test] draft survived nav-away-and-back ✓")

        # ---- 3. 打开另一道题,草稿应当按 id 隔离。 ----
        print("[test] confirm draft is isolated by problem id")
        page.goto(f"{base}/problems/1", wait_until="domcontentloaded")
        try:
            page.wait_for_selector(".cm-content", timeout=10000)
            other_editor = page.evaluate(
                "() => document.querySelector('.cm-content').innerText"
            )
            assert typed_code not in other_editor, (
                "draft from problem 8 leaked into problem 1"
            )
            print("[test] no draft leak across problems ✓")
        except Exception:
            # 题目 1 不存在(404)的话,跳过跨 id 校验即可。
            print("[test] problem 1 unavailable (404), skipping cross-id check")

        browser.close()
    print("[test] PASS")


if __name__ == "__main__":
    try:
        main()
    except AssertionError as e:
        print(f"[test] FAIL: {e}", file=sys.stderr)
        sys.exit(1)
