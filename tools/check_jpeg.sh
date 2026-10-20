#!/usr/bin/env bash
# mlx-libjpeg, libjpeg's C API (compiler/libc/jpeg, over std.jpeg):
#
#   - `./mlx build jpeg` builds the library (libmlxjpeg.so) and the
#     headers: mlx-cheaders makes mlx-out/include/mlx-jpeg/jpeglib.h,
#     jerror.h, jconfig.h and jmorecfg.h of the API's Mlx modules
#     (compiler/libc/jpeg/api);
#   - every function the headers declare is in the library;
#   - the headers in C and C++, and the structures laid out as the
#     system's jpeglib.h lays them out (tests/layout.c, where there is
#     gcc with libjpeg's headers);
#   - tests/decode.c, built by mlx-cc against the library, decodes the
#     images of tests/images (baseline and progressive, 4:2:0, 4:2:2,
#     4:4:4, grey, CMYK, restart markers, EXIF and ICC markers, damaged
#     and cut files) as WebKit's JPEGImageDecoder does (a suspending
#     source given the data whole and 7 bytes at a time, buffered-image
#     output of progressive files) and in Skia's ways (output colour
#     spaces, no fancy upsampling, cropping and skipping, raw data, the
#     scaled dimensions), printing the header, each call's progress,
#     warnings and errors and hashes of the pixels: what decode.expected
#     says (what the same program prints built by gcc against the
#     system's libjpeg-turbo 2.1.5);
#   - where there are gcc, libjpeg's headers and library and Python's
#     Pillow: tests/corpus.py writes some 160 more images (every sampling,
#     qualities, optimized tables, restart intervals...) and damaged
#     copies of them; decode.c built by both prints the same for each in
#     every mode, WebKit's with the data whole and 13 bytes at a time;
#   - Skia's units that include <jpeglib.h> compile with mlx-cc on these
#     headers, where the WebKit source is (WEBKIT_SOURCE).
#
#   tools/check_jpeg.sh
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d /tmp/mlx-jpeg.XXXXXX)
trap '[ -n "${KEEP:-}" ] || rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }
headers=mlx-out/include/mlx-jpeg
library=mlx-out/lib/libmlxjpeg.so
tests=compiler/libc/jpeg/tests
runs=("webkit 0" "webkit 7" "rgba" "gray" "plain" "crop" "raw" "multi" "scale")

./mlx build -j 1 cc jpeg > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "./mlx build jpeg"; }
echo "ok   ./mlx build jpeg"

