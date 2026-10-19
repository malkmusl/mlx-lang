#!/usr/bin/env bash
# std.tls against OpenSSL's s_server on this machine (no network beyond
# 127.0.0.1): tools/tls-test/client.mlx connects to a server per case,
# with the certificates tools/tls-test/make_certs.sh makes (an RSA root,
# a P-256 intermediate, RSA, P-256, P-384 and Ed25519 servers), and
# prints what was negotiated, the chain's flags and the server's page;
# each case checks those lines, and the page s_server writes (which says
# what OpenSSL saw: "New, TLSv1.3, Cipher is ...").
#
#   - TLS 1.3 with each suite, each kind of server key (RSA-PSS, ECDSA
#     P-256 and P-384, Ed25519), X25519 and, through a HelloRetryRequest,
#     P-256 and P-384; the client's secrets compared with the server's key
#     log (NSS format);
#   - TLS 1.2 with ECDHE-ECDSA and ECDHE-RSA, AES-GCM and ChaCha20, on
#     X25519, P-256 and P-384, the extended master secret compared with
#     the server's;
#   - ALPN, a server asking for a client certificate, a megabyte file in
#     many records;
#   - chains that do not verify: another name (BAD_IDENTITY, 2), an
#     unknown authority (UNKNOWN_CA, 1), a time after the certificate's
#     (EXPIRED, 8), refused unless accepted.
#
# Skips (exit 0) when openssl is not installed.
#
#   tools/check_tls.sh
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
if ! command -v openssl > /dev/null; then
    echo "skip std.tls against s_server: no openssl"
    exit 0
fi
work=$(mktemp -d "$repo_root/mlx-out/check-tls.XXXXXX")
server_pid=
cleanup() {
    if [[ -n "$server_pid" ]]; then kill "$server_pid" 2> /dev/null || true; fi
    rm -rf -- "$work"
}
trap cleanup EXIT
failures=0

compiler=$(tools/ensure_compiler.sh)
"$compiler" --quiet tools/tls-test/client.mlx -o "$work/client" || { echo "FAIL tools/tls-test/client.mlx does not compile"; exit 1; }
tools/tls-test/make_certs.sh "$work/certs"
certs=$work/certs
head -c 1048576 /dev/urandom > "$certs/big.bin"
big_sum=$(sha256sum < "$certs/big.bin" | cut -d' ' -f1)
port=$((20000 + RANDOM % 30000))

# Starts s_server for one connection with the arguments given, in the
# certificates' directory (for -WWW's files), logging to $work/server.log.
serve() {
    port=$((port + 1))
    (cd "$certs" && exec openssl s_server -accept "$port" -naccept 1 -keylogfile "$work/server.keylog" "$@") > "$work/server.log" 2>&1 &
    server_pid=$!
    local tries=0
    while ! grep -q ACCEPT "$work/server.log" 2> /dev/null; do
        tries=$((tries + 1))
        if [[ $tries -gt 100 ]]; then echo "FAIL s_server did not start: $*"; cat "$work/server.log"; exit 1; fi
        sleep 0.05
    done
}

finish_server() {
    wait "$server_pid" 2> /dev/null || true
    server_pid=
}

# check NAME "EXPECTED LINES" -- SERVER ARGUMENTS -- CLIENT ARGUMENTS
# The expected lines must each be among the client's output.
check() {
    local name=$1 expected=$2
    shift 2
    [[ $1 == -- ]] && shift
    local server_args=()
    while [[ $1 != -- ]]; do server_args+=("$1"); shift; done
    shift
    rm -f "$work/server.keylog" "$work/client.keylog"
    serve "${server_args[@]}"
    local status=0
    timeout 60 "$work/client" "$port" "$@" --keylog "$work/client.keylog" > "$work/out" 2> "$work/err" || status=$?
    finish_server
    local missing=0
    while IFS= read -r line; do
        [[ -z "$line" ]] && continue
        if ! grep -qxF -- "$line" "$work/out"; then
            echo "FAIL $name: no line \"$line\""
            missing=1
        fi
    done <<< "$expected"
    if [[ $missing -ne 0 ]]; then
        echo "--- client (exit $status):"; head -20 "$work/out"; cat "$work/err"
        echo "--- server:"; head -20 "$work/server.log"
        failures=$((failures + 1))
        return
    fi
    echo "ok   $name"
}

# The client's secrets are the server's: every line of the client's key
# log is in the server's.
same_secrets() {
    local name=$1
    if [[ -s "$work/client.keylog" ]] && ! grep -vxF -f "$work/server.keylog" "$work/client.keylog" > /dev/null; then
        echo "ok   $name: the $(wc -l < "$work/client.keylog") secrets are OpenSSL's"
        return
    fi
    echo "FAIL $name: the secrets differ from OpenSSL's"
    cat "$work/client.keylog" "$work/server.keylog"
    failures=$((failures + 1))
}

common=(-cert_chain intermediate.pem -www)
ca=(localhost "$certs/ca.pem")

check "TLS 1.3, ECDSA P-256 server, X25519, AES-128-GCM" "version: TLSv1.3
cipher: TLS_AES_128_GCM_SHA256
group: x25519
flags: 0
New, TLSv1.3, Cipher is TLS_AES_128_GCM_SHA256" -- -cert p256.pem -key p256.key "${common[@]}" -- "${ca[@]}"
same_secrets "TLS 1.3 key schedule"

check "TLS 1.3, RSA-PSS server, AES-256-GCM" "version: TLSv1.3
cipher: TLS_AES_256_GCM_SHA384
New, TLSv1.3, Cipher is TLS_AES_256_GCM_SHA384" -- -cert rsa.pem -key rsa.key -ciphersuites TLS_AES_256_GCM_SHA384 "${common[@]}" -- "${ca[@]}"
same_secrets "TLS 1.3 key schedule with SHA-384"

