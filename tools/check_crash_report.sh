#!/usr/bin/env bash
# The crash reports of every program (std/src/crash.mlx) end to end, on
# tests/support/crash_report.mlx and on mlx-cc:
#   - the compiler names every function path:line:name in the symbol table,
#     and its line table says the line of each address;
#   - a failed runtime check, a bad store, a stack overflow, an abort, a
#     failure (crash.fail) and a full value arena each say the
#     function they happened in and the functions that called it;
#   - what the program said it was doing (crash.context, contextAt) is
#     said after the reason, and left out when its memory is gone;
#   - each crash is kept as a line in $XDG_STATE_HOME/mlx/crashes.log,
#     the context with the reason;
#   - every program build.mlx declares installs the report (crash.install
#     first thing in main), every Wayland client watches its connection
#     (std.wayland.crash.watch);
#   - a crash of mlx-cc (a C program it still cannot compile: a 64 KiB
#     array member initialised with { 0 }) names the compiler, the function
#     and the file:line being compiled, and `mlx-codemap crashes` lists it
#     on that function, as it lists the probe's.
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
    grep -q ' tests/support/crash_report.mlx:16:middle$' "$work/symbols" || fail "no symbol for middle" "$work/symbols"
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
grep -q '^  at tests/support/crash_report.mlx:16:middle+0x[0-9a-f]* at tests/support/crash_report.mlx:13$' "$work/index.out" || fail "the failed check is not placed in middle, line 13 (pick, inlined)" "$work/index.out"
grep -q '^  from tests/support/crash_report.mlx:31:main+0x[0-9a-f]* at tests/support/crash_report.mlx:38$' "$work/index.out" || fail "main (line 38) is not among the callers" "$work/index.out"
grep -q '^  while ' "$work/index.out" && fail "a program that said no context has a while line" "$work/index.out"
echo "ok   a failed runtime check names its function, the callers and their lines"

run null 139
grep -q 'segmentation fault touching 0x8$' "$work/null.out" || fail "no fault address" "$work/null.out"
grep -q '^  at tests/support/crash_report.mlx:21:store+0x[0-9a-f]* at tests/support/crash_report.mlx:22$' "$work/null.out" || fail "the fault is not placed in store, line 22" "$work/null.out"
echo "ok   a bad store names its function"

run deep 139
grep -q 'the stack ran out' "$work/deep.out" || fail "the overflow is not recognised" "$work/deep.out"
grep -q '^  from tests/support/crash_report.mlx:25:deeper+0x[0-9a-f]* at tests/support/crash_report.mlx:28 ([0-9]* more times)$' "$work/deep.out" || fail "the recursion is not said once" "$work/deep.out"
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

# What the program said it was doing (crash.contextAt, as the compilers
# say the file and line they read): a line of its own after the reason,
# before the frames.
run context 132
grep -q '^crashy: crashed: illegal instruction' "$work/context.out" || fail "no crash line with a context" "$work/context.out"
[[ $(sed -n 2p "$work/context.out") == "  while compiling src/widget.c:12" ]] || fail "the context is not said after the reason" "$work/context.out"
grep -q '^  at tests/support/crash_report.mlx:16:middle+0x' "$work/context.out" || fail "the crash with a context does not name middle" "$work/context.out"
echo "ok   what the program was doing (crash.contextAt) is said after the reason"

# A context pointing at memory the program gave back is left out, not
# read: the report still names the function.
run where 139
grep -q '^crashy: crashed: segmentation fault touching 0x10$' "$work/where.out" || fail "the crash with a gone context has no crash line" "$work/where.out"
grep -q '^  while ' "$work/where.out" && fail "a context in unmapped memory was read" "$work/where.out"
grep -q '^  at tests/support/crash_report.mlx:21:store+0x' "$work/where.out" || fail "the crash with a gone context does not name store" "$work/where.out"
echo "ok   a context whose memory is gone is left out of the report"

log="$work/state/mlx/crashes.log"
[[ -f "$log" ]] || fail "no crashes.log"
[[ $(wc -l < "$log") -eq 8 ]] || fail "crashes.log does not hold eight crashes" "$log"
awk -F'\t' '$1 != "crash" || $2 !~ /^[0-9]+$/ || $3 != "crashy" || NF < 5 { bad = 1 } END { exit bad }' "$log" || fail "crashes.log lines are not crash/time/program/reason/frames" "$log"
grep -q "	tests/support/crash_report.mlx:16:middle+0x[0-9a-f]* at tests/support/crash_report.mlx:13	" "$log" || fail "the kept crash has no frames" "$log"
grep -q "	illegal instruction ([^	]*), while compiling src/widget.c:12	tests/support/crash_report.mlx:16:middle" "$log" || fail "the kept crash does not have the context with its reason" "$log"
echo "ok   every crash is kept in \$XDG_STATE_HOME/mlx/crashes.log, the context with the reason"

