#!/usr/bin/env bash
# brixOS's session end, from the desktop down to the kernel, all native:
# the top bar's session menu (projects/desktop/topbar/session.mlx: Log
# Out, Restart, Shut Down, each asking first) tells the compositor on
# the bar's channel; a log out ends the session and sentinel starts the
# greeter again; for a restart or a shut down the compositor asks
# sentinel (org.mlx.Login.Restart and PowerOff on the system bus, MLXIPC's
# own format), which asks the bus who calls (the kernel names the
# program and its user to the bus: channel_peer) and does it only for the
# user who holds the seat, then has impulse end the system (its control,
# which only the system may write); the kernel powers the machine off by
# ACPI's S5 (the FADT's PM1 control, the DSDT's \_S5) or resets it (the
# FADT's reset register, port 0xCF9). The greeter (aurora) has the same
# two under its card.
#
# Two machines: QEMU's q35 (its tables name a reset register), started
# with -no-reboot, restarted from the greeter (QEMU ends at the reset);
# and QEMU's pc without -no-reboot, where a log out brings the greeter
# back, a restart from the session menu resets the machine into a second
# boot, and a shut down from the menu ends QEMU by itself. The pointer is
# moved and clicked through QEMU's monitor, which also dumps the screen
# and types the password.
#
#   tools/check_brix_session.sh [compiler]
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
"$compiler" --quiet $brix projects/desktop/ipc/main.mlx -o "$work/synapse"
"$compiler" --quiet $brix projects/desktop/login/main.mlx -o "$work/sentinel"
"$compiler" --quiet $brix projects/desktop/greeter/main.mlx -o "$work/aurora"
# What a program of the desktop does to ask for a restart or a power off,
# as a command (the serial shell runs it: root, without the seat).
"$compiler" --quiet $brix projects/brix/tools/powerask/main.mlx -o "$work/bin/powerask"
keymap=()
if [[ -f /usr/share/X11/xkb/rules/evdev ]]; then
    "$compiler" --quiet projects/brix/tools/keymap/main.mlx -o "$work/brix-keymap"
    "$work/brix-keymap" us "$work/us.xkb"
    keymap=("settings/keyboard/us.xkb=$work/us.xkb")
fi
for program in cat ls; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done

cp projects/brix/settings/init/services/serial-shell.service "$work/services/"
# The bus and the login as shipped, their logs on the serial console
# (--verbose on the bus: which app may use which permission).
sed -e 's/^exec = synapse /exec = synapse --verbose /' -e '$a log = console' \
    projects/brix/settings/init/services/ipcd.service > "$work/services/ipcd.service"
grep -q '^exec = synapse --verbose ' "$work/services/ipcd.service" || fail "ipcd.service is not the shipped one"
sed -e '$a log = console' \
    projects/brix/settings/init/services/login.service > "$work/services/login.service"
grep -q '^exec = sentinel --verbose ' "$work/services/login.service" || fail "login.service is not the shipped one"

