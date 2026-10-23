#!/usr/bin/env bash
# The Mlx port of WebKit, M6 (projects/webkit/PORT.md): pages over http
# and https. tools/http-test-server (Mlx, std.net) serves
# projects/webkit/tests/network/site on 127.0.0.1 with paths of its own (a
# redirect chain, cookies set and echoed, a form's POST echoed, JSON, gzip
# and chunked responses, a WebSocket echo); webcore-dump --text loads each
# page of projects/webkit/tests/network/pages.txt over http and prints its
# URL, title and text, which the page's scripts write what they saw into
# (cookies, fetch and XMLHttpRequest results, the WebSocket's messages);
# the output, its port written PORT, must be expected/NAME.txt.
#
# https: openssl s_server (when openssl is installed) serves secure.html
# with a chain of tools/tls-test/make_certs.sh, whose root webcore-dump
# trusts through MLXWEB_CA_FILE; the page must come, and a server whose
# chain the root did not sign must not.
#
#   tools/check_webkit_mlx_network.sh [--no-build] [PAGE...]
set -uo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
network=projects/webkit/tests/network
fail() { echo "FAIL $1" >&2; exit 1; }
build=1
chosen=()
while [[ $# -gt 0 ]]; do
    case $1 in
        --no-build) build=0 ;;
        -*) fail "unknown argument $1" ;;
        *) chosen+=("$1") ;;
    esac
    shift
done

work=$(mktemp -d /tmp/mlx-webkit-network.XXXXXX)
server_pid=
tls_pid=
cleanup() {
    [[ -n "$server_pid" ]] && kill -- -"$server_pid" 2> /dev/null
    [[ -n "$tls_pid" ]] && kill "$tls_pid" 2> /dev/null
    [[ -n "${MLX_NETWORK_KEEP:-}" ]] && mkdir -p "$MLX_NETWORK_KEEP" && cp "$work"/*.txt "$MLX_NETWORK_KEEP"/ 2> /dev/null
    rm -rf -- "$work"
}
trap cleanup EXIT

if [[ $build -eq 1 ]]; then
    ./mlx build libc webkit-mlx-port webkit-mlx-webcore-dump > "$work/build.log" 2>&1 || { tail -20 "$work/build.log"; fail "./mlx build"; }
    echo "ok   ./mlx build libc webkit-mlx-port webkit-mlx-webcore-dump"
fi
program=$repo_root/mlx-out/webkit/mlx/bin/webcore-dump
[[ -x "$program" ]] || fail "$program is missing"
mlx-out/bin/compiler/mlx4 --quiet tools/http-test-server/main.mlx -o "$work/http-test-server" || fail "tools/http-test-server does not compile"

serif=$(find /usr/share/fonts /usr/local/share/fonts -name LiberationSerif-Regular.ttf 2> /dev/null | head -1)
[[ -n "$serif" ]] || fail "no Liberation fonts (LiberationSerif-Regular.ttf) in /usr/share/fonts"
cat > "$work/fonts.conf" << EOF
<?xml version="1.0"?>
<fontconfig>
    <dir>$(dirname "$serif")</dir>
    <include>$repo_root/projects/webkit/tests/layout/fonts.conf</include>
</fontconfig>
EOF
mkdir -p "$work/home"

# The server, in a process group of its own (its children serve the
# connections).
setsid "$work/http-test-server" "$repo_root/$network/site" > "$work/port" 2> "$work/server.log" &
server_pid=$!
for _ in $(seq 1 50); do [[ -s "$work/port" ]] && break; sleep 0.1; done
port=$(head -1 "$work/port")
[[ -n "$port" ]] || fail "the test server did not start: $(cat "$work/server.log")"
echo "ok   the test server listens on 127.0.0.1:$port"

dump() {
    local url=$1
    shift
    env -i HOME="$work/home" FONTCONFIG_FILE="$work/fonts.conf" ${MLXWEB_CA_FILE:+MLXWEB_CA_FILE=$MLXWEB_CA_FILE} timeout 120 "$program" --timeout 60 --text "$@" "$url" 2> "$work/err.txt"
}

failed=0
passed=0
while read -r name path wait; do
    case $name in "#"*|"") continue ;; esac
    if [[ ${#chosen[@]} -gt 0 ]] && [[ ! " ${chosen[*]} " == *" $name "* ]]; then continue; fi
    dump "http://127.0.0.1:$port$path" ${wait:+--wait-title "$wait"} | sed -e "s/127\.0\.0\.1:$port/127.0.0.1:PORT/g" -e 's/WebKitFormBoundary[A-Za-z0-9]*/WebKitFormBoundary/g' -e 's/[[:space:]]*$//' > "$work/$name.txt"
    if cmp -s "$work/$name.txt" "$network/expected/$name.txt"; then
        passed=$(( passed + 1 ))
        echo "PASS $name ($path)"
    else
        failed=$(( failed + 1 ))
        echo "FAIL $name ($path):"
        diff "$network/expected/$name.txt" "$work/$name.txt" | head -10 | sed 's/^/     /'
        grep -v '^  ' "$work/err.txt" | tail -2 | sed 's/^/     /'
    fi
done < "$network/pages.txt"

# https against openssl s_server, with a test CA.
if command -v openssl > /dev/null && [[ ${#chosen[@]} -eq 0 ]]; then
    tools/tls-test/make_certs.sh "$work/certs" > /dev/null 2>&1 || fail "tools/tls-test/make_certs.sh"
    mkdir -p "$work/www"
    cp "$network/site/secure.html" "$work/www/"
    tls_port=$(( 20000 + RANDOM % 20000 ))
    (cd "$work/www" && exec openssl s_server -quiet -accept "$tls_port" -cert "$work/certs/p256.pem" -key "$work/certs/p256.key" -cert_chain "$work/certs/intermediate.pem" -WWW) > "$work/tls.log" 2>&1 &
    tls_pid=$!
    sleep 1
    MLXWEB_CA_FILE=$work/certs/ca.pem dump "https://127.0.0.1:$tls_port/secure.html" | sed -e "s/127\.0\.0\.1:$tls_port/127.0.0.1:PORT/g" -e 's/[[:space:]]*$//' > "$work/secure.txt"
    if cmp -s "$work/secure.txt" "$network/expected/secure.txt"; then
        passed=$(( passed + 1 ))
        echo "PASS secure (https, the test CA trusted)"
    else
        failed=$(( failed + 1 ))
        echo "FAIL secure (https, the test CA trusted):"
        diff "$network/expected/secure.txt" "$work/secure.txt" | head -10 | sed 's/^/     /'
        grep -v '^  ' "$work/err.txt" | tail -2 | sed 's/^/     /'
    fi
    # Without the test CA the chain does not verify: no page.
    dump "https://127.0.0.1:$tls_port/secure.html" > "$work/untrusted.txt"
    if grep -q "^title secure" "$work/untrusted.txt"; then
        failed=$(( failed + 1 ))
        echo "FAIL untrusted (https): a chain of an unknown root was taken"
    else
        passed=$(( passed + 1 ))
        echo "PASS untrusted (https: a chain of an unknown root is refused)"
    fi
else
    echo "skip https: no openssl"
fi

echo "check_webkit_mlx_network.sh: $passed of $(( passed + failed )) pass"
[[ $failed -eq 0 ]] || fail "$failed failed"
echo "check_webkit_mlx_network.sh: all checks passed"
