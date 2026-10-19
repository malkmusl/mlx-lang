#!/usr/bin/env bash
# mlx-fontconfig, Fontconfig's C API in Mlx (compiler/capi/fontconfig, over
# std.fontconfig):
#
#   - `./mlx build fontconfig` builds the library (libmlxfontconfig.so) and
#     its headers: mlx-cheaders makes mlx-out/include/mlx-fontconfig/
#     fontconfig/fontconfig.h and fcfreetype.h of the API's Mlx modules
#     (compiler/capi/fontconfig/api);
#   - every function the headers declare is in the library;
#   - the headers compile as C and as C++;
#   - C programs built by mlx-cc against the library print what
#     NAME.expected says (what Fontconfig 2.15 prints for the same
#     program, with the exit status last):
#       tests/names.c     patterns, values, names, constants, weights,
#                         matrices, ranges, FcPatternFormat, iterators,
#                         printing; character and language sets, strings
#       tests/config.c    a configuration read from memory over the fonts
#                         of the tree: rules of the three targets, aliases,
#                         a rejected file; the fonts, matches, sorts,
#                         render preparation, lists, application fonts,
#                         FcFreeTypeQuery
#       tests/freetype.c  fcfreetype.h over mlx-freetype's faces
#   - with gcc and the system's libfontconfig installed, the same programs
#     built against it print the same (the expected files are still
#     Fontconfig's answers), and tests/system.c (the system's own
#     fonts.conf and fonts: configuration files, font directories, the
#     fonts, the matches and sorts of 42 queries, the lists) prints with
#     mlx-fontconfig what it prints with Fontconfig, Type 1 fonts (which
#     std.fontconfig does not read) passed over.
#
#   tools/check_fontconfig.sh
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d /tmp/mlx-fontconfig.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }
headers=mlx-out/include/mlx-fontconfig
ftheaders=mlx-out/include/mlx-freetype
library=mlx-out/lib/libmlxfontconfig.so
tests=compiler/capi/fontconfig/tests

./mlx build -j 1 cc freetype fontconfig > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "./mlx build fontconfig"; }
echo "ok   ./mlx build fontconfig"

# Every `pub extern("c") fn` of the API modules is defined by the library.
grep -ho 'pub extern("c") fn [A-Za-z0-9_]*' compiler/capi/fontconfig/api/*.mlx | awk '{print $4}' | sort -u > "$work/declared"
nm -D --defined-only "$library" | awk '{print $NF}' | sort -u > "$work/defined"
comm -23 "$work/declared" "$work/defined" > "$work/missing"
[ ! -s "$work/missing" ] || { cat "$work/missing"; fail "functions the headers declare are not in the library"; }
echo "ok   the library defines the $(wc -l < "$work/declared") functions the headers declare"

printf '#include <fontconfig/fontconfig.h>\n#include <fontconfig/fcfreetype.h>\nint main(void) { FcValue v = FcValueSave((FcValue){0}); return FcGetVersion() == FC_VERSION && v.type == FcTypeVoid ? 0 : 1; }\n' > "$work/all.c"
mlx-out/bin/mlx-cc -I"$headers" -I"$ftheaders" -c "$work/all.c" -o "$work/all.o" || fail "the headers do not compile as C"
printf '#include <fontconfig/fontconfig.h>\n#include <fontconfig/fcfreetype.h>\nint main(void) { FcValue v = FcValueSave(FcValue()); return FcGetVersion() == FC_VERSION && v.type == FcTypeVoid ? 0 : 1; }\n' > "$work/all.cpp"
mlx-out/bin/mlx-cc -I"$headers" -I"$ftheaders" -x c++ -std=c++20 -c "$work/all.cpp" -o "$work/all-cpp.o" || fail "the headers do not compile as C++"
echo "ok   the headers in C and C++"

system=0
if command -v gcc > /dev/null && printf '#include <fontconfig/fontconfig.h>\nint main(void){return 0;}\n' | gcc -x c - -o "$work/probe" -lfontconfig 2> /dev/null; then
    system=1
fi

# Runs a test program in a directory of its own (its caches there): its
# output, and its exit status last.
run_test() {
    local program=$1 output=$2 status=0
    shift 2
    rm -rf "$work/run" && mkdir "$work/run"
    (cd "$work/run" && HOME="$work/run" XDG_CACHE_HOME="$work/run/cache" LD_LIBRARY_PATH="$repo_root/mlx-out/lib" timeout 300 "$program" "$@" > "$output" 2> "$output.err") || status=$?
    echo "exit $status" >> "$output"
}

for test in names config freetype; do
    extra=""
    systemExtra=""
    if [ "$test" = freetype ]; then
        extra="-I$ftheaders mlx-out/lib/libmlxfreetype.so"
        systemExtra="-I/usr/include/freetype2 -lfreetype"
    fi
    mlx-out/bin/mlx-cc -I"$headers" $extra "$tests/$test.c" -o "$work/$test" "$library" || fail "tests/$test.c does not build"
    run_test "$work/$test" "$work/$test.out" "$repo_root"
    if ! diff -u "$tests/$test.expected" "$work/$test.out" > "$work/$test.diff"; then
        head -40 "$work/$test.diff"
        fail "tests/$test.c does not print what $test.expected says"
    fi
    if [ "$system" = 1 ]; then
        gcc -o "$work/$test.system" "$tests/$test.c" -lfontconfig $systemExtra
        run_test "$work/$test.system" "$work/$test.system.out" "$repo_root"
        diff -u "$tests/$test.expected" "$work/$test.system.out" > "$work/$test.system.diff" || { head -40 "$work/$test.system.diff"; fail "the system's Fontconfig no longer prints $test.expected"; }
        echo "ok   tests/$test.c (and the system's Fontconfig prints the same)"
    else
        echo "ok   tests/$test.c"
    fi
done

if [ "$system" = 1 ]; then
    # The system's configuration and fonts, read by each library (with the
    # real home: the configuration names its directories).
    mlx-out/bin/mlx-cc -I"$headers" "$tests/system.c" -o "$work/system" "$library" || fail "tests/system.c does not build"
    gcc -o "$work/system.system" "$tests/system.c" -lfontconfig
    (cd "$work" && XDG_CACHE_HOME="$work/cache" LD_LIBRARY_PATH="$repo_root/mlx-out/lib" timeout 300 ./system > system.out 2> system.err) || { cat "$work/system.err"; fail "tests/system.c failed with mlx-fontconfig"; }
    (cd "$work" && ./system.system > system.system.out 2> /dev/null) || fail "tests/system.c failed with Fontconfig"
    diff -u "$work/system.system.out" "$work/system.out" > "$work/system.diff" || { head -60 "$work/system.diff"; fail "the system's fonts are not matched as Fontconfig matches them"; }
    # Again from the caches the first run wrote.
    (cd "$work" && XDG_CACHE_HOME="$work/cache" LD_LIBRARY_PATH="$repo_root/mlx-out/lib" timeout 300 ./system > system-cached.out 2> system.err) || fail "tests/system.c failed from the cache"
    cmp -s "$work/system.out" "$work/system-cached.out" || fail "the fonts read from the cache are not the fonts read"
    echo "ok   tests/system.c: the system's $(grep -c '^match' "$work/system.out") queries and $(grep -c '^list' "$work/system.out") fonts as Fontconfig has them (and from the cache)"
fi
echo "check_fontconfig.sh: all checks passed"