# The rule: every program build.mlx declares (its root module) installs the
# crash report first thing in main, so a crash anywhere on the system is
# said and kept for MLX Observatory. The Android packages are left out:
# their entry is the activity's, and std.crash's handler is x86-64's.
# The coreutils install it through projects/coreutils' shared modules
# (digest_cli.mlx for the sums) or their own main.
missing=$(grep -oE '(Build\.)?program\(b, ([a-z_]+, )?"[^"]+", "[^"]+"' build.mlx | sed 's/.*", "//; s/"$//' | sort -u | while read -r root; do
    [[ "$root" == *coreutils* || "$root" == *android* ]] && continue
    grep -qE 'crash[a-z_]*\.install\(' "$root" || echo "$root"
done)
[[ -z "$missing" ]] || fail "these programs do not install the crash report (std.crash install() first in main): $missing"
for utility in projects/coreutils/*/main.mlx; do
    grep -qE 'crash[a-z_]*\.install\(|digest_cli\.' "$utility" || fail "$utility does not install the crash report"
done
echo "ok   every program build.mlx declares installs the crash report"

# mlx-cc: a crash of the C compiler names it, the function it was in and
# the file:line it was compiling (crash.contextAt in compiler/cc). The
# input is one it still crashes on (a stack overflow in the types of a
# 64 KiB array member initialised with { 0 }); the day it compiles, the
# report of a compiler crash is what the probe above checked.
mkdir -p "$work/cc"
"$compiler" --quiet compiler/cc/main.mlx -o "$work/cc/mlx-cc" || fail "mlx-cc does not build"
ln -s "$repo_root/compiler/cc/include" "$work/cc/include"
printf 'typedef struct { char buffer[65536]; int n; } Async;\nint main(void) { Async a = { 0 }; return a.n; }\n' > "$work/async.c"
set +e
(cd "$work" && XDG_STATE_HOME="$work/state" "$work/cc/mlx-cc" async.c -o "$work/async" > "$work/cc.out" 2>&1)
cc_status=$?
set -e
if [[ $cc_status -eq 0 ]]; then
    echo "ok   mlx-cc compiles the 64 KiB array member now (the crash it was checked on is gone)"
    cc_crashed=0
else
    [[ $cc_status -ge 128 ]] || fail "mlx-cc exited $cc_status on async.c, neither compiling nor crashing" "$work/cc.out"
    grep -q '^mlx-cc: crashed: ' "$work/cc.out" || fail "the crash of mlx-cc is not reported as mlx-cc's" "$work/cc.out"
    grep -q '^  while compiling async.c:2$' "$work/cc.out" || fail "the crash of mlx-cc does not say the file and line being compiled" "$work/cc.out"
    grep -q '^  at compiler/cc/[a-z0-9_]*\.mlx:[0-9]*:[A-Za-z0-9_]*+0x[0-9a-f]* at compiler/cc/' "$work/cc.out" || fail "the crash of mlx-cc does not name its function" "$work/cc.out"
    grep -q "	mlx-cc	[^	]*, while compiling async.c:2	compiler/cc/" "$log" || fail "the crash of mlx-cc is not kept with its context" "$log"
    echo "ok   a crash of mlx-cc names it, its function and the file:line it was compiling"
    cc_crashed=1
fi

# mlx-cc's programs: the linker writes their symbol table (link.mlx), the
# locals first, every function and variable with its type, binding and
# size, and the line table of where each function was defined; on
# mlxlibc a crash names each frame's function, a C++ one mangled and as
# it is written (std.demangle), and where it was defined. -s leaves the
# tables out.
tools/build_mlxlibc.sh --compiler "$compiler" --out "$work/libc" > "$work/libc.log" 2>&1 || fail "mlxlibc does not build" "$work/libc.log"
loader="$work/libc/libmlxc.so.1"
"$work/cc/mlx-cc" tests/support/crash_report_cc.c -o "$work/cc/crash_c" --dynamic-linker="$loader" 2> "$work/cc_c.err" || fail "the C probe does not build" "$work/cc_c.err"
"$work/cc/mlx-cc" -c tests/support/crash_report_cc.cpp -o "$work/cc/crash_cpp.o" 2> "$work/cc_cpp.err" || fail "the C++ probe does not compile" "$work/cc_cpp.err"
"$work/cc/mlx-cc" "$work/cc/crash_cpp.o" -o "$work/cc/crash_cpp" --dynamic-linker="$loader" 2>> "$work/cc_cpp.err" || fail "the C++ probe does not link" "$work/cc_cpp.err"
"$work/cc/mlx-cc" "$work/cc/crash_cpp.o" -o "$work/cc/crash_cpp_stripped" -s --dynamic-linker="$loader" 2>> "$work/cc_cpp.err" || fail "the C++ probe does not link with -s" "$work/cc_cpp.err"
if command -v readelf > /dev/null; then
    readelf -SW "$work/cc/crash_cpp" > "$work/sections"
    readelf -sW "$work/cc/crash_cpp" > "$work/symtab"
    grep -q ' \.symtab ' "$work/sections" && grep -q ' \.strtab ' "$work/sections" && grep -q ' \.mlx_lines ' "$work/sections" || fail "mlx-cc's program has no .symtab, .strtab and .mlx_lines" "$work/sections"
    # sh_info: the first global, after every local.
    first_global=$(awk '$2 == ".symtab" { print $(NF - 1) }' "$work/sections")
    awk -v first="$first_global" '/^Symbol table .\.symtab/ { on = 1; next } on && $1 ~ /^[0-9]+:$/ { n = $1 + 0; if (($5 == "LOCAL") != (n < first)) bad = 1 } END { exit bad }' "$work/symtab" || fail "the symbol table's locals do not all come before its globals (sh_info $first_global)" "$work/symtab"
    for symbol in 'FUNC +WEAK .* _ZN8geometry9Holder<i>4pokeEPN8geometry5ShapeEi$' 'FUNC +GLOBAL .* _ZN8geometry7measureEPN8geometry5ShapeE$' 'FUNC +GLOBAL .* _start$' 'FUNC +LOCAL .* __mlxcc_init$' 'FUNC +LOCAL .* printf@plt$' 'OBJECT +LOCAL .* _ZN8geometry5depthE$' 'SECTION +LOCAL +DEFAULT +6 '; do
        grep -qE "$symbol" "$work/symtab" || fail "no symbol like '$symbol' in mlx-cc's program" "$work/symtab"
    done
    awk '/^Symbol table .\.symtab/ { on = 1; next } on && $4 == "FUNC" && $3 == 0 { bad = 1 } END { exit bad }' "$work/symtab" || fail "a function of mlx-cc's program has no size" "$work/symtab"
    readelf -SW "$work/cc/crash_cpp_stripped" | grep -q ' \.symtab ' && fail "-s left the symbol table in"
    echo "ok   mlx-cc's linker writes the symbol table (locals first, types, bindings, sizes) and the line table; -s leaves them out"
fi
run_cc() { # PROGRAM OUTPUT
    set +e
    XDG_STATE_HOME="$work/state" "$work/cc/$1" > "$work/$1.out" 2>&1
    status=$?
    set -e
    [[ $status -eq 139 ]] || fail "$1 exited $status, not 139" "$work/$1.out"
}
run_cc crash_c
grep -q '^crash_c: crashed: segmentation fault touching 0x0$' "$work/crash_c.out" || fail "no crash line for the C probe" "$work/crash_c.out"
grep -q '^  at inner+0x[0-9a-f]* defined at tests/support/crash_report_cc.c:8$' "$work/crash_c.out" || fail "the C crash is not placed in inner (line 8)" "$work/crash_c.out"
grep -q '^  from middle+0x[0-9a-f]* defined at tests/support/crash_report_cc.c:10$' "$work/crash_c.out" || fail "middle (line 10) is not among the C probe's callers" "$work/crash_c.out"
grep -q '^  from main+0x[0-9a-f]* defined at tests/support/crash_report_cc.c:12$' "$work/crash_c.out" || fail "main (line 12) is not among the C probe's callers" "$work/crash_c.out"
grep -q '^  from _start+0x' "$work/crash_c.out" || fail "_start is not named" "$work/crash_c.out"
echo "ok   a crash of mlx-cc's C program names its functions and where they were defined"
run_cc crash_cpp
grep -q '^  at _ZN8geometry9Holder<i>4pokeEPN8geometry5ShapeEi+0x[0-9a-f]* (geometry::Holder<int>::poke(geometry::Shape\*, int)) defined at tests/support/crash_report_cc.cpp:12$' "$work/crash_cpp.out" || fail "the C++ crash is not placed in Holder<int>::poke (line 12), mangled and demangled" "$work/crash_cpp.out"
grep -q '^  from _ZN8geometry7measureEPN8geometry5ShapeE+0x[0-9a-f]* (geometry::measure(geometry::Shape\*)) defined at tests/support/crash_report_cc.cpp:14$' "$work/crash_cpp.out" || fail "geometry::measure (line 14) is not among the callers" "$work/crash_cpp.out"
grep -q '^  from _Z5outeri+0x[0-9a-f]* (outer(int)) defined at tests/support/crash_report_cc.cpp:21$' "$work/crash_cpp.out" || fail "outer(int) (line 21) is not among the callers" "$work/crash_cpp.out"
grep -q "	_ZN8geometry7measureEPN8geometry5ShapeE+0x[0-9a-f]* (geometry::measure(geometry::Shape\*)) defined at tests/support/crash_report_cc.cpp:14	" "$log" || fail "crashes.log does not keep the C++ frames with their names" "$log"
echo "ok   a crash of mlx-cc's C++ program names its functions mangled and as written, and where they were defined"
run_cc crash_cpp_stripped
grep -q '^  at crash_cpp_stripped+0x[0-9a-f]*$' "$work/crash_cpp_stripped.out" || fail "the stripped program's frames are not named by the file and offset" "$work/crash_cpp_stripped.out"
echo "ok   a stripped program's frames are the file and the offset"

# mlx-codemap crashes (tools/codemap, installed also as mlx-codemap) lists
# the kept crashes on the functions of this tree.
"$compiler" --quiet tools/codemap/main.mlx -o "$work/codemap" || fail "codemap does not build"
XDG_STATE_HOME="$work/state" "$work/codemap" -C "$repo_root" crashes > "$work/crashes.txt" 2>&1 || fail "codemap crashes failed" "$work/crashes.txt"
grep -q '^tests/support/crash_report.mlx:16:4: function middle (crashed here 2x)$' "$work/crashes.txt" || fail "codemap crashes does not list middle with its two crashes" "$work/crashes.txt"
grep -q '^== crashy: illegal instruction ([^)]*), while compiling src/widget.c:12$' "$work/crashes.txt" || fail "codemap crashes does not show the context with the reason" "$work/crashes.txt"
if [[ $cc_crashed -eq 1 ]]; then
    grep -q '^compiler/cc/[a-z0-9_]*\.mlx:[0-9]*:[0-9]*: function [A-Za-z0-9_]* (crashed here 1x)$' "$work/crashes.txt" || fail "codemap crashes does not list the function mlx-cc crashed in" "$work/crashes.txt"
    grep -q '^== mlx-cc: .*, while compiling async.c:2$' "$work/crashes.txt" || fail "codemap crashes does not show the crash of mlx-cc with its context" "$work/crashes.txt"
fi
grep -q '^   from _ZN8geometry7measureEPN8geometry5ShapeE+0x[0-9a-f]* (geometry::measure(geometry::Shape\*)) defined at tests/support/crash_report_cc.cpp:14$' "$work/crashes.txt" || fail "codemap crashes does not show the C++ frames with their names" "$work/crashes.txt"
# A frame an older handler kept mangled is shown with its C++ name too.
printf 'crash\t1\told\tsegmentation fault touching 0x0\tsrc/old.cpp:3:_Z5outeri+0x1f\n' >> "$log"
XDG_STATE_HOME="$work/state" "$work/codemap" -C "$repo_root" crashes > "$work/crashes.txt" 2>&1 || fail "codemap crashes failed" "$work/crashes.txt"
grep -q '^   at src/old.cpp:3:_Z5outeri+0x1f (outer(int))$' "$work/crashes.txt" || fail "codemap crashes does not demangle a mangled frame kept without its name" "$work/crashes.txt"
echo "ok   mlx-codemap crashes lists the kept crashes on their functions"

# The rule: every Wayland client in projects/, examples/ and tools/ watches
# its connection (std.wayland.crash.watch, right after it connects), so a protocol error
# or a request on an object that is gone is reported and kept like a
# crash instead of the program just ending. (tests/ exercise the library's
# own failure handling.)
missing=$(grep -rlE 'Display\.(connect|connectTo|connectToHandle)\(' --include=*.mlx projects examples tools | while read -r file; do grep -q 'wayland_crash\.watch(' "$file" || echo "$file"; done)
[[ -z "$missing" ]] || fail "these Wayland clients do not watch their connection (std.wayland.crash.watch after connecting): $missing"
echo "ok   every Wayland client watches its connection (std.wayland.crash.watch)"