entries=()
for service in "$work"/services/*.service; do entries+=("settings/init/services/$(basename "$service")=$service"); done
"$work/brix-image" "$work/kernel" "$work/brix.img" \
    "flux=$work/flux" "impulse=$work/impulse" "impulsectl=$work/impulsectl" "ignition=$work/ignition" \
    "kepler=$work/kepler" "vector=$work/vector" "launch=$work/launch" "orbit=$work/orbit" "horizon=$work/horizon" \
    "synapse=$work/synapse" "sentinel=$work/sentinel" "aurora=$work/aurora" \
    "settings/users=projects/brix/settings/users" "settings/groups=projects/brix/settings/groups" "settings/shadow=projects/brix/settings/shadow" \
    "share/wayland-sessions/brix.desktop=projects/brix/settings/share/wayland-sessions/brix.desktop" \
    "share/apps/Vector.app/app.ini=projects/desktop/apps/Vector.app/app.ini" \
    "share/apps/Vector.app/icon.png=projects/desktop/apps/Vector.app/icon.png" \
    "${keymap[@]}" "fonts/DejaVuSansMono.ttf=$fonts/DejaVuSansMono.ttf" "fonts/DejaVuSans.ttf=$fonts/DejaVuSans.ttf" \
    "${entries[@]}" "$work/bin/"
echo "ok   the kernel, the desktop, synapse, sentinel, aurora and a boot image with the bus and login services build"

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
    time.sleep(0.2)
    drain()
PY
}
# Of the screen now: how many pixels of the kind $1 the rectangle $2 $3
# (its left top) $4 $5 (its width and height) holds. light: text and
# symbols on the dark panels (every part above 170); blue: the button
# that does what a question asks (the accent colour).
pixels() {
    rm -f "$work/screen.ppm"
    monitor "screendump $work/screen.ppm"
    python3 - "$work/screen.ppm" "$@" <<'PY'
import sys, time
for _ in range(20):
    try:
        data = open(sys.argv[1], "rb").read()
        parts = data.split(b"\n", 3)
        width, height = map(int, parts[1].split())
        pixels = parts[3]
        if len(pixels) >= width * height * 3: break
    except (OSError, IndexError, ValueError):
        pass
    time.sleep(0.1)
else:
    print(0)
    sys.exit(0)
kind = sys.argv[2]
left, top, wide, high = map(int, sys.argv[3:7])
count = 0
for y in range(top, min(top + high, height)):
    row = y * width * 3
    for x in range(left, min(left + wide, width)):
        i = row + x * 3
        r, g, b = pixels[i], pixels[i + 1], pixels[i + 2]
        if kind == "light" and r > 170 and g > 170 and b > 170: count += 1
        if kind == "blue" and b > 150 and b - r > 60: count += 1
print(count)
PY
}
# Whether the rectangle $2 $3 $4 $5 comes to hold more than $6 pixels of
# the kind $1 within $7 looks (a third of a second apart); `seen`: how
# many it held at the last look.
seen=0
appears() {
    local i
    for (( i = 0; i < $7; i++ )); do
        seen=$(pixels "$1" "$2" "$3" "$4" "$5")
        (( seen > $6 )) && return 0
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.3
    done
    return 1
}
# Waits until it does ($1 says what that is).
wait_pixels() {
    local what=$1
    shift
    appears "$@" 100 || fail "$what is not on the screen ($seen $1 pixels at $2,$3 in $4x$5)"
}
# The pointer at $1 $2 of the screen, whatever it was at: into the top
# left corner (the compositor keeps it on the screen), then out to there
# in steps the PS/2 mouse's packets hold.
pointer() {
    local x=$1 y=$2 moves=("mouse_move -250 -250" "mouse_move -250 -250" "mouse_move -250 -250" "mouse_move -250 -250" "mouse_move -250 -250")
    while (( x > 0 || y > 0 )); do
        local dx=$(( x > 200 ? 200 : x )) dy=$(( y > 200 ? 200 : y ))
        moves+=("mouse_move $dx $dy")
        x=$(( x - dx ))
        y=$(( y - dy ))
    done
    monitor "${moves[@]}"
    sleep 0.3
}
click() {
    pointer "$1" "$2"
    monitor "mouse_button 1" "mouse_button 0"
    sleep 0.8
}
# A click at $2 $3 that makes the rectangle $5 $6 $7 $8 hold more than $9
# pixels of the kind $4 ($1 says what that is); clicked again when it
# does not (a click that came before the program looked is lost), three
# times at most.
click_for() {
    local what=$1 x=$2 y=$3
    shift 3
    for (( try = 0; try < 3; try++ )); do
        click "$x" "$y"
        appears "$@" 15 && return 0
    done
    fail "$what is not on the screen after a click at $x,$y ($seen $1 pixels at $2,$3 in $4x$5)"
}

# QEMU with the image and the options given, its serial line the root
# shell's, its monitor on the socket.
start() {
    rm -f "$work/input" "$work/monitor"
    mkfifo "$work/input"
    : > "$work/serial.txt"
    qemu-system-x86_64 -m 512M -display none -monitor "unix:$work/monitor,server,nowait" -cpu max -serial stdio "$@" \
        -kernel "$work/brix.img" < "$work/input" > "$work/serial.txt" 2>&1 &
    qemu_pid=$!
    exec 3> "$work/input"
    for (( i = 0; i < 100; i++ )); do [[ -S $work/monitor ]] && break; sleep 0.1; done
}
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(output | grep -a -c -- "$1" || true) < want )); do
        (( tries++ < 900 )) || fail "no \"$1\" (x$want) on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
# Waits for QEMU to end by itself ($1 says what should have ended it).
wait_end() {
    for (( i = 0; i < 300; i++ )); do kill -0 "$qemu_pid" 2> /dev/null || break; sleep 0.1; done
    kill -0 "$qemu_pid" 2> /dev/null && fail "QEMU still runs: $1"
    wait "$qemu_pid" 2> /dev/null || true
    qemu_pid=
    exec 3>&-
}

# Where things are on the 1024x768 screen QEMU's display has. The greeter:
# its card in the middle, under it "Restart ..." and "Shut Down ...", which
# a click turns into a question with Cancel and the button that does it
# (blue, in the rectangle named).
greeter_buttons=(380 575 270 30)
greeter_restart=(440 589)
greeter_confirm=(584 627)
greeter_confirm_button=(518 610 132 34)
# The session: the mark at the bar's left opens the session menu under it;
# its rows (the first, and where the last two's words stand), and the
# question's button that does it.
mark=(22 14)
row_settings=(20 90 250 30)
row_log_out=(88 178)
row_restart=(88 214)
row_shut_down=(88 250)
words_restart=(56 201 180 30)
words_shut_down=(56 237 180 30)
menu_confirm=(200 206)
menu_confirm_button=(147 190 115 32)

# The greeter is up $1 times now (since this QEMU started), and has the
# keyboard.
greeter_up() {
    wait_for '^login: greeter started$' "$1"
    wait_for '^ipc: :1\.[0-9]* is aurora (mlx)$' "$1"
    # Its two buttons under the card are drawn: it is on the screen.
    wait_pixels "the greeter with Restart and Shut Down under its card" light "${greeter_buttons[@]}" 60
}
# The password "brix" of the account brix, one key at a time, then Enter:
# the session is up $1 times now, its top bar drawn (the mark and the
# clock in it).
log_in() {
    sleep 1
    for key in b r i x ret; do monitor "sendkey $key"; sleep 0.3; done
    wait_for '^login: session started$' "$1"
    wait_for '^kepler: logging to /users/brix/' "$1"
    wait_pixels "the session's top bar" light 0 2 1024 26 150
    sleep 1
}
# The session menu opened with the pointer: its rows are there.
open_menu() {
    click_for "the session menu" "${mark[@]}" light "${row_settings[@]}" 40
}
# The row at $2 $3 of the open menu clicked ($1 says which): its question
# is there, with the button that does it.
ask() {
    click_for "the question of $1" "$2" "$3" blue "${menu_confirm_button[@]}" 1500
}

# ---------------------------------------------------------------------------
# q35: the kernel's line about ACPI, and a restart from the greeter.

start -machine q35 -no-reboot
wait_for '^brixOS: display 1024x768 '
wait_for '^brixOS: power off by ACPI: PM1a control at 0x[0-9a-f]*, S5 type [0-7].*; reset by ACPI.s reset register (port 0xcf9, value 0x[0-9a-f]*)$'
echo "ok   q35: $(output | grep -a '^brixOS: power off by ACPI' | head -1 | sed 's/^brixOS: //')"
greeter_up 1
echo "ok   the greeter shows Restart and Shut Down under its card"
click_for "the greeter's question before a restart" "${greeter_restart[@]}" blue "${greeter_confirm_button[@]}" 1500
click "${greeter_confirm[@]}"
# The bus's policy lets the greeter ask; sentinel finds its user holding
# the seat (nobody is logged in), ends the greeter and asks the init.
wait_for '^ipc: aurora may use login.power$'
wait_for '^login: restart asked by :1\.[0-9]*, user [0-9]*, who holds the seat$'
wait_for '^login: greeter stopped$'
wait_for '^impulse: reboot$'
wait_for '^brixOS: restarting$'
wait_for '^brixOS: reset by ACPI.s reset register (port 0xcf9, value 0x[0-9a-f]*)$'
wait_end "the kernel's write to ACPI's reset register did not reset q35"
echo "ok   Restart on the greeter: sentinel lets the user who holds the seat, impulse stops the services, the kernel resets q35 by ACPI's reset register"

# ---------------------------------------------------------------------------
# pc: log out, restart into a second boot, shut down.

start
wait_for '^brixOS: display 1024x768 '
wait_for '^brixOS: power off by ACPI: PM1a control at 0x[0-9a-f]*, S5 type [0-7].*; reset by port 0xcf9 (the tables name no reset register)$'
echo "ok   pc: $(output | grep -a '^brixOS: power off by ACPI' | head -1 | sed 's/^brixOS: //')"
greeter_up 1
log_in 1
echo "ok   the greeter logs brix in; the desktop is up"

# Who may: the serial shell is root, but root does not hold the seat. The
# bus lets the program ask (login.power), sentinel refuses its user, and
# nothing happens.
wait_for '^root@brix:[^#]*# '
printf 'DBUS_SYSTEM_BUS_ADDRESS=unix:path=/temporary/bus powerask poweroff\r' >&3
wait_for '^ipc: powerask may use login.power$'
wait_for '^login: power off refused: :1\.[0-9]* is user 0, the seat is user 1000.s$'
wait_for '^powerask: refused$'
output | grep -a -q '^impulse: poweroff$' && fail "a power off asked by a user who does not hold the seat reached the init"
echo "ok   a program of another user (root on the serial console) asks for a power off: sentinel refuses it, the init never hears of it"

# The session menu: five rows, the last two Restart and Shut Down.
open_menu
restart_row=$(pixels light "${words_restart[@]}")
shut_down_row=$(pixels light "${words_shut_down[@]}")
(( restart_row > 40 && shut_down_row > 40 )) || fail "the session menu has no Restart and Shut Down rows ($restart_row and $shut_down_row light pixels where they stand)"
echo "ok   the session menu has Restart and Shut Down below Log Out ($restart_row and $shut_down_row light pixels in their rows)"

# Log Out: asked first, then the session ends and the greeter is back.
ask "Log Out" "${row_log_out[@]}"
click "${menu_confirm[@]}"
# (Not anchored: the serial shell's prompt stands before it.)
wait_for 'login: session ended; the seat is free$'
greeter_up 2
echo "ok   Log Out (after its question) ends the session; sentinel starts the greeter again"
log_in 2
echo "ok   a second login works"

# Restart: the compositor closes the windows and asks sentinel, which
# finds brix holding the seat; the kernel resets the machine, which boots
# again.
open_menu
ask "Restart" "${row_restart[@]}"
click "${menu_confirm[@]}"
wait_for '^ipc: kepler may use login.power$'
wait_for '^login: restart asked by :1\.[0-9]*, user 1000, who holds the seat$'
wait_for '^login: session stopped$'
wait_for '^impulse: reboot$'
wait_for '^brixOS: restarting$'
wait_for '^brixOS: reset by port 0xcf9$'
wait_for '^brixOS: kernel at ' 2
wait_for '^brixOS: power off by ACPI: ' 2
echo "ok   Restart in the session menu: sentinel lets brix, who holds the seat; the kernel resets the machine (port 0xcf9) and it boots again"

# The second boot. Shut Down: the same way, and the kernel powers the
# machine off by ACPI: QEMU ends by itself.
greeter_up 3
log_in 3
open_menu
ask "Shut Down" "${row_shut_down[@]}"
click "${menu_confirm[@]}"
wait_for '^login: power off asked by :1\.[0-9]*, user 1000, who holds the seat$'
wait_for '^impulse: poweroff$'
wait_for '^brixOS: powering off$'
wait_for '^brixOS: power off by ACPI: S5 type [0-7] to PM1a control at 0x[0-9a-f]*$'
wait_end "the kernel's ACPI power off did not end the machine"
echo "ok   Shut Down in the session menu, after the second boot: the kernel powers the machine off by ACPI (S5 to PM1a control) and QEMU ends by itself"
echo "PASS brixOS session end: log out, restart and shut down from the desktop to ACPI, all native"
