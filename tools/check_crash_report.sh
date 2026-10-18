#!/usr/bin/env bash
# The crash reports of the desktop programs, the coreutils and mlxlibc
# (std/src/crash.mlx) end to end, on tests/support/crash_report.mlx:
#   - the compiler names every function path:line:name in the symbol table,
#     and its line table says the line of each address;
#   - a failed runtime check, a bad store, a stack overflow, an abort, a
#     failure (crash.fail) and a full value arena each say the
#     function they happened in and the functions that called it;
#   - each crash is kept as a line in $XDG_STATE_HOME/mlx/crashes.log;
#   - every program build.mlx declares installs the report (crash.install
#     first thing in main), every Wayland client watches its connection
#     (std.wayland.crash.watch).
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
grep -q '^  at tests/support/crash_report.mlx:12:middle+0x[0-9a-f]* at tests/support/crash_report.mlx:9$' "$work/index.out" || fail "the failed check is not placed in middle, line 9 (pick, inlined)" "$work/index.out"
grep -q '^  from tests/support/crash_report.mlx:27:main+0x[0-9a-f]* at tests/support/crash_report.mlx:33$' "$work/index.out" || fail "main (line 33) is not among the callers" "$work/index.out"
echo "ok   a failed runtime check names its function, the callers and their lines"

run null 139
grep -q 'segmentation fault touching 0x8$' "$work/null.out" || fail "no fault address" "$work/null.out"
grep -q '^  at tests/support/crash_report.mlx:17:store+0x[0-9a-f]* at tests/support/crash_report.mlx:18$' "$work/null.out" || fail "the fault is not placed in store, line 18" "$work/null.out"
echo "ok   a bad store names its function"

run deep 139
grep -q 'the stack ran out' "$work/deep.out" || fail "the overflow is not recognised" "$work/deep.out"
grep -q '^  from tests/support/crash_report.mlx:21:deeper+0x[0-9a-f]* at tests/support/crash_report.mlx:24 ([0-9]* more times)$' "$work/deep.out" || fail "the recursion is not said once" "$work/deep.out"
echo "ok   a stack overflow names the recursion once"

run abort 134
grep -q '^crashy: crashed: aborted' "$work/abort.out" || fail "an abort is not reported" "$work/abort.out"
grep -q '^  from tests/support/crash_report.mlx:[0-9]*:main+0x' "$work/abort.out" || fail "the abort does not name main" "$work/abort.out"
echo "ok   an abort (a C library giving up) is reported too"

run fail 1
grep -q '^crashy: failed: the compositor ended the connection (a test)$' "$work/fail.out" || fail "a failure (crash.fail) is not reported" "$work/fail.out"
grep -q '^  at tests/support/crash_report.mlx:[0-9]*:main+0x' "$work/fail.out" || fail "the failure does not name main" "$work/fail.out"
echo "ok   a failure the program cannot go on after (crash.fail) is reported with its callers"

# The handler's own values come from the value arena too: when the program
# filled it, the handler takes room of its own and says so.
run values 132
grep -q "^crashy: crashed: illegal instruction (the values' arena is full" "$work/values.out" || fail "a full value arena is not said" "$work/values.out"
grep -q '^  at tests/support/crash_report.mlx:[0-9]*:hoard+0x' "$work/values.out" || fail "the full arena does not name hoard" "$work/values.out"
echo "ok   a full value arena is said, in the function that asked for more"

log="$work/state/mlx/crashes.log"
[[ -f "$log" ]] || fail "no crashes.log"
[[ $(wc -l < "$log") -eq 6 ]] || fail "crashes.log does not hold six crashes" "$log"
awk -F'\t' '$1 != "crash" || $2 !~ /^[0-9]+$/ || $3 != "crashy" || NF < 5 { bad = 1 } END { exit bad }' "$log" || fail "crashes.log lines are not crash/time/program/reason/frames" "$log"
grep -q "	tests/support/crash_report.mlx:12:middle+0x[0-9a-f]* at tests/support/crash_report.mlx:9	" "$log" || fail "the kept crash has no frames" "$log"
echo "ok   every crash is kept in \$XDG_STATE_HOME/mlx/crashes.log"

# The rule: every program build.mlx declares (its root module) installs the
# crash report, so a crash anywhere on the system is said and kept for MLX
# Observatory. The Android packages are left out: their entry is the
# activity's, not main. Shared objects and plugins are left out too: they
# run in another program's process, whose handler it is.
missing=$(grep -oE '(Build\.)?program\(b, ([a-z_]+, )?"[^"]+", "[^"]+"' build.mlx | sed 's/.*", "//; s/"$//' | sort -u | while read -r root; do
    [[ "$root" == *coreutils* || "$root" == *android* ]] && continue
    grep -qE 'crash[a-z_]*\.install\(' "$root" || echo "$root"
done)
[[ -z "$missing" ]] || fail "these programs do not install the crash report (std.crash install() first in main): $missing"
for utility in projects/coreutils/*/main.mlx; do
    grep -qE 'crash[a-z_]*\.install\(' "$utility" || fail "$utility does not install the crash report"
done
echo "ok   every program build.mlx declares installs the crash report"

# The rule: every Wayland client in projects/, examples/ and tools/ watches
# its connection (std.wayland.crash.watch, right after it connects), so a protocol error
# or a request on an object that is gone is reported and kept like a
# crash instead of the program just ending. (tests/ exercise the library's
# own failure handling.)
missing=$(grep -rlE 'Display\.(connect|connectTo|connectToHandle)\(' --include=*.mlx projects examples tools | while read -r file; do grep -q 'wayland_crash\.watch(' "$file" || echo "$file"; done)
[[ -z "$missing" ]] || fail "these Wayland clients do not watch their connection (std.wayland.crash.watch after connecting): $missing"
echo "ok   every Wayland client watches its connection (std.wayland.crash.watch)"
