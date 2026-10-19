#!/usr/bin/env bash
# mlx-libwebp, libwebp's C API (compiler/capi/webp, over std.webp):
#
#   - `./mlx build webp` builds the library (libmlxwebp.so) and the
#     headers: mlx-cheaders makes mlx-out/include/mlx-webp/webp/decode.h,
#     demux.h, mux_types.h and types.h of the API's Mlx modules
#     (compiler/capi/webp/api);
#   - every function the headers declare is in the library;
#   - the headers in C and C++, and the structures laid out as the
#     system's headers lay them out (tests/layout.c, where there is gcc
#     with libwebp's headers);
#   - tests/decode.c, built by mlx-cc against the library, decodes the
#     files of tests/images (lossy with each filter, lossless at each
#     effort, alpha raw and lossless and filtered, animations, metadata
#     chunks, files cut short and damaged) with every API: the simple
#     decoders, WebPDecode in every colour space (fancy and plain
#     upsampling, cropped, flipped, unfiltered, into slow memory), the
#     incremental decoders as WebKit's WEBPImageDecoder drives them (all
#     the data so far at each step, WebPIUpdate) and appending, the
#     demuxer on the whole file and on parts of it, the animation
#     decoder, printing statuses, sizes, rows done and hashes of the
#     pixels: what decode.expected says (what the same program prints
#     built by gcc against the system's libwebp 1.3.2);
#   - where there are gcc, libwebp's headers and libraries (with its
#     encoder and muxer) and Python's Pillow: tests/sources.py and
#     tests/corpus.c make some 450 more files (every source picture
#     through every encoder setting) and damaged copies of them; decode.c
#     built by both prints the same for each.
#
#   tools/check_webp.sh
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d /tmp/mlx-webp.XXXXXX)
trap '[ -n "${KEEP:-}" ] || rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }
headers=mlx-out/include/mlx-webp
library=mlx-out/lib/libmlxwebp.so
tests=compiler/capi/webp/tests

./mlx build -j 1 cc webp > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "./mlx build webp"; }
echo "ok   ./mlx build webp"

