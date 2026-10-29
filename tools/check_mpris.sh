#!/usr/bin/env bash
# The port's MPRIS player on MLXIPC end to end: mlx-ipcd as the session
# bus, tests/support/mpris_program.mlx (projects/webkit/port/mlx/mediasession.mlx,
# what Voyager runs) owning org.mpris.MediaPlayer2.voyager in MLXIPC's
# own format, and D-Bus tools as the desktop's media controls: dbus-send
# reads the properties (GetAll, Get) and sends PlayPause, Next and
# SetPosition, gdbus introspects, and dbus-monitor sees the
# PropertiesChanged and Seeked signals the player sends.
#
#   tools/check_mpris.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
for tool in dbus-send gdbus dbus-monitor; do
    if ! command -v "$tool" > /dev/null; then
        echo "skip tools/check_mpris.sh (no $tool)"
        exit 0
    fi
done

work=$(mktemp -d)
bus_pid=
player_pid=
monitor_pid=
cleanup() {
    [[ -n "$player_pid" ]] && kill "$player_pid" 2> /dev/null || true
    [[ -n "$monitor_pid" ]] && kill "$monitor_pid" 2> /dev/null || true
    [[ -n "$bus_pid" ]] && kill "$bus_pid" 2> /dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
export XDG_RUNTIME_DIR="$work/runtime"
export XDG_CONFIG_HOME="$work/config"
export XDG_STATE_HOME="$work/state"
mkdir -m 700 "$XDG_RUNTIME_DIR"
mkdir -p "$XDG_CONFIG_HOME/mlx"
: > "$XDG_CONFIG_HOME/mlx/ipc.conf"

"$compiler" --quiet projects/desktop/ipc/main.mlx -o "$work/mlx-ipcd"
"$compiler" --quiet tests/support/mpris_program.mlx -o "$work/player"

fail() {
    echo "FAIL $1" >&2
    [[ -f "$work/bus.log" ]] && { echo "--- bus log" >&2; cat "$work/bus.log" >&2; }
    [[ -f "$work/player.log" ]] && { echo "--- player" >&2; cat "$work/player.log" >&2; }
    [[ -f "$work/monitor.log" ]] && { echo "--- monitor" >&2; cat "$work/monitor.log" >&2; }
    exit 1
}

"$work/mlx-ipcd" --verbose > "$work/bus.log" 2>&1 &
bus_pid=$!
for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/bus" ]] && break; sleep 0.1; done
export DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"

dbus-monitor "type='signal',path='/org/mpris/MediaPlayer2'" > "$work/monitor.log" 2>&1 &
monitor_pid=$!
sleep 0.3
"$work/player" > "$work/player.log" 2>&1 &
player_pid=$!
for _ in $(seq 1 50); do grep -q "^ready" "$work/player.log" 2> /dev/null && break; sleep 0.1; done
grep -q "^ready" "$work/player.log" || fail "the player did not take its name"

name=org.mpris.MediaPlayer2.voyager
path=/org/mpris/MediaPlayer2
all=$(dbus-send --session --print-reply --dest=$name $path org.freedesktop.DBus.Properties.GetAll string:org.mpris.MediaPlayer2.Player)
[[ "$all" == *'string "Playing"'* ]] || fail "PlaybackStatus is not Playing: $all"
[[ "$all" == *'string "Ein Lied"'* && "$all" == *'string "Jemand"'* && "$all" == *'string "Das Album"'* ]] || fail "the metadata lacks the track: $all"
[[ "$all" == *'int64 180000000'* && "$all" == *'int64 12000000'* ]] || fail "the length or position is off: $all"
[[ "$all" == *'object path "/org/mlx/voyager/track/2"'* ]] || fail "no track id: $all"
echo "ok   GetAll: the track, its length and position, Playing"
art=$(printf '%s' "$all" | grep -A1 '"mpris:artUrl"' | grep -o 'string "file://[^"]*"' | sed 's/^string "file:\/\///; s/"$//')
[[ -n "$art" ]] || fail "no file:// mpris:artUrl in the metadata: $all"
[[ "$art" == */voyager-cover-$player_pid-1.png ]] || fail "the cover is not named for the player: $art"
[[ -f "$art" && "$(head -c 4 "$art")" == $'\x89PNG' ]] || fail "the cover $art is no PNG file"
echo "ok   the metadata's cover is a PNG file of the player's: $art"
identity=$(dbus-send --session --print-reply --dest=$name $path org.freedesktop.DBus.Properties.Get string:org.mpris.MediaPlayer2 string:Identity)
[[ "$identity" == *'string "Voyager"'* ]] || fail "Identity: $identity"
root=$(dbus-send --session --print-reply --dest=$name $path org.freedesktop.DBus.Properties.GetAll string:org.mpris.MediaPlayer2)
[[ "$root" == *'string "DesktopEntry"'* && "$root" == *'string "org.mlx.voyager"'* ]] || fail "the root properties: $root"
echo "ok   Identity and the root properties"
introspection=$(gdbus introspect --session --dest $name --object-path $path)
[[ "$introspection" == *"org.mpris.MediaPlayer2.Player"* && "$introspection" == *"SetPosition"* ]] || fail "introspection: $introspection"
echo "ok   gdbus introspects both interfaces"
if dbus-send --session --print-reply --dest=$name $path org.freedesktop.DBus.Properties.Get string:org.mpris.MediaPlayer2.Player string:NoSuchProperty > "$work/error" 2>&1; then
    fail "a property that is not there was answered"
fi
grep -q "InvalidArgs" "$work/error" || fail "the wrong error for a property that is not there: $(cat "$work/error")"
echo "ok   a property that is not there is refused"

dbus-send --session --print-reply --dest=$name $path org.mpris.MediaPlayer2.Player.PlayPause > /dev/null
sleep 0.3
status=$(dbus-send --session --print-reply --dest=$name $path org.freedesktop.DBus.Properties.Get string:org.mpris.MediaPlayer2.Player string:PlaybackStatus)
[[ "$status" == *'string "Paused"'* ]] || fail "PlayPause did not pause: $status"
grep -q "^command 3 0" "$work/player.log" || fail "PlayPause did not reach the player"
echo "ok   PlayPause pauses (the player got the command, the property changed)"
dbus-send --session --print-reply --dest=$name $path org.mpris.MediaPlayer2.Player.Next > /dev/null
dbus-send --session --print-reply --dest=$name $path org.mpris.MediaPlayer2.Player.SetPosition objpath:/org/mlx/voyager/track/2 int64:65000000 > /dev/null
for _ in $(seq 1 50); do kill -0 "$player_pid" 2> /dev/null || break; sleep 0.1; done
set +e
wait "$player_pid"
player_status=$?
set -e
player_pid=
[[ $player_status -eq 13 ]] || fail "the player ended with $player_status (wanted 13 after SetPosition)"
grep -q "^command 5 0" "$work/player.log" || fail "Next did not reach the player"
grep -q "^command 7 65000000" "$work/player.log" || fail "SetPosition did not reach the player with its time"
echo "ok   Next and SetPosition reach the player with the position"
[[ ! -e "$art" ]] || fail "the cover $art outlived the player"
echo "ok   the cover went with the player"
sleep 0.3
grep -q "PropertiesChanged" "$work/monitor.log" || fail "no PropertiesChanged signal was seen"
grep -q "Seeked" "$work/monitor.log" || fail "no Seeked signal was seen"
grep -q 'string "Paused"' "$work/monitor.log" || fail "the PropertiesChanged signal did not carry Paused"
echo "ok   the monitor saw PropertiesChanged (with Paused) and Seeked"
echo "all MPRIS checks passed"
