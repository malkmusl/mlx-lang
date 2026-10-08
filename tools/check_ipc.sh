#!/usr/bin/env bash
# MLXIPC (projects/desktop/ipc) end to end: mlx-ipcd as the session bus for
# real D-Bus programs (dbus-send, gdbus, busctl, dbus-monitor) and for
# mlx-ipc (std.dbus's client), its permissions per app, activation and
# direct channels.
#
#   - the bus answers org.freedesktop.DBus (ListNames, GetNameOwner,
#     credentials, Introspect, ListActivatableNames) to dbus-send, gdbus and
#     busctl, and dbus-monitor sees the traffic (BecomeMonitor);
#   - `mlx-ipc serve` owns a name; dbus-send, gdbus and mlx-ipc call it, and
#     `mlx-ipc connect` gets a direct channel to it (org.mlx.IPC.Connect);
#   - a call to an unowned name starts its service from a .service file
#     (activation) and is answered once the service owns it;
#   - the policy (ipc.conf, read again when it changes): deny rules,
#     groups, and owning;
#   - a sandboxed app (bwrap with a .flatpak-info, as Flatpak runs apps)
#     may own only its own names, talk to the portals but not to others
#     until a rule allows it, and sees only what it may talk to;
#   - --run: the bus ends with its program and passes on its exit status.
#
#   tools/check_ipc.sh [compiler]
#
# Needs dbus-send and gdbus (busctl, dbus-monitor and bwrap are used when
# they are there).
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
for tool in dbus-send gdbus; do
    command -v "$tool" > /dev/null || { echo "check_ipc.sh: $tool is not installed" >&2; exit 2; }
done

