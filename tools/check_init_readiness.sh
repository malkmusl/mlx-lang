#!/usr/bin/env bash
# Checks that `ready = socket:PATH` means a successful connection, not merely
# that something exists at PATH.
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}

work=$(mktemp -d)
init_pid=
cleanup() {
    [[ -n "$init_pid" ]] && kill "$init_pid" 2>/dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
fail() { echo "FAIL $1" >&2; [[ -f "${2:-}" ]] && cat "$2" >&2; exit 1; }

"$compiler" --quiet projects/init/main.mlx -o "$work/mlx-init"
"$compiler" --quiet projects/init/tool.mlx -o "$work/mlx-initctl"
"$compiler" --quiet tools/init-readiness-test/server.mlx -o "$work/ready-server"
mkdir -p "$work/services" "$work/run" "$work/log"
cat > "$work/services/listener.service" <<SERVICE
description = becomes ready by accepting Unix connections
exec = $work/ready-server $work/run/service.sock
ready = socket:$work/run/service.sock
ready-timeout = 5s
SERVICE
cat > "$work/services/dependent.service" <<SERVICE
description = waits for the listening socket
after = listener
exec = /bin/sh -c "touch $work/run/dependent.started; exec sleep 1000"
SERVICE
"$work/mlx-init" --services "$work/services" --control "$work/run/init" --log "$work/log" --verbose 2>"$work/init.err" &
init_pid=$!

tries=0
while [[ ! -S "$work/run/init" ]]; do
    tries=$((tries + 1))
    [[ $tries -lt 50 ]] || fail "mlx-init did not open its control socket" "$work/init.err"
    sleep 0.1
done
ctl() { "$work/mlx-initctl" --control "$work/run/init" "$@"; }

tries=0
while [[ ! -e "$work/run/service.sock" ]]; do
    tries=$((tries + 1))
    [[ $tries -lt 30 ]] || fail "the readiness fixture did not create its placeholder" "$work/init.err"
    sleep 0.05
done
[[ -S "$work/run/service.sock" ]] || fail "the fixture did not leave its stale socket inode"
[[ $(ctl status | awk '$1 == "listener" {print $2}') == starting ]] || fail "a stale socket inode incorrectly made the service ready" "$work/init.err"
[[ $(ctl status | awk '$1 == "dependent" {print $2}') == waiting ]] || fail "the dependent started before a socket accepted connections" "$work/init.err"

tries=0
while [[ $(ctl status | awk '$1 == "dependent" {print $2}') != running ]]; do
    tries=$((tries + 1))
    [[ $tries -lt 50 ]] || fail "the listening socket did not release the dependent" "$work/init.err"
    sleep 0.1
done
tries=0
while [[ ! -f "$work/run/dependent.started" ]]; do
    tries=$((tries + 1))
    [[ $tries -lt 20 ]] || fail "the dependent did not execute" "$work/init.err"
    sleep 0.05
done
grep -q '^mlx-init: listener: ready$' "$work/init.err" || fail "the socket readiness transition was not reported" "$work/init.err"

kill -TERM "$init_pid"
wait "$init_pid"
init_pid=
echo "ok   socket readiness requires a successful connection before dependents start"
