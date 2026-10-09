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
#     same, to stdout and to stderr;
#   - the library as dynamic loader: run alone it says what it is; the
#     glibc-linked program run under it (`libmlxc.so.1 PROGRAM`, no glibc
#     in the process) prints the same again, now with dlopen of an Mlx
#     plugin and the program's thread-local variables; the program linked
#     with libmlxc as its PT_INTERP runs on its own; and the system's own
#     /bin/echo, env, cat and sh (glibc-linked binaries) run under it.
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

# The library as the dynamic loader. Run alone it introduces itself.
chmod +x "$lib"
banner=$("$lib" 2>&1) || fail "libmlxc.so.1 run alone exited $?"
[[ "$banner" == mlxlibc* ]] || fail "libmlxc.so.1 run alone said: $banner"
# The glibc-linked program under it, with an Mlx plugin for its dlopen and
# nothing of glibc in the process (env -i: no LD_LIBRARY_PATH either); the
# reference run is glibc's own with the same plugin.
plugin="$work/lib/libmlxplugin.so"
"$compiler" --quiet --plugin --soname=libmlxplugin.so tests/support/plugin_library.mlx -o "$plugin" || fail "building the plugin"
for which in reference loader; do
    mkdir -p "$work/$which"
    status=0
    if [[ $which == reference ]]; then
        env -i TZ=UTC LC_ALL=C MLXLIBC_CHECK=present "$work/check_glibc" "$work/$which" glibc "$plugin" > "$work/$which.out" 2> "$work/$which.err" || status=$?
    else
        env -i TZ=UTC LC_ALL=C MLXLIBC_CHECK=present "$lib" "$work/check_glibc" "$work/$which" mlx "$plugin" > "$work/$which.out" 2> "$work/$which.err" || status=$?
    fi
    [[ $status -eq 0 ]] || fail "the C program ($which) exited $status: $(tail -3 "$work/$which.err")"
done
cmp -s "$work/reference.out" "$work/loader.out" || fail "under the loader, stdout differs (glibc < > loader): $(diff "$work/reference.out" "$work/loader.out" | head -12)"
cmp -s "$work/reference.err" "$work/loader.err" || fail "under the loader, stderr differs: $(diff "$work/reference.err" "$work/loader.err" | head -12)"
grep -q '^== dlfcn' "$work/loader.out" || fail "the dlfcn section did not run under the loader"
echo "ok   loader: the glibc-linked program runs under libmlxc.so.1 with no glibc in the process ($(wc -l < "$work/loader.out") lines as glibc's, dlopen of an Mlx plugin, thread-local variables)"
# The program with libmlxc as its interpreter runs on its own.
gcc "$work/check.o" -o "$work/check_interp" -Wl,--dynamic-linker="$lib" -pthread
readelf -l "$work/check_interp" | grep -q "interpreter: $lib" || fail "the relinked program does not name libmlxc.so.1 as interpreter"
mkdir -p "$work/interp"
status=0
env -i TZ=UTC LC_ALL=C MLXLIBC_CHECK=present "$work/check_interp" "$work/interp" mlx "$plugin" > "$work/interp.out" 2> "$work/interp.err" || status=$?
[[ $status -eq 0 ]] || fail "the program with libmlxc as PT_INTERP exited $status: $(tail -3 "$work/interp.err")"
cmp -s "$work/reference.out" "$work/interp.out" || fail "as PT_INTERP, stdout differs: $(diff "$work/reference.out" "$work/interp.out" | head -12)"
echo "ok   PT_INTERP: the program linked with libmlxc.so.1 as its interpreter runs on its own"
# The system's own programs (glibc-linked) under the loader.
ran=()
if [[ -x /bin/echo ]]; then
    out=$(env -i "$lib" /bin/echo hello 2>&1) && [[ "$out" == "hello" ]] || fail "/bin/echo under the loader: $out"
    ran+=(echo)
fi
if [[ -x /usr/bin/env ]]; then
    out=$(env -i MLX_CHECK=yes "$lib" /usr/bin/env 2>&1) && [[ "$out" == "MLX_CHECK=yes" ]] || fail "/usr/bin/env under the loader: $out"
    ran+=(env)
fi
if [[ -x /bin/cat ]]; then
    printf 'through cat\n' > "$work/cat.txt"
    out=$(env -i "$lib" /bin/cat "$work/cat.txt" 2>&1) && [[ "$out" == "through cat" ]] || fail "/bin/cat under the loader: $out"
    ran+=(cat)
fi
if [[ -x /bin/sh ]]; then
    out=$(env -i "$lib" /bin/sh -c 'x=abc; echo $((2+3)) ${#x}' 2>&1) && [[ "$out" == "5 3" ]] || fail "/bin/sh under the loader: $out"
    ran+=(sh)
fi
echo "ok   system programs under the loader: ${ran[*]:-none here}"
echo "PASS mlxlibc"
