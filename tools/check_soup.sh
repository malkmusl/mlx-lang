#!/usr/bin/env bash
# Checks mlx-soup, libsoup 3's C API in Mlx (compiler/libc/soup):
#   - ./mlx build soup builds the library and its headers
#   - every function the headers declare is defined by the library
#   - the headers (<libsoup/soup.h>) compile together as C and as C++
#   - each test program, built by mlx-cc against the library, prints what
#     the same program prints built by gcc against the system's libsoup
#     3.4 (tests/NAME.expected, the exit status last):
#       tests/headers.c    SoupMessageHeaders, header parsing and lists,
#                          parameters, ranges, encodings
#       tests/misc.c       dates, forms, URI helpers, status phrases,
#                          message bodies, cookies' syntax
#       tests/session.c    SoupSession against a server in the test:
#                          bodies plain, chunked, compressed, sniffed;
#                          redirects, Basic and Digest, keep-alive,
#                          synchronous and asynchronous; SoupLogger
#       tests/cookies.c    the cookie jars and the Public Suffix List
#       tests/hsts.c       HTTP Strict Transport Security
#       tests/multipart.c  multipart bodies whole and as streams
#       tests/websocket.c  the WebSocket handshake and connections to an
#                          echo server in the test, permessage-deflate
#       tests/server.c     SoupServer: the bytes on the wire, handlers, ranges,
#                          auth domains, a WebSocket upgrade
#       tests/https.c      requests over TLS 1.3 and 1.2 to openssl
#                          s_servers (skipped without openssl)
#
# SOUP_COMPARE_SYSTEM=1 also builds each test with gcc against the
# system's libsoup (pkg-config libsoup-3.0) and compares what it prints
# with the expected output, to check the expected files themselves.
#
#   tools/check_soup.sh
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d /tmp/mlx-soup.XXXXXX)
servers=()
cleanup() {
    for pid in "${servers[@]}"; do kill "$pid" 2> /dev/null || true; done
    rm -rf -- "$work"
}
trap cleanup EXIT
fail() { echo "FAIL $1" >&2; exit 1; }
headers=mlx-out/include/mlx-soup
library=mlx-out/lib/libmlxsoup-3.0.so

./mlx build cc soup > "$work/build.log" 2>&1 || { cat "$work/build.log"; fail "./mlx build soup"; }
echo "ok   ./mlx build soup"

# Every `pub extern("c") fn` of the API modules is defined by the library.
grep -ho 'pub extern("c") fn [A-Za-z0-9_]*' compiler/libc/soup/api/*.mlx | awk '{print $4}' | sort -u > "$work/declared"
nm -D --defined-only "$library" | awk '{print $NF}' | sort -u > "$work/defined"
comm -23 "$work/declared" "$work/defined" > "$work/missing"
[ ! -s "$work/missing" ] || { cat "$work/missing"; fail "functions the headers declare are not in the library"; }
echo "ok   the library defines the $(wc -l < "$work/declared") functions the headers declare"

cc() { mlx-out/bin/mlx-cc -I"$headers" -Imlx-out/include/mlx-glib "$@"; }
run() { LD_LIBRARY_PATH="$repo_root/mlx-out/lib" "$@"; }

printf '#include <libsoup/soup.h>\nint main(void) { return 0; }\n' > "$work/all.c"
cc -c "$work/all.c" -o "$work/all.o" || fail "the headers together do not compile as C"
cc -x c++ -std=c++20 -c "$work/all.c" -o "$work/all-cpp.o" || fail "the headers together do not compile as C++"
echo "ok   the headers together in C and C++"

compare_system=0
if [ "${SOUP_COMPARE_SYSTEM:-0}" = 1 ]; then
    if command -v gcc > /dev/null && pkg-config --exists libsoup-3.0 2> /dev/null; then
        compare_system=1
    else
        echo "skip the system's libsoup: no gcc or no libsoup-3.0"
    fi
fi

# NAME [ARGUMENT...]: builds tests/NAME.c, runs it (from a directory of
# its own, at most two minutes), compares its output.
check() {
    local name=$1
    shift
    local status=0
    cc "compiler/libc/soup/tests/$name.c" -o "$work/$name" "$library" mlx-out/lib/libmlxglib.so || fail "tests/$name.c does not build"
    mkdir -p "$work/run-$name"
    (cd "$work/run-$name" && run timeout 120 "$work/$name" "$@") > "$work/$name.out" 2> "$work/$name.err" || status=$?
    echo "exit $status" >> "$work/$name.out"
    diff -u "compiler/libc/soup/tests/$name.expected" "$work/$name.out" || { cat "$work/$name.err"; fail "tests/$name.c prints otherwise"; }
    if [ "$compare_system" = 1 ]; then
        status=0
        # shellcheck disable=SC2046
        gcc -O1 -w "compiler/libc/soup/tests/$name.c" -o "$work/$name-system" $(pkg-config --cflags --libs libsoup-3.0) || fail "tests/$name.c does not build against the system's libsoup"
        (cd "$work/run-$name" && timeout 120 "$work/$name-system" "$@") > "$work/$name-system.out" 2>&1 || status=$?
        echo "exit $status" >> "$work/$name-system.out"
        diff -u "compiler/libc/soup/tests/$name.expected" "$work/$name-system.out" || fail "the system's libsoup prints otherwise for tests/$name.c"
    fi
    echo "ok   tests/$name.c"
}

for test in headers misc session cookies hsts multipart websocket server; do
    check "$test"
done

if ! command -v openssl > /dev/null; then
    echo "skip tests/https.c: no openssl"
else
    tools/tls-test/make_certs.sh "$work/certs"
    port=$((20000 + RANDOM % 30000))
    (cd "$work/certs" && exec openssl s_server -accept "$port" -cert p256.pem -key p256.key -cert_chain intermediate.pem -www -serverpref -num_tickets 0) > "$work/s13.log" 2>&1 &
    servers+=($!)
    (cd "$work/certs" && exec openssl s_server -accept "$((port + 1))" -tls1_2 -cert rsa.pem -key rsa.key -cert_chain intermediate.pem -www -serverpref -no_ticket) > "$work/s12.log" 2>&1 &
    servers+=($!)
    tries=0
    while ! grep -q ACCEPT "$work/s13.log" 2> /dev/null || ! grep -q ACCEPT "$work/s12.log" 2> /dev/null; do
        tries=$((tries + 1))
        [ "$tries" -lt 100 ] || fail "s_server did not start"
        sleep 0.05
    done
    check https "$port" "$((port + 1))" "$work/certs"
fi

echo "check_soup.sh: all checks passed"