grep -ho 'pub extern("c") fn [A-Za-z0-9_]*' compiler/capi/webp/api/*.mlx | awk '{print $4}' | sort -u > "$work/declared"
nm -D --defined-only "$library" | awk '{print $NF}' | sort -u > "$work/defined"
comm -23 "$work/declared" "$work/defined" > "$work/missing"
[ ! -s "$work/missing" ] || { cat "$work/missing"; fail "functions the headers declare are not in the library"; }
echo "ok   the library defines the $(wc -l < "$work/declared") functions the headers declare"

cc() { mlx-out/bin/mlx-cc -I"$headers" "$@"; }
run() { LD_LIBRARY_PATH="$repo_root/mlx-out/lib" "$@"; }

printf '#include <webp/decode.h>\n#include <webp/demux.h>\n#include <webp/mux_types.h>\nint main(void) { WebPDecoderConfig c; WebPData d; WebPDemuxState s; if (!WebPInitDecoderConfig(&c)) return 1; WebPDataInit(&d); if (WebPDemuxPartial(&d, &s) != NULL) return 1; return WebPGetDecoderVersion() == 0x10302 && WebPIsAlphaMode(MODE_bgrA) && !WebPIsRGBMode(MODE_YUV) ? 0 : 1; }\n' > "$work/all.c"
cc -c "$work/all.c" -o "$work/all.o" || fail "the headers do not compile as C"
cc -x c++ -std=c++20 -c "$work/all.c" -o "$work/all-cpp.o" || fail "the headers do not compile as C++"
cc "$work/all.c" -o "$work/all" "$library" || fail "a program does not link with the library"
run "$work/all" || fail "a program initializing a decoder configuration"
echo "ok   the headers in C and C++"

decodes() {
    local decode=$1 directory=$2 file
    for file in "$directory"/*.webp; do
        echo "== $(basename "$file")"
        "$decode" all "$file" 2>&1 || true
    done
}

cc "$tests/decode.c" -o "$work/decode" "$library" || fail "tests/decode.c does not build"
run decodes "$work/decode" "$tests/images" > "$work/decode.out"
diff -u "$tests/decode.expected" "$work/decode.out" > "$work/decode.diff" || { head -40 "$work/decode.diff"; fail "tests/decode.c prints otherwise"; }
echo "ok   tests/decode.c on tests/images: what the system's libwebp prints ($(grep -c '^==' "$work/decode.out") files, $(wc -l < "$work/decode.out") lines)"

if ! command -v gcc > /dev/null || ! printf '#include <webp/decode.h>\n#include <webp/demux.h>\n#include <webp/encode.h>\n#include <webp/mux.h>\nint main(void) { WebPConfig c; return !WebPConfigInit(&c) || WebPGetDemuxVersion() == 0 || WebPGetMuxVersion() == 0; }\n' | gcc -x c - -o "$work/probe" -lwebpmux -lwebpdemux -lwebp > /dev/null 2>&1; then
    echo "skip the comparisons with the system's libwebp: no gcc with libwebp's headers and libraries"
else
    gcc -O1 -w "$tests/layout.c" -o "$work/layout-ref" || fail "tests/layout.c does not build against the system's headers"
    cc "$tests/layout.c" -o "$work/layout" || fail "tests/layout.c does not build"
    diff -u <("$work/layout-ref") <(run "$work/layout") || fail "the structures are not laid out as the system's headers lay them out"
    echo "ok   the structures laid out as the system's headers lay them out"
    gcc -O1 -w "$tests/decode.c" -o "$work/decode-ref" -lwebpdemux -lwebp || fail "tests/decode.c does not build against the system's libwebp"
    if ! python3 -c 'import PIL' 2> /dev/null; then
        echo "skip the larger corpus: no Python with Pillow"
    else
        gcc -O1 -w "$tests/corpus.c" -o "$work/corpus-make" -lwebpmux -lwebp || fail "tests/corpus.c does not build against the system's libwebp"
        python3 "$tests/sources.py" "$work/sources" compiler/capi/jpeg/tests/images/progressive420.jpg || fail "tests/sources.py"
        mkdir -p "$work/corpus"
        for source in "$work"/sources/*.rgba; do
            name=$(basename "$source" .rgba)
            size=${name##*_}
            "$work/corpus-make" "$source" "${size%x*}" "${size#*x}" "$work/corpus/${name%_*}" || fail "tests/corpus.c on $name"
        done
        python3 - "$work/corpus" "$work/damaged" <<'PYTHON'
import os, random, sys
source, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
files = sorted(f for f in os.listdir(source) if f.endswith(".webp"))
rnd = random.Random(1234)
for n in range(200):
    name = rnd.choice(files)
    data = bytearray(open(os.path.join(source, name), "rb").read())
    kind = rnd.randint(0, 2)
    if kind == 0:
        data = data[:rnd.randint(12, len(data) - 1)]
    for i in range(rnd.randint(1, 4) if kind else 0):
        p = rnd.randint(12, len(data) - 1)
        if kind == 1:
            data[p] = rnd.randint(0, 255)
        else:
            data[p] ^= 1 << rnd.randint(0, 7)
    open(os.path.join(out, "z%03d_%s" % (n, name)), "wb").write(data)
PYTHON
        count=$(ls "$work/corpus" "$work/damaged" | grep -c '\.webp$')
        for directory in corpus damaged; do
            decodes "$work/decode-ref" "$work/$directory" > "$work/theirs"
            run decodes "$work/decode" "$work/$directory" > "$work/ours"
            diff -u "$work/theirs" "$work/ours" > "$work/diff" || { head -40 "$work/diff"; fail "decode.c differs from the system's libwebp on tests/corpus.c's $directory files"; }
        done
        echo "ok   decode.c on $count more files, good and damaged: the same as the system's libwebp"
    fi
fi

echo "check_webp.sh: all checks passed"
