#!/usr/bin/env bash
# End-to-end check of projects/desktop/compositor (nested compositor) and
# projects/desktop/terminal (terminal client) with real keyboard and
# pointer input.
#
# tools/wayland-test-host plays the "session compositor": it hosts the
# nested compositor's window, gives it a seat with an xkb keymap, and plays
# input scripts:
#
#   1. click the Mlx terminal and type a command (keyboard -> shell);
#   2. Alt+Enter opens a second terminal (compositor shortcut);
#   3. Alt+drag moves it (compositor-driven move); the pointer and keyboard
#      leave the window and come back, with a wheel notch and a Super key
#      in between (as when the user visits another window); then type into
#      it;
#   4. drag a window by the compositor's title bar (the title is drawn with
#      std.truetype when DejaVu Sans is installed);
#   5. resize a terminal by its border (bottom-right, then top-left with
#      the opposite corner kept) and with Alt+right-drag; the shell must
#      learn each size;
#   6. with weston-terminal (if installed): type with Shift through the
#      forwarded xkb keymap, drag it by its title bar (xdg_toplevel.move)
#      and open its right-click popup menu;
#   7. with wl-clipboard and gtk4-widget-factory (if installed): copy and
#      paste between two clients, and a GTK 4 window;
#   8. xdg-decoration: a client asking to draw its own title bar is told
#      the compositor draws it, until the settings file allows client
#      decorations (and again not when it stops);
#   9. pointer constraints and relative motion: a client locks the pointer
#      on its window; after a click it is locked, the pointer stays put and
#      the client gets the host's movement as relative motion;
#  10. with GTK 3 (through python3's ctypes, if installed): KDE's server
#      decoration tells it the compositor decorates, so it asks for server
#      side decoration and draws no title bar of its own.
#  10c. Aero snap: a window dragged to the left edge snaps to that half
#      (the translucent preview shows while it is dragged), Super+Right to
#      the right half, dragging it away restores its size, the top edge
#      maximizes.
#  10d. workspaces and scrollable tiling with the top bar (mlx-topbar): its
#      tiling half turns tiling on (the windows become columns), a second
#      window opens as a column after the first, Super+R, Super+Left and
#      Super+Shift+Right size, focus and move columns, the floating half
#      turns it off again; Super+Shift+2 moves a window to workspace 2, the
#      bar's numbers and Super+3 switch workspaces.
#  10e. animations (settings `animations = slow`, eight times slower, so a
#      frame in the middle is caught): switching workspaces slides the
#      window out, Super+Right glides it to the right half.
#  10f. the wheel and the touchpad on a tiling workspace: Super+wheel walks
#      the columns, Super and fingers drag the strip, which settles on a
#      column when they lift; Super+Ctrl+wheel (and Super+wheel on a
#      floating workspace) and the wheel over the top bar switch
#      workspaces.
#  10g. three-finger swipes (zwp_pointer_gestures_v1 from the host): the
#      strip follows the fingers and settles, up and down and sideways on
#      a floating workspace pull the next workspace in or spring back.
#  11. mlx-settings records hotkeys: while it records, the compositor
#      passes every key to it (keyboard-shortcuts-inhibit), Super+Up too;
#      Backspace unbinds one; the compositor takes the new keys at once.
#  12. mlx-capture captures the screen and a window
#      (ext-image-copy-capture-v1): the PNGs agree, and a later frame
#      waits for damage.
#  13. with Xwayland and xev: an X program's window shows, takes focus and
#      gets keys (the compositor's own X window manager).
#  14. mlx-settings' Sound and Display pages: on MLXIPC with MLX Audio
#      (started by the bus) it mutes the output and turns the volume down;
#      the nested compositor tells its output in <socket>.display, and the
#      Display page's night light switch reaches compositor.conf.
#  15. the permission dialog (mlx-permissions, with bwrap): a sandboxed
#      app records, MLXIPC starts the agent, its dialog opens; Remember and
#      Allow are clicked: the app records and ipc.conf keeps the rule;
#      mlx-settings' Apps page shows it and denies it again.
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
# Values a turn of the compositor's loop released are overwritten, so one
# wrongly kept across turns shows (projects/desktop/compositor/arena.mlx).
export MLX_ARENA_POISON=${MLX_ARENA_POISON:-1}
mkdir -m 700 "$XDG_RUNTIME_DIR"
# The compositor's settings file: the defaults unless a scenario writes one
# (not the user's own ~/.config/mlx/compositor.conf).
export XDG_CONFIG_HOME="$work/config"
# Which apps asked for which permission (MLXIPC keeps it there).
export XDG_STATE_HOME="$work/state"
mkdir -p "$XDG_CONFIG_HOME/mlx"
# GTK's document portal may have mounted itself under the runtime directory.
trap 'fusermount -u "$XDG_RUNTIME_DIR/doc" 2> /dev/null || true; rm -rf -- "$work"' EXIT

"$compiler" --quiet projects/desktop/compositor/main.mlx -o "$work/mlx-compositor"
"$compiler" --quiet projects/desktop/terminal/main.mlx -o "$work/mlx-terminal"
"$compiler" --quiet tools/wayland-test-host/main.mlx -o "$work/test-host"
# The top bar is kept out of the compositor's directory: next to it, every
# scenario would start it (and its bar would cover the title bars at the
# top); the scenarios that want it name it with --topbar.
mkdir -p "$work/bar"
"$compiler" --quiet projects/desktop/topbar/main.mlx -o "$work/bar/mlx-topbar"
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

# Scenario 1: Mlx terminals (the terminal's title follows its shell once
# bash names it, so the log lines are matched by their kind alone).
cat > "$work/terminal.script" <<SCRIPT
wait 2500
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
wait 300
leave
wait 200
pointer 430 390
scroll 2560
down 125
up 125
leave
wait 300
pointer 430 390
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
grep -q "^move: " "$work/terminal-compositor.log" || { echo "Alt+drag did not start a move" >&2; exit 1; }
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
grep -q "^move: " "$work/title-compositor.log" || { echo "dragging the title bar did not move the window" >&2; cat "$work/title-compositor.log" >&2; exit 1; }
# The window starts at (24, 24) and moves by (+100, +100): its frame's left
# edge is at x = 122, its title bar spans y = 102..121 (rounded above
# y = 116).
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
assert rgb(122, 300) == focus and rgb(122, 118) == focus, (rgb(122, 300), rgb(122, 118))
assert rgb(22, 300) != focus, "the window did not leave its original position"
if sys.argv[2] == "1":
    # The title in white over the bar (the bar's red is 0x5a).
    light = sum(1 for y in range(102, 122) for x in range(124, 320) if rgb(x, y)[0] > 180)
    assert light > 50, "no title text in the title bar (%d light pixels)" % light
