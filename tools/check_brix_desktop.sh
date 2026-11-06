#!/usr/bin/env bash
# brixOS's desktop (projects/desktop/compositor with its brixOS backend
# drm.brixos.mlx, std.wayland's transport over channels, vector as
# a client): impulse starts kepler as the screen service
# (desktop.service) after the boot splash; it draws on the framebuffer,
# reads the keyboard and the mouse, listens for Wayland clients on a
# listener node (/temporary/wayland-mlx), hands its clients the keymap
# from /system/settings/keyboard (compiled on the host by brix-keymap,
# when the host has the XKB data), and starts its helpers with
# process_spawn, each on a channel (children.brixos.mlx): the dock and
# the top bar take their layers, the dock's apps button shows the
# launcher; Alt+Enter opens vector, whose window the compositor
# maps; what is typed goes through the compositor (wl_keyboard) to the
# terminal and its flux on a session pair; the mouse moves the cursor.
# The screen is dumped through QEMU's monitor, which also moves and
# clicks the mouse.
#
#   tools/check_brix_desktop.sh [compiler]
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
for program in cat echo uname; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
cp projects/brix/settings/init/services/serial-shell.service "$work/services/"
# The desktop service as shipped, its log on the serial console
# (--verbose: clients, windows, focus and forwarded keys).
sed -e 's/^exec = kepler$/exec = kepler --verbose/' -e '/^disabled = yes$/d' -e '$a log = console' -e '$a env = MLX_COMPOSITOR_LOG=-' \
    projects/brix/settings/init/services/desktop.service > "$work/services/desktop.service"
