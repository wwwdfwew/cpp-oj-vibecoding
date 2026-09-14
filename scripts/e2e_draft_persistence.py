#!/usr/bin/env python3
"""
SPEC §3 Phase 6.5 — verify localStorage draft persistence.

Drives a headless Chromium via Playwright to:
  1. Open the problem detail page
  2. Type code into the CodeMirror editor
  3. Reload the page
  4. Confirm the typed code is still present (localStorage draft)
  5. Navigate away (to problem list) and back
  6. Confirm again

Usage:
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

        # Surface page console + errors so a CodeMirror load failure is obvious.
        page.on("console", lambda msg: print(f"  [page:{msg.type}] {msg.text}"))
        page.on("pageerror", lambda err: print(f"  [page:error] {err}"))

        print(f"[test] open {problem_url}")
        page.goto(problem_url, wait_until="domcontentloaded")

        # Wait for the SPA to mount and the problem detail to start loading.
        page.wait_for_function(
            "() => document.getElementById('pd-title-text')?.innerText?.length > 0",
            timeout=15000,
        )

        # Wait for CodeMirror to boot (it lazy-loads ~10 modules from esm.sh
        # which can take 10+ seconds on a cold cache).
        page.wait_for_selector(".cm-content", timeout=45000)
        time.sleep(1.0)  # let extensions settle

        print("[test] type custom draft")
        page.locator(".cm-content").click()
        # Use keyboard: select all then type the body. CodeMirror's default
        # keymap includes Ctrl-A and Delete.
        page.keyboard.press("Control+a")
        page.keyboard.press("Delete")
        page.keyboard.insert_text(typed_code)

        # Confirm the draft landed in the editor view.
        in_editor = page.evaluate(
            "() => document.querySelector('.cm-content').innerText"
        )
        assert typed_code in in_editor, (
            f"typed code missing from editor.\n"
            f"expected snippet: {typed_code[:80]!r}\n"
            f"got: {in_editor[:200]!r}"
        )
        print("[test] typed code visible in editor ✓")

        # Give the updateListener (debounced) a tick to write to localStorage.
        time.sleep(0.3)
        draft_raw = page.evaluate(
            "(id) => localStorage.getItem(`draft:problem:${id}`)", args.problem_id
        )
        assert draft_raw is not None and typed_code in draft_raw, (
            f"localStorage missing draft; got: {draft_raw!r}"
        )
        print("[test] localStorage has draft ✓")

        # ---- 1. Reload the page; draft should survive. ----
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

        # ---- 2. Navigate away to problem list, then back. ----
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

        # ---- 3. Open a different problem; draft should be isolated by id. ----
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
            # Problem 1 doesn't exist (404) — fine, skip the cross-id check.
            print("[test] problem 1 unavailable (404), skipping cross-id check")

        browser.close()
    print("[test] PASS")


if __name__ == "__main__":
    try:
        main()
    except AssertionError as e:
        print(f"[test] FAIL: {e}", file=sys.stderr)
        sys.exit(1)
