#!/usr/bin/env bash
# mlx-zlib, zlib's C API (compiler/libc/zlib, over std.flate and std.crc):
#
#   - `./mlx build zlib` builds the library (libmlxz.so) and the headers:
#     mlx-cheaders makes mlx-out/include/mlx-zlib/zlib.h and zconf.h of
#     the API's Mlx modules (compiler/libc/zlib/api);
#   - every function the headers declare is in the library;
#   - the headers in C and C++;
#   - C programs built by mlx-cc against the library print what
#     NAME.expected says (what the same program prints built by the
#     system's gcc against the system's zlib, 1.3, with the exit status
#     last):
#       tests/checks.c    Adler-32, CRC-32, their combinations, zError,
#                         the bounds, the structures' layout
#       tests/deflate.c   every level, strategy, window, memory level and
#                         flush, pieces of every size, parameters changed
#                         on the way, dictionaries, gzip headers, errors:
#                         the streams' sizes and CRCs, so zlib's bytes
#       tests/inflate.c   streams of every kind and in pieces, detection,
#                         headers, dictionaries, sync, marks, copies, each
#                         of zlib's errors and many damaged streams with
#                         its codes and messages, uncompress, inflateBack
#       tests/gzfile.c    the gz functions, writing and reading
#   - round trips with the system's zlib, where there are gcc and zlib's
#     headers and library: tests/interop.c built by gcc against the
#     system's zlib and by mlx-cc against mlx-zlib; each inflates what the
#     other deflated (zlib, gzip and raw streams, gzip files, compress),
#     as interop.expected says; and how many of mlx-zlib's streams are
#     byte for byte the system's;
#   - ANGLE's sources that use zlib (MemoryProgramCache.cpp, Program.cpp,
#     angletypes.cpp, compression_utils_portable.cc) compile with mlx-cc
#     on these headers (their steps of ./mlx build webkit-angle), where
#     the WebKit source is (WEBKIT_SOURCE, by default where
#     projects/webkit looks).
#
#   tools/check_zlib.sh
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d /tmp/mlx-zlib.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }
headers=mlx-out/include/mlx-zlib
library=mlx-out/lib/libmlxz.so
tests=compiler/libc/zlib/tests

./mlx build -j 2 cc zlib > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "./mlx build zlib"; }
echo "ok   ./mlx build zlib"

# Every `pub extern("c") fn` of the API modules is defined by the library.
grep -ho 'pub extern("c") fn [A-Za-z0-9_]*' compiler/libc/zlib/api/*.mlx | awk '{print $4}' | sort -u > "$work/declared"
nm -D --defined-only "$library" | awk '{print $NF}' | sort -u > "$work/defined"
comm -23 "$work/declared" "$work/defined" > "$work/missing"
[ ! -s "$work/missing" ] || { cat "$work/missing"; fail "functions the headers declare are not in the library"; }
echo "ok   the library defines the $(wc -l < "$work/declared") functions the headers declare"

cc() { mlx-out/bin/mlx-cc -I"$headers" "$@"; }
run() { LD_LIBRARY_PATH="$repo_root/mlx-out/lib" "$@"; }

printf '#include <zlib.h>\n#include <zconf.h>\nint main(void) { z_stream s; return deflateInit(&s, 6) == Z_OK ? 0 : 1; }\n' > "$work/all.c"
cc -c "$work/all.c" -o "$work/all.o" || fail "the headers do not compile as C"
cc -x c++ -std=c++20 -c "$work/all.c" -o "$work/all-cpp.o" || fail "the headers do not compile as C++"
echo "ok   the headers in C and C++"

mkdir -p "$work/run"
for test in checks deflate inflate gzfile; do
    cc "$tests/$test.c" -o "$work/$test" "$library" || fail "tests/$test.c does not build"
    status=0
    (cd "$work/run" && run "$work/$test") > "$work/$test.out" 2> "$work/$test.err" || status=$?
    echo "exit $status" >> "$work/$test.out"
    diff -u "$tests/$test.expected" "$work/$test.out" || { cat "$work/$test.err"; fail "tests/$test.c prints otherwise"; }
    echo "ok   tests/$test.c"
done

cc "$tests/interop.c" -o "$work/interop" "$library" || fail "tests/interop.c does not build"
if ! command -v gcc > /dev/null || ! printf '#include <zlib.h>\nint main(void) { return zlibVersion() == 0; }\n' | gcc -x c - -o "$work/probe" -lz > /dev/null 2>&1; then
    echo "skip the round trips with the system's zlib: no gcc with zlib's headers and library"
else
    gcc -O1 -w "$tests/interop.c" -o "$work/reference" -lz || fail "tests/interop.c does not build against the system's zlib"
    mkdir -p "$work/theirs" "$work/ours"
    "$work/reference" pack "$work/theirs" || fail "the reference does not write its streams"
    run "$work/interop" unpack "$work/theirs" > "$work/unpacked-theirs.out" || fail "mlx-zlib does not read the system's streams"
    diff -u "$tests/interop.expected" "$work/unpacked-theirs.out" || fail "the system zlib's streams inflate otherwise with mlx-zlib"
    echo "ok   the system zlib's streams and gzip files, read by mlx-zlib"
    run "$work/interop" pack "$work/ours" || fail "mlx-zlib does not write its streams"
    "$work/reference" unpack "$work/ours" > "$work/unpacked-ours.out" || fail "the system's zlib does not read mlx-zlib's streams"
    diff -u "$tests/interop.expected" "$work/unpacked-ours.out" || fail "mlx-zlib's streams inflate otherwise with the system's zlib"
    echo "ok   mlx-zlib's streams and gzip files, read by the system's zlib"
    total=0
    same=0
    for theirs in "$work/theirs"/*; do
        total=$((total + 1))
        if cmp -s "$theirs" "$work/ours/$(basename "$theirs")"; then same=$((same + 1)); fi
    done
    echo "ok   $same of $total streams are byte for byte the system zlib's"
fi

webkit=${WEBKIT_SOURCE:-projects/webkit/upstream}
if [ ! -d "$webkit/Source/ThirdParty/ANGLE" ]; then
    echo "skip ANGLE's sources that use zlib: no WebKit source at $webkit (WEBKIT_SOURCE)"
else
    angle=(angle/src/libANGLE/MemoryProgramCache.cpp angle/src/libANGLE/Program.cpp angle/src/libANGLE/angletypes.cpp angle/third_party/zlib/google/compression_utils_portable.cc)
    ./mlx build -j 2 "${angle[@]}" > "$work/angle.log" 2>&1 || { tail -20 "$work/angle.log"; fail "ANGLE's sources that use zlib do not compile"; }
    for unit in "${angle[@]}"; do echo "ok   $unit"; done
fi

echo "check_zlib.sh: all checks passed"