work=$(mktemp -d)
bus_pid=
cleanup() {
    [[ -n "$bus_pid" ]] && kill "$bus_pid" 2> /dev/null || true
    pkill -P $$ 2> /dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
export XDG_RUNTIME_DIR="$work/runtime"
export XDG_CONFIG_HOME="$work/config"
# Which apps asked for which permission (MLXIPC keeps it there).
export XDG_STATE_HOME="$work/state"
mkdir -m 700 "$XDG_RUNTIME_DIR"
mkdir -p "$XDG_CONFIG_HOME/mlx" "$work/services"

"$compiler" --quiet projects/desktop/ipc/main.mlx -o "$work/mlx-ipcd"
"$compiler" --quiet projects/desktop/ipc/tool.mlx -o "$work/mlx-ipc"
ipc="$work/mlx-ipc"

fail() {
    echo "FAIL $1" >&2
    echo "--- bus log" >&2
    cat "$work/bus.log" >&2
    exit 1
}
expect() {
    # expect NAME WANTED COMMAND...: the command's output contains WANTED.
    local name=$1 wanted=$2
    shift 2
    local output
    output=$(timeout 10 "$@" 2>&1 || true)
    [[ "$output" == *"$wanted"* ]] || fail "$name: wanted \"$wanted\", got: $output"
    echo "ok   $name"
}

# Activation: a service file whose Exec serves the name.
cat > "$work/services/org.example.Started.service" <<SERVICE
[D-BUS Service]
Name=org.example.Started
Exec=$ipc serve org.example.Started
SERVICE

"$work/mlx-ipcd" --verbose --services "$work/services" > "$work/bus.log" 2>&1 &
bus_pid=$!
for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/bus" ]] && break; sleep 0.1; done
[[ -S "$XDG_RUNTIME_DIR/bus" ]] || fail "the bus does not listen"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"

# D-Bus programs on the bus.
expect "dbus-send ListNames" 'string "org.freedesktop.DBus"' dbus-send --session --print-reply --dest=org.freedesktop.DBus / org.freedesktop.DBus.ListNames
expect "gdbus GetNameOwner" "('org.freedesktop.DBus',)" gdbus call --session --dest org.freedesktop.DBus --object-path /org/freedesktop/DBus --method org.freedesktop.DBus.GetNameOwner org.freedesktop.DBus
expect "gdbus introspects the bus" "BecomeMonitor" gdbus introspect --session --dest org.freedesktop.DBus --object-path /org/freedesktop/DBus
expect "credentials" '"ProcessID"' dbus-send --session --print-reply --dest=org.freedesktop.DBus / org.freedesktop.DBus.GetConnectionCredentials string:org.freedesktop.DBus
expect "activatable names" 'string "org.example.Started"' dbus-send --session --print-reply --dest=org.freedesktop.DBus / org.freedesktop.DBus.ListActivatableNames
if command -v busctl > /dev/null; then
    expect "busctl list" "org.example.Started" busctl --user list
fi
if command -v dbus-monitor > /dev/null; then
    timeout 6 dbus-monitor --session > "$work/monitor.txt" 2>&1 &
    sleep 0.5
fi

# A service of ours, called by everyone.
"$ipc" serve org.example.Echo > "$work/serve.log" 2>&1 &
for _ in $(seq 1 50); do grep -q serving "$work/serve.log" 2> /dev/null && break; sleep 0.1; done
expect "mlx-ipc call" "hallo welt" "$ipc" call org.example.Echo "hallo welt"
expect "dbus-send calls it" 'string "von dbus-send"' dbus-send --session --print-reply --dest=org.example.Echo / org.mlx.Echo.Echo "string:von dbus-send"
expect "gdbus calls it" "('von gdbus',)" gdbus call --session --dest org.example.Echo --object-path / --method org.mlx.Echo.Echo "von gdbus"
expect "a direct channel" "hello from org.example.Echo" "$ipc" connect org.example.Echo
expect "an unknown name" "ServiceUnknown" "$ipc" call org.example.Nobody x
expect "apps" "mlx-ipc" "$ipc" apps
expect "activation" "gestartet" "$ipc" call org.example.Started gestartet
grep -q "starting org.example.Started" "$work/bus.log" || fail "the service was not started by the bus"
if command -v dbus-monitor > /dev/null; then
    sleep 0.3
    grep -q "member=Echo" "$work/monitor.txt" || fail "dbus-monitor saw no Echo: $(cat "$work/monitor.txt")"
    echo "ok   dbus-monitor sees the bus"
fi

# The policy, read again as it changes.
cat > "$XDG_CONFIG_HOME/mlx/ipc.conf" <<'CONF'
# tests
group tools gdbus
deny mlx-ipc talk org.example.Echo
deny @tools talk org.example.*
deny dbus-send own org.example.*
allow dbus-send own org.example.Owned
CONF
sleep 0.3
expect "deny by app" "AccessDenied" "$ipc" call org.example.Echo x
expect "deny by group" "AccessDenied" gdbus call --session --dest org.example.Echo --object-path / --method org.mlx.Echo.Echo x
expect "others still may" 'string "ok"' dbus-send --session --print-reply --dest=org.example.Echo / org.mlx.Echo.Echo string:ok
expect "own denied" "AccessDenied" dbus-send --session --print-reply --dest=org.freedesktop.DBus / org.freedesktop.DBus.RequestName string:org.example.Other uint32:0
expect "own allowed (the later rule)" "uint32 1" dbus-send --session --print-reply --dest=org.freedesktop.DBus / org.freedesktop.DBus.RequestName string:org.example.Owned uint32:0
: > "$XDG_CONFIG_HOME/mlx/ipc.conf"
sleep 0.3
expect "the rules are gone again" "x" "$ipc" call org.example.Echo x

# A sandboxed app.
if command -v bwrap > /dev/null && bwrap --bind / / true 2> /dev/null; then
    printf '[Application]\nname=org.example.Sandboxed\n' > "$work/flatpak-info"
    sandboxed=(bwrap --bind / / --ro-bind "$work/flatpak-info" /.flatpak-info)
    "$ipc" serve org.freedesktop.portal.Test > "$work/portal.log" 2>&1 &
    for _ in $(seq 1 50); do grep -q serving "$work/portal.log" 2> /dev/null && break; sleep 0.1; done
    expect "sandboxed: who it is" "org.example.Sandboxed  sandboxed" "${sandboxed[@]}" "$ipc" apps
    expect "sandboxed: no talking to others" "AccessDenied" "${sandboxed[@]}" "$ipc" call org.example.Echo x
    expect "sandboxed: the portals" "portal" "${sandboxed[@]}" "$ipc" call org.freedesktop.portal.Test portal
    expect "sandboxed: no channel to others" "NameHasNoOwner" "${sandboxed[@]}" "$ipc" connect org.example.Echo
    expect "sandboxed: owns its own names" "serving org.example.Sandboxed.Player" timeout 2 "${sandboxed[@]}" "$ipc" serve org.example.Sandboxed.Player
    expect "sandboxed: no other names" "AccessDenied" "${sandboxed[@]}" "$ipc" serve org.example.Taken
    names=$(timeout 5 "${sandboxed[@]}" "$ipc" names)
    [[ "$names" != *org.example.Echo* && "$names" == *org.freedesktop.portal.Test* ]] || fail "sandboxed: it sees $names"
    echo "ok   sandboxed: sees only what it may talk to"
    echo "allow org.example.Sandboxed talk,see org.example.Echo" > "$XDG_CONFIG_HOME/mlx/ipc.conf"
    sleep 0.3
    expect "sandboxed: a rule allows it" "erlaubt" "${sandboxed[@]}" "$ipc" call org.example.Echo erlaubt
    expect "sandboxed: and a channel" "hello from org.example.Echo" "${sandboxed[@]}" "$ipc" connect org.example.Echo
else
    echo "skip the sandboxed app (no working bwrap)"
fi

kill "$bus_pid"
wait "$bus_pid" 2> /dev/null || true
bus_pid=
[[ ! -e "$XDG_RUNTIME_DIR/bus" ]] || fail "the socket stayed"
echo "ok   the bus ends on SIGTERM and removes its socket"

# --run: a session on its own bus.
set +e
"$work/mlx-ipcd" --address "$work/run.sock" --run sh -c 'case "$DBUS_SESSION_BUS_ADDRESS" in *run.sock) "$0" names | grep -q org.freedesktop.DBus && exit 7;; esac; exit 1' "$ipc"
status=$?
set -e
[[ $status -eq 7 ]] || fail "--run: exit status $status (wanted 7)"
echo "ok   --run: the program on the bus, its exit status"
echo "all MLXIPC checks passed"
