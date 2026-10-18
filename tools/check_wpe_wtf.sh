#!/usr/bin/env bash
# WTF of the WPE port at work: `./mlx build webkit-wpe-wtf-test` builds
# projects/webkit/tests/wtf-wpe.cpp with mlx-cc against WTF (WPE),
# bmalloc, mlx-glib, mlx-unicode and mlxlibcpp into
# mlx-out/webkit/wpe/bin/wtf-test; it runs the main run loop (GLib's main
# loop under it) with a repeating RunLoop::Timer and RunLoop::dispatchAfter,
# a WorkQueue's thread dispatching back to the main thread, and WTF::String
# and URL round trips. What it prints, with its exit status last, is
# compared with projects/webkit/tests/wtf-wpe.expected.
#
#   tools/check_wpe_wtf.sh
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"

fail() { echo "check_wpe_wtf: $*" >&2; exit 1; }

./mlx build webkit-wpe-wtf-test >/dev/null || fail "webkit-wpe-wtf-test does not build"
work=$(mktemp -d /tmp/wpe-wtf.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
status=0
(cd "$work" && LD_LIBRARY_PATH="$repo_root/mlx-out/lib" timeout 60 "$repo_root/mlx-out/webkit/wpe/bin/wtf-test") > "$work/out" 2> "$work/err" || status=$?
echo "exit $status" >> "$work/out"
diff -u projects/webkit/tests/wtf-wpe.expected "$work/out" || { cat "$work/err" >&2; fail "wtf-test prints otherwise"; }
cat "$work/out"
echo "check_wpe_wtf: WTF (WPE) runs its run loop, timers, work queue, strings and URLs as expected"