grep -ho 'pub extern("c") fn [A-Za-z0-9_]*' compiler/libc/jpeg/api/*.mlx | awk '{print $4}' | sort -u > "$work/declared"
nm -D --defined-only "$library" | awk '{print $NF}' | sort -u > "$work/defined"
comm -23 "$work/declared" "$work/defined" > "$work/missing"
[ ! -s "$work/missing" ] || { cat "$work/missing"; fail "functions the headers declare are not in the library"; }
echo "ok   the library defines the $(wc -l < "$work/declared") functions the headers declare"

cc() { mlx-out/bin/mlx-cc -I"$headers" "$@"; }
run() { LD_LIBRARY_PATH="$repo_root/mlx-out/lib" "$@"; }

printf '#include <stdio.h>\n#include <setjmp.h>\n#include <jpeglib.h>\n#include <jerror.h>\nstruct e { struct jpeg_error_mgr pub; jmp_buf jump; };\nint main(void) { struct jpeg_decompress_struct d; struct e err; d.err = jpeg_std_error(&err.pub); if (setjmp(err.jump)) return 1; jpeg_create_decompress(&d); jpeg_save_markers(&d, JPEG_APP0 + 1, 0xFFFF); d.out_color_space = JCS_EXT_BGRA; jpeg_destroy_decompress(&d); return JMSG_LASTMSGCODE == 128 ? 0 : 1; }\n' > "$work/all.c"
cc -c "$work/all.c" -o "$work/all.o" || fail "the headers do not compile as C"
cc -x c++ -std=c++20 -c "$work/all.c" -o "$work/all-cpp.o" || fail "the headers do not compile as C++"
cc "$work/all.c" -o "$work/all" "$library" || fail "a program does not link with the library"
run "$work/all" || fail "a program creating a decompressor"
echo "ok   the headers in C and C++"

decodes() {
    local decode=$1 directory=$2 file run
    for file in "$directory"/*.jpg; do
        for run in "${runs[@]}"; do
            echo "== $(basename "$file") $run"
            # shellcheck disable=SC2086
            "$decode" ${run%% *} "$file" ${run#* } 2>&1 || true
        done
    done
}
# (A run without a piece size passes its mode twice; decode.c ignores it.)

cc "$tests/decode.c" -o "$work/decode" "$library" || fail "tests/decode.c does not build"
LD_LIBRARY_PATH="$repo_root/mlx-out/lib" decodes "$work/decode" "$tests/images" > "$work/decode.out"
diff -u "$tests/decode.expected" "$work/decode.out" > "$work/decode.diff" || { head -40 "$work/decode.diff"; fail "tests/decode.c prints otherwise"; }
echo "ok   tests/decode.c on tests/images: what the system's libjpeg-turbo prints ($(grep -c '^==' "$work/decode.out") runs)"

if ! command -v gcc > /dev/null || ! printf '#include <stdio.h>\n#include <jpeglib.h>\nint main(void) { struct jpeg_error_mgr e; return jpeg_std_error(&e) == 0; }\n' | gcc -x c - -o "$work/probe" -ljpeg > /dev/null 2>&1; then
    echo "skip the comparisons with the system's libjpeg: no gcc with libjpeg's headers and library"
else
    gcc -O1 -w "$tests/layout.c" -o "$work/layout-ref" || fail "tests/layout.c does not build against the system's jpeglib.h"
    cc "$tests/layout.c" -o "$work/layout" || fail "tests/layout.c does not build"
    diff -u <("$work/layout-ref") <(run "$work/layout") || fail "the structures are not laid out as the system's jpeglib.h lays them out"
    echo "ok   the structures laid out as the system's jpeglib.h lays them out"
    gcc -O1 -w "$tests/decode.c" -o "$work/decode-ref" -ljpeg || fail "tests/decode.c does not build against the system's libjpeg"
    if ! python3 -c 'import PIL' 2> /dev/null; then
        echo "skip the larger corpus: no Python with Pillow"
    else
        python3 "$tests/corpus.py" "$work/corpus" || fail "tests/corpus.py"
        python3 - "$work/corpus" "$work/damaged" <<'PYTHON'
import os, random, sys
source, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
files = sorted(f for f in os.listdir(source) if f.endswith(".jpg"))
rnd = random.Random(1234)
for n in range(150):
    name = rnd.choice(files)
    data = bytearray(open(os.path.join(source, name), "rb").read())
    kind = rnd.randint(0, 2)
    for i in range(rnd.randint(1, 6)):
        if len(data) < 8:
            break
        p = rnd.randint(2, len(data) - 1)
        if kind == 0:
            data[p] = rnd.randint(0, 255)
        elif kind == 1:
            del data[p:p + rnd.randint(1, 40)]
        else:
            data[p:p] = bytes(rnd.randint(0, 255) for _ in range(rnd.randint(1, 8)))
    open(os.path.join(out, "z%03d_%s" % (n, name)), "wb").write(data)
PYTHON
        count=$(ls "$work/corpus" "$work/damaged" | grep -c '\.jpg$')
        runs=("webkit 0" "webkit 13" "rgba" "rgb" "bgr" "gray" "cmyk" "ycc" "plain" "nosmooth" "crop" "raw" "multi" "scale")
        for directory in corpus damaged; do
            decodes "$work/decode-ref" "$work/$directory" > "$work/theirs"
            LD_LIBRARY_PATH="$repo_root/mlx-out/lib" decodes "$work/decode" "$work/$directory" > "$work/ours"
            diff -u "$work/theirs" "$work/ours" > "$work/diff" || { head -40 "$work/diff"; fail "decode.c differs from the system's libjpeg on tests/corpus.py's $directory images"; }
        done
        echo "ok   decode.c on $count more images, good and damaged, in ${#runs[@]} modes: the same as the system's libjpeg-turbo"
    fi
fi

webkit=${WEBKIT_SOURCE:-/home/user/csrc/wpe/wpewebkit-2.54.1}
if [ ! -d "$webkit/Source/ThirdParty/skia" ]; then
    echo "skip Skia's sources that use libjpeg: no WebKit source at $webkit (WEBKIT_SOURCE)"
else
    units=(skia/src/codec/SkJpegCodec.cpp skia/src/codec/SkJpegDecoderMgr.cpp skia/src/codec/SkJpegUtility.cpp skia/src/encode/SkJPEGWriteUtility.cpp skia/src/encode/SkJpegEncoderImpl.cpp)
    ./mlx build -j 1 "${units[@]}" > "$work/units.log" 2>&1 || { tail -20 "$work/units.log"; fail "Skia's sources that use libjpeg do not compile"; }
    for unit in "${units[@]}"; do echo "ok   $unit"; done
fi

echo "check_jpeg.sh: all checks passed"
