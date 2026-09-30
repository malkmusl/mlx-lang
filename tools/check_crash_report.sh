#!/usr/bin/env bash
# The crash reports of the desktop programs (examples/wayland-compositor/
# crash.mlx) end to end, on tests/support/crash_report.mlx:
#   - the compiler names every function path:line:name in the symbol table;
#   - a failed runtime check, a bad store and a stack overflow each say the
#     function they happened in and the functions that called it;
#   - each crash is kept as a line in $XDG_STATE_HOME/mlx/crashes.log.
#
#   tools/check_crash_report.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; [[ -f "${2:-}" ]] && cat "$2" >&2; exit 1; }

"$compiler" --quiet tests/support/crash_report.mlx -o "$work/crashy"
if command -v nm > /dev/null; then
    nm "$work/crashy" > "$work/symbols"
    grep -q ' tests/support/crash_report.mlx:12:middle$' "$work/symbols" || fail "no symbol for middle" "$work/symbols"
    grep -q ' _start$' "$work/symbols" || fail "no symbol for _start" "$work/symbols"
    echo "ok   the symbol table names functions path:line:name"
fi

run() {
    set +e
    XDG_STATE_HOME="$work/state" "$work/crashy" "$1" > "$work/$1.out" 2>&1
    status=$?
    set -e
    [[ $status -eq $2 ]] || fail "crashy $1 exited $status, not $2" "$work/$1.out"
}

run index 132
grep -q '^crashy: crashed: illegal instruction' "$work/index.out" || fail "no crash line" "$work/index.out"
grep -q '^  at tests/support/crash_report.mlx:12:middle+0x' "$work/index.out" || fail "the failed check is not placed in middle" "$work/index.out"
grep -q '^  from tests/support/crash_report.mlx:27:main+0x' "$work/index.out" || fail "main is not among the callers" "$work/index.out"
echo "ok   a failed runtime check names its function and the callers"

run null 139
grep -q 'segmentation fault touching 0x8$' "$work/null.out" || fail "no fault address" "$work/null.out"
grep -q '^  at tests/support/crash_report.mlx:17:store+0x' "$work/null.out" || fail "the fault is not placed in store" "$work/null.out"
echo "ok   a bad store names its function"

run deep 139
grep -q 'the stack ran out' "$work/deep.out" || fail "the overflow is not recognised" "$work/deep.out"
grep -q '^  from tests/support/crash_report.mlx:21:deeper+0x[0-9a-f]* ([0-9]* more times)$' "$work/deep.out" || fail "the recursion is not said once" "$work/deep.out"
echo "ok   a stack overflow names the recursion once"

log="$work/state/mlx/crashes.log"
[[ -f "$log" ]] || fail "no crashes.log"
[[ $(wc -l < "$log") -eq 3 ]] || fail "crashes.log does not hold three crashes" "$log"
awk -F'\t' '$1 != "crash" || $2 !~ /^[0-9]+$/ || $3 != "crashy" || NF < 5 { bad = 1 } END { exit bad }' "$log" || fail "crashes.log lines are not crash/time/program/reason/frames" "$log"
grep -q "	tests/support/crash_report.mlx:12:middle+0x[0-9a-f]*	" "$log" || fail "the kept crash has no frames" "$log"
echo "ok   every crash is kept in \$XDG_STATE_HOME/mlx/crashes.log"
