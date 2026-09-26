#!/usr/bin/env bash
# End-to-end check of examples/wayland-compositor (nested compositor) and
# examples/wayland-terminal (terminal client) with real keyboard and
# pointer input.
#
# tools/wayland-test-host plays the "session compositor": it hosts the
# nested compositor's window, gives it a seat with an xkb keymap, and plays
# input scripts:
#
#   1. click the Mlx terminal and type a command (keyboard -> shell);
#   2. Alt+Enter opens a second terminal (compositor shortcut);
#   3. Alt+drag moves it (compositor-driven move), then type into it;
#   4. drag a window by the compositor's title bar (the title is drawn with
#      std.truetype when DejaVu Sans is installed);
#   5. with weston-terminal (if installed): type with Shift through the
#      forwarded xkb keymap, drag it by its title bar (xdg_toplevel.move)
#      and open its right-click popup menu.
#
# Typed commands create marker files, so keyboard delivery is verified at the
# shell; the moved window's focus frame is checked in the host's screenshot.
# Needs xkbcli (libxkbcommon-tools). Usage:
#
#   tools/check_wayland_compositor.sh [compiler] [cpu|vulkan]
#
# The second argument picks the compositor's renderer (default cpu; vulkan
# needs a Vulkan driver, e.g. VK_DRIVER_FILES naming lavapipe's manifest).
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
renderer=${2:-cpu}
command -v xkbcli > /dev/null || { echo "check_wayland_compositor.sh: xkbcli is not installed" >&2; exit 2; }

work=$(mktemp -d)
export XDG_RUNTIME_DIR="$work/runtime"
mkdir -m 700 "$XDG_RUNTIME_DIR"
trap 'rm -rf -- "$work"' EXIT

"$compiler" --quiet examples/wayland-compositor/main.mlx -o "$work/mlx-compositor"
"$compiler" --quiet examples/wayland-terminal/main.mlx -o "$work/mlx-terminal"
"$compiler" --quiet tools/wayland-test-host/main.mlx -o "$work/test-host"
xkbcli compile-keymap --layout us > "$work/us.xkb"

run_scenario() {
    local name=$1 script=$2
    shift 2
    "$work/test-host" "host-$name" "$work/us.xkb" "$script" > "$work/$name-host.log" 2>&1 &
    local host_pid=$!
    for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/host-$name" ]] && break; sleep 0.1; done
    WAYLAND_DISPLAY="host-$name" timeout 60 "$work/mlx-compositor" --verbose --renderer "$renderer" --socket "nested-$name" "$@" > "$work/$name-compositor.log" 2>&1
    wait "$host_pid" || { echo "test host failed ($name):" >&2; cat "$work/$name-host.log" >&2; exit 1; }
}

# Scenario 1: Mlx terminals.
cat > "$work/terminal.script" <<SCRIPT
wait 1500
pointer 200 200
press 272
release 272
wait 300
type echo mlx-typed > $work/first.marker
enter
wait 800
down 56
down 28
up 28
up 56
wait 2000
pointer 300 300
down 56
press 272
pointer 360 340
pointer 420 380
release 272
up 56
wait 500
type echo moved > $work/second.marker
enter
wait 1000
shot $work/terminals.ppm
close
SCRIPT
run_scenario terminal "$work/terminal.script" --terminal "$work/mlx-terminal" --run "$work/mlx-terminal"
[[ "$(cat "$work/first.marker" 2> /dev/null)" == "mlx-typed" ]] || { echo "keyboard input did not reach the first terminal's shell" >&2; cat "$work/terminal-compositor.log" >&2; exit 1; }
echo "ok   keys typed on the host reached the shell in the Mlx terminal"
[[ $(grep -c "^map: Mlx Terminal" "$work/terminal-compositor.log") -eq 2 ]] || { echo "Alt+Enter did not open a second terminal" >&2; exit 1; }
echo "ok   Alt+Enter launched a second terminal"
grep -q "^move: Mlx Terminal" "$work/terminal-compositor.log" || { echo "Alt+drag did not start a move" >&2; exit 1; }
[[ "$(cat "$work/second.marker" 2> /dev/null)" == "moved" ]] || { echo "keyboard input did not reach the second terminal" >&2; exit 1; }
# The second window starts at (64, 56); dragging by (+120, +80) puts its
# focus frame's left edge at x = 182..183, from y = 136 down.
python3 - "$work/terminals.ppm" <<'PY'
import sys
data = open(sys.argv[1], 'rb').read()
header, rest = data.split(b'\n', 1)
size, rest = rest.split(b'\n', 1)
_, pixels = rest.split(b'\n', 1)
width, height = map(int, size.split())
def rgb(x, y):
    offset = (y * width + x) * 3
    return tuple(pixels[offset:offset + 3])
focus = (0x5a, 0xa0, 0xff)
assert rgb(182, 300) == focus and rgb(183, 400) == focus, (rgb(182, 300), rgb(183, 400))
assert rgb(62, 300) != focus, "the window did not leave its original position"
PY
echo "ok   Alt+drag moved the focused window by the pointer's travel"

