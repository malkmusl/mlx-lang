#!/usr/bin/env bash
# brixOS's desktop draws what changed, not the screen. The whole picture is
# the processor's work (clients draw into shared memory, kepler composes
# into the framebuffer), so a frame costs what it touches:
#
# - vector (std.ui.terminal) keeps what each cell shows and draws the
#   cells that changed; text that scrolls is moved a row of pixels at a
#   time. It names the rectangle to the compositor (wl_surface.damage).
# - A client that names nothing (the top bar, the dock) has its frame
#   compared with the one before by std.ui's panel, and the rectangle
#   that differs is the damage: the clock's digits, an icon under the
#   pointer.
# - kepler takes that part of the buffer and composes that part of the
#   output.
#
# With MLX_FRAMES_SAY=1 in the desktop's environment each client says its
# frames on its errors and kepler what it composed:
#
#     frame: org.mlx.terminal 652x396 drawn in 153 us, changed 16x16 at 110,22
#     composed 16x16 at 296,208 of 1024x768
#
# Here, in QEMU: one character typed in the terminal is two cells drawn
# and composed (the character's and the cursor's); a minute's turn is the
# clock's digits; the pointer on the dock is less than the dock; and the
# picture reached by frames of parts (a line typed, a file's text
# scrolling by) is, pixel for pixel, the picture the terminal draws whole
# after its window was made large and small again: nothing stale.
#
# Before that, on the host: the panel's part of it (which buffer a frame
# is drawn into, what of it is brought up to date, the rectangle named)
# against a model of a compositor that takes only the damage
# (tests/support/panel_damage_model.mlx): its copy of the surface must be
# the client's picture after every frame.
#
#   tools/check_brix_draw.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
fonts=/usr/share/fonts/truetype/dejavu

# First, without a display: what the panel tells a compositor, on a model
# of one that takes of each buffer only the damage (as kepler does). The
# steps below see the top bar's and the dock's frames be small; whether
# the rectangle the comparison finds holds all that changed (a pixel
# short, and old pixels stay on the screen) is seen here, for a client
# that keeps its frames, one that names nothing and one that names its
# damage.
model=$(mktemp -d)
"$compiler" --quiet tests/support/panel_damage_model.mlx -o "$model/model" || { rm -rf -- "$model"; echo "FAIL tests/support/panel_damage_model.mlx did not compile" >&2; exit 1; }
model_status=0
"$model/model" || model_status=$?
rm -rf -- "$model"
(( model_status == 13 )) || { echo "FAIL the panel's damage on a model of a compositor (tests/support/panel_damage_model.mlx): check $model_status" >&2; exit 1; }
echo "ok   the panel's damage on a model of a compositor: 600000 frames of three kinds of client, the compositor's copy is the picture after each"

if ! command -v qemu-system-x86_64 > /dev/null; then
    echo "skip: no qemu-system-x86_64"
    exit 0
fi
if ! command -v python3 > /dev/null; then
    echo "skip: no python3 for QEMU's monitor"
    exit 0
fi
if [[ ! -f $fonts/DejaVuSansMono.ttf || ! -f $fonts/DejaVuSans.ttf ]]; then
    echo "skip: no DejaVu fonts in $fonts"
    exit 0
