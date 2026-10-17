#!/usr/bin/env bash
# mlx-unicode, Unicode with ICU's C API in Mlx (projects/unicode):
#
#   - `./mlx build unicode` builds unicode-generate, the tables and
#     libmlxunicode.so;
#   - the tables in the tree are what the generator makes of the UCD
#     (projects/unicode/ucd) now;
#   - C programs built by mlx-cc against the library:
#       tests/normalization.c  every line of NormalizationTest.txt
#       tests/breaks.c         every line of the grapheme, word, sentence
#                              and line break tests of the UCD
#       tests/properties.c, tests/strings.c  print what NAME.expected says
#
#   tools/check_unicode.sh
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d /tmp/mlx-unicode.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }

./mlx build cc unicode > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "./mlx build unicode"; }
echo "ok   ./mlx build unicode"

mkdir -p "$work/tables/include/unicode"
mlx-out/bin/unicode-generate projects/unicode/ucd "$work/tables" || fail "unicode-generate"
cmp -s "$work/tables/tables.mlx" projects/unicode/tables.mlx || fail "projects/unicode/tables.mlx is not what the generator makes"
cmp -s "$work/tables/include/unicode/ugenerated.h" projects/unicode/include/unicode/ugenerated.h || fail "ugenerated.h is not what the generator makes"
echo "ok   the tables are what the generator makes of the UCD"

cc() { mlx-out/bin/mlx-cc -Iprojects/unicode/include "$1" mlx-out/lib/libmlxunicode.so -o "$2"; }
run() { LD_LIBRARY_PATH=mlx-out/lib "$@"; }

cc projects/unicode/tests/normalization.c "$work/normalization" || fail "tests/normalization.c does not build"
run "$work/normalization" > "$work/normalization.out" || { cat "$work/normalization.out"; fail "NormalizationTest.txt"; }
echo "ok   $(cat "$work/normalization.out") of NormalizationTest.txt"

cc projects/unicode/tests/breaks.c "$work/breaks" || fail "tests/breaks.c does not build"
run "$work/breaks" > "$work/breaks.out" || { cat "$work/breaks.out"; fail "the break tests"; }
while read -r line; do echo "ok   $line"; done < "$work/breaks.out"

for test in properties strings; do
    cc "projects/unicode/tests/$test.c" "$work/$test" || fail "tests/$test.c does not build"
    run "$work/$test" > "$work/$test.out" || fail "tests/$test.c fails"
    diff -u "projects/unicode/tests/$test.expected" "$work/$test.out" || fail "tests/$test.c prints otherwise"
    echo "ok   tests/$test.c"
done

echo "check_unicode.sh: all checks passed"
