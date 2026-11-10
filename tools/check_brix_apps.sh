#!/usr/bin/env bash
# The apps of brixOS's desktop as its helpers show them (the launcher
# launch, the dock orbit, the top bar horizon; projects/desktop/shared/
# apps.mlx): one notion of which app a window, a pin or a name stands
# for, and of its name and its icon. The image is the desktop's (the
# manner of tools/check_brix_desktop.sh: kepler as the screen service,
# its log on the serial line) with the bundles the build ships:
# Vector.app, Polaris.app and Control.app in /system/share/apps, each an
# app.ini (with Id=, the app id its program gives its windows) and an
# icon.png.
#
# Each app is started from the launcher by the keyboard (Super, the first
# letters of its name, Enter). Then
# - the top bar names its window by the entry's Name ("Polaris", not
#   "org.mlx.files"): the bar's line, and for Polaris and Control the
#   width of the name on the screen;
# - the dock shows it with the entry's icon, the bundle's icon.png: the
#   dock's line naming the file, and the icon's colour counted on the
#   screen (Polaris's blue, Control's grey, Vector's green);
# - Vector's icon is the same file in the launcher and in the dock, and
#   its green is on the screen in both (the dock draws no sign of its own
#   for the terminal when it has an entry);
# - "Pin to Dock" in the dock's menu (a right click on Control's icon)
#   pins: the pins file (/users/root/.config/mlx/dock: the settings of
#   the desktop's user) names the app, and after the desktop is started
#   again (impulsectl restart desktop: a new compositor, a new dock) it
#   is still pinned, its icon in the dock with no window open; "Unpin from
#   Dock" takes it out again; the launcher's menu (a right click on an
#   app there) pins too, and unpins an app that a pins file of before
#   names twice; and Control dragged from the launcher onto the dock is
#   pinned (the drop's data through a native stream).
# The helpers say what they show because the compositor runs --verbose
# (MLX_DESKTOP_VERBOSE in their environment). Every check that fails is
# listed; the first does not end the run.
#
#   tools/check_brix_apps.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
fonts=/usr/share/fonts/truetype/dejavu
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
fail() { echo "FAIL $1" >&2; [[ -f "$work/serial.txt" ]] && cat -v "$work/serial.txt" | tail -n 30 >&2; exit 1; }
# A check that failed: said, counted, and the run goes on.
problems=0
problem() { echo "FAIL $1"; problems=$((problems + 1)); }

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
"$compiler" --quiet $brix projects/desktop/files/main.mlx -o "$work/polaris"
"$compiler" --quiet $brix projects/desktop/settings/main.mlx -o "$work/control"
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
# The desktop service as shipped, its log on the serial console
# (--verbose: windows and focus, and the helpers say what they show).
sed -e 's/^exec = kepler$/exec = kepler --verbose/' -e '/^disabled = yes$/d' -e '$a log = console' -e '$a env = MLX_COMPOSITOR_LOG=-' \
    projects/brix/settings/init/services/desktop.service > "$work/services/desktop.service"
