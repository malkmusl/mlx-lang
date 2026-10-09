#!/usr/bin/env bash
# mlxlibc (projects/libc, built by tools/build_mlxlibc.sh) against glibc:
#
#   - the image: soname libmlxc.so.1, no library needed (no DT_NEEDED at
#     all), the C names exported, stdin/stdout/stderr, environ and the
#     getopt variables as data symbols;
#   - tools/mlxlibc_check.py: the same calls through ctypes into both
#     libraries answer the same (strings, ctype, malloc, stdlib, unistd,
#     signals, printf, stdio, time, pthread);
#   - tools/mlxlibc_check.c: a C program gcc links twice, against
#     libmlxc.so (every libc symbol it uses binds there; glibc is left the
#     program start alone) and against glibc alone; both runs print the
#     same, to stdout and to stderr.
#
#   tools/check_mlxlibc.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
for tool in gcc python3 readelf nm; do
    command -v "$tool" > /dev/null || { echo "check_mlxlibc.sh: $tool is not installed" >&2; exit 2; }
done

work=$(mktemp -d /tmp/mlx-libc.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }

tools/build_mlxlibc.sh --compiler "$compiler" --out "$work/lib" > /dev/null
lib="$work/lib/libmlxc.so.1"
[[ -L "$work/lib/libmlxc.so" ]] || fail "no libmlxc.so link"

# The image.
dynamic=$(readelf -d "$lib")
[[ "$dynamic" == *"Library soname: [libmlxc.so.1]"* ]] || fail "no soname libmlxc.so.1"
if grep -q 'Shared library:' <<< "$dynamic"; then fail "needs a library: $(grep -o 'Shared library: \[[^]]*\]' <<< "$dynamic" | tr '\n' ' ')"; fi
defined=$(nm -D --defined-only "$lib")
for name in malloc free printf fopen pthread_create strtod qsort __errno_location; do
    grep -q " T $name\$" <<< "$defined" || fail "$name is not exported"
done
for name in stdin stdout stderr environ optind optarg; do
    grep -q " D $name\$" <<< "$defined" || fail "$name is not a data symbol"
done
count=$(awk '{print $3}' <<< "$defined" | grep -c .)
echo "ok   libmlxc.so.1: $count symbols, soname, nothing needed"

# The same calls into both libraries.
result=$(python3 -I tools/mlxlibc_check.py "$lib" 2>&1) || fail "ctypes: $(tail -5 <<< "$result")"
echo "ok   ctypes: $(tail -1 <<< "$result") as glibc's"

# The C program, on libmlxc and on glibc.
gcc -O2 -Wall -Wextra -c tools/mlxlibc_check.c -o "$work/check.o" 2> "$work/gcc.log" || fail "gcc: $(head -5 "$work/gcc.log")"
[[ ! -s "$work/gcc.log" ]] || fail "gcc warnings: $(head -5 "$work/gcc.log")"
gcc "$work/check.o" -o "$work/check_mlx" -L"$work/lib" -lmlxc -Wl,-rpath,"$work/lib" -pthread
gcc "$work/check.o" -o "$work/check_glibc" -pthread
# Every libc name the program uses binds to libmlxc; glibc keeps the start.
unresolved=$(nm -D -u "$work/check_mlx" | awk '{print $NF}' | sed 's/@.*//' | sort -u)
ours=$(awk '{print $3}' <<< "$defined" | sort -u)
from_glibc=$(comm -23 <(echo "$unresolved") <(echo "$ours") | grep -v '^__libc_start_main$\|^__cxa_finalize$\|^__gmon_start__$\|^_ITM_' || true)
[[ -z "$from_glibc" ]] || fail "the C program takes from glibc: $(tr '\n' ' ' <<< "$from_glibc")"
for which in mlx glibc; do
    mkdir -p "$work/$which"
    status=0
    (
        export TZ=UTC LC_ALL=C MLXLIBC_CHECK=present
        unset MLXLIBC_MISSING
        "$work/check_$which" "$work/$which" "$which" > "$work/$which.out" 2> "$work/$which.err"
    ) || status=$?
    [[ $status -eq 0 ]] || fail "the C program on $which exited $status: $(tail -3 "$work/$which.err")"
done
cmp -s "$work/mlx.out" "$work/glibc.out" || fail "stdout differs (glibc < > mlx): $(diff "$work/glibc.out" "$work/mlx.out" | head -12)"
cmp -s "$work/mlx.err" "$work/glibc.err" || fail "stderr differs (glibc < > mlx): $(diff "$work/glibc.err" "$work/mlx.err" | head -12)"
grep -q '^atexit ran$' "$work/mlx.out" || fail "the atexit handler did not run on libmlxc"
echo "ok   C program: $(wc -l < "$work/mlx.out") lines on libmlxc as on glibc"
echo "PASS mlxlibc"