PY
echo "ok   dragging the compositor's title bar moved the window"

# Scenario 3: resizing. The Mlx terminal (652x396 at 24,24: 80x24 cells)
# snaps to whole cells, so every size is exact: its bottom-right border
# dragged by (+200, +160) gives 105x34 cells (852x556), its top-left border
# dragged by (+200, +160) gives 80x24 again with the bottom-right corner
# kept, and Alt+right-drag near that corner by (-160, -64) gives 60x20.
# The shell learns each size (stty size).
cat > "$work/resize.script" <<SCRIPT
wait 1500
pointer 678 422
wait 200
press 272
pointer 750 480
pointer 878 582
wait 500
release 272
wait 800
pointer 300 300
press 272
release 272
wait 300
type stty size > $work/resize-large.marker
enter
wait 500
pointer 21 1
wait 200
press 272
pointer 100 80
pointer 221 161
wait 500
release 272
wait 800
pointer 400 400
press 272
release 272
wait 300
type stty size > $work/resize-anchored.marker
enter
wait 500
pointer 800 500
down 56
press 273
pointer 700 470
pointer 640 436
wait 500
release 273
up 56
wait 800
type stty size > $work/resize-small.marker
enter
wait 500
close
SCRIPT
run_scenario resize "$work/resize.script" --terminal "$work/mlx-terminal" --run "$work/mlx-terminal"
for expected in "window: 852x556 at 24,24" "window: 652x396 at 224,184" "window: 492x332 at 224,184"; do
    grep -qx "$expected" "$work/resize-compositor.log" || { echo "resize: no \"$expected\"" >&2; cat "$work/resize-compositor.log" >&2; exit 1; }
done
[[ "$(cat "$work/resize-large.marker" 2> /dev/null)" == "34 105" ]] || { echo "the shell did not learn the larger size" >&2; exit 1; }
[[ "$(cat "$work/resize-anchored.marker" 2> /dev/null)" == "24 80" ]] || { echo "the shell did not learn the size after the top-left resize" >&2; exit 1; }
[[ "$(cat "$work/resize-small.marker" 2> /dev/null)" == "20 60" ]] || { echo "the shell did not learn the size after Alt+right-drag" >&2; exit 1; }
echo "ok   windows resize by their border (the opposite corner kept) and with Alt+right-drag; the Mlx terminal follows"

# Scenario 4: weston-terminal (libwayland, cairo, xkbcommon) if available.
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

# Scenario 5: a subsurface (examples/wayland-client --subsurface): green,
# 100x80, at (40, 30) in a 320x240 window whose content starts at (24, 24).
"$compiler" --quiet examples/wayland-client/main.mlx -o "$work/hello-wayland"
cat > "$work/subsurface.sh" <<SCRIPT
#!/bin/sh
exec "$work/hello-wayland" --subsurface
SCRIPT
chmod +x "$work/subsurface.sh"
cat > "$work/subsurface.script" <<SCRIPT
wait 1500
shot $work/subsurface.ppm
close
SCRIPT
run_scenario subsurface "$work/subsurface.script" --run "$work/subsurface.sh"
python3 - "$work/subsurface.ppm" <<'PY'
import sys
data = open(sys.argv[1], 'rb').read()
header, rest = data.split(b'\n', 1)
size, rest = rest.split(b'\n', 1)
_, pixels = rest.split(b'\n', 1)
width, height = map(int, size.split())
def rgb(x, y):
    offset = (y * width + x) * 3
    return tuple(pixels[offset:offset + 3])
green = (0, 255, 0)
for x, y in ((64, 54), (100, 90), (163, 133)):
    assert rgb(x, y) == green, ("subsurface pixel", x, y, rgb(x, y))
for x, y in ((40, 40), (170, 90), (100, 140), (60, 90)):
    assert rgb(x, y) != green, ("window pixel", x, y, rgb(x, y))
PY
echo "ok   a subsurface is drawn at its offset in its window"

# Scenario 5b: the pointer over a subsurface that its client then destroys
# (--subsurface-gone destroys it on the first pointer enter). A subsurface
# is never "mapped", so the compositor once kept its pointer focus on the
# freed surface and crashed in sameClient on the next motion.
cat > "$work/subsurface-gone.sh" <<SCRIPT
#!/bin/sh
exec "$work/hello-wayland" --subsurface-gone > "$work/subsurface-gone-client.log" 2>&1
SCRIPT
chmod +x "$work/subsurface-gone.sh"
cat > "$work/subsurface-gone.script" <<SCRIPT
wait 1500
pointer 100 90
wait 600
pointer 110 95
pointer 120 100
pointer 60 60
pointer 100 90
wait 400
shot $work/subsurface-gone.ppm
close
SCRIPT
run_scenario subsurface-gone "$work/subsurface-gone.script" --run "$work/subsurface-gone.sh"
grep -q "subsurface destroyed on pointer enter" "$work/subsurface-gone-client.log" || { echo "the client did not destroy its subsurface:" >&2; cat "$work/subsurface-gone-client.log" >&2; exit 1; }
grep -q "crash" "$work/subsurface-gone-compositor.log" && { echo "the compositor crashed after the subsurface went:" >&2; cat "$work/subsurface-gone-compositor.log" >&2; exit 1; }
python3 - "$work/subsurface-gone.ppm" <<'PY'
import sys
data = open(sys.argv[1], 'rb').read()
header, rest = data.split(b'\n', 1)
size, rest = rest.split(b'\n', 1)
_, pixels = rest.split(b'\n', 1)
width, height = map(int, size.split())
def rgb(x, y):
    offset = (y * width + x) * 3
    return tuple(pixels[offset:offset + 3])
for x, y in ((64, 54), (100, 90), (163, 133)):
    assert rgb(x, y) != (0, 255, 0), ("the subsurface is still drawn", x, y)
PY
echo "ok   the pointer moves on after the subsurface under it is destroyed"

# Scenario 8: xdg-decoration. The client asks for client-side decorations;
# the compositor answers server side (its default), client side once the
# settings file says client-decorations = on, server side again after off.
cat > "$work/decoration.sh" <<SCRIPT
#!/bin/sh
( sleep 2; printf 'client-decorations = on\n' > "$XDG_CONFIG_HOME/mlx/compositor.conf"
  sleep 2; printf 'client-decorations = off\n' > "$XDG_CONFIG_HOME/mlx/compositor.conf" ) &
