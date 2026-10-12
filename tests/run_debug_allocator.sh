#!/usr/bin/env bash
# std.debug_allocator and std.leaks with the self-hosted compiler:
# tests/314_debug_allocator_runtime.mlx runs (a double free, an invalid
# free, a wrong length, a leak, poison, a resize; then the page allocator
# watched, a block kept on purpose), and its reports name the functions;
# tests/support/leaks_program.mlx says the blocks its loop kept and the
# arena's growth with MLX_LEAK_CHECK=1 (no growth on std.turns), and
# nothing without it.
#
#   tests/run_debug_allocator.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
failures=0
fail() {
    echo "FAIL $1"
    failures=$((failures + 1))
}

"$compiler" --quiet tests/314_debug_allocator_runtime.mlx -o "$work/debug"
status=0
"$work/debug" 2> "$work/debug.err" || status=$?
if [[ $status -eq 13 ]]; then
    echo "ok   314_debug_allocator_runtime: double, invalid and short frees, a leak, poison, a resize, the page allocator watched"
else
    fail "314_debug_allocator_runtime exited $status"
fi
expect() {
    local file=$1 text=$2 what=$3
    if grep -qF -- "$text" "$file"; then echo "ok   $what"; else fail "$what: no '$text' in"; sed 's/^/     /' "$file" | head -20; fi
}
expect "$work/debug.err" "double free: 64 bytes at" "a double free is said"
expect "$work/debug.err" "first freed at tests/314_debug_allocator_runtime.mlx:26:freeTwice" "with where it was freed first, named"
expect "$work/debug.err" "freed with another length or alignment than allocated: 200 bytes" "a wrong length is said"
expect "$work/debug.err" "invalid free: 16 bytes" "an invalid free is said"
expect "$work/debug.err" "allocated at tests/314_debug_allocator_runtime.mlx:21:leakOne" "a leak names where it was allocated"
expect "$work/debug.err" "1 block leaked (48 bytes) from 1 place; 5 allocations, 4 frees, 256 bytes out at most; 3 bad frees" "the totals"
expect "$work/debug.err" "allocated at tests/314_debug_allocator_runtime.mlx:37:leakPage" "a page block kept is said, named"
if grep -q "allocator.mlx:" "$work/debug.err"; then fail "the allocators' own frames are shown"; else echo "ok   the allocators' own frames are left out"; fi

"$compiler" --quiet tests/support/leaks_program.mlx -o "$work/program"
status=0
MLX_LEAK_CHECK=1 "$work/program" 2> "$work/program.err" || status=$?
if [[ $status -eq 3 ]]; then echo "ok   MLX_LEAK_CHECK=1: the loop's three kept blocks counted, the set-up's not"; else fail "MLX_LEAK_CHECK=1: the report counted $status blocks, not 3"; cat "$work/program.err"; fi
expect "$work/program.err" "leak: 3 blocks, 12288 bytes" "they are said as one place"
expect "$work/program.err" "allocated at tests/support/leaks_program.mlx:18:turn" "named by the function that allocated them"
if grep -q "8192 bytes" "$work/program.err"; then fail "the block kept on purpose (leaks.keep) is said"; else echo "ok   a block kept on purpose (leaks.keep) is left out"; fi
expect "$work/program.err" "values (the compiler's arena) grew by" "a loop without std.turns: the arena's growth is said"
status=0
MLX_LEAK_CHECK=1 "$work/program" turns 2> "$work/turns.err" || status=$?
if [[ $status -eq 3 ]] && ! grep -q "arena) grew" "$work/turns.err"; then
    echo "ok   a loop on std.turns: the same blocks, no growth said"
else
    fail "a loop on std.turns: exit $status, said: $(head -c 300 "$work/turns.err")"
fi
status=0
"$work/program" 2> "$work/quiet.err" || status=$?
if [[ $status -eq 0 && ! -s "$work/quiet.err" ]]; then echo "ok   without MLX_LEAK_CHECK nothing is watched or said"; else fail "without MLX_LEAK_CHECK: exit $status, said: $(head -c 200 "$work/quiet.err")"; fi
status=0
MLX_LEAK_CHECK="$work/leaks.log" "$work/program" 2> "$work/file.err" || status=$?
if [[ $status -eq 3 && ! -s "$work/file.err" ]] && grep -q "^program\[[0-9]*\]: leak: 3 blocks" "$work/leaks.log"; then
    echo "ok   MLX_LEAK_CHECK=/a/path: the report goes to that file, named by program and process"
else
    fail "MLX_LEAK_CHECK=/a/path: exit $status, file: $(head -c 300 "$work/leaks.log" 2>/dev/null)"
fi
status=0
MLX_LEAK_CHECK=0 "$work/program" 2> "$work/zero.err" || status=$?
if [[ $status -eq 0 && ! -s "$work/zero.err" ]]; then echo "ok   MLX_LEAK_CHECK=0 is off"; else fail "MLX_LEAK_CHECK=0: exit $status"; fi

if [[ $failures -ne 0 ]]; then
    echo "$failures debug allocator checks failed"
    exit 1
fi
echo "all debug allocator checks passed"