check "TLS 1.3, ECDSA P-384 server, ChaCha20-Poly1305" "version: TLSv1.3
cipher: TLS_CHACHA20_POLY1305_SHA256
New, TLSv1.3, Cipher is TLS_CHACHA20_POLY1305_SHA256" -- -cert p384.pem -key p384.key -ciphersuites TLS_CHACHA20_POLY1305_SHA256 "${common[@]}" -- "${ca[@]}"

check "TLS 1.3, Ed25519 server" "version: TLSv1.3
flags: 0
New, TLSv1.3, Cipher is TLS_AES_128_GCM_SHA256" -- -cert ed25519.pem -key ed25519.key "${common[@]}" -- "${ca[@]}"

check "TLS 1.3, HelloRetryRequest for P-256" "version: TLSv1.3
group: secp256r1
New, TLSv1.3, Cipher is TLS_AES_128_GCM_SHA256" -- -cert p256.pem -key p256.key -groups P-256 "${common[@]}" -- "${ca[@]}"
same_secrets "TLS 1.3 key schedule after a HelloRetryRequest"

check "TLS 1.3, HelloRetryRequest for P-384" "version: TLSv1.3
group: secp384r1" -- -cert rsa.pem -key rsa.key -groups P-384 "${common[@]}" -- "${ca[@]}"

check "TLS 1.2, ECDHE-ECDSA, ChaCha20-Poly1305" "version: TLSv1.2
cipher: TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256
New, TLSv1.2, Cipher is ECDHE-ECDSA-CHACHA20-POLY1305
    Extended master secret: yes" -- -tls1_2 -cert p256.pem -key p256.key -cipher ECDHE-ECDSA-CHACHA20-POLY1305 "${common[@]}" -- "${ca[@]}"
same_secrets "TLS 1.2 extended master secret"

check "TLS 1.2, ECDHE-RSA on P-384, AES-256-GCM" "version: TLSv1.2
cipher: TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
group: secp384r1
New, TLSv1.2, Cipher is ECDHE-RSA-AES256-GCM-SHA384" -- -tls1_2 -cert rsa.pem -key rsa.key -cipher ECDHE-RSA-AES256-GCM-SHA384 -groups P-384 "${common[@]}" -- "${ca[@]}"
same_secrets "TLS 1.2 master secret with SHA-384"

check "TLS 1.2, ECDHE-ECDSA on P-256, AES-128-GCM, a client offering TLS 1.2 only" "version: TLSv1.2
cipher: TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
group: secp256r1" -- -cert p384.pem -key p384.key -groups P-256 "${common[@]}" -- "${ca[@]}" --tls12

check "ALPN" "alpn: http/1.1" -- -cert p256.pem -key p256.key -alpn http/1.1 "${common[@]}" -- "${ca[@]}" --alpn h2,http/1.1

check "a server asking for a client certificate (TLS 1.3)" "version: TLSv1.3
no client certificate available" -- -verify 1 -cert p256.pem -key p256.key "${common[@]}" -- "${ca[@]}"

check "a server asking for a client certificate (TLS 1.2)" "version: TLSv1.2
no client certificate available" -- -tls1_2 -verify 1 -cert rsa.pem -key rsa.key "${common[@]}" -- "${ca[@]}"

check "a wildcard name" "flags: 0" -- -cert p256.pem -key p256.key "${common[@]}" -- www.mlx.test "$certs/ca.pem"
check "an address" "flags: 0" -- -cert p256.pem -key p256.key "${common[@]}" -- 127.0.0.1 "$certs/ca.pem"

check "another name, refused" "flags: 2
failed: bad certificate" -- -cert p256.pem -key p256.key "${common[@]}" -- other.test "$certs/ca.pem"
check "another name, accepted" "flags: 2
version: TLSv1.3" -- -cert p256.pem -key p256.key "${common[@]}" -- other.test "$certs/ca.pem" --accept
check "an unknown authority" "flags: 1
failed: bad certificate" -- -cert p256.pem -key p256.key "${common[@]}" -- localhost /etc/ssl/certs/ca-certificates.crt
check "an expired certificate" "flags: 8
failed: bad certificate" -- -cert p256.pem -key p256.key "${common[@]}" -- "${ca[@]}" --time 4102444800

# A megabyte in many records, with its digest compared.
serve -cert p384.pem -key p384.key -cert_chain intermediate.pem -WWW
start=$(date +%s.%N)
timeout 120 "$work/client" "$port" "${ca[@]}" --request 'GET /big.bin HTTP/1.0\r\n\r\n' > "$work/big.out" 2> "$work/err" || true
finish_server
elapsed=$(echo "$(date +%s.%N) - $start" | bc 2> /dev/null || echo "?")
got_sum=$(python3 -c '
import sys, hashlib
data = open(sys.argv[1], "rb").read()
body = data.split(b"\r\n\r\n", 1)[1] if b"\r\n\r\n" in data else b""
print(hashlib.sha256(body).hexdigest())' "$work/big.out")
if [[ "$got_sum" == "$big_sum" ]]; then
    echo "ok   a megabyte over TLS 1.3, its SHA-256 the file's (${elapsed}s)"
else
    echo "FAIL a megabyte over TLS 1.3: the digest differs"
    head -c 600 "$work/big.out"; cat "$work/err"
    failures=$((failures + 1))
fi

if [[ $failures -ne 0 ]]; then
    echo "$failures failed"
    exit 1
fi
echo "ok   std.tls against openssl s_server"
