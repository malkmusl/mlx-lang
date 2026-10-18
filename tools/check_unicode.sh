#!/usr/bin/env bash
# std.unicode (std/src/unicode) and mlx-unicode, its C layer with ICU's C
# API (compiler/capi/icu):
#
#   - `./mlx build unicode` builds unicode-generate, the tables, the
#     library (libmlxunicode.so) and the headers: mlx-cheaders makes
#     mlx-out/include/mlx-unicode/unicode/*.h of the API's Mlx modules
#     (compiler/capi/icu/api);
#   - the tables in the tree are what the generators make of the data in
#     std/registry/unicode and std/registry/tz now: std/src/unicode/tables.mlx
#     and compiler/capi/icu/api/ugenerated.mlx of the UCD (ucd/),
#     std/src/unicode/locale/data.mlx of CLDR (cldr/),
#     std/src/unicode/collation/rules.mlx of CLDR's collation data
#     (cldr/collation/), std/src/timezone/tzdata.mlx of the IANA database;
#   - std/registry/tz/compare.mlx: every zone's offsets, 1850 to 2100,
#     against the files zic made of the same database, where the system
#     has them;
#   - std.unicode's own tests, Mlx programs that exit 13
#     (tests/*_unicode_*_runtime.mlx);
#   - compiler/capi/icu/tests/api.mlx: the API modules compile as Mlx,
#     and the macros, run as the Mlx they are written in, read UTF-8 and
#     UTF-16 right;
#   - tests/macros.c, compiled as C and as C++ by mlx-cc: every header
#     together, the macros and initializers as C code uses them;
#   - C programs (compiler/capi/icu/tests) built by mlx-cc against the
#     library:
#       tests/normalization.c  every line of NormalizationTest.txt
#       tests/breaks.c         every line of the grapheme, word, sentence
#                              and line break tests of the UCD
#       tests/collation.c      every line of the Collation Algorithm's
#                              tests (non-ignorable and shifted) in order
#       tests/tailoring.c      collation by locale (CLDR's tailorings) and
#                              by rules, as ICU 74 sorts the same words
#       tests/idna.c           every line of IdnaTestV2.txt (UTS #46)
#       tests/calendars.c      the calendars other than the Gregorian:
#                              their fields over three thousand years,
#                              add, roll, limits, as ICU 74 has them
#       tests/calendarformats.c  dates of those calendars formatted as
#                              JavaScriptCore formats them
#       tests/zonenames.c      zones' names by ucal_getTimeZoneDisplayName
#       tests/properties.c, tests/strings.c, tests/calendar.c,
#       tests/charsets.c, tests/locales.c, tests/bundles.c (over
#       cldr.dat), ... tests/numbers.c, tests/units.c (measure units)
#                                       print what NAME.expected says
#
#   tools/check_unicode.sh
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d /tmp/mlx-unicode.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }
headers=mlx-out/include/mlx-unicode
tests=compiler/capi/icu/tests

./mlx build cc unicode > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "./mlx build unicode"; }
echo "ok   ./mlx build unicode"

mlx-out/bin/unicode-generate std/registry/unicode/ucd "$work/tables.mlx" "$work/ugenerated.mlx" || fail "unicode-generate"
cmp -s "$work/tables.mlx" std/src/unicode/tables.mlx || fail "std/src/unicode/tables.mlx is not what the generator makes"
cmp -s "$work/ugenerated.mlx" compiler/capi/icu/api/ugenerated.mlx || fail "compiler/capi/icu/api/ugenerated.mlx is not what the generator makes"
echo "ok   the tables are what the generator makes of the UCD"

mlx-out/bin/cldr-generate std/registry/unicode/cldr "$work/locales.mlx" || fail "cldr-generate"
cmp -s "$work/locales.mlx" std/src/unicode/locale/data.mlx || fail "std/src/unicode/locale/data.mlx is not what cldr-generate makes"
echo "ok   the locale data is what cldr-generate makes of CLDR"