exec "$work/hello-wayland" --decoration client > "$work/decoration.log"
SCRIPT
chmod +x "$work/decoration.sh"
printf 'wait 6000\nclose\n' > "$work/decoration.script"
run_scenario decoration "$work/decoration.script" --run "$work/decoration.sh"
rm -f "$XDG_CONFIG_HOME/mlx/compositor.conf"
modes=$(uniq "$work/decoration.log" | tr '\n' ' ')
[[ "$modes" == "decoration: server side decoration: client side decoration: server side " ]] || { echo "decoration modes: $modes" >&2; cat "$work/decoration-compositor.log" >&2; exit 1; }
echo "ok   xdg-decoration: server side by default, client side while the settings allow it"

# Scenario 9: a pointer lock (examples/wayland-client --lock, its window at
# (24, 24)). The click gives it the pointer and the keyboard, so the lock
# holds: the cursor stays at (100, 100) while the host's pointer travels
# (+100, +50), which reaches the client as relative motion.
cat > "$work/lock.sh" <<SCRIPT
#!/bin/sh
exec "$work/hello-wayland" --lock > "$work/lock.log"
SCRIPT
chmod +x "$work/lock.sh"
cat > "$work/lock.script" <<SCRIPT
wait 1500
pointer 100 100
press 272
release 272
wait 300
pointer 130 110
pointer 170 125
pointer 200 150
wait 300
shot $work/lock.ppm
close
SCRIPT
run_scenario lock "$work/lock.script" --run "$work/lock.sh"
grep -q "^pointer: locked" "$work/lock.log" || { echo "the pointer was not locked" >&2; cat "$work/lock.log" "$work/lock-compositor.log" >&2; exit 1; }
[[ "$(grep '^relative:' "$work/lock.log" | tail -1)" == "relative: 100 50" ]] || { echo "relative motion: $(grep '^relative:' "$work/lock.log" | tail -1)" >&2; cat "$work/lock-compositor.log" >&2; exit 1; }
python3 - "$work/lock.ppm" <<'PY'
import sys
data = open(sys.argv[1], 'rb').read()
_, size, _, pixels = data.split(b'\n', 3)
width, height = map(int, size.split())
def white(x0, y0):
    return sum(1 for y in range(y0, y0 + 17) for x in range(x0, x0 + 17)
               if pixels[(y * width + x) * 3:(y * width + x) * 3 + 3] == b'\xff\xff\xff')
# The arrow (white inside) where the lock held it, not where the host's
# pointer went.
assert white(100, 100) > 10 and white(200, 150) == 0, (white(100, 100), white(200, 150))
PY
echo "ok   a pointer lock holds the cursor and relative motion reaches the client"

# Scenario 10c: Aero snap. The terminal opens at (24, 24) and moves below
# the top bar (its title bar at y 30..49). Dragged to the left edge, the
# left half of the work area shows the preview (the focus colour, three
# eighths over what is there) and the window snaps there when let go;
# Super+Right puts it in the right half; dragging its title bar away
# brings back its old size under the pointer; the top edge maximizes.
cat > "$work/snap.script" <<SCRIPT
wait 5000
pointer 200 40
press 272
pointer 100 200
pointer 0 300
wait 300
shot $work/snap-preview.ppm
release 272
wait 600
down 125
down 106
up 106
up 125
wait 600
pointer 700 40
press 272
pointer 600 200
pointer 500 300
wait 200
release 272
wait 600
pointer 500 300
press 272
pointer 500 100
pointer 500 30
wait 200
release 272
wait 600
close
SCRIPT
run_scenario snap "$work/snap.script" --no-fps --topbar "$work/bar/mlx-topbar" --terminal "$work/mlx-terminal" --run "$work/mlx-terminal"
log="$work/snap-compositor.log"
# (The work area: 1024x738 below the bar; its halves 512 wide.)
grep -q "^snap: left " "$log" && grep -q "^snapped: 512x738 at 0,30" "$log" && grep -q "^window: 508x[0-9]* at 2,52" "$log" || { echo "dragging the window to the left edge did not snap it to the left half" >&2; cat "$log" >&2; exit 1; }
grep -q "^snap: right " "$log" && grep -q "^snapped: 512x738 at 512,30" "$log" || { echo "Super+Right did not snap the window to the right half" >&2; cat "$log" >&2; exit 1; }
[[ $(grep -c "^window: 652x396 " "$log") -ge 2 ]] || { echo "dragging a snapped window away did not bring back its size" >&2; cat "$log" >&2; exit 1; }
grep -q "^maximize: " "$log" || { echo "dragging the window to the top edge did not maximize it" >&2; cat "$log" >&2; exit 1; }
python3 - "$work/snap-preview.ppm" <<'PY'
import sys
data = open(sys.argv[1], 'rb').read()
_, size, _, pixels = data.split(b'\n', 3)
width, height = map(int, size.split())
def rgb(x, y):
    at = (y * width + x) * 3
    return tuple(pixels[at:at + 3])
# The left half is tinted blue, the right half is not, the edge at 512.
left, right = rgb(300, 150), rgb(800, 150)
assert left[2] - right[2] > 40 and left[0] - right[0] > 10, (left, right)
assert rgb(508, 150)[2] - rgb(516, 150)[2] > 40, (rgb(508, 150), rgb(516, 150))
PY
echo "ok   Aero snap: the edges snap a dragged window (with a preview) to a half, a quarter or the whole work area; Super+Right too"

