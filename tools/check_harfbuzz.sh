#!/usr/bin/env bash
# mlx-harfbuzz, HarfBuzz's C API in Mlx (compiler/capi/harfbuzz) over the
# shaping engine std.shaping (std/src/shaping), hb-ft over mlx-freetype:
#
#   - `./mlx build harfbuzz` builds the library (libmlxharfbuzz.so) and the
#     headers: mlx-cheaders makes mlx-out/include/mlx-harfbuzz of the API's
#     Mlx modules (compiler/capi/harfbuzz/api): <hb.h>, <hb-ot.h>,
#     <hb-ft.h>, <hb-icu.h>, <hb-features.h> and the headers they include;
#   - every function the headers declare is in the library (and every
#     export of compiler/capi/harfbuzz/exports.mlx is one of std.shaping's
#     functions: tools/harfbuzz_exports.py --check);
#   - the headers in C and in C++;
#   - C programs built by mlx-cc against the library print what HarfBuzz
#     8.3 prints for them (tests/expected/NAME.expected, the exit status
#     last), for the cases of tests/cases.txt: the programs of tests/
#       buffer.c     buffers, features, variations, languages, scripts and
#                    the Unicode functions
#       tags.c       OpenType tags of scripts and languages
#       fonts.c      fonts: extents, metrics, characters, advances, glyph
#                    extents and names, scaled, emboldened, slanted
#       shape.c      shaping lines of text (tests/texts) of many scripts
#       math.c       the MATH table
#       ft.c         hb-ft (fonts over FreeType faces) and hb-icu
#       outlines.c   glyph outlines (hb_font_draw_glyph) and extents
#       glyphnames.c glyph names both ways
#       color.c      CPAL, COLR (layers and paint graphs), SVG, CBDT, sbix
#       names.c      the name table and the style values
#       base.c       BASE baselines and extents, meta, feature names
#       collect.c    the glyphs of each lookup, glyph alternates
#       closure.c    substitution closures, the glyph closure of shaping
#       cmap.c       characters and nominal glyphs collected
#       layout.c     script and language systems, features and lookups
#     on the fonts of a Debian or Ubuntu system (fonts-dejavu-core,
#     fonts-liberation, fonts-freefont-ttf, fonts-crosextra-carlito,
#     fonts-ipafont-gothic, fonts-inter, fonts-tlwg-loma-otf, fonts-unifont,
#     fonts-noto-color-emoji, fonts-wqy-zenhei; a case whose font is
#     missing is skipped) and on fonts tests/fonts/make_fonts.sh makes of
#     some of them;
#   - with the system's HarfBuzz 8.3 (gcc, pkg-config harfbuzz):
#       a sweep: the programs built against it and against the library
#         print the same for every font of the system and the made ones
#         (shape.c with every text of tests/texts, the others per font);
#     (HARFBUZZ_SWEEP=0 leaves it out);
#   - with HarfBuzz 8.3's source (HARFBUZZ_SOURCE, the directory of
#     harfbuzz-8.3.0.tar.xz unpacked; left out when not set):
#       its shaping tests (test/shape/data/*/tests/*.tests, run as its
#         run-tests.py runs them, with tests/hb-shape.c in place of
#         hb-shape): the library gives the expected glyphs for every case
#         but those of Apple's AAT tables (morx, kerx, trak; not
#         implemented), and the same as the system's HarfBuzz for all;
#       hb_shape_justify (not in the system's library: an experimental
#         function) against HarfBuzz built from that source with
#         HB_EXPERIMENTAL_API, tests/justify.c;
#   - Skia's SkShaper_harfbuzz.cpp compiles against these headers (with the
#     C++ compiler CXX, g++ by default, where the WebKit source is:
#     WEBKIT_SOURCE), and every HarfBuzz function it and WebCore's sources
#     on HarfBuzz call is in the library.
#
#   tools/check_harfbuzz.sh             the checks
#   tools/check_harfbuzz.sh --update    writes tests/expected/*.expected:
#                                       the programs built by gcc against
#                                       the system's HarfBuzz 8.3
#                                       (libharfbuzz-dev), run on the cases
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d /tmp/mlx-harfbuzz.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }
headers=mlx-out/include/mlx-harfbuzz
ft_headers=mlx-out/include/mlx-freetype
icu_headers=mlx-out/include/mlx-unicode
library=mlx-out/lib/libmlxharfbuzz.so
ft_library=mlx-out/lib/libmlxfreetype.so
tests=compiler/capi/harfbuzz/tests
update=0
[ "${1:-}" = "--update" ] && update=1