mlx-out/bin/collation-generate "$work/collationrules.mlx" std/registry/unicode/cldr/collation/radicals.txt std/registry/unicode/cldr/collation/*.xml || fail "collation-generate"
cmp -s "$work/collationrules.mlx" std/src/unicode/collation/rules.mlx || fail "std/src/unicode/collation/rules.mlx is not what collation-generate makes"
echo "ok   the collation rules are what collation-generate makes of CLDR"

mlx-out/bin/tz-generate std/registry/tz "$work/timezones.mlx" || fail "tz-generate"
cmp -s "$work/timezones.mlx" std/src/timezone/tzdata.mlx || fail "std/src/timezone/tzdata.mlx is not what tz-generate makes"
echo "ok   the time zones are what tz-generate makes of the IANA database"

./mlx --quiet std/registry/tz/compare.mlx -o "$work/zones" || fail "std/registry/tz/compare.mlx does not build"
if [ -d /usr/share/zoneinfo ]; then
    "$work/zones" > "$work/zones.out" || { cat "$work/zones.out"; fail "the zones against zic's files"; }
    echo "ok   $(cat "$work/zones.out")"
else
    echo "skip the zones against zic's files: no /usr/share/zoneinfo here"
fi

./mlx --quiet compiler/capi/icu/tests/api.mlx -o "$work/api" || fail "the API modules do not compile as Mlx"
"$work/api" || fail "tests/api.mlx exits $?"
echo "ok   the API modules compile, their macros run as Mlx"

for test in tests/*_unicode_*_runtime.mlx; do
    name=$(basename "$test" .mlx)
    ./mlx --quiet "$test" -o "$work/$name" || fail "$test does not build"
    status=0
    MLX_UNICODE_DATA="$repo_root/mlx-out/share/mlx-unicode/cldr.dat" "$work/$name" || status=$?
    [ "$status" -eq 13 ] || fail "$test exits $status"
    echo "ok   $test"
done

cc() { mlx-out/bin/mlx-cc -I"$headers" "$@" mlx-out/lib/libmlxunicode.so; }
run() { LD_LIBRARY_PATH=mlx-out/lib "$@"; }

cc $tests/macros.c -o "$work/macros" || fail "tests/macros.c does not build as C"
run "$work/macros" > /dev/null || fail "tests/macros.c (C) exits $?"
cc -x c++ -std=c++20 $tests/macros.c -o "$work/macros-cpp" || fail "tests/macros.c does not build as C++"
run "$work/macros-cpp" > /dev/null || fail "tests/macros.c (C++) exits $?"
echo "ok   the headers in C and C++, their macros as C code uses them"

cc $tests/normalization.c -o "$work/normalization" || fail "tests/normalization.c does not build"
run "$work/normalization" > "$work/normalization.out" || { cat "$work/normalization.out"; fail "NormalizationTest.txt"; }
echo "ok   $(cat "$work/normalization.out") of NormalizationTest.txt"

cc $tests/breaks.c -o "$work/breaks" || fail "tests/breaks.c does not build"
run "$work/breaks" > "$work/breaks.out" || { cat "$work/breaks.out"; fail "the break tests"; }
while read -r line; do echo "ok   $line"; done < "$work/breaks.out"

cc $tests/collation.c -o "$work/collation" || fail "tests/collation.c does not build"
run "$work/collation" > "$work/collation.out" || { cat "$work/collation.out"; fail "the collation tests"; }
while read -r line; do echo "ok   $line"; done < "$work/collation.out"

cc $tests/idna.c -o "$work/idna" || fail "tests/idna.c does not build"
run "$work/idna" > "$work/idna.out" || { cat "$work/idna.out"; fail "IdnaTestV2.txt"; }
echo "ok   $(cat "$work/idna.out")"

for test in properties strings calendar calendars charsets locales bundles lists displaynames dates calendarformats zonenames numbers units tailoring; do
    cc "$tests/$test.c" -o "$work/$test" || fail "tests/$test.c does not build"
    run "$work/$test" > "$work/$test.out" || fail "tests/$test.c fails"
    diff -u "$tests/$test.expected" "$work/$test.out" || fail "tests/$test.c prints otherwise"
    echo "ok   tests/$test.c"
done

echo "check_unicode.sh: all checks passed"
