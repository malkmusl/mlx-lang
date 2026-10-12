#!/usr/bin/env bash
# std.tinypage_allocator, std.debug_allocator and std.leaks with the
# self-hosted compiler: tests/315_tinypage_runtime.mlx runs (small blocks
# of the page allocator on tiny pages, four threads at once);
# tests/314_debug_allocator_runtime.mlx runs (a double free, an invalid
# free, a wrong length, a leak, poison, a resize; then the page allocator
# watched, a block kept on purpose), and its reports name the functions;
# tests/support/leaks_program.mlx, watched from crash.install as every
# program is, says the blocks its loop kept and the arena's growth with
# MLX_LEAK_CHECK=1 (no growth on std.turns), and nothing without it; its
# memory is written to $XDG_RUNTIME_DIR/mlx/memory, which
# `mlx-codemap memory` reads.
#
#   tests/run_debug_allocator.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
# The memory snapshots go here, not to the session's runtime directory.
export XDG_RUNTIME_DIR=$work/run
mkdir -p "$XDG_RUNTIME_DIR"
failures=0
fail() {
    echo "FAIL $1"
    failures=$((failures + 1))
}

"$compiler" --quiet tests/315_tinypage_runtime.mlx -o "$work/tiny"
status=0
"$work/tiny" > "$work/tiny.out" 2>&1 || status=$?
if [[ $status -eq 13 ]]; then
    echo "ok   315_tinypage_runtime: small blocks on tiny pages, apart, zeroed, squeezed into holes, freed by address, aligned; four threads"
else
    fail "315_tinypage_runtime exited $status"
fi

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
expect "$work/program.err" "allocated at tests/support/leaks_program.mlx:21:turn" "named by the function that allocated them"
if grep -q "8192 bytes" "$work/program.err"; then fail "the block kept on purpose (leaks.keep) is said"; else echo "ok   a block kept on purpose (leaks.keep) is left out"; fi
expect "$work/program.err" "values (the compiler's arena) grew by" "a loop without std.turns: the arena's growth is said"
status=0
MLX_LEAK_CHECK=1 "$work/program" turns 2> "$work/turns.err" || status=$?
if [[ $status -eq 3 ]] && ! grep -q "arena) grew" "$work/turns.err"; then
    echo "ok   a loop on std.turns: the same blocks, no growth said"
else
    fail "a loop on std.turns: exit $status, said: $(head -c 300 "$work/turns.err")"
fi

# The memory as the runtime wrote it (at the mark and the report): the
# totals, the places with their functions, the blocks, the mappings.
snapshot=$(ls "$XDG_RUNTIME_DIR"/mlx/memory/program-*.memory 2>/dev/null | head -1)
if [[ -n "$snapshot" ]] && head -1 "$snapshot" | grep -q "^memory	program	[0-9]*	[0-9]*	11	6	5	24576	24576	28672	[0-9]*	[0-9]*$"; then
    echo "ok   the runtime writes the memory: allocations, frees, blocks, bytes, pages, peak"
    grep -q "^tiny	[0-9]*	[0-9]*	[0-9]*	[0-9]*$" "$snapshot" && echo "ok   and how full the tiny pages are" || fail "no tiny line in the snapshot"
else
    fail "no memory snapshot, or its totals are wrong: $(head -1 "$snapshot" 2>/dev/null)"
fi
if [[ -n "$snapshot" ]] && grep -q "^place	[0-9]*	3	12288	12288	3	0	tests/support/leaks_program.mlx:21:turn	tests/support/leaks_program.mlx:[0-9]*:main" "$snapshot" \
    && grep -q "^place	[0-9]*	1	8192	8192	0	1	tests/support/leaks_program.mlx:21:turn	" "$snapshot" \
    && [[ $(grep -c "^block	" "$snapshot") -eq 5 ]] && grep -q "^map	[0-9a-f]*	[0-9a-f]*	r" "$snapshot"; then
    echo "ok   with its places (since the mark, kept), named, its blocks and its mappings"
else
    fail "the snapshot's places, blocks or mappings: $(head -c 600 "$snapshot" 2>/dev/null)"
fi
"$compiler" --quiet tools/codemap/main.mlx -o "$work/mlx-codemap"
"$work/mlx-codemap" memory program > "$work/memory.txt" 2>&1 || true
expect "$work/memory.txt" "ended): 5 blocks, 24 KiB asked, 24 KiB in pages (0% wasted)" "mlx-codemap memory: the program's headline"
expect "$work/memory.txt" "12 KiB in 3 blocks tests/support/leaks_program.mlx:21:4: function turn (3 since the mark)" "mlx-codemap memory: the loop's place, since the mark"
expect "$work/memory.txt" "8192 B in 1 block tests/support/leaks_program.mlx:21:4: function turn (kept)" "mlx-codemap memory: the block kept on purpose"
expect "$work/memory.txt" "fragmentation 0." "mlx-codemap memory: the address space and its fragmentation"

status=0
rm -rf -- "${XDG_RUNTIME_DIR:?}/mlx"
"$work/program" 2> "$work/quiet.err" || status=$?
if [[ $status -eq 0 && ! -s "$work/quiet.err" && ! -e "$XDG_RUNTIME_DIR/mlx/memory" ]]; then echo "ok   without MLX_LEAK_CHECK nothing is watched, said or written"; else fail "without MLX_LEAK_CHECK: exit $status, said: $(head -c 200 "$work/quiet.err")"; fi
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