# Scenario 10d: workspaces and scrollable tiling, with the top bar. The
# bar's controls are centred: with workspace 1 shown and no other in use,
# the buttons 1 and 2 at x 453..476 and 481..504, then the floating half
# (515..542) and the tiling half (543..570) of the mode button; once
# workspace 2 has a window the buttons 1 to 3 start at 439, 467 and 495
# (the mode button at 529); on workspace 3 four buttons show, the first
# at 425.
cat > "$work/workspaces.script" <<SCRIPT
wait 5000
shot $work/bar-floating.ppm
pointer 557 15
press 272
release 272
wait 800
shot $work/bar-tiling.ppm
down 56
down 28
up 28
up 56
wait 2500
down 125
down 19
up 19
up 125
wait 500
down 125
down 105
up 105
up 125
wait 500
down 125
down 42
down 106
up 106
up 42
up 125
wait 600
pointer 529 15
press 272
release 272
wait 600
down 125
down 42
down 3
up 3
up 42
up 125
wait 600
pointer 479 15
press 272
release 272
wait 600
down 125
down 4
up 4
up 125
wait 400
pointer 437 15
press 272
release 272
wait 600
close
SCRIPT
run_scenario workspaces "$work/workspaces.script" --no-fps --topbar "$work/bar/mlx-topbar" --terminal "$work/mlx-terminal" --run "$work/mlx-terminal"
log="$work/workspaces-compositor.log"
expect_line() {
    grep -q -- "$1" "$log" || { echo "workspaces: $2 (no '$1' in the log)" >&2; cat "$log" >&2; exit 1; }
}
expect_line "^tiling: on (workspace 1)" "the bar's tiling half did not turn tiling on"
expect_line "^column: .* at 8 width 500" "the first window is not the first column"
expect_line "^column: .* at 516 width 500" "the second window did not open as the column after the first"
expect_line "^column width: 670 " "Super+R did not widen the column"
expect_line "^column: .* at 686 width 500" "Super+Shift+Right did not move the column right"
expect_line "^tiling: off (workspace 1)" "the bar's floating half did not turn tiling off"
expect_line "^moved to workspace 2: " "Super+Shift+2 did not move the window"
[[ $(grep -c "^workspace: " "$log") -ge 3 ]] && grep -q "^workspace: 3" "$log" || { echo "the bar's numbers and Super+3 did not switch workspaces" >&2; cat "$log" >&2; exit 1; }
[[ "$(grep "^workspace: " "$log" | tail -1)" == "workspace: 1" ]] || { echo "the bar's 1 did not bring back workspace 1" >&2; cat "$log" >&2; exit 1; }
python3 - "$work/bar-floating.ppm" "$work/bar-tiling.ppm" <<'PY'
import sys
def load(path):
    data = open(path, 'rb').read()
    _, size, _, pixels = data.split(b'\n', 3)
    width, height = map(int, size.split())
    return lambda x, y: tuple(pixels[(y * width + x) * 3:(y * width + x) * 3 + 3])
floating, tiling = load(sys.argv[1]), load(sys.argv[2])
def lit(pixel): return min(pixel) > 200
# Workspace 1's button is lit in both; the lit half of the mode button
# moves from floating to tiling.
assert lit(floating(465, 7)) and lit(tiling(465, 7)), (floating(465, 7), tiling(465, 7))
assert lit(floating(520, 7)) and not lit(floating(566, 7)), (floating(520, 7), floating(566, 7))
assert lit(tiling(566, 7)) and not lit(tiling(520, 7)), (tiling(520, 7), tiling(566, 7))
PY
echo "ok   workspaces and scrollable tiling: the bar's buttons and the hotkeys switch, tile, size, focus and move"

# Scenario 10e: animations, eight times slower (`animations = slow`). The
# terminal rests at (24, 24): its frame's columns 22..23 and 676..677. Super+2
# slides workspace 1 out to the left: 300 ms in, its right edge is on the
# way (left of 600, not yet gone). Back on workspace 1, Super+Right snaps it
# to the right half: 300 ms in, its left edge glides between where it was
# and x = 511, where it rests in the end.
printf 'animations = slow\n' > "$XDG_CONFIG_HOME/mlx/compositor.conf"
cat > "$work/animations.script" <<SCRIPT
wait 3000
down 125
down 3
up 3
up 125
wait 300
shot $work/slide-mid.ppm
wait 3000
down 125
down 2
up 2
up 125
wait 3000
down 125
down 106
up 106
up 125
wait 300
shot $work/glide-mid.ppm
wait 3000
shot $work/glide-end.ppm
close
SCRIPT
run_scenario animations "$work/animations.script" --no-fps --terminal "$work/mlx-terminal" --run "$work/mlx-terminal"
rm -f "$XDG_CONFIG_HOME/mlx/compositor.conf"
grep -q "^settings: .*animations slow" "$work/animations-compositor.log" || { echo "the compositor did not read animations = slow" >&2; cat "$work/animations-compositor.log" >&2; exit 1; }
grep -q "^snap: right " "$work/animations-compositor.log" || { echo "Super+Right did not snap the window" >&2; cat "$work/animations-compositor.log" >&2; exit 1; }
python3 - "$work/slide-mid.ppm" "$work/glide-mid.ppm" "$work/glide-end.ppm" <<'PY'
import sys
def frame_columns(path, row):
    data = open(path, 'rb').read()
    _, size, _, pixels = data.split(b'\n', 3)
    width, height = map(int, size.split())
    frame = ((0x5a, 0xa0, 0xff), (0x50, 0x50, 0x60))
    return [x for x in range(width) if tuple(pixels[(row * width + x) * 3:(row * width + x) * 3 + 3]) in frame]
slide = frame_columns(sys.argv[1], 200)
assert slide and 22 not in slide and max(slide) < 600, ("the window did not slide out", slide)
glide = frame_columns(sys.argv[2], 200)
assert glide and 40 < min(glide) < 500, ("the window did not glide to the right half", glide)
end = frame_columns(sys.argv[3], 200)
assert min(end) in (511, 512), ("the window did not end on the right half", end)
PY
echo "ok   animations: workspaces slide, snapped windows glide into place"

# Scenario 10f: the wheel and the touchpad. Three terminals on a tiling
# workspace (columns 500 wide from x = 8: the strip is 1516 wide, the third
# column focused at its right end). Super+wheel up twice focuses the first
# column (the strip glides back to the start: columns at 8 and 516); Super
# and fingers drag the strip 500 pixels on (the second column's left edge
# at 16); they lift and it settles with the second column at x = 8, which
# takes the focus. Super+Ctrl+wheel goes to workspace 2, Super+wheel up on
# that (floating) one back to 1, the wheel over the top bar to 2 again.
cat > "$work/wheel.script" <<SCRIPT
wait 5000
pointer 500 400
down 125
down 20
up 20
up 125
wait 600
down 56
down 28
up 28
up 56
wait 2500
down 56
down 28
up 28
up 56
wait 2500
down 125
scroll -2560
up 125
wait 600
down 125
scroll -2560
up 125
wait 800
shot $work/wheel-first.ppm
down 125
swipe 25600
swipe 25600
swipe 25600
swipe 25600
swipe 25600
up 125
wait 300
shot $work/wheel-swiped.ppm
lift
wait 800
shot $work/wheel-settled.ppm
down 125
down 29
scroll 2560
up 29
up 125
wait 1000
down 125
scroll -2560
up 125
wait 1000
pointer 500 15
scroll 2560
wait 1000
close
SCRIPT
run_scenario wheel "$work/wheel.script" --no-fps --topbar "$work/bar/mlx-topbar" --terminal "$work/mlx-terminal" --run "$work/mlx-terminal"
log="$work/wheel-compositor.log"
grep -q "^settled on column 2: " "$log" || { echo "the strip did not settle on the second column when the fingers lifted" >&2; cat "$log" >&2; exit 1; }
[[ "$(grep "^workspace: " "$log" | tr '\n' ' ')" == "workspace: 2 workspace: 1 workspace: 2 " ]] || { echo "Super+Ctrl+wheel, Super+wheel and the wheel over the bar did not switch workspaces 2, 1, 2" >&2; cat "$log" >&2; exit 1; }
python3 - "$work/wheel-first.ppm" "$work/wheel-swiped.ppm" "$work/wheel-settled.ppm" <<'PY'
import sys
def frame_columns(path, row):
    data = open(path, 'rb').read()
    _, size, _, pixels = data.split(b'\n', 3)
    width, height = map(int, size.split())
    frame = ((0x5a, 0xa0, 0xff), (0x50, 0x50, 0x60))
    return [x for x in range(width) if tuple(pixels[(row * width + x) * 3:(row * width + x) * 3 + 3]) in frame]
