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
#  11. mlx-settings records hotkeys: while it records, the compositor
#      passes every key to it (keyboard-shortcuts-inhibit), Super+Up too;
#      Backspace unbinds one; the compositor takes the new keys at once.
#  12. mlx-capture captures the screen and a window
#      (ext-image-copy-capture-v1): the PNGs agree, and a later frame
#      waits for damage.
#  13. with Xwayland and xev: an X program's window shows, takes focus and
#      gets keys (the compositor's own X window manager).
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
mkdir -p "$XDG_CONFIG_HOME/mlx"
# GTK's document portal may have mounted itself under the runtime directory.
trap 'fusermount -u "$XDG_RUNTIME_DIR/doc" 2> /dev/null || true; rm -rf -- "$work"' EXIT

"$compiler" --quiet projects/desktop/compositor/main.mlx -o "$work/mlx-compositor"
"$compiler" --quiet projects/desktop/terminal/main.mlx -o "$work/mlx-terminal"
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

# Scenario 11: mlx-settings (its window at (24, 24); the categories in
# its sidebar 28 pixels apart from y 44; on the Window management page the
# hotkey rows 34 pixels apart from y 336, on the General page the
# terminal's at y 186). Maximize gets Super+Shift+M; minimize gets
# Super+Up (the maximize hotkey: it must reach the window); the terminal
# hotkey is cleared; then Super+Up minimizes.
"$compiler" --quiet projects/desktop/settings/main.mlx -o "$work/mlx-settings"
rm -f "$XDG_CONFIG_HOME/mlx/compositor.conf"
cat > "$work/hotkeys.script" <<SCRIPT
wait 2500
pointer 124 110
press 272
release 272
wait 300
pointer 524 375
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
pointer 524 409
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
    cat > "$work/gtk4.sh" <<SCRIPT
#!/bin/sh
unset DISPLAY
# No portals: they would mount the document portal under XDG_RUNTIME_DIR.
# No accessibility bus either: both can take seconds to start (or time out)
# where the desktop's services are installed but not running.
GDK_DEBUG=no-portals GTK_A11Y=none NO_AT_BRIDGE=1 exec dbus-run-session gtk4-widget-factory
SCRIPT
    chmod +x "$work/gtk4.sh"
    printf 'wait 12000\nclose\n' > "$work/gtk4.script"
    run_scenario gtk4 "$work/gtk4.script" --run "$work/gtk4.sh"
    grep -q "^map: GTK Widget Factory" "$work/gtk4-compositor.log" || { echo "the GTK 4 window did not appear" >&2; cat "$work/gtk4-compositor.log" >&2; exit 1; }
    echo "ok   a GTK 4 application (gtk4-widget-factory) opened its window"
else
    echo "skip gtk4-widget-factory or dbus-run-session is not installed"
fi