# Scenario 2: the compositor's own title bars (std.truetype titles, when
# the default font is installed): dragging one moves its window.
cat > "$work/title.script" <<SCRIPT
wait 1500
pointer 120 12
press 272
pointer 170 62
pointer 220 112
release 272
wait 800
shot $work/title.ppm
close
SCRIPT
run_scenario title "$work/title.script" --terminal "$work/mlx-terminal" --run "$work/mlx-terminal"
grep -q "^move: Mlx Terminal" "$work/title-compositor.log" || { echo "dragging the title bar did not move the window" >&2; cat "$work/title-compositor.log" >&2; exit 1; }
# The window starts at (24, 24) and moves by (+100, +100): its frame's left
# edge is at x = 122, its title bar spans y = 102..121.
font=0
[[ -f /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf ]] && font=1
python3 - "$work/title.ppm" "$font" <<'PY'
import sys
data = open(sys.argv[1], 'rb').read()
_, size, _, pixels = data.split(b'\n', 3)
width, height = map(int, size.split())
def rgb(x, y):
    offset = (y * width + x) * 3
    return tuple(pixels[offset:offset + 3])
focus = (0x5a, 0xa0, 0xff)
assert rgb(122, 300) == focus and rgb(122, 110) == focus, (rgb(122, 300), rgb(122, 110))
assert rgb(22, 300) != focus, "the window did not leave its original position"
if sys.argv[2] == "1":
    # The title in white over the bar (the bar's red is 0x5a).
    light = sum(1 for y in range(102, 122) for x in range(124, 320) if rgb(x, y)[0] > 180)
    assert light > 50, "no title text in the title bar (%d light pixels)" % light
PY
echo "ok   dragging the compositor's title bar moved the window"

# Scenario 3: weston-terminal (libwayland, cairo, xkbcommon) if available.
if command -v weston-terminal > /dev/null; then
    cat > "$work/weston.script" <<SCRIPT
wait 2500
pointer 300 250
press 272
release 272
wait 300
type echo Weston SAYS hi | tr A-Z a-z > $work/weston.marker
enter
wait 1000
pointer 250 38
press 272
wait 100
pointer 300 120
pointer 350 200
release 272
wait 600
pointer 500 450
press 273
release 273
wait 800
shot $work/weston.ppm
pointer 900 700
press 272
release 272
wait 300
close
SCRIPT
    run_scenario weston "$work/weston.script" --run weston-terminal
    [[ "$(cat "$work/weston.marker" 2> /dev/null)" == "weston says hi" ]] || { echo "weston-terminal did not receive the typed command" >&2; exit 1; }
    echo "ok   weston-terminal received keys through the forwarded xkb keymap (Shift included)"
    grep -q "^move: " "$work/weston-compositor.log" || { echo "title-bar drag (xdg_toplevel.move) did not move the window" >&2; exit 1; }
    echo "ok   weston-terminal moved by dragging its title bar"
else
    echo "skip weston-terminal is not installed"
fi

# Scenario 4: copy and paste between two clients through the compositor's
# wl_data_device_manager, with wl-clipboard (if available): wl-copy sets
# the selection, wl-paste (another client) reads it through a pipe.
if command -v wl-copy > /dev/null && command -v wl-paste > /dev/null; then
    cat > "$work/clipboard.sh" <<SCRIPT
#!/bin/sh
wl-copy "copied through mlx"
sleep 0.5
timeout 5 wl-paste --no-newline > "$work/clipboard.marker"
timeout 5 wl-paste --list-types > "$work/clipboard.types"
SCRIPT
    chmod +x "$work/clipboard.sh"
    printf 'wait 4000\nclose\n' > "$work/clipboard.script"
    run_scenario clipboard "$work/clipboard.script" --run "$work/clipboard.sh"
    [[ "$(cat "$work/clipboard.marker" 2> /dev/null)" == "copied through mlx" ]] || { echo "wl-paste did not read what wl-copy copied" >&2; cat "$work/clipboard-compositor.log" >&2; exit 1; }
    grep -qx "text/plain;charset=utf-8" "$work/clipboard.types" || { echo "the offer lacks the source's MIME types" >&2; exit 1; }
    echo "ok   copy and paste between two clients (wl-copy, wl-paste)"
else
    echo "skip wl-clipboard is not installed"
fi

# Scenario 5: a GTK 4 application (GTK 4 needs wl_data_device_manager to
# use a Wayland display at all), if available.
if command -v gtk4-widget-factory > /dev/null && command -v dbus-run-session > /dev/null; then
    cat > "$work/gtk4.sh" <<SCRIPT
#!/bin/sh
unset DISPLAY
# No portals: they would mount the document portal under XDG_RUNTIME_DIR.
GDK_DEBUG=no-portals exec dbus-run-session gtk4-widget-factory
SCRIPT
    chmod +x "$work/gtk4.sh"
    printf 'wait 6000\nclose\n' > "$work/gtk4.script"
    run_scenario gtk4 "$work/gtk4.script" --run "$work/gtk4.sh"
    grep -q "^map: GTK Widget Factory" "$work/gtk4-compositor.log" || { echo "the GTK 4 window did not appear" >&2; cat "$work/gtk4-compositor.log" >&2; exit 1; }
    echo "ok   a GTK 4 application (gtk4-widget-factory) opened its window"
else
    echo "skip gtk4-widget-factory or dbus-run-session is not installed"
fi
