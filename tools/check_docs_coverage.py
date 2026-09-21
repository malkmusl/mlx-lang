#!/usr/bin/env python3
"""Docs-conformance checks for docs/.

This script enforces the citation discipline docs/ already follows: every
page grounds its claims in a concrete test fixture, a real link to another
file, or a spec diagnostic code, rather than inventing behavior. It exists so
that discipline survives new contributions, not just the pass that first
established it (see docs/README.md).

Checks, each independent and all required to pass:

  1. test-coverage   Every *.mlx file under tests/ (including tests/modules/
                      and tests/support/) is cited by filename somewhere
                      under docs/. A new test fixture that lands without a
                      matching docs update fails this check.
  2. links           Every relative markdown link in docs/**/*.md and
                      README.md resolves to a real file (or a real directory,
                      for a link like `spec/`).
  3. fences          Every ``` code-fence in docs/**/*.md is balanced
                      (opened and closed), catching a truncated edit.
  4. diagnostic-codes Every MLX-E/MLX-W code in
                      spec/02-compiler/diagnostics/codes.xml appears
                      somewhere in docs/guide/11-diagnostics.md, so the
                      guide's diagnostics table can't silently drop a code
                      the spec adds.

Exit status is 0 if every check passes, 1 otherwise. Run it locally with:

    python3 tools/check_docs_coverage.py

No third-party dependencies; stdlib only, so CI needs nothing but python3.
"""

from __future__ import annotations

import glob
import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def repo_path(*parts: str) -> str:
    return os.path.join(REPO_ROOT, *parts)


def all_doc_files() -> list[str]:
    files = []
    for root, _dirs, names in os.walk(repo_path("docs")):
        for name in names:
            if name.endswith(".md"):
                files.append(os.path.join(root, name))
    readme = repo_path("README.md")
    if os.path.exists(readme):
        files.append(readme)
    return sorted(files)


def check_test_coverage() -> list[str]:
    test_files = (
        glob.glob(repo_path("tests", "*.mlx"))
        + glob.glob(repo_path("tests", "modules", "*.mlx"))
        + glob.glob(repo_path("tests", "support", "*.mlx"))
    )
    basenames = sorted(os.path.basename(f) for f in test_files)

    corpus = ""
    for f in all_doc_files():
        with open(f, encoding="utf-8") as fh:
            corpus += fh.read() + "\n"

    missing = [name for name in basenames if name not in corpus]
    errors = []
    for name in missing:
        errors.append(
            f"tests/**/{name} is not cited anywhere under docs/. "
            f"Add an example or a citation to the guide/reference chapter "
            f"this fixture belongs to (see docs/README.md)."
        )
    return errors


LINK_RE = re.compile(r"\]\(([^)]+)\)")


def check_links() -> list[str]:
    errors = []
    for f in all_doc_files():
        with open(f, encoding="utf-8") as fh:
            text = fh.read()
        for match in LINK_RE.finditer(text):
            target = match.group(1)
            if target.startswith(("http://", "https://", "mailto:")):
                continue
            path_part = target.split("#", 1)[0]
            if path_part == "":
                continue  # pure in-page anchor
            resolved = os.path.normpath(os.path.join(os.path.dirname(f), path_part))
            if not os.path.exists(resolved):
                rel = os.path.relpath(f, REPO_ROOT)
                errors.append(f"{rel}: link target '{target}' does not resolve (looked for {resolved})")
    return errors


def check_fences() -> list[str]:
    errors = []
    for f in all_doc_files():
        with open(f, encoding="utf-8") as fh:
            text = fh.read()
        fence_lines = re.findall(r"^```", text, re.MULTILINE)
        if len(fence_lines) % 2 != 0:
            rel = os.path.relpath(f, REPO_ROOT)
            errors.append(f"{rel}: odd number of ``` fence markers ({len(fence_lines)}) - a code block is unclosed")
    return errors


CODE_RE = re.compile(r'<Code id="(MLX-[EW]\d+)"')


def check_diagnostic_codes() -> list[str]:
    codes_xml = repo_path("spec", "02-compiler", "diagnostics", "codes.xml")
    if not os.path.exists(codes_xml):
        return [f"{os.path.relpath(codes_xml, REPO_ROOT)} not found - diagnostic-codes check needs it"]

    with open(codes_xml, encoding="utf-8") as fh:
        codes = CODE_RE.findall(fh.read())

    diagnostics_doc = repo_path("docs", "guide", "11-diagnostics.md")
    if not os.path.exists(diagnostics_doc):
        return [f"{os.path.relpath(diagnostics_doc, REPO_ROOT)} not found"]
    with open(diagnostics_doc, encoding="utf-8") as fh:
        doc_text = fh.read()

    missing = [code for code in codes if code not in doc_text]
    errors = []
    for code in missing:
        errors.append(
            f"{code} is defined in spec/02-compiler/diagnostics/codes.xml but "
            f"missing from docs/guide/11-diagnostics.md's code table."
        )
    return errors


def main() -> int:
    checks = [
        ("test-coverage", check_test_coverage),
        ("links", check_links),
        ("fences", check_fences),
        ("diagnostic-codes", check_diagnostic_codes),
    ]

    all_errors: list[tuple[str, str]] = []
    for name, fn in checks:
        errors = fn()
        status = "OK" if not errors else f"FAIL ({len(errors)})"
        print(f"[{name}] {status}")
        for err in errors:
            all_errors.append((name, err))

    if all_errors:
        print("\nFailures:")
        for name, err in all_errors:
            print(f"  [{name}] {err}")
        print(f"\n{len(all_errors)} docs-conformance failure(s).")
        return 1

    print("\nAll docs-conformance checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
