#!/usr/bin/env bash
# Our Vulkan driver's stage (a) on brixOS without the card (QEMU has no
# RX 580; projects/brix/vulkan, the kernel's /devices/gpuN and the calls
# 80 to 85 of spec/04-stdlib/os/brix.xml):
#
# 1. the kernel offers programs no graphics card: it says nothing of a
#    /devices/gpu0 and there is none;
# 2. vkinfo (the image's own tool, the driver linked in) says in one line
#    that there is no Vulkan device, and ends with status 1;
# 3. the calls of a card's context answer as the spec says without a card:
#    no card, no context of one (not_found), each of the six on no handle
#    and on a made-up one invalid_handle, on a handle that is no context
#    wrong_kind, the number after the last unsupported (gpucalls, a probe
#    built here);
# 4. the desktop as shipped: the compositor looks for the card's device
#    through the driver and std.gpu, says it found none, and draws on the
#    processor as before: the wallpaper and the top bar on the screen
#    (tools/check_brix_desktop.sh checks the rest of what it draws).
#
#   tools/check_brix_vulkan.sh [compiler]
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
work=$(mktemp -d)
qemu_pid=
cleanup() {
    exec 3>&- 2> /dev/null || true
    [[ -n "$qemu_pid" ]] && kill "$qemu_pid" 2> /dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
serial=$work/serial.txt
fail() { echo "FAIL $1" >&2; [[ -f "$serial" ]] && cat -v "$serial" | tail -n 40 >&2; exit 1; }

brix=--target=x86_64-brixos
mkdir -p "$work/bin" "$work/services"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet $brix projects/brix/tools/vkinfo/main.mlx -o "$work/vkinfo"
"$compiler" --quiet $brix projects/brix/tools/gpucalls/main.mlx -o "$work/gpucalls"
for program in ls cat echo; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "vkinfo=$work/vkinfo" "gpucalls=$work/gpucalls" "$work/bin/"
echo "ok   the kernel, vkinfo, the probe of the calls and the image build"

boot() {
    rm -f "$work/input" "$work/monitor"
    mkfifo "$work/input"
    : > "$serial"
    qemu-system-x86_64 -m 256M -display none -monitor "unix:$work/monitor,server,nowait" -no-reboot -cpu max -smp 2 -vga std \
        -serial stdio -kernel "$1" "${@:2}" < "$work/input" > "$serial" 2>&1 &
    qemu_pid=$!
    exec 3> "$work/input"
    prompts=0
    for (( i = 0; i < 100; i++ )); do [[ -S $work/monitor ]] && break; sleep 0.1; done
}
stop() {
    exec 3>&-
    kill "$qemu_pid" 2> /dev/null || true
    wait "$qemu_pid" 2> /dev/null || true
    qemu_pid=
}
output() { sed 's/\x1b\[K//g' "$serial" | tr '\r' '\n'; }
patience=1200
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(output | grep -ac -- "$1" || true) < want )); do
        (( tries++ < patience )) || fail "no \"$1\" (x$want) on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
prompts=0
type_line() {
    prompts=$((prompts + 1))
    wait_for '^root@brix:[^#]*# $' "$prompts"
    printf '%s\r' "$1" >&3
}
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

# ---- 1. no card offered ----
boot "$work/brix.img"
wait_for '^brixOS: probing gpu$'
wait_for '^root@brix:[^#]*# $'
output | grep -aq 'devices/gpu' && fail "the kernel speaks of a /devices/gpuN on a machine without the card"
type_line 'ls /devices/gpu0; echo gpu0 $?; ls /devices/null; echo null $?'
wait_for '^null [0-9]*$'
output | grep -aq '^gpu0 [1-9][0-9]*$' || fail "/devices/gpu0 is there on a machine without the card"
output | grep -aq '^null 0$' || fail "ls does not find /devices/null"
echo "ok   the kernel offers programs no graphics card: no line of /devices/gpu0, no node"

# ---- 2. vkinfo ----
type_line 'vkinfo; echo vkinfo $?'
wait_for '^vkinfo [0-9]*$'
output | grep -aq '^vkinfo: no Vulkan device: this machine has no graphics card our driver serves (no /devices/gpu0) and no other driver$' || fail "vkinfo does not say that there is no Vulkan device"
output | grep -aq '^vkinfo 1$' || fail "vkinfo does not end with status 1 without a device"
output | grep -aq '^GPU0:' && fail "vkinfo lists a device"
echo "ok   vkinfo runs on brixOS without the card: \"no Vulkan device\", status 1"

