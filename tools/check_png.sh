#!/usr/bin/env bash
# mlx-libpng, libpng's C API (compiler/libc/png, over std.png):
#
#   - `./mlx build png` builds the library (libmlxpng16.so) and the
#     headers: mlx-cheaders makes mlx-out/include/mlx-png/png.h,
#     pngconf.h and pnglibconf.h of the API's Mlx modules
#     (compiler/libc/png/api);
#   - every function the headers declare is in the library;
#   - the headers in C and C++;
#   - tests/corpus.py writes the test images (every colour type and bit
#     depth, interlaced and not, transparency, gamma values that need
#     correction, sRGB, cHRM and iCCP chunks good and bad, text, APNG's
#     chunks, image data in many IDATs, and damaged files);
#   - tests/decode.c, built by mlx-cc against the library, decodes them
#     with the progressive reader as WebKit's PNGImageDecoder does and
#     prints the header, the info, the unknown chunks, the warnings and
#     errors, the rows and a hash of the pixels: what decode.expected
#     says (what the same program prints built by gcc against libpng
#     1.6.43);
#   - where there are gcc and libpng's headers and library: decode.c
#     built by both, in each of its modes (WebKit's transformations, none,
#     Skia's, and 16-bit scaling with others) and with the data given
#     whole, 13 bytes and 1 byte at a time, must print the same;
#     tests/encode.c (Skia's PNG encoder's calls) must write the same
#     files byte for byte and read them back the same;
#   - Skia's and WebCore's units that include <png.h> compile with mlx-cc
#     on these headers, where the WebKit source is (WEBKIT_SOURCE).
#
#   tools/check_png.sh
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d /tmp/mlx-png.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }
headers=mlx-out/include/mlx-png
library=mlx-out/lib/libmlxpng16.so
tests=compiler/libc/png/tests

./mlx build -j 1 cc png > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "./mlx build png"; }
echo "ok   ./mlx build png"

grep -ho 'pub extern("c") fn [A-Za-z0-9_]*' compiler/libc/png/api/*.mlx | awk '{print $4}' | sort -u > "$work/declared"
nm -D --defined-only "$library" | awk '{print $NF}' | sort -u > "$work/defined"
comm -23 "$work/declared" "$work/defined" > "$work/missing"
[ ! -s "$work/missing" ] || { cat "$work/missing"; fail "functions the headers declare are not in the library"; }
echo "ok   the library defines the $(wc -l < "$work/declared") functions the headers declare"

cc() { mlx-out/bin/mlx-cc -I"$headers" "$@"; }
run() { LD_LIBRARY_PATH="$repo_root/mlx-out/lib" "$@"; }

printf '#include <png.h>\nint main(void) { png_structp p = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0); if (setjmp(png_jmpbuf(p))) return 1; return 0; }\n' > "$work/all.c"
cc -c "$work/all.c" -o "$work/all.o" || fail "the headers do not compile as C"
cc -x c++ -std=c++20 -c "$work/all.c" -o "$work/all-cpp.o" || fail "the headers do not compile as C++"
echo "ok   the headers in C and C++"

python3 "$tests/corpus.py" "$work/corpus" || fail "tests/corpus.py"
images=("$work"/corpus/*.png)
echo "ok   tests/corpus.py: ${#images[@]} images"

cc "$tests/decode.c" -o "$work/decode" "$library" || fail "tests/decode.c does not build"
cc "$tests/encode.c" -o "$work/encode" "$library" || fail "tests/encode.c does not build"
(cd "$work/corpus" && run "$work/decode" webkit 0 *.png) > "$work/decode.out" 2>&1 || fail "tests/decode.c failed"
diff -u "$tests/decode.expected" "$work/decode.out" > "$work/decode.diff" || { head -40 "$work/decode.diff"; fail "tests/decode.c prints otherwise"; }
echo "ok   tests/decode.c (WebKit's decoder) prints what libpng's prints"
run "$work/encode" > "$work/encode.out" 2>&1 || fail "tests/encode.c failed"
diff -u "$tests/encode.expected" "$work/encode.out" || fail "tests/encode.c prints otherwise"
echo "ok   tests/encode.c (Skia's encoder) writes what libpng writes"

if ! command -v gcc > /dev/null || ! printf '#include <png.h>\nint main(void) { return png_access_version_number() == 0; }\n' | gcc -x c - -o "$work/probe" -lpng > /dev/null 2>&1; then
    echo "skip the comparisons with the system's libpng: no gcc with libpng's headers and library"
else
    gcc -O1 -w "$tests/decode.c" -o "$work/decode-ref" -lpng || fail "tests/decode.c does not build against the system's libpng"
    gcc -O1 -w "$tests/encode.c" -o "$work/encode-ref" -lpng || fail "tests/encode.c does not build against the system's libpng"
    for mode in webkit raw skia scale; do
        for piece in 0 13 1; do
            (cd "$work/corpus" && "$work/decode-ref" $mode $piece *.png) > "$work/theirs" 2>&1
            (cd "$work/corpus" && run "$work/decode" $mode $piece *.png) > "$work/ours" 2>&1
            diff -u "$work/theirs" "$work/ours" > "$work/diff" || { head -40 "$work/diff"; fail "decode $mode $piece differs from the system's libpng"; }
        done
        echo "ok   decode $mode: the same as the system's libpng, the data whole, in 13-byte pieces and byte by byte"
    done
    "$work/encode-ref" > "$work/theirs" 2>&1
    diff -u "$work/theirs" "$work/encode.out" || fail "encode differs from the system's libpng"
    echo "ok   encode: the same files as the system's libpng writes"
fi

webkit=${WEBKIT_SOURCE:-/home/user/csrc/wpe/wpewebkit-2.54.1}
if [ ! -d "$webkit/Source/ThirdParty/skia" ]; then
    echo "skip Skia's and WebCore's sources that use libpng: no WebKit source at $webkit (WEBKIT_SOURCE)"
else
    units=(skia/src/codec/SkPngCodec.cpp skia/src/encode/SkPngEncoderImpl.cpp)
    ./mlx build -j 1 "${units[@]}" > "$work/units.log" 2>&1 || { tail -20 "$work/units.log"; fail "Skia's sources that use libpng do not compile"; }
    for unit in "${units[@]}"; do echo "ok   $unit"; done
fi

echo "check_png.sh: all checks passed"
