#!/usr/bin/env bash
# MLX Capture's client library (projects/desktop/libpipewire, built by
# tools/build_libpipewire.sh) as the stand-in for PipeWire's libpipewire:
#
#   - it exports what libpipewire 1.0.5 does (every function and the two
#     data symbols, compared with PipeWire's when that is installed),
#     under its soname, needing libc alone;
#   - its functions on values (versions and names, state strings,
#     properties, object infos, a thread loop) return what PipeWire's
#     return (tools/libpipewire_values.py, both driven the same);
#   - screen sharing end to end: the ScreenCast portal (mlx-capture
#     --portal on MLXIPC under a nested compositor) and GStreamer's
#     pipewiresrc taking the stream with THIS library (LD_LIBRARY_PATH):
#     the screen's frames at the screen's size, following the pointer;
#   - the Debian package (dpkg-deb): its file, and that it provides and
#     replaces libpipewire-0.3-0 (libpipewire-0.3-0t64).
#
# tools/check_screencast.sh runs the same sharing with the system's
# libpipewire, and with Firefox.
#
#   tools/check_libpipewire.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
command -v xkbcli > /dev/null || { echo "check_libpipewire.sh: xkbcli is not installed" >&2; exit 2; }
command -v gst-launch-1.0 > /dev/null && gst-inspect-1.0 pipewiresrc > /dev/null 2>&1 || { echo "check_libpipewire.sh: needs GStreamer with pipewiresrc (gstreamer1.0-tools, gstreamer1.0-pipewire)" >&2; exit 2; }

work=$(mktemp -d /tmp/mlx-libpipewire.XXXXXX)
# A short runtime directory: Unix socket paths are limited.
export XDG_RUNTIME_DIR=$(mktemp -d /tmp/cast.XXXXXX)
export XDG_CONFIG_HOME="$work/config" XDG_STATE_HOME="$work/state"
mkdir -p "$XDG_CONFIG_HOME/mlx" "$work/services" "$work/screen"
bus_pid=
cleanup() {
    [[ -n "$bus_pid" ]] && kill "$bus_pid" 2> /dev/null || true
    pkill -P $$ 2> /dev/null || true
    [[ -n "${KEEP:-}" ]] && cp -r "$work" "$KEEP"; rm -rf -- "$work" "$XDG_RUNTIME_DIR"
}
trap cleanup EXIT

fail() {
    echo "FAIL $1" >&2
    for log in bus share compositor host; do
        [[ -f "$work/$log.log" ]] && { echo "--- $log log" >&2; grep -v '^ *[0-9]*:' "$work/$log.log" | tail -30 >&2; }
    done
    exit 1
}

deb=()
command -v dpkg-deb > /dev/null && deb=(--deb)
tools/build_libpipewire.sh --compiler "$compiler" --out "$work/lib" "${deb[@]}" > /dev/null
lib="$work/lib/libpipewire-0.3.so.0"
system=$(ldconfig -p 2> /dev/null | awk '/libpipewire-0.3.so.0 \(/ {print $NF; exit}' || true)
[[ -n "$system" && -f "$system" ]] || system=""

# Exports, soname, what it needs.
if command -v readelf > /dev/null; then
    dynamic=$(readelf -d "$lib")
    [[ "$dynamic" == *"Library soname: [libpipewire-0.3.so.0]"* ]] || fail "no soname libpipewire-0.3.so.0"
    needed=$(grep -o 'Shared library: \[[^]]*\]' <<< "$dynamic" | tr '\n' ' ')
    [[ "$needed" == "Shared library: [libc.so.6] " ]] || fail "needs more than libc: $needed"
fi
defined=$(nm -D --defined-only "$lib")
[[ "$defined" == *" D pw_log_level"* && "$defined" == *" D PW_LOG_TOPIC_DEFAULT"* ]] || fail "pw_log_level and PW_LOG_TOPIC_DEFAULT are not data symbols"
if [[ -n "$system" ]]; then
    ours=$(awk '{print $3}' <<< "$defined" | grep '^pw_\|^PW_' | sort)
    theirs=$(nm -D --defined-only "$system" | awk '{print $3}' | grep '^pw_\|^PW_' | sort)
    [[ "$ours" == "$theirs" ]] || fail "exports otherwise than PipeWire's: $(diff <(echo "$ours") <(echo "$theirs") | head -5)"
    echo "ok   libpipewire-0.3.so.0: PipeWire's $(echo "$theirs" | wc -l) symbols, soname, libc alone"
else
    echo "ok   libpipewire-0.3.so.0: $(awk '{print $3}' <<< "$defined" | grep -c '^pw_') pw_ symbols, soname, libc alone (no PipeWire library here to compare with)"
fi

# Values.
if [[ -n "$system" ]]; then
    result=$(python3 tools/libpipewire_values.py "$lib" "$system" 2>&1) || fail "values: $result"
    echo "ok   values: $(tail -1 <<< "$result") as PipeWire's"