sh "$tests/fonts/make_fonts.sh" "$work/fonts" > /dev/null

# The arguments of a case, @ made the made fonts' directory and % the
# texts'; empty when a font it names is missing.
case_arguments() {
    local args=() word
    for word in "$@"; do
        word=${word/#@/$work/fonts}
        word=${word/#%/$tests/texts}
        case "$word" in
            /*) [ -e "${word%:*}" ] || return 1 ;;
        esac
        args+=("$word")
    done
    printf '%s\n' "${args[@]+"${args[@]}"}"
}

run_cases() {
    local build=$1 kind=$2 name program
    while read -r name program rest; do
        case "$name" in ''|'#'*) continue ;; esac
        if [ ! -x "$work/$kind-$program" ]; then
            $build "$tests/$program.c" "$work/$kind-$program" < /dev/null || fail "tests/$program.c does not build ($kind)"
        fi
        # shellcheck disable=SC2086
        if ! case_arguments $rest > "$work/args"; then
            echo "skip $name: a font is missing"
            continue
        fi
        mapfile -t args < "$work/args"
        status=0
        LD_LIBRARY_PATH="$repo_root/mlx-out/lib" "$work/$kind-$program" "${args[@]+"${args[@]}"}" < /dev/null > "$work/$name.out" 2> "$work/$name.err" || status=$?
        echo "exit $status" >> "$work/$name.out"
        # (The made fonts' directory is a temporary one.)
        sed -i "s|$work/fonts|@|g; s|$tests/texts|%|g" "$work/$name.out"
        if [ "$kind" = reference ]; then
            cp "$work/$name.out" "$tests/expected/$name.expected"
            echo "wrote tests/expected/$name.expected"
        else
            [ -f "$tests/expected/$name.expected" ] || fail "no tests/expected/$name.expected"
            diff -u "$tests/expected/$name.expected" "$work/$name.out" > "$work/$name.diff" || { head -40 "$work/$name.diff"; cat "$work/$name.err"; fail "$name: tests/$program.c prints otherwise"; }
            echo "ok   $name ($(($(wc -l < "$work/$name.out") - 1)) lines)"
        fi
    done < "$tests/cases.txt"
}

system_cflags() { pkg-config --cflags harfbuzz freetype2 icu-uc; }
system_libs() { pkg-config --libs harfbuzz harfbuzz-icu freetype2 icu-uc; }

if [ "$update" = 1 ]; then
    # shellcheck disable=SC2046
    reference() { gcc -O1 -w $(system_cflags) "$1" -o "$2" $(system_libs) -lm; }
    mkdir -p "$tests/expected"
    run_cases reference reference
    exit 0
fi

./mlx build -j 2 cc freetype unicode-headers harfbuzz > "$work/build.log" 2>&1 || { tail -30 "$work/build.log"; fail "./mlx build harfbuzz"; }
echo "ok   ./mlx build harfbuzz"

# Every `pub extern("c") fn` of the API modules is defined by the library.
grep -ho 'pub extern("c") fn [A-Za-z0-9_]*' compiler/capi/harfbuzz/api/*.mlx | awk '{print $4}' | sort -u > "$work/declared"
nm -D --defined-only "$library" | awk '{print $NF}' | sort -u > "$work/defined"
comm -23 "$work/declared" "$work/defined" > "$work/missing"
[ ! -s "$work/missing" ] || { cat "$work/missing"; fail "functions the headers declare are not in the library"; }
python3 tools/harfbuzz_exports.py --check > "$work/exports.log" 2>&1 || { cat "$work/exports.log"; fail "tools/harfbuzz_exports.py --check"; }
echo "ok   the library defines the $(wc -l < "$work/declared") functions the headers declare"

cc() { mlx-out/bin/mlx-cc -I"$headers" -I"$ft_headers" -I"$icu_headers" "$@"; }
{
    for header in "$headers"/*.h; do echo "#include <$(basename "$header")>"; done
    echo 'int main(void) { return hb_version_atleast(8, 3, 0) ? 0 : 1; }'
} > "$work/all.c"
cc -c "$work/all.c" -o "$work/all.o" || fail "the headers together do not compile as C"
cc -x c++ -std=c++20 -c "$work/all.c" -o "$work/all-cpp.o" || fail "the headers together do not compile as C++"
echo "ok   the headers ($(grep -c '#include' "$work/all.c") of them) in C and C++"

mine() { cc "$1" -o "$2" "$library" "$ft_library"; }
run_cases mine mine

have_system=0
if command -v gcc > /dev/null && pkg-config --exists harfbuzz harfbuzz-icu freetype2 icu-uc 2> /dev/null; then
    have_system=1
    # shellcheck disable=SC2046
    reference() { gcc -O1 -w $(system_cflags) "$1" -o "$2" $(system_libs) -lm; }
fi

# Runs program $1 (built as ref-$1 and mine-$1) with the remaining
# arguments; fails when the outputs differ.
same() {
    local program=$1 status=0
    shift
    "$work/ref-$program" "$@" > "$work/same.ref" 2>&1 < /dev/null || echo "exit $?" >> "$work/same.ref"
    LD_LIBRARY_PATH="$repo_root/mlx-out/lib" timeout 300 "$work/mine-$program" "$@" > "$work/same.mine" 2>&1 < /dev/null || status=$?
    [ "$status" = 0 ] || echo "exit $status" >> "$work/same.mine"
    diff -u "$work/same.ref" "$work/same.mine" > "$work/same.diff" || { head -30 "$work/same.diff"; fail "tests/$program.c $*: the library answers otherwise than the system's HarfBuzz"; }
}

if [ "${HARFBUZZ_SWEEP:-1}" = 0 ]; then
    echo "skip the sweep (HARFBUZZ_SWEEP=0)"
elif [ "$have_system" = 0 ]; then
    echo "skip the sweep: no gcc or no HarfBuzz, FreeType or ICU development files"
else
    for program in shape fonts outlines glyphnames color names base collect closure cmap layout math; do
        reference "$tests/$program.c" "$work/ref-$program" || fail "tests/$program.c does not build with the system's HarfBuzz"
        mine "$tests/$program.c" "$work/mine-$program" || fail "tests/$program.c does not build"
    done
    find /usr/share/fonts -type f \( -name '*.ttf' -o -name '*.otf' -o -name '*.ttc' \) 2> /dev/null | sort > "$work/fonts.list"
    ls "$work"/fonts/*.ttf >> "$work/fonts.list"
    count=0
    while read -r font; do
        same shape "$font" "$tests"/texts/*.txt
        same fonts "$font"
        for program in outlines glyphnames; do same $program "$font" 300; done
        same color "$font" 120
        for program in names base collect closure cmap layout math; do same $program "$font"; done
        count=$((count + 1))
    done < "$work/fonts.list"
    echo "ok   the sweep: $count fonts as the system's HarfBuzz shapes and reads them"
fi

source_dir=${HARFBUZZ_SOURCE:-}
if [ -z "$source_dir" ]; then
    echo "skip HarfBuzz's shaping tests: HARFBUZZ_SOURCE is not set"
elif [ ! -d "$source_dir/test/shape/data" ]; then
    fail "HARFBUZZ_SOURCE: no test/shape/data in $source_dir"
elif [ "$have_system" = 0 ]; then
    echo "skip HarfBuzz's shaping tests: no gcc or no HarfBuzz development files"
else
    reference "$tests/hb-shape.c" "$work/ref-hb-shape" || fail "tests/hb-shape.c does not build with the system's HarfBuzz"
    mine "$tests/hb-shape.c" "$work/mine-hb-shape" || fail "tests/hb-shape.c does not build"
    LD_LIBRARY_PATH="$repo_root/mlx-out/lib" python3 "$tests/run_shape_tests.py" "$work/ref-hb-shape" "$work/mine-hb-shape" "$source_dir"/test/shape/data/*/tests/*.tests > "$work/shape-tests.log" 2>&1 || { tail -40 "$work/shape-tests.log"; fail "HarfBuzz's shaping tests"; }
    echo "ok   HarfBuzz's shaping tests: $(tail -1 "$work/shape-tests.log")"
    if command -v g++ > /dev/null; then
        g++ -O1 -w -DHB_EXPERIMENTAL_API -DHB_NO_MT -c "$source_dir/src/harfbuzz.cc" -o "$work/harfbuzz.o" || fail "HarfBuzz's source does not build"
        gcc -O1 -w -DHB_EXPERIMENTAL_API -I"$source_dir/src" -c "$tests/justify.c" -o "$work/justify.o"
        g++ "$work/justify.o" "$work/harfbuzz.o" -o "$work/ref-justify" -lm
        mine "$tests/justify.c" "$work/mine-justify" || fail "tests/justify.c does not build"
        count=0
        for font in "$work/fonts/VarTest.ttf" "$source_dir"/test/subset/data/fonts/Roboto-Variable.ttf "$source_dir"/test/subset/data/fonts/RobotoFlex-Variable.ttf "$source_dir"/test/shape/data/text-rendering-tests/fonts/TestGPOSFour.ttf /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf; do
            [ -e "$font" ] || continue
            same justify "$font"
            count=$((count + 1))
        done
        echo "ok   hb_shape_justify as HarfBuzz 8.3 built with HB_EXPERIMENTAL_API, $count fonts"
    fi
fi

webkit=${WEBKIT_SOURCE:-/home/user/csrc/wpe/wpewebkit-2.54.1}
skia=$webkit/Source/ThirdParty/skia
cxx=${CXX:-g++}
if [ ! -d "$skia/modules/skshaper" ]; then
    echo "skip SkShaper and WebCore: no WebKit source at $webkit (WEBKIT_SOURCE)"
elif ! command -v "$cxx" > /dev/null; then
    echo "skip SkShaper and WebCore: no $cxx (CXX)"
else
    # Skia as WebKit's WPE port builds it (its compile_commands.json).
    defines="-DBUILDING_WEBKIT=1 -DBUILDING_WPE__=1 -DSKIA_IMPLEMENTATION=1 -DSK_GANESH -DSK_GL -DSK_RELEASE -DNDEBUG -DSK_SHAPER_HARFBUZZ_AVAILABLE -DSK_SHAPER_UNICODE_AVAILABLE"
    # shellcheck disable=SC2086
    "$cxx" $defines '-DSK_USER_CONFIG_HEADER="./WebKitSkiaConfig.h"' -I"$skia" -I"$headers" -std=c++23 -fno-exceptions -fno-rtti -fPIC -w -c "$skia/modules/skshaper/src/SkShaper_harfbuzz.cpp" -o "$work/skshaper.o" > "$work/skshaper.log" 2>&1 || { head -30 "$work/skshaper.log"; fail "Skia's SkShaper_harfbuzz.cpp does not compile against the headers"; }
    nm -u "$work/skshaper.o" | awk '{print $2}' | grep '^hb_' | sort -u > "$work/skia-needs" || true
    comm -23 "$work/skia-needs" "$work/defined" > "$work/skia-missing"
    [ ! -s "$work/skia-missing" ] || { cat "$work/skia-missing"; fail "functions SkShaper calls are not in the library"; }
    echo "ok   Skia's SkShaper_harfbuzz.cpp compiles against the headers ($cxx) and calls $(wc -l < "$work/skia-needs") of the library's functions"
    core=$webkit/Source/WebCore/platform
    grep -oh '\bhb_[a-z0-9_]*\b' "$core"/text/LocaleICU.cpp "$core"/graphics/skia/ComplexTextControllerSkia.cpp "$core"/graphics/opentype/OpenTypeMathData.* "$core"/graphics/harfbuzz/*.cpp "$core"/graphics/harfbuzz/*.h "$core"/graphics/freetype/*.cpp "$core"/graphics/FontPlatformData.h 2> /dev/null | grep -v '_t$' | sort -u > "$work/webcore-needs"
    comm -23 "$work/webcore-needs" "$work/defined" > "$work/webcore-missing"
    [ ! -s "$work/webcore-missing" ] || { cat "$work/webcore-missing"; fail "functions WebCore calls are not in the library"; }
    echo "ok   the $(wc -l < "$work/webcore-needs") HarfBuzz functions WebCore's sources call are in the library"
fi
echo "PASS mlx-harfbuzz"