first, swiped, settled = (frame_columns(path, 300) for path in sys.argv[1:4])
assert 8 in first and 516 in first, ("Super+wheel did not bring the first column back", first)
assert 16 in swiped and 524 in swiped, ("the strip did not follow the fingers", swiped)
assert 8 in settled and 516 in settled and 16 not in settled, ("the strip did not settle on a column", settled)
PY
echo "ok   the wheel walks columns and switches workspaces, fingers drag the strip and it settles on a column"

# Scenario 10g: three-finger swipes (the host's zwp_pointer_gestures_v1).
# Three columns on a tiling workspace, the third focused (the view at 508):
# fingers going right drag the strip 500 pixels back (the first column's
# frame at x = 0), and when they lift it settles with the first column at
# x = 8. Up 400 pixels pulls workspace 2 in from below (the windows on
# their way up: nothing of them at y = 600) and lifting goes there; 100
# pixels down and a rest spring back; sideways on that floating workspace
# goes back to workspace 1.
cat > "$work/gestures.script" <<SCRIPT
wait 5000
pointer 500 400
down 125
down 20
up 20
up 125
wait 600
down 56
down 28
up 28
up 56
wait 2500
down 56
down 28
up 28
up 56
wait 2500
gesture-begin 3
gesture-move 100 0
gesture-move 100 0
gesture-move 100 0
gesture-move 100 0
gesture-move 100 0
wait 300
shot $work/strip-swiped.ppm
gesture-end
wait 800
shot $work/strip-settled.ppm
gesture-begin 3
gesture-move 0 -100
gesture-move 0 -100
gesture-move 0 -100
gesture-move 0 -100
wait 300
shot $work/vertical-mid.ppm
gesture-end
wait 1000
gesture-begin 3
gesture-move 0 100
wait 300
gesture-end
wait 1000
gesture-begin 3
gesture-move 100 0
gesture-move 100 0
gesture-move 100 0
gesture-move 100 0
wait 300
gesture-end
wait 1000
close
SCRIPT
run_scenario gestures "$work/gestures.script" --no-fps --topbar "$work/bar/mlx-topbar" --terminal "$work/mlx-terminal" --run "$work/mlx-terminal"
log="$work/gestures-compositor.log"
for line in "^settled on column 1: " "^swipe: workspaces up and down" "^swipe: to workspace 2" "^swipe: back from workspace 1" "^swipe: workspaces sideways" "^swipe: to workspace 1"; do
    grep -q "$line" "$log" || { echo "the swipes did not log \"$line\"" >&2; cat "$log" >&2; exit 1; }
done
[[ "$(grep "^workspace: " "$log" | tr '\n' ' ')" == "workspace: 2 workspace: 1 " ]] || { echo "the swipes did not switch to workspace 2 and back to 1" >&2; cat "$log" >&2; exit 1; }
python3 - "$work/strip-swiped.ppm" "$work/strip-settled.ppm" "$work/vertical-mid.ppm" <<'PY'
import sys
def frame_columns(path, row):
    data = open(path, 'rb').read()
    _, size, _, pixels = data.split(b'\n', 3)
    width, height = map(int, size.split())
    frame = ((0x5a, 0xa0, 0xff), (0x50, 0x50, 0x60))
    return [x for x in range(width) if tuple(pixels[(row * width + x) * 3:(row * width + x) * 3 + 3]) in frame]
swiped, settled = frame_columns(sys.argv[1], 300), frame_columns(sys.argv[2], 300)
assert 0 in swiped and 508 in swiped, ("the strip did not follow three fingers", swiped)
assert 8 in settled and 516 in settled, ("the strip did not settle on the first column", settled)
assert frame_columns(sys.argv[3], 200) and not frame_columns(sys.argv[3], 600), "workspace 1 did not move up with the fingers"
PY
echo "ok   three-finger swipes drag the strip and settle, pull workspaces in up and down or sideways, and spring back"

# Scenario 11: mlx-settings (its window at (24, 24); the categories in
# its sidebar 28 pixels apart from y 44: General, Display, Sound, Apps,
# Window management, Appearance, Language; on the Window management page the
# hotkey rows 34 pixels apart from y 436, on the General page the
# terminal's at y 186). Maximize gets Super+Shift+M; minimize gets
# Super+Up (the maximize hotkey: it must reach the window); the terminal
# hotkey is cleared; then Super+Up minimizes.
"$compiler" --quiet projects/desktop/settings/main.mlx -o "$work/mlx-settings"
rm -f "$XDG_CONFIG_HOME/mlx/compositor.conf"
cat > "$work/hotkeys.script" <<SCRIPT
wait 2500
pointer 124 194
press 272
release 272
wait 300
pointer 524 475
press 272
release 272
wait 300
down 125
down 42
down 50
up 50
up 42
up 125
wait 300
pointer 524 509
press 272
release 272
wait 300
down 125
down 103
up 103
up 125
wait 300
pointer 124 82
press 272
release 272
wait 300
pointer 524 225
press 272
release 272
wait 300
down 14
up 14
wait 800
down 125
down 103
up 103
up 125
wait 500
close
SCRIPT
run_scenario hotkeys "$work/hotkeys.script" --no-fps --run "$work/mlx-settings"
conf="$XDG_CONFIG_HOME/mlx/compositor.conf"
for line in "key-maximize = Super+Shift+M" "key-minimize = Super+Up" "key-terminal = none" "key-close = Alt+F4"; do
    grep -qx "$line" "$conf" 2> /dev/null || { echo "mlx-settings did not write \"$line\"" >&2; cat "$conf" "$work/hotkeys-compositor.log" >&2; exit 1; }