grep -q '^exec = kepler --verbose$' "$work/services/desktop.service" || fail "desktop.service is not the shipped one"
entries=()
for service in "$work"/services/*.service; do entries+=("settings/init/services/$(basename "$service")=$service"); done
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "impulse=$work/impulse" "impulsectl=$work/impulsectl" "ignition=$work/ignition" "kepler=$work/kepler" "vector=$work/vector" "launch=$work/launch" "orbit=$work/orbit" "horizon=$work/horizon" "share/applications/vector.desktop=projects/desktop/terminal/vector.desktop" "${keymap[@]}" "fonts/DejaVuSansMono.ttf=$fonts/DejaVuSansMono.ttf" "fonts/DejaVuSans.ttf=$fonts/DejaVuSans.ttf" "${entries[@]}" "$work/bin/"
echo "ok   the kernel, impulse, ignition, kepler, vector, the launcher, the dock, the top bar$([[ ${#keymap[@]} -gt 0 ]] && echo ", the us keymap") and a boot image with the desktop service build"

# The QEMU monitor on a socket: screen dumps, keys, the mouse.
monitor() {
    python3 - "$work/monitor" "$@" <<'PY'
import socket, sys, time
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sys.argv[1])
s.settimeout(0.5)
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
# Of a PPM dump: how many pixels are the compositor's title-bar blue
# (the focused window's bar and border, #5aa0ff), and how many are white
# (the cursor's arrow and the terminal's text) right of column 700,
# between the top bar and the dock.
measure() {
    python3 - "$1" <<'PY'
import sys
data = open(sys.argv[1], "rb").read()
parts = data.split(b"\n", 3)
width, height = map(int, parts[1].split())
pixels = parts[3]
blue = 0
white_right = 0
for y in range(height):
    row = y * width * 3
    for x in range(width):
        i = row + x * 3
        r, g, b = pixels[i], pixels[i + 1], pixels[i + 2]
        if abs(r - 0x5a) < 24 and abs(g - 0xa0) < 24 and b > 220: blue += 1
        if x > 700 and 40 < y < 600 and r > 240 and g > 240 and b > 240: white_right += 1
print(blue, white_right)
PY
}

rm -f "$work/input"
mkfifo "$work/input"
: > "$work/serial.txt"
qemu-system-x86_64 -m 256M -display none -monitor "unix:$work/monitor,server,nowait" -no-reboot -cpu max -serial stdio \
    -kernel "$work/brix.img" < "$work/input" > "$work/serial.txt" 2>&1 &
qemu_pid=$!
exec 3> "$work/input"
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(output | grep -c -- "$1" || true) < want )); do
        (( tries++ < 600 )) || fail "no \"$1\" (x$want) on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
for (( i = 0; i < 100; i++ )); do [[ -S $work/monitor ]] && break; sleep 0.1; done
wait_for '^impulse: brixOS.s init$'
wait_for '^impulse: services defined: 2$'
wait_for '^root@brix:[^#]*# '
# The splash quits for the screen service; the compositor says its settings.
wait_for '^ignition: told to quit, the picture stays$'
wait_for '^settings: border #5aa0ff'
echo "ok   impulse starts the desktop as the screen service after the splash; kepler is up on the framebuffer"
if [[ ${#keymap[@]} -gt 0 ]]; then
    wait_for '^keyboard: keymap from /system/settings/keyboard$'
    echo "ok   the compositor hands its clients the keymap of /system/settings/keyboard/us.xkb (brix-keymap, from the host's XKB data)"
fi
# The helpers, started with process_spawn on their channels: the dock
# and the top bar as layers.
wait_for '^launch: orbit$'
wait_for '^layer: orbit '
wait_for '^layer: horizon 1024x30 at 0,0 in layer 2$'
echo "ok   the compositor starts its helpers natively (process_spawn): the dock and the top bar take their layers"
# Alt+Enter: the compositor starts vector, which connects over the
# listener node and maps its window.
monitor "sendkey alt-ret"
wait_for '^client connected$'
wait_for '^map: Mlx Terminal$'
wait_for '^focus: Mlx Terminal$'
echo "ok   Alt+Enter starts vector: it connects over /temporary/wayland-mlx (a channel), its window is mapped and focused"
# The window on the screen: its blue title bar and border.
blue=0
white=0
for (( i = 0; i < 20; i++ )); do
    monitor "screendump $work/window.ppm"
    sleep 0.3
    read -r blue white < <(measure "$work/window.ppm")
    (( blue > 8000 )) && break
    sleep 0.3
done
(( blue > 8000 )) || fail "the terminal's window is not on the display ($blue title-bar pixels)"
echo "ok   the window is on the display: its title bar and border ($blue blue pixels)"
# Keys through the compositor to the terminal's shell: a line written
# to a file, read from the serial shell.
monitor "sendkey e" "sendkey c" "sendkey h" "sendkey o" "sendkey spc" "sendkey t" "sendkey y" "sendkey p" "sendkey e" "sendkey d" \
    "sendkey spc" "sendkey shift-dot" "sendkey slash" "sendkey t" "sendkey e" "sendkey m" "sendkey p" "sendkey o" "sendkey r" "sendkey a" "sendkey r" "sendkey y" \
    "sendkey slash" "sendkey k" "sendkey e" "sendkey y" "sendkey s" "sendkey ret"
sleep 1
printf 'cat /temporary/keys\r' >&3
wait_for '^typed$'
echo "ok   what is typed goes through the compositor (wl_keyboard) to vector and its flux on a session pair"
# The mouse: QEMU's PS/2 mouse moves the cursor from the screen's middle
# to its right, outside the window.
read -r blue white < <(measure "$work/window.ppm")
(( white < 5 )) || fail "something white is right of the window before the mouse moves ($white pixels)"
monitor "mouse_move 200 100" "mouse_move 100 60"
moved=0
for (( i = 0; i < 20; i++ )); do
    monitor "screendump $work/mouse.ppm"
    sleep 0.3
    read -r blue moved < <(measure "$work/mouse.ppm")
    (( moved > 20 )) && break
    sleep 0.3
done
(( moved > 20 )) || fail "the cursor did not move right ($moved white pixels right of the window)"
echo "ok   the mouse (/devices/mouse, PS/2 packets by IRQ 12) moves the cursor ($moved pixels of it right of the window)"
# A click on the dock's apps button (the dock's middle bottom, 353+45
# from the left and 618+95 from the top; the cursor is at 812,544 now):
# the dock tells the compositor on its channel, which shows the launcher.
monitor "mouse_move -200 100" "mouse_move -167 69" "mouse_button 1" "mouse_button 0"
wait_for '^layer: launch '
wait_for '^focus: launch$'
echo "ok   the dock's apps button (a message on its channel) shows the launcher, which takes the keyboard"
monitor "sendkey esc"
printf 'impulsectl poweroff\r' >&3
wait_for '^impulse: poweroff$'
wait_for '^brixOS: powering off$'
for (( i = 0; i < 100; i++ )); do kill -0 "$qemu_pid" 2> /dev/null || break; sleep 0.1; done
kill -0 "$qemu_pid" 2> /dev/null && fail "QEMU did not power off"
qemu_pid=
echo "ok   impulsectl poweroff stops the desktop and the machine powers off"
echo PASS
