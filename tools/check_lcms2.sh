#!/usr/bin/env bash
# mlx-lcms2, Little CMS's C API (compiler/libc/lcms2, over
# std.colorprofile):
#
#   - `./mlx build lcms2` builds the library (libmlxlcms2.so) and the
#     header: mlx-cheaders makes mlx-out/include/mlx-lcms2/lcms2.h of the
#     API's Mlx module (compiler/libc/lcms2/api/lcms2.mlx);
#   - every function the header declares is in the library;
#   - the header in C and C++, and a program linked with the library;
#   - tests/transform.c, built by mlx-cc against the library, opens the
#     profiles of tests/profiles (made by the system's Little CMS with
#     tests/profiles.c: matrix-shapers of version 2 and 4 with `curv`
#     gammas and tables and `para` curves, grey, lut8, lut16, lutAtoB
#     and lutBtoA profiles of XYZ and Lab PCS, damaged copies) from
#     memory as WebKit's image decoders do, prints their headers and tags,
#     tone curves and colorimetry, and transforms pixels of each profile
#     to the built-in sRGB (BGRA_8 in place, as WebKit does) and grey
#     profiles and to every other profile, in 8- and 16-bit formats, for
#     the four intents (black point compensation where Little CMS applies
#     it), unoptimized and forced to a grid, with alpha copied: what
#     transform.expected says (what the same program prints built by gcc
#     against the system's Little CMS 2.14), every pixel's hash the same;
#   - where there are gcc and Little CMS's headers and library: the
#     profiles made again by tests/profiles.c (larger grids among them)
#     and transform.c built by both prints the same for them;
#   - WebCore's PNG and JPEG image decoders (USE_LCMS) compile with mlx-cc
#     on this header, where the WebKit source is (WEBKIT_SOURCE) and
#     WebCore's derived sources are made.
#
#   tools/check_lcms2.sh
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d /tmp/mlx-lcms2.XXXXXX)
trap '[ -n "${KEEP:-}" ] || rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }
headers=mlx-out/include/mlx-lcms2
library=mlx-out/lib/libmlxlcms2.so
tests=compiler/libc/lcms2/tests

./mlx build -j 1 cc lcms2 > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "./mlx build lcms2"; }
echo "ok   ./mlx build lcms2"

grep -ho 'pub extern("c") fn [A-Za-z0-9_]*' compiler/libc/lcms2/api/*.mlx | awk '{print $4}' | sort -u > "$work/declared"
nm -D --defined-only "$library" | awk '{print $NF}' | sort -u > "$work/defined"
comm -23 "$work/declared" "$work/defined" > "$work/missing"
[ ! -s "$work/missing" ] || { cat "$work/missing"; fail "functions the header declares are not in the library"; }
echo "ok   the library defines the $(wc -l < "$work/declared") functions the header declares"

cc() { mlx-out/bin/mlx-cc -I"$headers" "$@"; }
run() { LD_LIBRARY_PATH="$repo_root/mlx-out/lib" "$@"; }

printf '#include <lcms2.h>\nint main(void) { cmsHPROFILE p = cmsCreate_sRGBProfile(); cmsHTRANSFORM t = cmsCreateTransform(p, TYPE_BGRA_8, p, TYPE_BGRA_8, INTENT_RELATIVE_COLORIMETRIC, 0); unsigned char px[4] = { 10, 20, 30, 40 }; cmsDoTransform(t, px, px, 1); cmsDeleteTransform(t); int ok = cmsGetColorSpace(p) == cmsSigRgbData && px[3] == 40 && TYPE_BGRA_8 == (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1)|DOSWAP_SH(1)|SWAPFIRST_SH(1)); cmsCloseProfile(p); return ok && LCMS_VERSION == 2140 ? 0 : 1; }\n' > "$work/all.c"
cc -c "$work/all.c" -o "$work/all.o" || fail "the header does not compile as C"
cc -x c++ -std=c++20 -c "$work/all.c" -o "$work/all-cpp.o" || fail "the header does not compile as C++"
cc "$work/all.c" -o "$work/all" "$library" || fail "a program does not link with the library"
run "$work/all" || fail "a program transforming a pixel from sRGB to sRGB"
echo "ok   the header in C and C++"

cc "$tests/transform.c" -o "$work/transform" "$library" || fail "tests/transform.c does not build"
run "$work/transform" "$tests"/profiles/*.icc > "$work/transform.out" 2>&1 || fail "tests/transform.c"
diff -u "$tests/transform.expected" "$work/transform.out" > "$work/transform.diff" || { head -40 "$work/transform.diff"; fail "tests/transform.c prints otherwise"; }
echo "ok   tests/transform.c on tests/profiles: what the system's Little CMS prints ($(ls "$tests"/profiles/*.icc | wc -l) profiles, $(grep -c ': [0-9a-f]\{8\}$' "$work/transform.out") transforms)"

if ! command -v gcc > /dev/null || ! printf '#include <lcms2.h>\nint main(void) { return cmsGetEncodedCMMversion() == 0; }\n' | gcc -x c - -o "$work/probe" -llcms2 > /dev/null 2>&1; then
    echo "skip the comparisons with the system's Little CMS: no gcc with Little CMS's header and library"
else
    gcc -O1 -w "$tests/profiles.c" -o "$work/profiles" -llcms2 -lm || fail "tests/profiles.c does not build against the system's Little CMS"
    gcc -O1 -w "$tests/transform.c" -o "$work/transform-ref" -llcms2 || fail "tests/transform.c does not build against the system's Little CMS"
    mkdir -p "$work/corpus"
    "$work/profiles" "$work/corpus" || fail "tests/profiles.c"
    "$work/transform-ref" "$work"/corpus/*.icc > "$work/theirs" 2>&1 || fail "tests/transform.c built by gcc"
    run "$work/transform" "$work"/corpus/*.icc > "$work/ours" 2>&1 || fail "tests/transform.c on the corpus"
    diff -u "$work/theirs" "$work/ours" > "$work/diff" || { head -40 "$work/diff"; fail "transform.c differs from the system's Little CMS on tests/profiles.c's profiles"; }
    echo "ok   transform.c on $(ls "$work"/corpus/*.icc | wc -l) profiles made again: the same as the system's Little CMS ($(grep -c ': [0-9a-f]\{8\}$' "$work/ours") transforms)"
fi

webkit=${WEBKIT_SOURCE:-/home/user/csrc/wpe/wpewebkit-2.54.1}
if [ ! -d "$webkit/Source/WebCore" ]; then
    echo "skip WebCore's sources that use Little CMS: no WebKit source at $webkit (WEBKIT_SOURCE)"
elif [ ! -d mlx-out/webkit/wpe/generated/WebCore ]; then
    echo "skip WebCore's sources that use Little CMS: WebCore's derived sources are not made (./mlx build webkit-wpe-webcore)"
else
    units=(wpe-webcore/platform/image-decoders/png/PNGImageDecoder.cpp wpe-webcore/platform/image-decoders/jpeg/JPEGImageDecoder.cpp)
    ./mlx build -j 1 "${units[@]}" > "$work/units.log" 2>&1 || { tail -20 "$work/units.log"; fail "WebCore's sources that use Little CMS do not compile"; }
    for unit in "${units[@]}"; do echo "ok   $unit"; done
fi

echo "check_lcms2.sh: all checks passed"
