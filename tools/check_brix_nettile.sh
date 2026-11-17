#!/usr/bin/env bash
# The network on brixOS's top bar (horizon: projects/desktop/topbar,
# network.brixos.mlx): the bar reads the kernel's links natively
# (/devices/net0 and on, std.os.brix's networkLink) on its tick, and its
# control centre's network tile and the sign on the bar, left of the
# toggles, show what the kernel says: a cable with its address, else no
# cable, no address yet, or no device. The image is the desktop's (the
# manner of tools/check_brix_desktop.sh: kepler as the screen service
# with --verbose, its log and its helpers' on the serial line); the bar
# says what its tile shows in a line each time that changes.
#
# Three machines:
# - QEMU's user network on a virtio card: "horizon: network: net0
#   connected, 10.0.2.15", the sign a network on the screen; the cable
#   pulled (the monitor's set_link): "net0 no cable", the sign crossed
#   out; the control centre opened with the pointer: its tile is not lit;
#   the cable back in: connected again, and the open tile lights up by
#   itself; a click on the tile starts the settings on their Network page
#   (control's own line of what the kernel says);
# - no network device (-nic none): "horizon: network: no links";
# - a wire without a DHCP server: "horizon: network: net0 no address yet".
#
#   tools/check_brix_nettile.sh [compiler]
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
mkdir -p "$work/services"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet $brix projects/init/brixos.mlx -o "$work/impulse"
"$compiler" --quiet $brix projects/init/tool.mlx -o "$work/impulsectl"
"$compiler" --quiet $brix projects/desktop/splash/brixos.mlx -o "$work/ignition"
"$compiler" --quiet $brix projects/desktop/compositor/main.mlx -o "$work/kepler"
"$compiler" --quiet $brix projects/desktop/launcher/main.mlx -o "$work/launch"
"$compiler" --quiet $brix projects/desktop/dock/main.mlx -o "$work/orbit"
"$compiler" --quiet $brix projects/desktop/topbar/main.mlx -o "$work/horizon"
"$compiler" --quiet $brix projects/desktop/settings/main.mlx -o "$work/control"
cp projects/brix/settings/init/services/serial-shell.service "$work/services/"
# The desktop service as shipped, its log on the serial console
# (--verbose: windows, and the helpers say what they show).
classic_look "$work"
sed -e 's/^exec = kepler$/exec = kepler --verbose/' -e '/^disabled = yes$/d' -e '$a log = console' -e '$a env = MLX_COMPOSITOR_LOG=-' \
    -e "\$a $classic_env" projects/brix/settings/init/services/desktop.service > "$work/services/desktop.service"