grep -q '^exec = kepler --verbose$' "$work/services/desktop.service" || fail "desktop.service is not the shipped one"
entries=()
for service in "$work"/services/*.service; do entries+=("settings/init/services/$(basename "$service")=$service"); done
# The bundles as build.mlx's image command names them.
for app in Vector Polaris Control; do
    entries+=("share/apps/$app.app/app.ini=projects/desktop/apps/$app.app/app.ini" "share/apps/$app.app/icon.png=projects/desktop/apps/$app.app/icon.png")
done
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "impulse=$work/impulse" "impulsectl=$work/impulsectl" "ignition=$work/ignition" "kepler=$work/kepler" "vector=$work/vector" "launch=$work/launch" "orbit=$work/orbit" "horizon=$work/horizon" "polaris=$work/polaris" "control=$work/control" "${keymap[@]}" "fonts/DejaVuSansMono.ttf=$fonts/DejaVuSansMono.ttf" "fonts/DejaVuSans.ttf=$fonts/DejaVuSans.ttf" "${entries[@]}" "$work/bin/"
echo "ok   the kernel, the desktop with polaris and control, and a boot image with the bundles Vector.app, Polaris.app and Control.app build"

# The QEMU monitor on a socket: screen dumps, keys, the mouse.
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
    time.sleep(0.15)
    drain()
PY
}
# Of a PPM dump, five numbers. In the dock's strip (the screen's last
# rows): the pixels of Polaris's blue, of Vector's green and of
# Control's grey. In the launcher's first row of apps: the pixels of
# Vector's green. And how wide the app's name at the top bar's left is
# (from its first lit column to its last, left of the workspace buttons).
measure() {
    python3 - "$1" <<'PY'
import sys
data = open(sys.argv[1], "rb").read()
parts = data.split(b"\n", 3)
width, height = map(int, parts[1].split())
pixels = parts[3]
def is_green(r, g, b): return g > 150 and r < 80 and 80 < b < 180 and g - r > 100
blue = green = grey = launcher_green = 0
for y in range(height - 72, height - 8):
    row = y * width * 3
    for x in range(width):
        i = row + x * 3
        r, g, b = pixels[i], pixels[i + 1], pixels[i + 2]
        if b > 200 and g > 110 and r < 110 and b - r > 110: blue += 1
        if is_green(r, g, b): green += 1
        if 140 < r < 200 and abs(r - g) < 14 and 0 <= b - r < 26: grey += 1
for y in range(200, 290):
    row = y * width * 3
    for x in range(160, 860):
        i = row + x * 3
        if is_green(pixels[i], pixels[i + 1], pixels[i + 2]): launcher_green += 1
left, right = width, -1
for y in range(4, 26):
    row = y * width * 3
    for x in range(42, 440):
        i = row + x * 3
        if pixels[i] > 170 and pixels[i + 1] > 170 and pixels[i + 2] > 170:
            left = min(left, x)
            right = max(right, x)
print(blue, green, grey, launcher_green, right - left + 1 if right >= left else 0)
PY
}
# Looks at the screen: the measures into blue, green, grey (the dock's),
# launcher_green and name.
look() {
    local numbers
    monitor "screendump $work/$1.ppm"
    sleep 0.4
    numbers=$(measure "$work/$1.ppm" || true)
    read -r blue green grey launcher_green name <<< "${numbers:-0 0 0 0 0}"
}

rm -f "$work/input"
mkfifo "$work/input"
: > "$work/serial.txt"
qemu-system-x86_64 -m 384M -display none -monitor "unix:$work/monitor,server,nowait" -no-reboot -cpu max -serial stdio \
    -kernel "$work/brix.img" < "$work/input" > "$work/serial.txt" 2>&1 &
qemu_pid=$!
exec 3> "$work/input"
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
count_of() { output | grep -c -- "$1" || true; }
# Waits for a line (x times); the run ends without it.
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(count_of "$1") < want )); do
        (( tries++ < 900 )) || fail "no \"$1\" (x$want) on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
# Whether a line comes (x times) within twenty seconds: for what a
# helper says of what it shows. (The helpers' lines are not anchored at
# their start: the shell's prompt or a part of the compositor's line may
# stand before one; each goes out in one piece, so its end is its end.)
comes() {
    local tries=0 want=${2:-1}
    while (( $(count_of "$1") < want )); do
        (( tries++ < 200 )) || return 1
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
# The cursor (in the screen's middle when the compositor starts) to a
# place on the 1024x768 screen: QEMU's mouse moves it by what is asked.
px=512
py=384
pointer() {
    local steps=() dx dy
    while (( px != $1 || py != $2 )); do
        dx=$(( $1 - px ))
        dy=$(( $2 - py ))
        (( dx > 150 )) && dx=150
        (( dx < -150 )) && dx=-150
        (( dy > 150 )) && dy=150
        (( dy < -150 )) && dy=-150
        steps+=("mouse_move $dx $dy")
        px=$((px + dx))
        py=$((py + dy))
    done
    (( ${#steps[@]} == 0 )) || monitor "${steps[@]}"
    sleep 0.6
}
# Super shows the launcher, which takes the keyboard.
show_launcher() {
    local shown
    shown=$(count_of '^focus: launch$')
    monitor "sendkey meta_l"
    wait_for '^focus: launch$' $((shown + 1))
    sleep 1
}
# An app from the launcher by the keyboard: the first letters of its
# name narrow the list to it, Enter starts it; its window is mapped. Then
# what the top bar and the dock say of it is waited for (bar_said,
# dock_said: whether they did) and the screen is looked at.
start_app() {
    local windows
    windows=$(count_of '^window: ')
    show_launcher
    monitor "sendkey $1" "sendkey $2" "sendkey $3" "sendkey ret"
    wait_for "$4"
    wait_for '^window: ' $((windows + 1))
    bar_said=no
    dock_said=no
    comes "$5" && bar_said=yes
    comes "$6" && dock_said=yes
    sleep 1.5
    look "$7"
}
# The pins file into `pins`, its lines between spaces: read in the serial
# shell between two marks.
reads=0
read_pins() {
    reads=$((reads + 1))
    printf 'echo pins-%s-begin; cat /users/root/.config/mlx/dock; echo pins-%s-end\r' "$reads" "$reads" >&3
    wait_for "^pins-$reads-end\$"
    pins=" $(output | awk -v first="pins-$reads-begin" -v last="pins-$reads-end" '$0 == last { on = 0 } on { print } $0 == first { on = 1 }' | tr '\n' ' ')"
}

for (( i = 0; i < 100; i++ )); do [[ -S $work/monitor ]] && break; sleep 0.1; done
wait_for '^impulse: brixOS.s init$'
wait_for '^settings: border #5aa0ff'
wait_for '^launch: orbit$'
wait_for '^layer: orbit '
wait_for '^layer: horizon 1024x30 at 0,0 in layer 2$'
wait_for '^root@brix:[^#]*# '
sleep 2
echo "ok   the desktop is up: the compositor, the launcher, the dock and the top bar"

# The launcher read the bundles: each app with its own icon.png.
launcher_icon=
if comes 'launch: shows Vector (org.mlx.terminal) with /system/share/apps/Vector.app/icon.png$' \
    && comes 'launch: shows Polaris (org.mlx.files) with /system/share/apps/Polaris.app/icon.png$' \
    && comes 'launch: shows Control (org.mlx.settings) with /system/share/apps/Control.app/icon.png$'; then
    launcher_icon=$(output | sed -n 's/.*launch: shows Vector (org\.mlx\.terminal) with \(.*\)$/\1/p' | tail -n 1)
    echo "ok   the launcher shows each app by its entry (the id its program gives its windows) with its bundle's icon.png"
else
    problem "the launcher does not say which entry and icon it shows for Vector, Polaris and Control"
fi

# Polaris: its window's app id is org.mlx.files. (The name's width: "Mlx",
# the bar without a window, is 21 pixels, an app id 78 and more.)
start_app p o l 'polaris: drawing on the ' \
    'horizon: shows Polaris for org.mlx.files$' \
    'orbit: shows Polaris (org.mlx.files) with /system/share/apps/Polaris.app/icon.png, 1 window, \(not \)\?pinned$' polaris
if [[ $bar_said == yes ]] && (( name > 30 && name < 62 )); then
    echo "ok   the top bar names Polaris's window \"Polaris\" (its entry's Name; $name pixels wide)"
else
    problem "the top bar does not name Polaris's window by its entry (the name there is $name pixels wide: \"org.mlx.files\" is about 78, \"Polaris\" about 41)"
fi
if [[ $dock_said == yes ]] && (( blue > 800 )); then
    echo "ok   the dock shows Polaris's window with its icon ($blue pixels of its blue in the dock)"
else
    problem "the dock does not show Polaris's window with its icon ($blue pixels of its blue in the dock; over 800 with it)"
fi

# Control: org.mlx.settings.
start_app c o n 'control: drawing on the ' \
    'horizon: shows Control for org.mlx.settings$' \
    'orbit: shows Control (org.mlx.settings) with /system/share/apps/Control.app/icon.png, 1 window, not pinned$' control
if [[ $bar_said == yes ]] && (( name > 30 && name < 62 )); then
    echo "ok   the top bar names Control's window \"Control\" ($name pixels wide)"
else
    problem "the top bar does not name Control's window by its entry (the name there is $name pixels wide: \"org.mlx.settings\" is about 100, \"Control\" about 44)"
fi
if [[ $dock_said == yes ]] && (( grey > 900 )); then
    echo "ok   the dock shows Control's window with its icon ($grey pixels of its grey in the dock)"
else
    problem "the dock does not show Control's window with its icon ($grey pixels of its grey in the dock; over 900 with it)"
fi

# Vector: org.mlx.terminal. The same icon in the launcher and the dock.
start_app v e c '^map: Mlx Terminal$' \
    'horizon: shows Vector for org.mlx.terminal$' \
    'orbit: shows Vector (org.mlx.terminal) with /system/share/apps/Vector.app/icon.png, 1 window, ' vector
in_dock=$green
if [[ $bar_said == yes ]]; then
    echo "ok   the top bar names the terminal's window \"Vector\""
else
    problem "the top bar does not name the terminal's window \"Vector\", its entry's Name"
fi
show_launcher
look launcher
monitor "sendkey esc"
sleep 1
dock_icon=
[[ $dock_said == yes ]] && dock_icon=$(output | sed -n 's/.*orbit: shows Vector (org\.mlx\.terminal) with \(.*\), 1 window, .*$/\1/p' | tail -n 1)
if [[ -n "$dock_icon" && "$dock_icon" == "$launcher_icon" ]] && (( in_dock > 700 && launcher_green > 900 )); then
    echo "ok   Vector has the same icon in the launcher and in the dock: $dock_icon ($launcher_green pixels of its green in the launcher, $in_dock in the dock)"
else
    problem "Vector's icon is not the same in the launcher and in the dock (the launcher took \"$launcher_icon\", the dock \"$dock_icon\"; $launcher_green pixels of its green in the launcher, $in_dock in the dock: over 900 and over 700 with it)"
fi

# "Pin to Dock" in the dock's menu. The dock now: the apps button, Vector
# and Polaris (pinned without a pins file: the terminal and the file
# manager), a separator, Control (it runs and is not pinned), a
# separator, the trash: 330 pixels wide in the screen's middle, Control's
# icon around 570,728. A right click opens its menu above it: "New
# Window", "Pin to Dock" (39 pixels a row; 97 above the icon's middle),
# "Close Window".
pointer 570 728
monitor "mouse_button 2" "mouse_button 0"
sleep 2
pointer 570 631
monitor "mouse_button 1" "mouse_button 0"
sleep 2
pinned=no
read_pins
if [[ $pins == *" org.mlx.settings "* ]] && comes 'orbit: shows Control (org.mlx.settings) with /system/share/apps/Control.app/icon.png, 1 window, pinned$'; then
    pinned=yes
    echo "ok   \"Pin to Dock\" in the dock's menu pins Control: /users/root/.config/mlx/dock names org.mlx.settings and the dock shows it pinned"
else
    problem "\"Pin to Dock\" in the dock's menu did not pin Control (the pins file:$pins)"
fi

# The desktop again (a new compositor and new helpers): the pin is there,
# Control's icon in the dock with no window open.
bars=$(count_of '^layer: horizon 1024x30 at 0,0 in layer 2$')
docks=$(count_of 'orbit: drawing on the ')
printf 'impulsectl restart desktop\r' >&3
wait_for '^layer: horizon 1024x30 at 0,0 in layer 2$' $((bars + 1))
wait_for 'orbit: drawing on the ' $((docks + 1))
px=512
py=384
sleep 3
look again
read_pins
if [[ $pins == *" org.mlx.settings "* ]] && comes 'orbit: shows Control (org.mlx.settings) with /system/share/apps/Control.app/icon.png, 0 windows, pinned$' && (( grey > 900 )); then
    echo "ok   after the desktop is started again Control is still pinned (its icon in the dock with no window open: $grey pixels of its grey)"
else
    pinned=no
    problem "after the desktop is started again Control is not pinned ($grey pixels of its grey in the dock; the pins file:$pins)"
fi

# "Unpin from Dock": the dock is the apps button, Vector, Polaris and
# Control, a separator and the trash (313 wide), Control's icon around
# 561,728; its menu: "New Window", "Unpin from Dock" (58 above).
if [[ $pinned == yes ]]; then
    pointer 561 728
    monitor "mouse_button 2" "mouse_button 0"
    sleep 2
    pointer 561 670
    monitor "mouse_button 1" "mouse_button 0"
    sleep 2
    read_pins
    if [[ $pins == *" org.mlx.settings "* ]]; then
        problem "\"Unpin from Dock\" in the dock's menu did not unpin Control (the pins file:$pins)"
    else
        echo "ok   \"Unpin from Dock\" in the dock's menu takes Control out of the pins file"
    fi
fi

# The launcher's menu (a right click on an app there: Control's cell is
# the first, 232,246; the menu opens at the pointer, its one entry 60 to
# the right and 21 below): "Pin to Dock" pins, and the dock takes it in.
in_dock=$(count_of 'orbit: shows Control (org.mlx.settings) with /system/share/apps/Control.app/icon.png, 0 windows, pinned$')
show_launcher
pointer 232 246
monitor "mouse_button 2" "mouse_button 0"
sleep 2
pointer 292 267
monitor "mouse_button 1" "mouse_button 0"
sleep 2
read_pins
if [[ $pins == *" org.mlx.settings "* ]] && comes 'orbit: shows Control (org.mlx.settings) with /system/share/apps/Control.app/icon.png, 0 windows, pinned$' $((in_dock + 1)); then
    echo "ok   \"Pin to Dock\" in the launcher's menu pins Control: the pins file names org.mlx.settings and the dock shows it"
else
    problem "\"Pin to Dock\" in the launcher's menu did not pin Control (the pins file:$pins)"
fi
# A pins file of before the apps had ids names an app by its bundle (the
# launcher wrote that); with its id beside it Control is pinned twice.
# "Unpin from Dock" in the launcher's menu takes both, the others stay.
printf 'echo terminal > /users/root/.config/mlx/dock; echo org.mlx.files >> /users/root/.config/mlx/dock; echo Control >> /users/root/.config/mlx/dock; echo org.mlx.settings >> /users/root/.config/mlx/dock; echo pins-of-before\r' >&3
wait_for '^pins-of-before$'
pointer 232 246
monitor "mouse_button 2" "mouse_button 0"
sleep 2
pointer 292 267
monitor "mouse_button 1" "mouse_button 0"
sleep 2
read_pins
if [[ $pins == *" terminal "* && $pins == *" org.mlx.files "* && $pins != *" Control "* && $pins != *" org.mlx.settings "* ]]; then
    echo "ok   \"Unpin from Dock\" in the launcher's menu takes every pin of Control (two in a pins file of before: its bundle's name and its id), the others stay"
else
    problem "\"Unpin from Dock\" in the launcher's menu did not unpin Control (the pins file:$pins: terminal and org.mlx.files are what stays)"
fi
monitor "sendkey esc"
sleep 1

# Control dragged from the launcher (its cell is the first: 232,246) onto
# the dock (between Polaris and the trash): pinned where it lands. The
# drop's data comes through a pipe of the system's own (a stream pair).
show_launcher
pointer 232 246
monitor "mouse_button 1"
sleep 0.5
pointer 262 276
pointer 566 728
sleep 1
monitor "mouse_button 0"
dragged=no
comes 'orbit: pinned org.mlx.settings$' && dragged=yes
read_pins
if [[ $dragged == yes && $pins == *" org.mlx.settings "* ]]; then
    echo "ok   Control dragged from the launcher onto the dock is pinned (its bundle as the drop's data, through a native stream)"
else
    problem "Control dragged from the launcher onto the dock is not pinned (the pins file:$pins)"
fi
monitor "sendkey esc"

printf 'impulsectl poweroff\r' >&3
wait_for '^impulse: poweroff$'
wait_for '^brixOS: powering off$'
for (( i = 0; i < 100; i++ )); do kill -0 "$qemu_pid" 2> /dev/null || break; sleep 0.1; done
kill -0 "$qemu_pid" 2> /dev/null && fail "QEMU did not power off"
qemu_pid=
if (( problems > 0 )); then
    echo "$problems of the checks failed"
    exit 1
fi
echo PASS
