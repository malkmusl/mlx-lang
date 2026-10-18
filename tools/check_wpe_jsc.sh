#!/usr/bin/env bash
# JavaScriptCore of the WPE port at work:
#
#   - `./mlx build webkit-wpe-jsc-shell` links jsc (WebKit's shell) with
#     JavaScriptCore, WTF and bmalloc for WPE on mlx-glib and mlx-unicode
#     (mlx-out/webkit/wpe/bin/jsc), which runs projects/webkit/tests/wpe.js;
#   - `./mlx build webkit-wpe-jsc-glib-test` builds
#     projects/webkit/tests/jsc-glib.c, a C program of JavaScriptCore's
#     GLib API (<jsc/jsc.h>), into mlx-out/webkit/wpe/bin/jsc-glib.
#
# What each prints, with its exit status last, is compared with
# projects/webkit/tests/wpe-js.expected and jsc-glib.expected.
#
#   tools/check_wpe_jsc.sh
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"

fail() { echo "check_wpe_jsc: $*" >&2; exit 1; }

./mlx build webkit-wpe-jsc-shell webkit-wpe-jsc-glib-test >/dev/null || fail "webkit-wpe-jsc-shell or webkit-wpe-jsc-glib-test does not build"
work=$(mktemp -d /tmp/wpe-jsc.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
export LD_LIBRARY_PATH="$repo_root/mlx-out/lib"

status=0
(cd "$work" && timeout 120 "$repo_root/mlx-out/webkit/wpe/bin/jsc" "$repo_root/projects/webkit/tests/wpe.js") > "$work/js.out" 2> "$work/js.err" || status=$?
echo "exit $status" >> "$work/js.out"
diff -u projects/webkit/tests/wpe-js.expected "$work/js.out" || { cat "$work/js.err" >&2; fail "jsc prints otherwise"; }
echo "ok   jsc (WPE) runs tests/wpe.js"

status=0
(cd "$work" && timeout 120 "$repo_root/mlx-out/webkit/wpe/bin/jsc-glib") > "$work/glib.out" 2> "$work/glib.err" || status=$?
echo "exit $status" >> "$work/glib.out"
diff -u projects/webkit/tests/jsc-glib.expected "$work/glib.out" || { cat "$work/glib.err" >&2; fail "jsc-glib prints otherwise"; }
echo "ok   tests/jsc-glib.c: JavaScriptCore's GLib API"
echo "check_wpe_jsc: all checks passed"
