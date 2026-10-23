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
# https: the same server serves the same over TLS 1.3 (std.tls.server) on a
# second port, with a certificate signed by a test root it makes at start
# (tools/http-test-server/certs.mlx: std.asn1, Ed25519) and writes as PEM;
# webcore-dump trusts that root through MLXWEB_CA_FILE. Pages whose path
# is written https:/PATH come that way (wss: for their WebSockets); then,
# without the root, the page must not come.
#
# Internal schemes (the port's MlxSchemeHandler, Voyager's voyager:):
# pages whose name starts with "scheme" run with webcore-dump --scheme test,
# which serves test://page and lists what the scheme was asked. An http
# page (site/scheme.html) must reach it in no way (fetch, XMLHttpRequest,
# an image, a script, a style sheet, an iframe, a redirect, a link, a form,
# location: none asked); the scheme's own page (a path written test:...,
# loaded as it is) loads and fetches its own URLs, a 404 among them.
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
cleanup() {
    [[ -n "$server_pid" ]] && kill -- -"$server_pid" 2> /dev/null
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
setsid "$work/http-test-server" "$repo_root/$network/site" "$work/ca.pem" > "$work/port" 2> "$work/server.log" &
server_pid=$!
for _ in $(seq 1 50); do [[ $(wc -l < "$work/port") -ge 2 ]] && break; sleep 0.1; done
port=$(sed -n 1p "$work/port")
tls_port=$(sed -n 2p "$work/port")
[[ -n "$port" && -n "$tls_port" && -s "$work/ca.pem" ]] || fail "the test server did not start: $(cat "$work/server.log")"
echo "ok   the test server listens on 127.0.0.1:$port (http) and :$tls_port (https)"

dump() {
    local url=$1
    shift
    env -i HOME="$work/home" FONTCONFIG_FILE="$work/fonts.conf" ${trusted:+MLXWEB_CA_FILE=$work/ca.pem} timeout 120 "$program" --timeout 60 --text "$@" "$url" 2> "$work/err.txt"
}

failed=0
passed=0
trusted=1
while read -r name path wait; do
    case $name in "#"*|"") continue ;; esac
    if [[ ${#chosen[@]} -gt 0 ]] && [[ ! " ${chosen[*]} " == *" $name "* ]]; then continue; fi
    url="http://127.0.0.1:$port$path"
    [[ $path == https:* ]] && url="https://127.0.0.1:$tls_port${path#https:}"
    [[ $path == test:* ]] && url=$path
    scheme=()
    [[ $name == scheme* ]] && scheme=(--scheme test)
    dump "$url" "${scheme[@]}" ${wait:+--wait-title "$wait"} | sed -e "s/127\.0\.0\.1:\($port\|$tls_port\)/127.0.0.1:PORT/g" -e 's/WebKitFormBoundary[A-Za-z0-9]*/WebKitFormBoundary/g' -e 's/[[:space:]]*$//' > "$work/$name.txt"
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

# Without the test root the https server's chain does not verify: no page.
if [[ ${#chosen[@]} -eq 0 ]]; then
    trusted=
    dump "https://127.0.0.1:$tls_port/secure.html" > "$work/untrusted.txt"
    if grep -q "^title secure" "$work/untrusted.txt"; then
        failed=$(( failed + 1 ))
        echo "FAIL untrusted (https): a chain of an unknown root was taken"
    else
        passed=$(( passed + 1 ))
        echo "PASS untrusted (https: a chain of an unknown root is refused)"
    fi
fi

echo "check_webkit_mlx_network.sh: $passed of $(( passed + failed )) pass"
[[ $failed -eq 0 ]] || fail "$failed failed"
echo "check_webkit_mlx_network.sh: all checks passed"
