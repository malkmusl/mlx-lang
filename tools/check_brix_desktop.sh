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
# The settings app (control) on its Network page shows what the kernel
# says of the link (QEMU's user network on a virtio network device:
# /devices/net0's record, read natively), and follows the cable.
# The screen is dumped through QEMU's monitor, which also moves and
# clicks the mouse and pulls the cable.
#
#   tools/check_brix_desktop.sh [compiler]
set -euo pipefail
. "$(dirname "$0")/brix_classic_look.sh"

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
"$compiler" --quiet $brix projects/desktop/settings/main.mlx -o "$work/control"
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
classic_look "$work"
sed -e 's/^exec = kepler$/exec = kepler --verbose/' -e '/^disabled = yes$/d' -e '$a log = console' -e '$a env = MLX_COMPOSITOR_LOG=-' \
    -e "\$a $classic_env" projects/brix/settings/init/services/desktop.service > "$work/services/desktop.service"
grep -q '^exec = kepler --verbose$' "$work/services/desktop.service" || fail "desktop.service is not the shipped one"
entries=()
for service in "$work"/services/*.service; do entries+=("settings/init/services/$(basename "$service")=$service"); done
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "impulse=$work/impulse" "impulsectl=$work/impulsectl" "ignition=$work/ignition" "kepler=$work/kepler" "vector=$work/vector" "launch=$work/launch" "orbit=$work/orbit" "horizon=$work/horizon" "control=$work/control" "share/apps/Vector.app/app.ini=projects/desktop/apps/Vector.app/app.ini" "share/apps/Vector.app/icon.png=projects/desktop/apps/Vector.app/icon.png" "${keymap[@]}" "fonts/DejaVuSansMono.ttf=$fonts/DejaVuSansMono.ttf" "fonts/DejaVuSans.ttf=$fonts/DejaVuSans.ttf" "${entries[@]}" "${classic_entries[@]}" "$work/bin/"
echo "ok   the kernel, impulse, ignition, kepler, vector, the launcher, the dock, the top bar, control$([[ ${#keymap[@]} -gt 0 ]] && echo ", the us keymap") and a boot image with the desktop service build"

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
# Of a PPM dump with the settings window in front: how many pixels of its
# title bar are blue right of column 700 (where the terminal's window
# does not reach), and how many are light where its page's rows have
# their values (columns 480 to 700, rows 190 to 400).
page() {
    python3 - "$1" <<'PY'
import sys
data = open(sys.argv[1], "rb").read()
parts = data.split(b"\n", 3)
width, height = map(int, parts[1].split())
pixels = parts[3]
bar = 0
text = 0
for y in range(height):
    row = y * width * 3
    for x in range(width):
        i = row + x * 3
        r, g, b = pixels[i], pixels[i + 1], pixels[i + 2]
        if x > 700 and y < 60 and abs(r - 0x5a) < 24 and abs(g - 0xa0) < 24 and b > 220: bar += 1
        if 480 < x < 700 and 190 < y < 400 and r > 200 and g > 200 and b > 200: text += 1
print(bar, text)
PY
}

rm -f "$work/input"
mkfifo "$work/input"
: > "$work/serial.txt"
qemu-system-x86_64 -m 256M -display none -monitor "unix:$work/monitor,server,nowait" -no-reboot -cpu max -serial stdio \
    -kernel "$work/brix.img" -netdev user,id=n0 -device virtio-net-pci,netdev=n0 < "$work/input" > "$work/serial.txt" 2>&1 &
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
# The splash handed its last frame on in a file (/temporary/screen-picture,
# projects/desktop/splash/picture.mlx): the compositor's first frames fade
# in from that, and the framebuffer is never read back. (Not anchored:
# the serial shell's prompt may stand before it.)
wait_for 'brix: fading in from the picture the program before left$'
echo "ok   the compositor fades in from the picture the splash left in /temporary (the framebuffer is not read back)"
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
# The top bar's workspace controls (in its middle: a button per
# workspace, then the floating/tiling button), both ways on the bar's
# channel: a click on the button "2" (481 to 505 from the left while two
# show; the cursor is at 445,713) reaches the compositor, which switches
# and tells the bar the workspaces; the bar then shows a third button and
# its tiling part has moved to 557 to 585, where the next click turns
# tiling on for workspace 2 (a bar that had not heard of the switch would
# not be hit there, and would name workspace 1). Then back to workspace 1.
monitor "mouse_move 48 -200" "mouse_move 0 -200" "mouse_move 0 -200" "mouse_move 0 -98" "mouse_button 1" "mouse_button 0"
wait_for '^workspace: 2$'
sleep 1
monitor "mouse_move 78 0" "mouse_button 1" "mouse_button 0"
wait_for '^tiling: on (workspace 2)$'
sleep 1
monitor "mouse_move -120 0" "mouse_button 1" "mouse_button 0"
wait_for '^workspace: 1$'
echo "ok   the top bar's workspace buttons and its tiling button work on the bar's channel, both ways (workspace 2, tiling on there, back to 1)"
# The settings app on its Network page, started from the serial shell as
# one more client of the compositor. On brixOS the kernel runs the
# network: the page reads each link's record from /devices/net0 (natively)
# and says on its errors what it shows, once each time that changes.
# (Not anchored: the serial shell's prompt stands before these lines.)
printf 'XDG_RUNTIME_DIR=/temporary WAYLAND_DISPLAY=wayland-mlx control network &\r' >&3
wait_for 'map: Settings$'
wait_for 'control: network: net0 (virtio, 52:54:00:12:34:56) connected, 10.0.2.15/24 by DHCP, gateway 10.0.2.2, name server 10.0.2.3$'
# The page on the screen: the settings window in front (its title bar)
# with the link's rows in it.
bar=0
rows=0
for (( i = 0; i < 20; i++ )); do
    monitor "screendump $work/network.ppm"
    sleep 0.3
    read -r bar rows < <(page "$work/network.ppm")
    (( bar > 1500 && rows > 200 )) && break
    sleep 0.3
done
(( bar > 1500 && rows > 200 )) || fail "the Network page is not on the display ($bar title-bar pixels, $rows of its rows)"
echo "ok   control's Network page shows what the kernel says of net0: connected, 10.0.2.15/24 by DHCP, its gateway and name server ($rows pixels of its rows on the display)"
# The cable pulled (QEMU's set_link): the page reads the kernel again
# within two seconds and shows it.
monitor "set_link n0 off"
wait_for 'control: network: net0 (virtio, 52:54:00:12:34:56) no cable, 10.0.2.15/24 by DHCP, gateway 10.0.2.2, name server 10.0.2.3$'
echo "ok   the page follows the link on its own: the cable pulled, it says no cable"
printf 'impulsectl poweroff\r' >&3
wait_for '^impulse: poweroff$'
# Told to end (the terminate event), the compositor leaves its last frame
# for whoever takes the screen next.
wait_for 'brix: the last frame left for the next program$'
wait_for '^brixOS: powering off$'
for (( i = 0; i < 100; i++ )); do kill -0 "$qemu_pid" 2> /dev/null || break; sleep 0.1; done
kill -0 "$qemu_pid" 2> /dev/null && fail "QEMU did not power off"
qemu_pid=
echo "ok   impulsectl poweroff stops the desktop (which leaves its last frame for the next program) and the machine powers off"
echo PASS
