#!/usr/bin/env bash
# mlx-tasn1, libtasn1's C API in Mlx (compiler/capi/tasn1):
#
#   - `./mlx build tasn1` builds the library (libmlxtasn1.so) and its
#     header: mlx-cheaders makes mlx-out/include/mlx-tasn1/libtasn1.h of
#     the API's Mlx module (compiler/capi/tasn1/api);
#   - every function the header declares is in the library;
#   - the header compiles as C and as C++;
#   - tests/tasn1.c, built by mlx-cc against the library, prints what
#     tasn1.expected says (what the system's libtasn1 4.19.0 prints for
#     the same program, with the exit status last): WebCore's WebCrypto
#     definitions made a tree, the DER of OpenSSL's EC, RSA, Ed25519 and
#     X25519 keys (SubjectPublicKeyInfo, PKCS #8, ECPrivateKey,
#     RSAPrivateKey) decoded and read as WebCore reads them, malformed
#     DER, the structures WebCore writes encoded, broken definitions;
#   - with gcc and the system's libtasn1 installed, the same program
#     built against libtasn1 prints the same.
#
#   tools/check_tasn1.sh
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d /tmp/mlx-tasn1.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }
headers=mlx-out/include/mlx-tasn1
library=mlx-out/lib/libmlxtasn1.so
tests=compiler/capi/tasn1/tests

./mlx build -j 1 cc tasn1 > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "./mlx build tasn1"; }
echo "ok   ./mlx build tasn1"

# Every `pub extern("c") fn` of the API module is defined by the library.
grep -ho 'pub extern("c") fn [A-Za-z0-9_]*' compiler/capi/tasn1/api/*.mlx | awk '{print $4}' | sort -u > "$work/declared"
nm -D --defined-only "$library" | awk '{print $NF}' | sort -u > "$work/defined"
comm -23 "$work/declared" "$work/defined" > "$work/missing"
[ ! -s "$work/missing" ] || { cat "$work/missing"; fail "functions the header declares are not in the library"; }
echo "ok   the library defines the $(wc -l < "$work/declared") functions the header declares"

printf '#include <libtasn1.h>\nint main(void) { return asn1_check_version(ASN1_VERSION) == 0; }\n' > "$work/all.c"
mlx-out/bin/mlx-cc -I"$headers" -c "$work/all.c" -o "$work/all.o" || fail "the header does not compile as C"
mlx-out/bin/mlx-cc -I"$headers" -x c++ -std=c++20 -c "$work/all.c" -o "$work/all-cpp.o" || fail "the header does not compile as C++"
echo "ok   the header in C and C++"

system=0
if command -v gcc > /dev/null && printf '#include <libtasn1.h>\nint main(void){return 0;}\n' | gcc -x c - -o "$work/probe" -ltasn1 2> /dev/null; then
    system=1
fi

for test in tasn1; do
    mlx-out/bin/mlx-cc -I"$headers" "$tests/$test.c" -o "$work/$test" "$library" || fail "tests/$test.c does not build"
    status=0
    LD_LIBRARY_PATH="$repo_root/mlx-out/lib" "$work/$test" > "$work/$test.out" 2> "$work/$test.err" || status=$?
    echo "exit $status" >> "$work/$test.out"
    if ! diff -u "$tests/$test.expected" "$work/$test.out" > "$work/$test.diff"; then
        head -40 "$work/$test.diff"
        fail "tests/$test.c does not print what $test.expected says"
    fi
    if [ "$system" = 1 ]; then
        gcc -o "$work/$test.system" "$tests/$test.c" -ltasn1
        status=0
        "$work/$test.system" > "$work/$test.system.out" 2> /dev/null || status=$?
        echo "exit $status" >> "$work/$test.system.out"
        diff -u "$tests/$test.expected" "$work/$test.system.out" > "$work/$test.system.diff" || { head -40 "$work/$test.system.diff"; fail "the system's libtasn1 no longer prints $test.expected"; }
        echo "ok   tests/$test.c (and the system's libtasn1 prints the same)"
    else
        echo "ok   tests/$test.c"
    fi
done
echo "check_tasn1.sh: all checks passed"
