#!/usr/bin/env bash
# mlx-capture --record (projects/desktop/capture/record.mlx) end to end: a
# nested compositor under tools/wayland-test-host (the pointer moves, so
# frames change), MLXIPC as the session bus and MLX Audio started by it,
# its input a 880 Hz tone (the "microphone") while mlx-audio plays 440 Hz
# (the desktop's sound). tools/check_mkv.py reads the Matroska file it
# writes: the structure, the frames (JPEGs PIL decodes, at the screen's
# size), and each sound track's frequencies:
#
#   - separate tracks: the desktop's has 440 Hz and not 880, the
#     microphone's 880 and not 440;
#   - --mix: one track with both;
#   - --audio none: video only.
#
#   tools/check_capture_record.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
command -v xkbcli > /dev/null || { echo "check_capture_record.sh: xkbcli is not installed" >&2; exit 2; }
python3 -c 'import PIL' 2> /dev/null || { echo "check_capture_record.sh: needs Python with PIL" >&2; exit 2; }

work=$(mktemp -d)
# A short runtime directory: Unix socket paths are limited.
export XDG_RUNTIME_DIR=$(mktemp -d /tmp/record.XXXXXX)
export XDG_CONFIG_HOME="$work/config"
# Which apps asked for which permission (MLXIPC keeps it there).
export XDG_STATE_HOME="$work/state"
mkdir -p "$XDG_CONFIG_HOME/mlx" "$work/services"
bus_pid=
cleanup() {
    [[ -n "$bus_pid" ]] && kill "$bus_pid" 2> /dev/null || true
    pkill -P $$ 2> /dev/null || true
    [[ -n "${KEEP:-}" ]] && cp -r "$work" "$KEEP"; rm -rf -- "$work" "$XDG_RUNTIME_DIR"
}
trap cleanup EXIT

for program in ipc/main:mlx-ipcd audio/main:mlx-audiod audio/tool:mlx-audio compositor/main:mlx-compositor capture/main:mlx-capture; do
    "$compiler" --quiet "projects/desktop/${program%%:*}.mlx" -o "$work/${program##*:}"
done
"$compiler" --quiet tools/wayland-test-host/main.mlx -o "$work/test-host"
xkbcli compile-keymap --layout us > "$work/us.xkb"

cat > "$work/services/org.mlx.Audio.service" <<SERVICE
[D-BUS Service]
Name=org.mlx.Audio
Exec=$work/mlx-audiod --verbose --output null --input sine:880
SERVICE
"$work/mlx-ipcd" --verbose --services "$work/services" > "$work/bus.log" 2>&1 &
bus_pid=$!
for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/bus" ]] && break; sleep 0.1; done
export DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"

fail() {
    echo "FAIL $1" >&2
    for log in bus record compositor host; do
        [[ -f "$work/$log.log" ]] && { echo "--- $log log" >&2; tail -30 "$work/$log.log" >&2; }
    done
    exit 1
}

# record NAME SECONDS ARGUMENTS...: a recording while the pointer moves and
# a 440 Hz tone plays.
record() {
    local name=$1 seconds=$2
    shift 2
    {
        echo "wait 1500"
        for step in $(seq 1 40); do
            echo "pointer $((100 + step * 10)) $((100 + step * 5))"
            echo "wait 100"
        done
        echo "wait 1500"
        echo "close"
    } > "$work/$name.script"
    cat > "$work/$name.sh" <<SCRIPT
#!/bin/sh
"$work/mlx-audio" tone 440 $((seconds + 2)) > /dev/null 2>&1 &
sleep 0.3
exec "$work/mlx-capture" --record "$work/$name.mkv" --duration $seconds $* > "$work/record.log" 2>&1
SCRIPT
    chmod +x "$work/$name.sh"
    "$work/test-host" "host-$name" "$work/us.xkb" "$work/$name.script" > "$work/host.log" 2>&1 &
    local host=$!
    for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/host-$name" ]] && break; sleep 0.1; done
    WAYLAND_DISPLAY="host-$name" timeout 60 "$work/mlx-compositor" --renderer cpu --size 640x400 --no-fps --socket "nested-$name" \
        --dock none --topbar none --launcher none --xwayland none --run "$work/$name.sh" > "$work/compositor.log" 2>&1 || true
    wait "$host" || true
    grep -q "^saved $work/$name.mkv" "$work/record.log" || fail "$name: mlx-capture did not save the recording"
}

record separate 3
python3 tools/check_mkv.py "$work/separate.mkv" --frames 5 --duration 3000 \
    --track Desktop:440=16000,880=0 --track Microphone:880=16384,440=0 > "$work/check.log" || { cat "$work/check.log" "$work/record.log"; fail "separate tracks"; }
cat "$work/check.log"
echo "ok   a recording: frames when the screen changed, the desktop's sound and the microphone in tracks of their own"

record mixed 2 --mix
python3 tools/check_mkv.py "$work/mixed.mkv" --frames 3 --duration 2000 \
    --track "Desktop and microphone:440=16000,880=16384" > "$work/check.log" || { cat "$work/check.log" "$work/record.log"; fail "mixed"; }
echo "ok   --mix: the desktop's sound and the microphone in one track"

record silent 2 --audio none
python3 tools/check_mkv.py "$work/silent.mkv" --frames 3 --duration 2000 > "$work/check.log" || { cat "$work/check.log" "$work/record.log"; fail "video only"; }
grep -q "A_PCM" "$work/check.log" && fail "--audio none still recorded sound"
echo "ok   --audio none: the video alone"
echo "all mlx-capture recording checks passed"