fi
work=$(mktemp -d)
qemu_pid=
cleanup() {
    exec 3>&- 2> /dev/null || true
    [[ -n "$qemu_pid" ]] && kill "$qemu_pid" 2> /dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
fail() { echo "FAIL $1" >&2; [[ -f "$work/serial.txt" ]] && cat -v "$work/serial.txt" | tail -n 40 >&2; exit 1; }

brix=--target=x86_64-brixos
mkdir -p "$work/bin" "$work/services"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet $brix projects/init/brixos.mlx -o "$work/impulse"
"$compiler" --quiet $brix projects/init/tool.mlx -o "$work/impulsectl"
"$compiler" --quiet $brix projects/desktop/splash/brixos.mlx -o "$work/ignition"
"$compiler" --quiet $brix projects/desktop/compositor/main.mlx -o "$work/kepler"
"$compiler" --quiet $brix projects/desktop/terminal/main.mlx -o "$work/vector"
"$compiler" --quiet $brix projects/desktop/launcher/main.mlx -o "$work/launch"
"$compiler" --quiet $brix projects/desktop/dock/main.mlx -o "$work/orbit"
"$compiler" --quiet $brix projects/desktop/topbar/main.mlx -o "$work/horizon"
keymap=()
if [[ -f /usr/share/X11/xkb/rules/evdev ]]; then
    "$compiler" --quiet projects/brix/tools/keymap/main.mlx -o "$work/brix-keymap"
    "$work/brix-keymap" us "$work/us.xkb"
    keymap=("settings/keyboard/us.xkb=$work/us.xkb")
fi
for program in cat echo; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
cp projects/brix/settings/init/services/serial-shell.service "$work/services/"
# The desktop service as shipped, its log on the serial console, every
# frame said (MLX_FRAMES_SAY), and no frames-per-second counters in the
# title bars (they are drawn anew each second while frames go by: frames of
# the compositor's own, not of what is checked here).
sed -e 's/^exec = kepler$/exec = kepler --verbose --no-fps/' -e '/^disabled = yes$/d' -e '$a log = console' -e '$a env = MLX_COMPOSITOR_LOG=-' -e '$a env = MLX_FRAMES_SAY=1' \
    projects/brix/settings/init/services/desktop.service > "$work/services/desktop.service"
grep -q '^exec = kepler --verbose --no-fps$' "$work/services/desktop.service" || fail "desktop.service is not the shipped one"
# A file to cat: forty lines, more than the terminal's window holds.
for (( line = 1; line <= 40; line++ )); do
    printf 'line %d of the file: its text goes by, a row of pixels at a time\n' "$line"
done > "$work/lines"
entries=()
for service in "$work"/services/*.service; do entries+=("settings/init/services/$(basename "$service")=$service"); done
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "impulse=$work/impulse" "impulsectl=$work/impulsectl" "ignition=$work/ignition" "kepler=$work/kepler" "vector=$work/vector" "launch=$work/launch" "orbit=$work/orbit" "horizon=$work/horizon" "share/apps/Vector.app/app.ini=projects/desktop/apps/Vector.app/app.ini" "share/apps/Vector.app/icon.png=projects/desktop/apps/Vector.app/icon.png" "${keymap[@]}" "fonts/DejaVuSansMono.ttf=$fonts/DejaVuSansMono.ttf" "fonts/DejaVuSans.ttf=$fonts/DejaVuSans.ttf" "share/lines=$work/lines" "${entries[@]}" "$work/bin/"
echo "ok   the kernel, impulse, ignition, kepler, vector, the launcher, the dock, the top bar and a boot image whose desktop says its frames build"

# The QEMU monitor on a socket: keys, the mouse, screen dumps.
monitor() {
    python3 - "$work/monitor" "$@" <<'PY'
import socket, sys, time
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sys.argv[1])
s.settimeout(0.3)
def drain():
    try:
        while s.recv(4096): pass
    except socket.timeout:
        pass
drain()
for command in sys.argv[2:]:
    s.sendall((command + "\n").encode())
    time.sleep(0.1)
    drain()
PY
}
# Types a text through the monitor's sendkey.
type_keys() {
    local commands=()
    while IFS= read -r key; do commands+=("sendkey $key"); done < <(python3 - "$1" <<'PY'
import sys
names = {" ": "spc", "/": "slash", ".": "dot", "-": "minus", "\n": "ret"}
for c in sys.argv[1]:
    print(names.get(c, c))
PY
)
    monitor "${commands[@]}"
}

rm -f "$work/input"
mkfifo "$work/input"
: > "$work/serial.txt"
# The clock starts ten seconds past a minute: the minute's turn comes while
# the desktop is up, not a minute after.
qemu-system-x86_64 -m 256M -display none -monitor "unix:$work/monitor,server,nowait" -no-reboot -cpu max -serial stdio -net none \
    -rtc base=2026-03-02T10:00:10 -kernel "$work/brix.img" < "$work/input" > "$work/serial.txt" 2>&1 &
qemu_pid=$!
exec 3> "$work/input"
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(output | grep -a -c -- "$1" || true) < want )); do
        (( tries++ < 1200 )) || fail "no \"$1\" (x$want) on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
# How many lines the serial line has, and those after that many.
mark() { output | wc -l; }
since() { output | tail -n +"$(( $1 + 1 ))"; }
# Waits until no frame was said or composed for two seconds (the desktop
# is at rest: what glides has arrived).
settle() {
    local last=-1 now=0 quiet=0 tries=0
    while (( quiet < 20 )); do
        now=$(output | grep -a -c '^frame: \|^composed ' || true)
        if [[ $now == "$last" ]]; then quiet=$(( quiet + 1 )); else quiet=0; fi
        last=$now
        (( tries++ < 900 )) || fail "the desktop does not come to rest"
        sleep 0.1
    done
}
# Of the lines on stdin: the largest part of the output composed in one
# frame, in pixels (0: nothing composed), and the output's size. With
# `below-the-bar`, what was composed within the top bar's 30 rows does not
# count (its clock may turn at any time).
largest_composed() {
    python3 -c '
import re, sys
most = 0
whole = 0
for line in sys.stdin:
    m = re.match(r"composed (\d+)x(\d+) at (\d+),(\d+) of (\d+)x(\d+)", line.strip())
    if m:
        whole = int(m.group(5)) * int(m.group(6))
        if len(sys.argv) > 1 and int(m.group(4)) + int(m.group(2)) <= 30: continue
        most = max(most, int(m.group(1)) * int(m.group(2)))
print(most, whole)
' "$@"
}
# Of the lines on stdin: the frames `$1` drew, as "WIDTH HEIGHT CHANGED X
# MICROSECONDS": the surface's size, the pixels of the rectangle changed
# (all of them for "all", 0 for "nothing"), where that rectangle begins
# from the left, and how long the draw took.
frames_of() {
    python3 -c '
import re, sys
for line in sys.stdin:
    m = re.match(r"frame: " + re.escape(sys.argv[1]) + r" (\d+)x(\d+) drawn in (\d+) us, changed (.*)$", line.strip())
    if not m: continue
    w, h, took, what = int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4)
    # (Another program may have written onto the end of the line.)
    part = re.match(r"(\d+)x(\d+) at (\d+),\d+", what)
    if what.startswith("all"): print(w, h, w * h, 0, took)
    elif what.startswith("nothing"): print(w, h, 0, 0, took)
    elif part: print(w, h, int(part.group(1)) * int(part.group(2)), int(part.group(3)), took)
' "$1"
}
# The terminal's window in a PPM dump: the box of the compositor's
# title-bar blue (#5aa0ff: the focused window's bar and border), as
# "LEFT TOP RIGHT BOTTOM" ("0 0 0 0": none).
window_box() {
    python3 - "$1" <<'PY'
import sys
data = open(sys.argv[1], "rb").read()
parts = data.split(b"\n", 3)
width, height = map(int, parts[1].split())
pixels = parts[3]
left, top, right, bottom = width, height, 0, 0
for y in range(height):
    row = pixels[y * width * 3:(y + 1) * width * 3]
    for x in range(width):
        r, g, b = row[x * 3], row[x * 3 + 1], row[x * 3 + 2]
        if abs(r - 0x5a) < 24 and abs(g - 0xa0) < 24 and b > 220:
            if x < left: left = x
            if x > right: right = x
            if y < top: top = y
            if y > bottom: bottom = y
print(left, top, right, bottom) if right > left else print(0, 0, 0, 0)
PY
}
# How many pixels of two PPM dumps differ inside the box `$3 $4 $5 $6`
# (left, top, right, bottom), and how many of the box are not the first
# one's most common colour there (the text: a picture of one colour would
# compare equal and say nothing).
compare_box() {
    python3 - "$@" <<'PY'
import sys
def load(path):
    data = open(path, "rb").read()
    parts = data.split(b"\n", 3)
    width, height = map(int, parts[1].split())
    return width, height, parts[3]
w1, h1, a = load(sys.argv[1])
w2, h2, b = load(sys.argv[2])
left, top, right, bottom = map(int, sys.argv[3:7])
if (w1, h1) != (w2, h2):
    print(-1, 0)
    sys.exit(0)
differ = 0
colours = {}
for y in range(top, bottom):
    one = a[(y * w1 + left) * 3:(y * w1 + right) * 3]
    other = b[(y * w1 + left) * 3:(y * w1 + right) * 3]
    if one != other:
        differ += sum(1 for x in range(0, len(one), 3) if one[x:x + 3] != other[x:x + 3])
    for x in range(0, len(one), 3):
        colours[one[x:x + 3]] = colours.get(one[x:x + 3], 0) + 1
ink = (right - left) * (bottom - top) - max(colours.values())
print(differ, ink)
PY
}
# A dump of the screen into `$1`, whole (asked for again until it is).
dump() {
    local i
    for (( i = 0; i < 20; i++ )); do
        rm -f "$1"
        monitor "screendump $1"
        sleep 0.5
        [[ -s $1 ]] && python3 -c 'import sys; d = open(sys.argv[1], "rb").read(); p = d.split(b"\n", 3); w, h = map(int, p[1].split()); sys.exit(0 if len(p[3]) >= w * h * 3 else 1)' "$1" 2> /dev/null && return 0
    done
    fail "no whole screen dump"
}

for (( i = 0; i < 100; i++ )); do [[ -S $work/monitor ]] && break; sleep 0.1; done
wait_for '^impulse: brixOS.s init$'
wait_for '^settings: border #5aa0ff'
wait_for '^layer: orbit '
wait_for '^layer: horizon 1024x30 at 0,0 in layer 2$'
# The helpers say their frames (std.ui's panel under MLX_FRAMES_SAY).
wait_for '^frame: horizon 1024x30 drawn in [0-9]* us, changed '
wait_for '^frame: orbit [0-9]*x[0-9]* drawn in [0-9]* us, changed '
echo "ok   the desktop is up; the top bar and the dock say their frames, kepler what it composed"

# The minute's turn (the machine's clock started at ten past a minute): the
# desktop at rest, the top bar draws its clock anew, a frame that differs
# from the one before in the digits at its right, and kepler composes them
# with what blurs behind them, not the bar.
settle
before=$(mark)
tick=
for (( i = 0; i < 900 && ${#tick} == 0; i++ )); do
    tick=$(since "$before" | frames_of horizon | awk '$3 > 0 && $3 <= 6000 && $4 > $1 / 2 { print; exit }')
    sleep 0.1
done
[[ -n $tick ]] || fail "the top bar said no frame that changed its clock alone within a minute and a half"
read -r bar_width bar_height bar_changed bar_at bar_took <<< "$tick"
settle
bar_wholes=$(since "$before" | frames_of horizon | awk '$3 == $1 * $2 { n++ } END { print n + 0 }')
(( bar_wholes == 0 )) || fail "the top bar said $bar_wholes frames changed all of it while only its clock turned"
read -r composed whole < <(since "$before" | largest_composed)
(( composed > 0 )) || fail "kepler composed nothing for the clock's turn"
(( composed * 2 <= bar_width * bar_height )) || fail "kepler composed $composed pixels for the clock's turn: the bar has $(( bar_width * bar_height )), its clock changed $bar_changed"
echo "ok   a minute's turn: the top bar changes $bar_changed of its $(( bar_width * bar_height )) pixels (the clock's digits, found by comparing the frame with the one before), kepler composes $composed of $whole at most"

# Alt+Enter: the terminal. Its first frame is all of its window.
monitor "sendkey alt-ret"
wait_for '^focus: Mlx Terminal$'
wait_for '^frame: org.mlx.terminal [0-9]*x[0-9]* drawn in [0-9]* us, changed all$'
settle

# One character typed: the terminal draws two cells (the character's and
# the cursor's new one), says that rectangle, and kepler composes it.
before=$(mark)
monitor "sendkey a"
for (( i = 0; i < 200; i++ )); do [[ -n $(since "$before" | frames_of org.mlx.terminal | awk '$3 > 0') ]] && break; sleep 0.1; done
settle
typed=$(since "$before" | frames_of org.mlx.terminal | awk '$3 > 0 { print; exit }')
[[ -n $typed ]] || fail "the terminal said no frame for a character typed"
read -r window_width window_height changed changed_at took <<< "$typed"
(( changed > 0 && changed <= 2048 )) || fail "a character typed changed $changed pixels of the terminal's ${window_width}x${window_height} window, not two cells (512 pixels of a 15 pixel font)"
read -r composed whole < <(since "$before" | largest_composed below-the-bar)
(( composed > 0 )) || fail "kepler composed nothing for the character typed"
(( composed <= 4096 )) || fail "kepler composed $composed pixels of $whole for one character typed (the terminal changed $changed)"
echo "ok   one character typed: the terminal changes $changed pixels of its ${window_width}x${window_height} window (two cells, drawn in $took us here), kepler composes $composed of $whole"

# A line typed and a file's forty lines: frames of parts (the text
# scrolls: rows moved, the new lines drawn). Then the screen as those
# frames left it.
before=$(mark)
monitor "sendkey backspace"
type_keys $'cat /system/share/lines\n'
# (Until the text went by: a frame that moved the rows changes more than
# half of the window.)
moved=0
for (( i = 0; i < 300 && moved == 0; i++ )); do
    moved=$(since "$before" | frames_of org.mlx.terminal | awk -v all=$(( window_width * window_height )) '$3 * 2 > all && $3 < all { n++ } END { print n + 0 }')
    sleep 0.1
done
settle
parts=$(since "$before" | frames_of org.mlx.terminal | awk -v all=$(( window_width * window_height )) '$3 > 0 && $3 < all { n++ } END { print n + 0 }')
wholes=$(since "$before" | frames_of org.mlx.terminal | awk -v all=$(( window_width * window_height )) '$3 == all { n++ } END { print n + 0 }')
(( moved > 0 )) || fail "no frame of the terminal moved its rows for a file's forty lines (the text did not scroll by)"
(( parts >= 14 )) || fail "the terminal drew $parts frames of parts for a line typed and a file's text (and $wholes whole ones)"
(( wholes == 0 )) || fail "the terminal drew $wholes frames whole while text was typed and scrolled"
dump "$work/parts.ppm"
read -r left top right bottom < <(window_box "$work/parts.ppm")
(( right - left > 300 && bottom - top > 200 )) || fail "the terminal's window is not on the screen dump (the box of its title bar and border: $left,$top to $right,$bottom)"
echo "ok   a line typed and a file's text scrolling by: $parts frames of parts ($moved of them moved the rows), none whole"

# The window made large and small again (Super+Up, twice): the terminal
# has new buffers each time and draws all of it. The same text, drawn
# whole: the screen is the one the frames of parts left, pixel for pixel
# inside the window (under its title bar).
before=$(mark)
monitor "sendkey meta_l-up"
wait_for '^maximize: Mlx Terminal$'
settle
monitor "sendkey meta_l-up"
wait_for '^restore: Mlx Terminal$'
settle
wholes=$(since "$before" | frames_of org.mlx.terminal | awk '$3 == $1 * $2 { n++ } END { print n + 0 }')
(( wholes >= 2 )) || fail "the terminal drew $wholes whole frames for its window made large and small again"
last=$(since "$before" | frames_of org.mlx.terminal | tail -1)
[[ $last == "$window_width $window_height "* ]] || fail "the terminal's window is not its first size again (its last frame: $last)"
dump "$work/whole.ppm"
read -r left2 top2 right2 bottom2 < <(window_box "$work/whole.ppm")
[[ "$left $top $right $bottom" == "$left2 $top2 $right2 $bottom2" ]] || fail "the window is at $left2,$top2 to $right2,$bottom2 after it was made large and small again, not at $left,$top to $right,$bottom"
read -r differ ink < <(compare_box "$work/parts.ppm" "$work/whole.ppm" $(( left + 8 )) $(( top + 40 )) $(( right - 8 )) $(( bottom - 8 )))
(( ink > 2000 )) || fail "the window shows no text to compare ($ink pixels that are not its background)"
(( differ == 0 )) || fail "$differ pixels of the window differ between the picture frames of parts left and the picture drawn whole: something stale"
echo "ok   the window made large and small again is drawn whole ($wholes whole frames), and its picture is the one the frames of parts left: 0 of its pixels differ ($ink of them text)"

# The dock: the pointer onto it (the screen's middle bottom). What changes
# is the icon under it and its name, not the dock.
before=$(mark)
monitor "mouse_move -114 200" "mouse_move 0 129"
for (( i = 0; i < 100; i++ )); do [[ -n $(since "$before" | frames_of orbit) ]] && break; sleep 0.1; done
settle
dock=$(since "$before" | frames_of orbit | awk '$3 > 0 { if (best == "" || $3 < small) { small = $3; best = $0 } } END { print best }')
[[ -n $dock ]] || fail "the dock said no frame that changed something for the pointer on it"
read -r dock_width dock_height dock_changed dock_at dock_took <<< "$dock"
(( dock_changed < dock_width * dock_height )) || fail "the pointer on the dock changed all $dock_changed pixels of it"
echo "ok   the pointer on the dock: $dock_changed of its $(( dock_width * dock_height )) pixels change (an icon and its name), found by comparing the frame with the one before"

printf 'impulsectl poweroff\r' >&3
wait_for '^brixOS: powering off$'
for (( i = 0; i < 100; i++ )); do kill -0 "$qemu_pid" 2> /dev/null || break; sleep 0.1; done
kill -0 "$qemu_pid" 2> /dev/null && fail "QEMU did not power off"
qemu_pid=
echo PASS
