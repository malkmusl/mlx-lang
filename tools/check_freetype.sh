#!/usr/bin/env bash
# mlx-freetype, FreeType 2's C API in Mlx (projects/freetype):
#
#   - `./mlx build freetype` builds the library (libmlxfreetype.so) and
#     the headers: mlx-cheaders makes mlx-out/include/mlx-freetype of the
#     API's Mlx modules (projects/freetype/api), <ft2build.h> and
#     <freetype/...> as FreeType names them;
#   - every function the headers declare is in the library;
#   - the headers, every FT_*_H of them, in C and in C++;
#   - C programs built by mlx-cc against the library print what FreeType
#     2.13 prints for them (tests/expected/NAME.expected, the exit status
#     last), for the cases of tests/cases.txt: the programs of tests/
#       abi.c        the layout of every structure, every constant
#       calc.c       FreeType's arithmetic and trigonometry
#       face.c       faces: flags, names, sizes, charmaps, SFNT tables,
#                    glyph names, kerning, named instances
#       glyphs.c     glyph outlines and metrics at several sizes and load
#                    flags, advances, transforms
#       render.c     gray, monochrome and LCD renderings, outline rendering
#       hinted.c     CFF glyphs hinted by the CFF driver's hinter
#       glyphapi.c   glyph objects, bitmaps, synthetic styles, outlines
#       variations.c variable fonts: axes, instances, coordinates, varied
#                    glyphs and metrics (TrueType and CFF2)
#       color.c      CPAL palettes, COLR layers and version 1 paints
#       bitmaps.c    embedded bitmaps: EBDT, CBDT (PNG), sbix
#       damaged.c    fonts with bytes changed or cut short: the errors,
#                    and what loads of them still give
#     on the fonts of a Debian or Ubuntu system (fonts-dejavu-core,
#     fonts-liberation, fonts-freefont-ttf, fonts-crosextra-*,
#     fonts-ipafont-gothic, fonts-inter, fonts-tlwg-loma-otf, fonts-unifont,
#     fonts-noto-color-emoji, fonts-wqy-zenhei; a case whose font is
#     missing is skipped) and on the small fonts of tests/fonts (made by
#     tests/fonts/make_fonts.py);
#   - Skia's FreeType sources (SkFontHost_FreeType*.cpp, the font managers
#     on FreeType) compile against these headers (with the C++ compiler
#     CXX, g++ by default, where the WebKit source is: WEBKIT_SOURCE), and
#     every FreeType function they call is in the library.
#
#   tools/check_freetype.sh             the checks
#   tools/check_freetype.sh --update    writes tests/expected/*.expected:
#                                       the programs built by gcc against
#                                       the system's FreeType 2.13
#                                       (libfreetype-dev), run on the cases
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d /tmp/mlx-freetype.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }
headers=mlx-out/include/mlx-freetype
library=mlx-out/lib/libmlxfreetype.so
tests=projects/freetype/tests
update=0
[ "${1:-}" = "--update" ] && update=1