done
[[ $(grep -c "^shortcuts: inhibited" "$work/hotkeys-compositor.log") -eq 3 ]] || { echo "the compositor did not pass the keys to mlx-settings while it recorded" >&2; cat "$work/hotkeys-compositor.log" >&2; exit 1; }
! grep -q "^maximize: Settings" "$work/hotkeys-compositor.log" || { echo "Super+Up maximized the window while mlx-settings recorded it" >&2; exit 1; }
grep -q "^minimize: Settings" "$work/hotkeys-compositor.log" || { echo "the recorded Super+Up did not minimize" >&2; cat "$work/hotkeys-compositor.log" >&2; exit 1; }
rm -f "$conf"
echo "ok   mlx-settings records hotkeys (the compositor's too) and the compositor uses them at once"

# Scenario 12: screen and window capture (ext-image-copy-capture-v1 with
# ext-image-capture-source-v1 and ext-foreign-toplevel-list-v1), with
# projects/desktop/capture (mlx-capture): the window list names mlx-settings; the
# screen and the window are saved as PNGs whose pixels agree (the window's
# content at (24, 24) on the screen); a second frame of the screen waits
# until something changes (the pointer moves) and carries only that as
# damage.
"$compiler" --quiet projects/desktop/capture/main.mlx -o "$work/mlx-capture"
cat > "$work/capture.sh" <<SCRIPT
#!/bin/sh
sleep 2
"$work/mlx-capture" --list > "$work/capture-list.log" 2>&1
"$work/mlx-capture" "$work/capture-screen.png" > "$work/capture.log" 2>&1
"$work/mlx-capture" --window org.mlx.settings "$work/capture-window.png" >> "$work/capture.log" 2>&1
# (A client leaving redraws everything: that frame first.)
sleep 1
"$work/mlx-capture" --frames 2 "$work/capture-frames.png" >> "$work/capture.log" 2>&1
SCRIPT
chmod +x "$work/capture.sh"
cat > "$work/capture.script" <<SCRIPT
wait 5500
pointer 600 600
wait 200
pointer 620 610
wait 200
pointer 640 620
wait 200
pointer 660 630
wait 1500
close
SCRIPT
run_scenario capture "$work/capture.script" --no-fps --run "$work/mlx-settings" --run "$work/capture.sh"
grep -q "org.mlx.settings  Settings" "$work/capture-list.log" || { echo "the window list does not name mlx-settings" >&2; cat "$work/capture-list.log" "$work/capture-compositor.log" >&2; exit 1; }
[[ $(grep -c "^saved " "$work/capture.log") -eq 3 ]] || { echo "mlx-capture did not save three captures" >&2; cat "$work/capture.log" "$work/capture-compositor.log" >&2; exit 1; }
python3 - "$work" <<'PY'
import struct, sys, zlib
def load(path):
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', path
    at, idat = 8, b''
    while at < len(data):
        length, kind = struct.unpack('>I4s', data[at:at + 8])
        body = data[at + 8:at + 8 + length]
        assert zlib.crc32(kind + body) == struct.unpack('>I', data[at + 8 + length:at + 12 + length])[0], (path, kind)
        if kind == b'IHDR':
            width, height, depth, colour = struct.unpack('>IIBB', body[:10])
        elif kind == b'IDAT':
            idat += body
        at += 12 + length
    channels = 4 if colour == 6 else 3
    raw, stride = zlib.decompress(idat), width * channels
    rows, previous = [], bytearray(stride)
    for y in range(height):
        kind, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            left = line[i - channels] if i >= channels else 0
            up, corner = previous[i], previous[i - channels] if i >= channels else 0
            if kind == 1: line[i] = (line[i] + left) & 255
            elif kind == 2: line[i] = (line[i] + up) & 255
            elif kind == 3: line[i] = (line[i] + (left + up) // 2) & 255
            elif kind == 4:
                p = left + up - corner
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - corner)
                line[i] = (line[i] + (left if pa <= pb and pa <= pc else up if pb <= pc else corner)) & 255
        rows.append(bytes(line))
        previous = line
    return width, height, channels, rows