# ---- 3. the calls ----
type_line 'gpucalls; echo probed $?'
wait_for '^probed 0$'
for want in 'gpucalls: cards 0' 'gpucalls: describe gpu0 -6' 'gpucalls: open gpu0 -6' \
    'gpucalls: no handle: -1 -1 -1 -1 -1 -1' 'gpucalls: made-up handle: -1 -1 -1 -1 -1 -1' \
    "gpucalls: a node's handle: -2 -2 -2 -2 -2 -2" 'gpucalls: call 86 -18'; do
    output | grep -aqx -- "$want" || fail "the probe of the calls does not say \"$want\""
done
stop
echo "ok   the calls 80 to 85 without a card: no card, no context (not_found); invalid_handle on no handle and a made-up one, wrong_kind on a node's handle; 86 unsupported"

# ---- 4. the desktop ----
if [[ ! -f $fonts/DejaVuSansMono.ttf || ! -f $fonts/DejaVuSans.ttf ]]; then
    echo "skip 4: no DejaVu fonts in $fonts"
    exit 0
fi
"$compiler" --quiet $brix projects/init/brixos.mlx -o "$work/impulse"
"$compiler" --quiet $brix projects/init/tool.mlx -o "$work/impulsectl"
"$compiler" --quiet $brix projects/desktop/splash/brixos.mlx -o "$work/ignition"
"$compiler" --quiet $brix projects/desktop/compositor/main.mlx -o "$work/kepler"
"$compiler" --quiet $brix projects/desktop/launcher/main.mlx -o "$work/launch"
"$compiler" --quiet $brix projects/desktop/dock/main.mlx -o "$work/orbit"
"$compiler" --quiet $brix projects/desktop/topbar/main.mlx -o "$work/horizon"
cp projects/brix/settings/init/services/serial-shell.service "$work/services/"
classic_look "$work"
sed -e 's/^exec = kepler$/exec = kepler --verbose/' -e '/^disabled = yes$/d' -e '$a log = console' -e '$a env = MLX_COMPOSITOR_LOG=-' \
    -e "\$a $classic_env" projects/brix/settings/init/services/desktop.service > "$work/services/desktop.service"
grep -q '^exec = kepler --verbose$' "$work/services/desktop.service" || fail "desktop.service is not the shipped one"
entries=()
for service in "$work"/services/*.service; do entries+=("settings/init/services/$(basename "$service")=$service"); done
"$work/brix-image" "$work/kernel" "$work/desktop.img" "flux=$work/flux" "impulse=$work/impulse" "impulsectl=$work/impulsectl" "ignition=$work/ignition" "kepler=$work/kepler" "launch=$work/launch" "orbit=$work/orbit" "horizon=$work/horizon" "vkinfo=$work/vkinfo" "fonts/DejaVuSansMono.ttf=$fonts/DejaVuSansMono.ttf" "fonts/DejaVuSans.ttf=$fonts/DejaVuSans.ttf" "${entries[@]}" "${classic_entries[@]}" "$work/bin/"
echo "ok   impulse, ignition, kepler, the launcher, the dock, the top bar and a boot image with the desktop service build"
boot "$work/desktop.img"
wait_for '^settings: border #5aa0ff'
wait_for '^renderer: no Vulkan device (no graphics card our driver serves); composing on the processor$'
wait_for '^layer: horizon 1024x30 at 0,0 in layer 2$'
# The screen: the flat wallpaper (24, 26, 32) over most of it, the top
# bar lighter above it.
drawn=
for (( i = 0; i < 30; i++ )); do
    rm -f "$work/screen.ppm"
    monitor "screendump $work/screen.ppm"
    sleep 0.3
    drawn=$(python3 - "$work/screen.ppm" <<'PY' 2>&1
import sys
try:
    data = open(sys.argv[1], "rb").read()
except OSError:
    sys.exit("no dump")
parts = data.split(b"\n", 3)
w, h = map(int, parts[1].split())
px = parts[3]
if len(px) < w * h * 3: sys.exit("a dump cut short")
wall = sum(1 for i in range(0, w * h * 3, 3) if (px[i], px[i + 1], px[i + 2]) == (24, 26, 32))
bar = sum(1 for y in range(0, 30) for x in range(w) if sum(px[(y * w + x) * 3:(y * w + x) * 3 + 3]) > 3 * 60)
if wall * 2 < w * h: sys.exit("the wallpaper covers %d of %d pixels" % (wall, w * h))
if bar < 200: sys.exit("the top bar has %d light pixels" % bar)
print("%dx%d: the wallpaper on %d pixels, %d light ones in the top bar" % (w, h, wall, bar))
PY
    ) && break
    drawn=
    sleep 0.5
done
[[ -n $drawn ]] || fail "the desktop is not drawn as before"
stop
echo "ok   the compositor looks for the card's device through the driver and std.gpu, says it found none, and composes on the processor: $drawn"
