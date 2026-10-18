#!/usr/bin/env bash
# mlx-unicode, Unicode with ICU's C API in Mlx (projects/unicode):
#
#   - `./mlx build unicode` builds unicode-generate, the tables, the
#     library (libmlxunicode.so) and the headers: mlx-cheaders makes
#     mlx-out/include/mlx-unicode/unicode/*.h of the API's Mlx modules
#     (projects/unicode/api);
#   - the tables in the tree (tables.mlx, api/ugenerated.mlx) are what
#     the generator makes of the UCD (projects/unicode/ucd) now, and
#     timezones.mlx what tz-generate makes of the IANA database (tz/),
#     locales.mlx what cldr-generate makes of CLDR (cldr/),
#     collationrules.mlx what collation-generate makes of CLDR's
#     collation data (cldr/collation/);
#   - tests/zones.mlx: every zone's offsets, 1850 to 2100, against the
#     files zic made of the same database, where the system has them;
#   - tests/api.mlx: the API modules compile as Mlx, and the macros, run
#     as the Mlx they are written in, read UTF-8 and UTF-16 right;
#   - tests/macros.c, compiled as C and as C++ by mlx-cc: every header
#     together, the macros and initializers as C code uses them;
#   - C programs built by mlx-cc against the library:
#       tests/normalization.c  every line of NormalizationTest.txt
#       tests/breaks.c         every line of the grapheme, word, sentence
#                              and line break tests of the UCD
#       tests/collation.c      every line of the Collation Algorithm's
#                              tests (non-ignorable and shifted) in order
#       tests/tailoring.c      collation by locale (CLDR's tailorings) and
#                              by rules, as ICU 74 sorts the same words
#       tests/idna.c           every line of IdnaTestV2.txt (UTS #46)
#       tests/properties.c, tests/strings.c, tests/calendar.c,
#       tests/charsets.c, tests/locales.c, tests/bundles.c (over
#       cldr.dat)                       print what NAME.expected says
#
#   tools/check_unicode.sh
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d /tmp/mlx-unicode.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }
headers=mlx-out/include/mlx-unicode

./mlx build cc unicode > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "./mlx build unicode"; }
echo "ok   ./mlx build unicode"

mkdir -p "$work/tables/api"
mlx-out/bin/unicode-generate projects/unicode/ucd "$work/tables" || fail "unicode-generate"
cmp -s "$work/tables/tables.mlx" projects/unicode/tables.mlx || fail "projects/unicode/tables.mlx is not what the generator makes"
cmp -s "$work/tables/api/ugenerated.mlx" projects/unicode/api/ugenerated.mlx || fail "api/ugenerated.mlx is not what the generator makes"
echo "ok   the tables are what the generator makes of the UCD"

mlx-out/bin/cldr-generate projects/unicode/cldr "$work/locales.mlx" || fail "cldr-generate"
cmp -s "$work/locales.mlx" projects/unicode/locales.mlx || fail "projects/unicode/locales.mlx is not what cldr-generate makes"
echo "ok   the locale data is what cldr-generate makes of CLDR"

mlx-out/bin/collation-generate "$work/collationrules.mlx" projects/unicode/cldr/collation/radicals.txt projects/unicode/cldr/collation/*.xml || fail "collation-generate"
cmp -s "$work/collationrules.mlx" projects/unicode/collationrules.mlx || fail "projects/unicode/collationrules.mlx is not what collation-generate makes"
echo "ok   the collation rules are what collation-generate makes of CLDR"

mlx-out/bin/tz-generate projects/unicode/tz "$work/timezones.mlx" || fail "tz-generate"
cmp -s "$work/timezones.mlx" projects/unicode/timezones.mlx || fail "projects/unicode/timezones.mlx is not what tz-generate makes"
echo "ok   the time zones are what tz-generate makes of the IANA database"

./mlx --quiet projects/unicode/tests/zones.mlx -o "$work/zones" || fail "tests/zones.mlx does not build"
if [ -d /usr/share/zoneinfo ]; then
    "$work/zones" > "$work/zones.out" || { cat "$work/zones.out"; fail "the zones against zic's files"; }
    echo "ok   $(cat "$work/zones.out")"
else
    echo "skip the zones against zic's files: no /usr/share/zoneinfo here"
fi

./mlx --quiet projects/unicode/tests/api.mlx -o "$work/api" || fail "the API modules do not compile as Mlx"
"$work/api" || fail "tests/api.mlx exits $?"
echo "ok   the API modules compile, their macros run as Mlx"

cc() { mlx-out/bin/mlx-cc -I"$headers" "$@" mlx-out/lib/libmlxunicode.so; }
run() { LD_LIBRARY_PATH=mlx-out/lib "$@"; }

cc projects/unicode/tests/macros.c -o "$work/macros" || fail "tests/macros.c does not build as C"
run "$work/macros" > /dev/null || fail "tests/macros.c (C) exits $?"
cc -x c++ -std=c++20 projects/unicode/tests/macros.c -o "$work/macros-cpp" || fail "tests/macros.c does not build as C++"
run "$work/macros-cpp" > /dev/null || fail "tests/macros.c (C++) exits $?"
echo "ok   the headers in C and C++, their macros as C code uses them"

cc projects/unicode/tests/normalization.c -o "$work/normalization" || fail "tests/normalization.c does not build"
run "$work/normalization" > "$work/normalization.out" || { cat "$work/normalization.out"; fail "NormalizationTest.txt"; }
echo "ok   $(cat "$work/normalization.out") of NormalizationTest.txt"

cc projects/unicode/tests/breaks.c -o "$work/breaks" || fail "tests/breaks.c does not build"
run "$work/breaks" > "$work/breaks.out" || { cat "$work/breaks.out"; fail "the break tests"; }
while read -r line; do echo "ok   $line"; done < "$work/breaks.out"

cc projects/unicode/tests/collation.c -o "$work/collation" || fail "tests/collation.c does not build"
run "$work/collation" > "$work/collation.out" || { cat "$work/collation.out"; fail "the collation tests"; }
while read -r line; do echo "ok   $line"; done < "$work/collation.out"

cc projects/unicode/tests/idna.c -o "$work/idna" || fail "tests/idna.c does not build"
run "$work/idna" > "$work/idna.out" || { cat "$work/idna.out"; fail "IdnaTestV2.txt"; }
echo "ok   $(cat "$work/idna.out")"

for test in properties strings calendar charsets locales bundles lists displaynames dates numbers tailoring; do
    cc "projects/unicode/tests/$test.c" -o "$work/$test" || fail "tests/$test.c does not build"
    run "$work/$test" > "$work/$test.out" || fail "tests/$test.c fails"
    diff -u "projects/unicode/tests/$test.expected" "$work/$test.out" || fail "tests/$test.c prints otherwise"
    echo "ok   tests/$test.c"
done

echo "check_unicode.sh: all checks passed"