# The arguments of a case, @ made the test fonts' directory; empty when a
# font it names is missing.
case_arguments() {
    local args=() word
    for word in "$@"; do
        word=${word/#@/$tests/fonts}
        case "$word" in
            /*|$tests/*) [ -e "$word" ] || return 1 ;;
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

if [ "$update" = 1 ]; then
    reference() { gcc -O1 $(pkg-config --cflags freetype2) "$1" -o "$2" $(pkg-config --libs freetype2) -lm; }
    mkdir -p "$tests/expected"
    run_cases reference reference
    exit 0
fi

./mlx build cc freetype > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "./mlx build freetype"; }
echo "ok   ./mlx build freetype"

# Every `pub extern("c") fn` of the API modules is defined by the library.
grep -ho 'pub extern("c") fn [A-Za-z0-9_]*' projects/freetype/api/*.mlx | awk '{print $4}' | sort -u > "$work/declared"
nm -D --defined-only "$library" | awk '{print $NF}' | sort -u > "$work/defined"
comm -23 "$work/declared" "$work/defined" > "$work/missing"
[ ! -s "$work/missing" ] || { cat "$work/missing"; fail "functions the headers declare are not in the library"; }
echo "ok   the library defines the $(wc -l < "$work/declared") functions the headers declare"

cc() { mlx-out/bin/mlx-cc -I"$headers" "$@"; }

{
    echo '#include <ft2build.h>'
    grep -ho '^#define FT_[A-Z0-9_]*_H ' "$headers/freetype/config/ftheader.h" | awk '{print "#include " $2}'
    echo 'int main(void) { return 0; }'
} > "$work/all.c"
cc -c "$work/all.c" -o "$work/all.o" || fail "the headers together do not compile as C"
cc -x c++ -std=c++20 -c "$work/all.c" -o "$work/all-cpp.o" || fail "the headers together do not compile as C++"
echo "ok   the headers ($(grep -c '#include FT_' "$work/all.c") of them) in C and C++"

mine() { cc "$1" -o "$2" "$library"; }
run_cases mine mine

webkit=${WEBKIT_SOURCE:-/home/user/csrc/wpe/wpewebkit-2.54.1}
skia=$webkit/Source/ThirdParty/skia
cxx=${CXX:-g++}
if [ ! -d "$skia/src/ports" ]; then
    echo "skip Skia's FreeType sources: no WebKit source at $webkit (WEBKIT_SOURCE)"
elif ! command -v "$cxx" > /dev/null; then
    echo "skip Skia's FreeType sources: no $cxx (CXX)"
else
    # Skia as WebKit's WPE port builds it (its compile_commands.json).
    defines="-DBUILDING_WEBKIT=1 -DBUILDING_WPE__=1 -DSKIA_IMPLEMENTATION=1 -DSK_ASSUME_GL=0 -DSK_ASSUME_GL_ES=1 -DSK_ASSUME_WEBGL=0 -DSK_CODEC_DECODES_PNG -DSK_DISABLE_LEGACY_GL_MAKE_NATIVE_INTERFACE -DSK_DISABLE_LEGACY_IMAGE_READBUFFER -DSK_DISABLE_LEGACY_INIT_DECODERS -DSK_DISABLE_LEGACY_PNG_WRITEBUFFER -DSK_DISABLE_LEGACY_SKSHAPER_FUNCTIONS -DSK_DISABLE_TRACING -DSK_DISABLE_WEBGL_INTERFACE -DSK_ENABLE_PRECOMPILE -DSK_GAMMA_APPLY_TO_A8 -DSK_GANESH -DSK_GL -DSK_R32_SHIFT=16 -DSK_RELEASE -DSK_TYPEFACE_FACTORY_FREETYPE -DNDEBUG"
    : > "$work/skia-needs"
    for source in SkFontHost_FreeType.cpp SkFontHost_FreeType_common.cpp SkFontMgr_custom.cpp SkFontMgr_custom_directory.cpp SkFontMgr_custom_embedded.cpp SkFontMgr_custom_empty.cpp SkFontMgr_fontconfig.cpp SkFontConfigInterface.cpp SkFontConfigInterface_direct.cpp SkFontConfigInterface_direct_factory.cpp; do
        # shellcheck disable=SC2086
        "$cxx" $defines '-DSK_USER_CONFIG_HEADER="./WebKitSkiaConfig.h"' -I"$skia" -I"$headers" -std=c++23 -fno-exceptions -fno-rtti -fPIC -w -c "$skia/src/ports/$source" -o "$work/$source.o" > "$work/$source.log" 2>&1 || { head -30 "$work/$source.log"; fail "Skia's $source does not compile against the headers"; }
        nm -u "$work/$source.o" | awk '{print $2}' | grep '^FT_' >> "$work/skia-needs" || true
    done
    sort -u "$work/skia-needs" | comm -23 - "$work/defined" > "$work/skia-missing"
    [ ! -s "$work/skia-missing" ] || { cat "$work/skia-missing"; fail "functions Skia calls are not in the library"; }
    echo "ok   Skia's FreeType sources compile against the headers ($cxx) and call $(sort -u "$work/skia-needs" | wc -l) of the library's functions"
fi
echo "PASS mlx-freetype"
