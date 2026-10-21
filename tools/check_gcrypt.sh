#!/usr/bin/env bash
# mlx-gcrypt, libgcrypt's C API in Mlx (compiler/libc/gcrypt):
#
#   - `./mlx build gcrypt` builds the library (libmlxgcrypt.so) and its
#     headers: mlx-cheaders makes mlx-out/include/mlx-gcrypt/gcrypt.h and
#     gpg-error.h of the API's Mlx modules (compiler/libc/gcrypt/api);
#   - every function the headers declare is in the library;
#   - the headers compile as C and as C++;
#   - C programs built by mlx-cc against the library print what
#     NAME.expected says (what the system's libgcrypt 1.10.3 prints for
#     the same program, with the exit status last):
#       tests/symmetric.c  digests, HMAC, PBKDF2, AES in ECB, CBC, CTR,
#                          GCM and key wrap
#       tests/mpi.c        MPIs: every format, signed arithmetic, powm,
#                          gcd, invm, bits, opaque values
#       tests/sexp.c       S-expressions: reading, building, printing,
#                          walking, extract_param, canon_len
#       tests/pk.c         RSA (PKCS #1 v1.5, PSS, OAEP, keys), ECDSA and
#                          ECDH on P-256/384/521, EC contexts, X25519 and
#                          Ed25519, as WebCore's WebCrypto calls them
#   - with gcc and the system's libgcrypt installed, the same programs
#     built against libgcrypt print the same (the expected files are
#     still libgcrypt's answers).
#
#   tools/check_gcrypt.sh
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d /tmp/mlx-gcrypt.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }
headers=mlx-out/include/mlx-gcrypt
library=mlx-out/lib/libmlxgcrypt.so
tests=compiler/libc/gcrypt/tests

./mlx build -j 1 cc gcrypt > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "./mlx build gcrypt"; }
echo "ok   ./mlx build gcrypt"

# Every `pub extern("c") fn` of the API modules is defined by the library.
grep -ho 'pub extern("c") fn [A-Za-z0-9_]*' compiler/libc/gcrypt/api/*.mlx | awk '{print $4}' | sort -u > "$work/declared"
nm -D --defined-only "$library" | awk '{print $NF}' | sort -u > "$work/defined"
comm -23 "$work/declared" "$work/defined" > "$work/missing"
[ ! -s "$work/missing" ] || { cat "$work/missing"; fail "functions the headers declare are not in the library"; }
echo "ok   the library defines the $(wc -l < "$work/declared") functions the headers declare"

printf '#include <gcrypt.h>\n#include <gpg-error.h>\nint main(void) { return gcry_check_version(GCRYPT_VERSION) == 0; }\n' > "$work/all.c"
mlx-out/bin/mlx-cc -I"$headers" -c "$work/all.c" -o "$work/all.o" || fail "the headers do not compile as C"
mlx-out/bin/mlx-cc -I"$headers" -x c++ -std=c++20 -c "$work/all.c" -o "$work/all-cpp.o" || fail "the headers do not compile as C++"
echo "ok   the headers in C and C++"

system=0
if command -v gcc > /dev/null && printf '#include <gcrypt.h>\nint main(void){return 0;}\n' | gcc -x c - -o "$work/probe" -lgcrypt 2> /dev/null; then
    system=1
fi

for test in symmetric mpi sexp pk; do
    mlx-out/bin/mlx-cc -I"$headers" "$tests/$test.c" -o "$work/$test" "$library" || fail "tests/$test.c does not build"
    status=0
    LD_LIBRARY_PATH="$repo_root/mlx-out/lib" "$work/$test" > "$work/$test.out" 2> "$work/$test.err" || status=$?
    echo "exit $status" >> "$work/$test.out"
    if ! diff -u "$tests/$test.expected" "$work/$test.out" > "$work/$test.diff"; then
        head -40 "$work/$test.diff"
        fail "tests/$test.c does not print what $test.expected says"
    fi
    if [ "$system" = 1 ]; then
        gcc -o "$work/$test.system" "$tests/$test.c" -lgcrypt
        status=0
        "$work/$test.system" > "$work/$test.system.out" 2> /dev/null || status=$?
        echo "exit $status" >> "$work/$test.system.out"
        diff -u "$tests/$test.expected" "$work/$test.system.out" > "$work/$test.system.diff" || { head -40 "$work/$test.system.diff"; fail "the system's libgcrypt no longer prints $test.expected"; }
        echo "ok   tests/$test.c (and the system's libgcrypt prints the same)"
    else
        echo "ok   tests/$test.c"
    fi
done
echo "check_gcrypt.sh: all checks passed"
