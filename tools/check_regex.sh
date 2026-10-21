#!/usr/bin/env bash
# mlxlibc's POSIX regular expressions (compiler/libc/regex.mlx) against
# glibc's, and mlx-cc's <regex.h> (glibc's layout of regex_t and
# regmatch_t):
#
#   - compiler/libc/tests/regex.c (extended and basic expressions,
#     brackets, case, back-references, GNU's operators, the execution
#     flags, the errors and their messages), linked by gcc against
#     libmlxc.so (every regex call binds there) and against glibc alone:
#     both print the same;
#   - the same program compiled by mlx-cc on its <regex.h> prints the
#     same again with libmlxc.so.1 as its loader (no glibc in the
#     process).
#
#   tools/check_regex.sh [--no-build]   (--no-build: mlx-out/libc as it is)
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
command -v gcc > /dev/null || { echo "check_regex.sh: gcc is not installed" >&2; exit 2; }
work=$(mktemp -d /tmp/mlx-regex.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }

if [[ "${1:-}" != --no-build ]]; then
    tools/build_mlxlibc.sh > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "tools/build_mlxlibc.sh"; }
    ./mlx build cc > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "./mlx build cc"; }
    echo "ok   mlxlibc and mlx-cc build"
fi
lib=mlx-out/libc

test=compiler/libc/tests/regex.c
gcc -O1 -Wall -Wextra -c "$test" -o "$work/regex.o" 2> "$work/gcc.log" || fail "gcc: $(head -5 "$work/gcc.log")"
[[ ! -s "$work/gcc.log" ]] || fail "gcc warnings: $(head -5 "$work/gcc.log")"
gcc "$work/regex.o" -o "$work/regex_mlx" -L"$lib" -lmlxc -Wl,-rpath,"$repo_root/$lib"
gcc "$work/regex.o" -o "$work/regex_glibc"
defined=$(nm -D --defined-only "$lib/libmlxc.so.1")
for name in regcomp regexec regerror regfree; do
    grep -q " T $name\$" <<< "$defined" || fail "$name is not exported"
done
LC_ALL=C "$work/regex_glibc" > "$work/glibc.out" || fail "the program on glibc exits $?"
LC_ALL=C "$work/regex_mlx" > "$work/mlx.out" || fail "the program on mlxlibc exits $?"
diff -u "$work/glibc.out" "$work/mlx.out" > "$work/diff" || { head -40 "$work/diff"; fail "mlxlibc's regex answers otherwise than glibc's"; }
echo "ok   tests/regex.c on mlxlibc prints what it prints on glibc ($(wc -l < "$work/mlx.out") lines)"

mlx-out/bin/mlx-cc "$test" -o "$work/regex_cc" 2> "$work/cc.log" || fail "mlx-cc does not compile tests/regex.c: $(head -5 "$work/cc.log")"
# (Not on glibc: mlx-cc's programs name no symbol versions, and glibc
# binds an unversioned regexec to its GLIBC_2.2.5 one, which ignores
# REG_STARTEND.)
LC_ALL=C "$lib/libmlxc.so.1" "$work/regex_cc" > "$work/loader.out" || fail "the program built by mlx-cc under libmlxc.so.1 exits $?"
diff -u "$work/glibc.out" "$work/loader.out" > "$work/diff" || { head -40 "$work/diff"; fail "the program built by mlx-cc prints otherwise under libmlxc.so.1"; }
echo "ok   the same under libmlxc.so.1 as its loader"

echo "check_regex.sh: all checks passed"