work = sys.argv[1]
sw, sh, sc, screen = load(work + '/capture-screen.png')
ww, wh, wc, window = load(work + '/capture-window.png')
assert sc == 3 and wc == 4, (sc, wc)
assert ww < sw and wh < sh, (sw, sh, ww, wh)
# Opaque content of the window, where the screen shows it.
for x, y in ((ww - 60, wh - 60), (ww // 2, wh // 2)):
    on_window = window[y][x * 4:x * 4 + 3]
    on_screen = screen[24 + y][(24 + x) * 3:(24 + x) * 3 + 3]
    assert window[y][x * 4 + 3] == 255 and on_window == on_screen, (x, y, on_window, on_screen)
PY
damage=$(grep "^frame 2: damage" "$work/capture.log" | sed 's/.* damage //')
[[ -n "$damage" && "$damage" != "0,0 "* ]] || { echo "the second frame did not wait for a change, or carried full damage ($damage)" >&2; cat "$work/capture.log" >&2; exit 1; }
echo "ok   screen and window capture (ext-image-copy-capture): PNGs agree, later frames wait for damage"

# Scenario 13: X programs through Xwayland (if Xwayland and xev are
# installed): xev's window is paired with its wl_surface by the window
# manager (projects/desktop/compositor/xwm.mlx), mapped with its title,
# focused by a click, and gets the keys typed (KeyPress for a, b, c).
if command -v Xwayland > /dev/null && command -v xev > /dev/null; then
    cat > "$work/x11.sh" <<SCRIPT
#!/bin/sh
exec xev -geometry 300x200 > "$work/xev.log" 2>&1
SCRIPT
    chmod +x "$work/x11.sh"
    cat > "$work/x11.script" <<SCRIPT
wait 3000
pointer 150 150
press 272
release 272
wait 300
type abc
wait 800
close
SCRIPT
    run_scenario x11 "$work/x11.script" --no-fps --run "$work/x11.sh"
    grep -q "^xwm: paired" "$work/x11-compositor.log" || { echo "the X window was not paired with its surface" >&2; cat "$work/x11-compositor.log" >&2; exit 1; }
    grep -q "^map: Event Tester" "$work/x11-compositor.log" || { echo "xev's window was not mapped with its title" >&2; cat "$work/x11-compositor.log" >&2; exit 1; }
    for key in a b c; do
        grep -A2 "^KeyPress" "$work/xev.log" | grep -q "keysym 0x6[123], $key)" || { echo "xev did not get the key $key" >&2; cat "$work/xev.log" "$work/x11-compositor.log" >&2; exit 1; }
    done
    echo "ok   X programs run through Xwayland: xev's window is mapped, titled, focused and gets keys"
else
    echo "skip X programs (Xwayland or xev not installed)"
fi

# Scenario 14: mlx-settings' Sound and Display pages. MLXIPC is the
# session bus and starts MLX Audio (into a file) when mlx-settings first
# asks for it. On the Sound page (no sound card: Automatic at y 141, None
# at y 179, the volume slider at y 220) None is chosen and the volume
# dragged from 100% to about half; on the Display page the night light
# switch (y 371) is turned on.
"$compiler" --quiet projects/desktop/ipc/main.mlx -o "$work/mlx-ipcd"
"$compiler" --quiet projects/desktop/audio/main.mlx -o "$work/mlx-audiod"
"$compiler" --quiet projects/desktop/audio/tool.mlx -o "$work/mlx-audio"
mkdir -p "$work/services"
cat > "$work/services/org.mlx.Audio.service" <<SERVICE
[D-BUS Service]
Name=org.mlx.Audio
Exec=$work/mlx-audiod --verbose --output file:$work/mix.wav --input sine:440
SERVICE
"$work/mlx-ipcd" --services "$work/services" > "$work/ipc.log" 2>&1 &
ipc_pid=$!
for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/bus" ]] && break; sleep 0.1; done
export DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"
rm -f "$XDG_CONFIG_HOME/mlx/compositor.conf"
cat > "$work/pages.script" <<SCRIPT
wait 2500
pointer 124 138
press 272
release 272
wait 1200
pointer 300 179
press 272
release 272
wait 600
pointer 718 220
press 272
pointer 650 220
wait 50
pointer 600 220
wait 100
release 272
wait 600
pointer 124 110
press 272
release 272
wait 800
pointer 753 371
press 272
release 272
wait 1500
close
SCRIPT
cat > "$work/pages-watch.sh" <<SCRIPT
#!/bin/sh
sleep 2
cp "$XDG_RUNTIME_DIR/nested-pages.display" "$work/pages.display"
SCRIPT
chmod +x "$work/pages-watch.sh"
run_scenario pages "$work/pages.script" --no-fps --run "$work/mlx-settings" --run "$work/pages-watch.sh"
"$work/mlx-audio" status > "$work/pages-audio.txt" 2>&1 || true
kill "$ipc_pid" 2> /dev/null || true
pkill -x mlx-audiod 2> /dev/null || true
unset DBUS_SESSION_BUS_ADDRESS
grep -qx "output = null" "$work/pages-audio.txt" || { echo "mlx-settings did not switch MLX Audio's output off" >&2; cat "$work/pages-audio.txt" "$work/pages-compositor.log" >&2; exit 1; }
volume=$(sed -n 's/^volume = //p' "$work/pages-audio.txt")
[[ -n "$volume" && $volume -gt 20000 && $volume -lt 50000 ]] || { echo "mlx-settings' volume slider set $volume" >&2; cat "$work/pages-audio.txt" >&2; exit 1; }
grep -qx "backend = nested" "$work/pages.display" && grep -q "^mode = 1024x768@" "$work/pages.display" || { echo "the nested compositor did not tell its output" >&2; cat "$work/pages.display" "$work/pages-compositor.log" >&2; exit 1; }
[[ ! -e "$XDG_RUNTIME_DIR/nested-pages.display" ]] || { echo "nested-pages.display outlived the compositor" >&2; exit 1; }
grep -qx "night-light = on" "$XDG_CONFIG_HOME/mlx/compositor.conf" && grep -qx "display-mode = auto" "$XDG_CONFIG_HOME/mlx/compositor.conf" || { echo "the Display page did not write night light" >&2; cat "$XDG_CONFIG_HOME/mlx/compositor.conf" >&2; exit 1; }
grep -q "night light 4000 K" "$work/pages-compositor.log" || { echo "the compositor did not read night light" >&2; cat "$work/pages-compositor.log" >&2; exit 1; }
rm -f "$XDG_CONFIG_HOME/mlx/compositor.conf"
echo "ok   mlx-settings' Sound page controls MLX Audio (output off, volume $((volume * 100 / 65536))%); the Display page reads the output and writes night light"

# Scenario 15: the permission dialog (projects/desktop/permissions). A
# sandboxed app (bwrap with a .flatpak-info) records; MLXIPC has to ask
# (audio.record), starts mlx-permissions from its service file, and its
# dialog opens on the compositor (at (24, 24); the question takes two
# lines: "Remember this decision" at y 153, Allow at (422, 198)). Both are
# clicked: the app records and ipc.conf keeps the answer. Then
# mlx-settings' Apps page (the category at y 166) lists the app's
# microphone as allowed; Deny (at (662, 208)) is clicked: ipc.conf says
# so.
if command -v bwrap > /dev/null && bwrap --bind / / true 2> /dev/null; then
    "$compiler" --quiet projects/desktop/permissions/main.mlx -o "$work/mlx-permissions"
    cat > "$work/services/org.mlx.PermissionAgent.service" <<SERVICE
[D-BUS Service]
Name=org.mlx.PermissionAgent
Exec=$work/mlx-permissions
SERVICE
    : > "$XDG_CONFIG_HOME/mlx/ipc.conf"
    # The bus starts before the compositor (as in mlx-session): the agent
    # gets the compositor's display from it.
    # The apps that asked: only this scenario's.
    XDG_STATE_HOME="$work/permission-state" WAYLAND_DISPLAY=nested-permission LC_ALL=C "$work/mlx-ipcd" --verbose --services "$work/services" > "$work/permission-ipc.log" 2>&1 &
    ipc_pid=$!
    for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/bus" ]] && break; sleep 0.1; done
    export DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"
    printf '[Application]\nname=org.example.Recorder\n' > "$work/flatpak-info"
    cat > "$work/record.sh" <<SCRIPT
#!/bin/sh
bwrap --bind / / --ro-bind "$work/flatpak-info" /.flatpak-info "$work/mlx-audio" record "$work/permitted.wav" 1 > "$work/record.log" 2>&1
echo "status \$?" >> "$work/record.log"
SCRIPT
    chmod +x "$work/record.sh"
    cat > "$work/permission.script" <<SCRIPT
wait 5000
pointer 123 153
press 272
release 272
wait 300
pointer 422 198
press 272
release 272
wait 2500
close
SCRIPT
    run_scenario permission "$work/permission.script" --no-fps --run "$work/record.sh"
    grep -q "^map: Permission" "$work/permission-compositor.log" || { echo "the permission dialog did not open" >&2; cat "$work/permission-ipc.log" "$work/permission-compositor.log" >&2; exit 1; }
    grep -q "status 0" "$work/record.log" && [[ -s "$work/permitted.wav" ]] || { echo "the app did not record after Allow: $(cat "$work/record.log")" >&2; cat "$work/permission-ipc.log" >&2; exit 1; }
    grep -qx "allow org.example.Recorder use audio.record" "$XDG_CONFIG_HOME/mlx/ipc.conf" || { echo "Remember did not keep the answer: $(cat "$XDG_CONFIG_HOME/mlx/ipc.conf")" >&2; cat "$work/permission-ipc.log" >&2; exit 1; }
    echo "ok   the permission dialog asks the user; Allow with Remember lets the app record and becomes a rule"
    cat > "$work/apps.script" <<SCRIPT
wait 2500
pointer 124 166
press 272
release 272
wait 1000
pointer 662 208
press 272
release 272
wait 1500
close
SCRIPT
    run_scenario apps "$work/apps.script" --no-fps --run "$work/mlx-settings"
    kill "$ipc_pid" 2> /dev/null || true
    pkill -x -P "$ipc_pid" mlx-audiod 2> /dev/null || true
    unset DBUS_SESSION_BUS_ADDRESS
    sleep 0.3
    grep -qx "deny org.example.Recorder use audio.record" "$XDG_CONFIG_HOME/mlx/ipc.conf" && [[ $(wc -l < "$XDG_CONFIG_HOME/mlx/ipc.conf") -eq 1 ]] || { echo "the Apps page did not deny the microphone: $(cat "$XDG_CONFIG_HOME/mlx/ipc.conf")" >&2; cat "$work/permission-ipc.log" >&2; exit 1; }
    grep -q "org.example.Recorder audio.record: deny" "$work/permission-ipc.log" || { echo "the bus got no SetPermission" >&2; cat "$work/permission-ipc.log" >&2; exit 1; }
    echo "ok   mlx-settings' Apps page lists the app's permission and Deny changes it (SetPermission)"
    : > "$XDG_CONFIG_HOME/mlx/ipc.conf"
else
    echo "skip the permission dialog (no working bwrap)"
fi

# Scenario 10: a GTK 3 window (libgtk-3 through ctypes: no GTK 3 program
# is needed). GTK 3 knows only KDE's server decoration, not xdg-decoration.
if python3 -c 'import ctypes; ctypes.CDLL("libgtk-3.so.0")' 2> /dev/null; then
    cat > "$work/gtk3.py" <<'PY'
import ctypes
gtk = ctypes.CDLL("libgtk-3.so.0")
glib = ctypes.CDLL("libglib-2.0.so.0")
gtk.gtk_init(None, None)
gtk.gtk_window_new.restype = ctypes.c_void_p
window = ctypes.c_void_p(gtk.gtk_window_new(0))
gtk.gtk_window_set_title(window, b"GTK 3")
gtk.gtk_window_set_default_size(window, 300, 200)
gtk.gtk_widget_show_all(window)
quit = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p)(lambda data: (gtk.gtk_main_quit(), 0)[1])
glib.g_timeout_add(5000, quit, None)
gtk.gtk_main()
PY
    cat > "$work/gtk3.sh" <<SCRIPT
#!/bin/sh
unset DISPLAY
WAYLAND_DEBUG=client GDK_DEBUG=no-portals GTK_A11Y=none NO_AT_BRIDGE=1 exec python3 "$work/gtk3.py" 2> "$work/gtk3-debug.log"
SCRIPT
    chmod +x "$work/gtk3.sh"
    printf 'wait 3000\nclose\n' > "$work/gtk3.script"
    run_scenario gtk3 "$work/gtk3.script" --run "$work/gtk3.sh"
    grep -q "^map: GTK 3" "$work/gtk3-compositor.log" || { echo "the GTK 3 window did not appear" >&2; cat "$work/gtk3-compositor.log" >&2; exit 1; }
    grep -q "org_kde_kwin_server_decoration@[0-9]*\.mode(2)" "$work/gtk3-debug.log" || { echo "GTK 3 was not told the compositor decorates" >&2; grep -i decoration "$work/gtk3-debug.log" >&2; exit 1; }
    echo "ok   GTK 3 uses the compositor's title bar (KDE server decoration)"
else
    echo "skip GTK 3 is not installed"
fi

# Scenario 6: copy and paste between two clients through the compositor's
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

# Scenario 7: a GTK 4 application (GTK 4 needs wl_data_device_manager to
# use a Wayland display at all), if available.
if command -v gtk4-widget-factory > /dev/null && command -v dbus-run-session > /dev/null; then
    # A session bus that activates no service: GTK still asks the portals
    # (org.freedesktop.portal.Desktop) for its settings, and where they are
    # installed but no desktop runs them, their activation can take longer
    # than this scenario waits, or mount the document portal under
    # XDG_RUNTIME_DIR; on this bus the name is unknown at once and GTK
    # goes on without it.
    cat > "$work/session.conf" <<'CONF'
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN" "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:tmpdir=/tmp</listen>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
CONF
    cat > "$work/gtk4.sh" <<SCRIPT
#!/bin/sh
unset DISPLAY
# No portals, no accessibility bus: both can take seconds to start (or
# time out) where the desktop's services are installed but not running.
GDK_DEBUG=no-portals GTK_A11Y=none NO_AT_BRIDGE=1 exec dbus-run-session --config-file="$work/session.conf" gtk4-widget-factory
SCRIPT
    chmod +x "$work/gtk4.sh"
    printf 'wait 12000\nclose\n' > "$work/gtk4.script"
    run_scenario gtk4 "$work/gtk4.script" --run "$work/gtk4.sh"
    grep -q "^map: GTK Widget Factory" "$work/gtk4-compositor.log" || { echo "the GTK 4 window did not appear" >&2; cat "$work/gtk4-compositor.log" >&2; exit 1; }
    echo "ok   a GTK 4 application (gtk4-widget-factory) opened its window"
else
    echo "skip gtk4-widget-factory or dbus-run-session is not installed"
fi
