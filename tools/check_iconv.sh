#!/usr/bin/env bash
# mlxlibc's iconv and gettext (compiler/libc/iconv.mlx, misc.mlx) against
# glibc's, and their headers (iconv.h, libintl.h, made by mlx-cheaders of
# compiler/libc/api into mlx-out/include/mlxlibc):
#
#   - `./mlx build libc cc` builds libmlxc.so and the headers;
#   - compiler/libc/tests/iconv.c (the Unicode forms with and without byte
#     order marks, invalid and incomplete input, a full output, //TRANSLIT
#     and //IGNORE, the names, input in pieces, every byte of the
#     single-byte charsets there and back, and gettext's functions),
#     linked by gcc against libmlxc.so (every iconv and gettext call binds
#     there) and against glibc alone: both print the same;
#   - the same program compiled by mlx-cc with the generated headers prints
#     the same again, on the system's C library and with libmlxc.so.1 as
#     its loader (no glibc in the process).
#
#   tools/check_iconv.sh
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
command -v gcc > /dev/null || { echo "check_iconv.sh: gcc is not installed" >&2; exit 2; }
work=$(mktemp -d /tmp/mlx-iconv.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }

./mlx build libc cc > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "./mlx build libc cc"; }
echo "ok   ./mlx build libc cc"

test=compiler/libc/tests/iconv.c
gcc -O1 -Wall -Wextra -c "$test" -o "$work/iconv.o" 2> "$work/gcc.log" || fail "gcc: $(head -5 "$work/gcc.log")"
[[ ! -s "$work/gcc.log" ]] || fail "gcc warnings: $(head -5 "$work/gcc.log")"
gcc "$work/iconv.o" -o "$work/iconv_mlx" -Lmlx-out/lib -lmlxc -Wl,-rpath,"$repo_root/mlx-out/lib"
gcc "$work/iconv.o" -o "$work/iconv_glibc"
# The program's iconv and gettext calls bind to libmlxc.
defined=$(nm -D --defined-only mlx-out/lib/libmlxc.so.1)
for name in iconv_open iconv iconv_close gettext textdomain bindtextdomain; do
    grep -q " T $name\$" <<< "$defined" || fail "$name is not exported"
done
LC_ALL=C "$work/iconv_glibc" > "$work/glibc.out" || fail "the program on glibc exits $?"
LC_ALL=C "$work/iconv_mlx" > "$work/mlx.out" || fail "the program on mlxlibc exits $?"
diff -u "$work/glibc.out" "$work/mlx.out" > "$work/diff" || { head -40 "$work/diff"; fail "mlxlibc's iconv and gettext answer otherwise than glibc's"; }
echo "ok   tests/iconv.c on mlxlibc prints what it prints on glibc ($(wc -l < "$work/mlx.out") lines)"

mlx-out/bin/mlx-cc -Imlx-out/include/mlxlibc "$test" -o "$work/iconv_cc" || fail "mlx-cc does not compile tests/iconv.c with the generated headers"
LC_ALL=C "$work/iconv_cc" > "$work/cc.out" || fail "the program built by mlx-cc exits $?"
diff -u "$work/glibc.out" "$work/cc.out" > "$work/diff" || { head -40 "$work/diff"; fail "the program built by mlx-cc prints otherwise"; }
echo "ok   tests/iconv.c built by mlx-cc on the generated iconv.h and libintl.h"
# And with mlxlibc as its loader: no glibc in the process at all.
LC_ALL=C mlx-out/lib/libmlxc.so.1 "$work/iconv_cc" > "$work/loader.out" || fail "the program built by mlx-cc under libmlxc.so.1 exits $?"
diff -u "$work/glibc.out" "$work/loader.out" > "$work/diff" || { head -40 "$work/diff"; fail "the program built by mlx-cc prints otherwise under libmlxc.so.1"; }
echo "ok   the same under libmlxc.so.1 as its loader"

echo "check_iconv.sh: all checks passed"