grep -q '^exec = kepler --verbose$' "$work/services/desktop.service" || fail "desktop.service is not the shipped one"
entries=()
for service in "$work"/services/*.service; do entries+=("settings/init/services/$(basename "$service")=$service"); done
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "impulse=$work/impulse" "impulsectl=$work/impulsectl" "ignition=$work/ignition" "kepler=$work/kepler" "launch=$work/launch" "orbit=$work/orbit" "horizon=$work/horizon" "control=$work/control" "fonts/DejaVuSansMono.ttf=$fonts/DejaVuSansMono.ttf" "fonts/DejaVuSans.ttf=$fonts/DejaVuSans.ttf" "${entries[@]}" "${classic_entries[@]}"
echo "ok   the kernel, the desktop with the top bar and control, and a boot image with the desktop service build"

# The QEMU monitor on a socket: screen dumps, the mouse, the cable.
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
# Of the screen now, five numbers. The bar's right holds, from the right
# edge in, the clock's text, the control centre's toggles and, left of
# them, the network's sign (18 pixels wide, 57 left of where the clock's
# text starts; the text is found as the columns lit at the right with no
# gap of 15 columns in them): how many pixels of the sign's place are
# light (every part above 170: the network's boxes and lines are, of the
# sign crossed out only its stroke), how many are lit at all (above 90),
# and the place's left edge (0: no clock on the bar yet). Then the control
# centre's first tile while the popup is open (it is 360 wide and ends 8
# from the screen's right edge; the tile is at 672,92): how many pixels of
# its circle are the accent's blue (the tile lit), and how many of its two
# lines of words are lit (the popup is there).
measure() {
    rm -f "$work/screen.ppm"
    monitor "screendump $work/screen.ppm"
    python3 - "$work/screen.ppm" <<'PY'
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
    print(0, 0, 0, 0, 0)
    sys.exit(0)
def at(x, y):
    i = (y * width + x) * 3
    return pixels[i], pixels[i + 1], pixels[i + 2]
def column_lit(x): return any(min(at(x, y)) > 120 for y in range(5, 25))
text = 0
gap = 0
for x in range(width - 1, width // 2, -1):
    if column_lit(x):
        text = x
        gap = 0
    elif text:
        gap += 1
        if gap >= 15: break
light = lit = blue = words = 0
left = text - 57 if text else 0
if text:
    for y in range(6, 24):
        for x in range(left, left + 18):
            if min(at(x, y)) > 170: light += 1
            if min(at(x, y)) > 90: lit += 1
for y in range(103, 135):
    for x in range(682, 714):
        r, g, b = at(x, y)
        if b > 150 and b - r > 60: blue += 1
for y in range(98, 140):
    for x in range(724, 828):
        if min(at(x, y)) > 120: words += 1
print(light, lit, left, blue, words)
PY
}
# Looks at the screen: the measures into light, lit, sign_left, blue and
# words.
look() {
    local numbers
    numbers=$(measure || true)
    read -r light lit sign_left blue words <<< "${numbers:-0 0 0 0 0}"
}
# Looks until the condition $1 holds (thirty looks at most); false when it
# never did.
light=0
lit=0
sign_left=0
blue=0
words=0
shows() {
    local i
    for (( i = 0; i < 30; i++ )); do
        look
        if eval "(( $1 ))"; then return 0; fi
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.3
    done
    return 1
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

# QEMU with the image and the network options given, its serial line the
# root shell's, its monitor on the socket.
start() {
    rm -f "$work/input" "$work/monitor"
    mkfifo "$work/input"
    : > "$work/serial.txt"
    qemu-system-x86_64 -m 256M -display none -monitor "unix:$work/monitor,server,nowait" -no-reboot -cpu max -serial stdio "$@" \
        -kernel "$work/brix.img" < "$work/input" > "$work/serial.txt" 2>&1 &
    qemu_pid=$!
    exec 3> "$work/input"
    for (( i = 0; i < 100; i++ )); do [[ -S $work/monitor ]] && break; sleep 0.1; done
}
# Ends the QEMU we started (the checks of this machine are done).
stop() {
    kill "$qemu_pid" 2> /dev/null || true
    wait "$qemu_pid" 2> /dev/null || true
    qemu_pid=
    exec 3>&-
}
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
count_of() { output | grep -a -c -- "$1" || true; }
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(count_of "$1") < want )); do
        (( tries++ < 900 )) || fail "no \"$1\" (x$want) on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
# Whether a line comes (x times) within thirty seconds: for what the bar
# says of the network. (Its lines are not anchored at their start: the
# shell's prompt may stand before one; each goes out in one piece, so its
# end is its end.)
comes() {
    local tries=0 want=${2:-1}
    while (( $(count_of "$1") < want )); do
        (( tries++ < 300 )) || return 1
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
# The desktop is up: the compositor and the top bar's layer.
desktop_up() {
    wait_for '^impulse: brixOS.s init$'
    wait_for '^settings: border #5aa0ff'
    wait_for '^layer: horizon 1024x30 at 0,0 in layer 2$'
}

# ---------------------------------------------------------------------------
# A virtio card on QEMU's user network: connected, the cable out and in
# again, the tile's click.

start -netdev user,id=n0 -device virtio-net-pci,netdev=n0
desktop_up
comes 'horizon: network: net0 connected, 10.0.2.15$' \
    || fail "the top bar does not say what it shows of the network (\"horizon: network: net0 connected, 10.0.2.15\"): it does not read the kernel's links"
echo "ok   the top bar reads the kernel's link: horizon: network: net0 connected, 10.0.2.15"
shows 'sign_left > 0 && light > 40' || fail "the bar's network sign is not a network's on the screen ($light light pixels, $lit lit, where it stands at $sign_left)"
echo "ok   the sign on the bar, left of the toggles, shows a network ($light light pixels at $sign_left)"

# The cable pulled: the bar reads the kernel again on its tick.
monitor "set_link n0 off"
comes 'horizon: network: net0 no cable$' || fail "the bar does not follow the link: the cable pulled, it does not say no cable"
shows 'light < 35 && lit > 25' || fail "the bar's network sign is not crossed out with the cable pulled ($light light pixels, $lit lit)"
echo "ok   the cable pulled, the bar follows by itself: horizon: network: net0 no cable, the sign crossed out ($light light pixels, $lit lit)"

# The control centre, opened with a click on its toggles (their middle is
# 31 right of the sign's left edge; a click that came before the bar
# looked is lost, so it is clicked again when the popup does not come):
# its network tile is there and not lit.
toggles=$(( sign_left + 31 ))
opened=no
for (( try = 0; try < 3; try++ )); do
    click "$toggles" 15
    if shows 'words > 60'; then
        opened=yes
        break
    fi
done
[[ $opened == yes ]] || fail "the control centre did not open on a click at $toggles,15 ($words lit pixels where its network tile's words stand)"
shows 'blue < 50' || fail "the network tile is lit with the cable pulled ($blue blue pixels in its circle)"
echo "ok   the control centre opens; its network tile is not lit with the cable out ($blue blue pixels in its circle)"

# The cable back in: the open tile follows and lights up.
monitor "set_link n0 on"
comes 'horizon: network: net0 connected, 10.0.2.15$' 2 || fail "the bar does not follow the link: the cable back in, it does not say connected again"
shows 'blue > 400' || fail "the open network tile did not light up with the cable back in ($blue blue pixels in its circle)"
echo "ok   the cable back in: horizon: network: net0 connected, 10.0.2.15 again, and the open tile lights up by itself ($blue blue pixels)"

# A click on the tile (its middle at 750,119): nothing is switched, the
# settings open on their Network page, which says what the kernel says.
settings=no
for (( try = 0; try < 3; try++ )); do
    click 750 119
    if comes 'map: Settings$'; then
        settings=yes
        break
    fi
done
[[ $settings == yes ]] || fail "a click on the network tile did not start the settings"
wait_for 'control: network: net0 (virtio, 52:54:00:12:34:56) connected, 10.0.2.15/24 by DHCP, gateway 10.0.2.2, name server 10.0.2.3$'
echo "ok   a click on the tile opens the settings on their Network page (control: network: net0 ... connected, 10.0.2.15/24 by DHCP)"
stop

# ---------------------------------------------------------------------------
# A machine without a network device.

start -nic none
desktop_up
comes 'horizon: network: no links$' || fail "without a network device the bar does not say \"horizon: network: no links\""
shows 'sign_left > 0 && light < 35 && lit > 25' || fail "the bar's network sign is not crossed out without a network device ($light light pixels, $lit lit)"
echo "ok   without a network device: horizon: network: no links, the sign crossed out ($light light pixels, $lit lit)"
stop

# ---------------------------------------------------------------------------
# A wire without a DHCP server: the link is up and has no address.

start -netdev hubport,id=n0,hubid=0 -device virtio-net-pci,netdev=n0
desktop_up
comes 'horizon: network: net0 no address yet$' || fail "on a wire without a DHCP server the bar does not say \"horizon: network: net0 no address yet\""
echo "ok   on a wire without a DHCP server: horizon: network: net0 no address yet"
stop
echo "PASS brixOS's top bar shows the network as the kernel says it: connected with its address, no cable, no address yet, no links"