else
    echo "skip values (no PipeWire library here to compare with)"
fi

# Screen sharing through this library (tools/check_screencast.sh's first
# case, with LD_LIBRARY_PATH).
for program in ipc/main:mlx-ipcd compositor/main:mlx-compositor capture/main:mlx-capture; do
    "$compiler" --quiet "projects/desktop/${program%%:*}.mlx" -o "$work/${program##*:}"
done
"$compiler" --quiet tools/wayland-test-host/main.mlx -o "$work/test-host"
xkbcli compile-keymap --layout us > "$work/us.xkb"
cat > "$work/services/org.freedesktop.portal.Desktop.service" <<SERVICE
[D-BUS Service]
Name=org.freedesktop.portal.Desktop
Exec=$work/mlx-capture --portal --verbose --choose screen
SERVICE
"$work/mlx-ipcd" --verbose --services "$work/services" > "$work/bus.log" 2>&1 &
bus_pid=$!
for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/bus" ]] && break; sleep 0.1; done
[[ -S "$XDG_RUNTIME_DIR/bus" ]] || fail "MLXIPC did not come up"
# The pointer moving for six seconds, after 1.5.
{
    printf 'wait 1500\n'
    for step in $(seq 1 60); do printf 'pointer %s %s\nwait 100\n' $((100 + step * 5)) $((100 + step * 3)); done
    printf 'wait 1000\nclose\n'
} > "$work/screen.script"
cat > "$work/screen.sh" <<INNER
#!/bin/sh
export DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"
sleep 0.5
LD_LIBRARY_PATH=$work/lib LD_DEBUG=libs timeout 60 $work/mlx-capture --share -- gst-launch-1.0 pipewiresrc fd=3 path=%n num-buffers=6 ! videoconvert ! pngenc ! multifilesink location=$work/screen/frame%02d.png > $work/share.log 2>&1
echo \$? > $work/share.status
INNER
chmod +x "$work/screen.sh"
"$work/test-host" host-screen "$work/us.xkb" "$work/screen.script" > "$work/host.log" 2>&1 &
host=$!
for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/host-screen" ]] && break; sleep 0.1; done
WAYLAND_DISPLAY=host-screen timeout 120 "$work/mlx-compositor" --renderer cpu --size 640x400 --no-fps --socket wayland-mlx \
    --dock none --topbar none --launcher none --xwayland none --run "$work/screen.sh" > "$work/compositor.log" 2>&1 || true
wait "$host" || true
[[ "$(cat "$work/share.status" 2> /dev/null)" == 0 ]] || fail "the pipeline did not end well"
grep -q "calling init: $work/lib/libpipewire-0.3.so.0" "$work/share.log" || fail "pipewiresrc did not load this library"
! grep -q "calling init: .*/usr/lib.*libpipewire-0.3.so.0\|calling init: /lib.*libpipewire-0.3.so.0" "$work/share.log" || fail "pipewiresrc loaded the system's libpipewire too"
# Six PNGs of 640x400 (the IHDR's width and height), most of them differing.
count=$(ls "$work/screen"/frame*.png 2> /dev/null | wc -l)
[[ $count -eq 6 ]] || fail "$count frames came, not 6"
for png in "$work/screen"/frame*.png; do
    size=$(od -An -tu1 -j16 -N8 "$png" | awk '{print ($1*16777216+$2*65536+$3*256+$4) "x" ($5*16777216+$6*65536+$7*256+$8)}')
    [[ "$size" == 640x400 ]] || fail "$(basename "$png") is $size, not 640x400"
done
distinct=$(md5sum "$work/screen"/frame*.png | awk '{print $1}' | sort -u | wc -l)
[[ $distinct -ge 4 ]] || fail "the frames do not follow the pointer ($distinct distinct of 6)"
grep -q "capture: sharing the screen as node" "$work/bus.log" && grep -q "runs with" "$work/bus.log" || fail "the portal's log does not show the stream"
echo "ok   screen sharing: pipewiresrc on this library took 6 frames of 640x400 following the pointer ($distinct distinct)"

# The package.
if [[ ${#deb[@]} -gt 0 ]]; then
    package=$(ls "$work"/lib/mlx-capture-libs_*.deb)
    files=$(dpkg-deb -c "$package" | awk '{print $6}')
    [[ "$files" == *"/libpipewire-0.3.so.0"* ]] || fail "the package lacks the library: $files"
    control=$(dpkg-deb -f "$package")
    for field in "Provides: libpipewire-0.3-0 (" "libpipewire-0.3-0t64 (" "Conflicts: libpipewire-0.3-0, libpipewire-0.3-0t64" "Replaces: libpipewire-0.3-0, libpipewire-0.3-0t64"; do
        [[ "$control" == *"$field"* ]] || fail "the package's control lacks $field"
    done
    echo "ok   the package $(basename "$package"): the library, provides and replaces libpipewire-0.3-0 (t64)"
else
    echo "skip the package (no dpkg-deb)"
fi
echo "all libpipewire checks passed"
