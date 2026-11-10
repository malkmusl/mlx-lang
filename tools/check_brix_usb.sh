#!/usr/bin/env bash
# brixOS and USB: an xHCI controller (QEMU's `-device qemu-xhci` on the Q35
# machine, a stand-in for the Supermicro H11's USB ports) with a USB
# keyboard and a USB mouse, for real hardware that has no PS/2 i8042. The
# kernel (usb.mlx) brings the controller up (reset, the command and event
# rings), and for each device enables a slot, addresses it, reads its
# descriptors, sets its configuration, configures the interrupt endpoint
# and puts it in boot protocol -- a full control-transfer path -- so each
# shows up as /devices/keyboard or /devices/mouse.
#
# Then what happens while the system runs: the controller's interrupt
# comes as a message (MSI-X here) and a key typed on the USB keyboard is
# read from /devices/keyboard; a hub is plugged in, a second keyboard
# behind it (each brought up by the same steps the boot takes), a key
# typed on that one arrives too; the hub is pulled out and both its slot
# and the keyboard's behind it are given back. A second run on a
# controller with no message-signalled interrupt (QEMU's nec-usb-xhci with
# MSI and MSI-X off) shows the same keyboard served by the timer tick
# alone.
#
#   tools/check_brix_usb.sh [compiler]
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
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
fail() { echo "FAIL $1" >&2; [[ -f "$work/serial.txt" ]] && tr -d '\r' < "$work/serial.txt" | tail -n 40 >&2; exit 1; }

brix=--target=x86_64-brixos
mkdir -p "$work/bin"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet $brix projects/brix/tools/sysinfo/main.mlx -o "$work/sysinfo"
for program in cat head base64; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "sysinfo=$work/sysinfo" "$work/bin/"
echo "ok   the kernel and a Q35 boot image build"

# The QEMU monitor on a socket: keys, devices plugged in and pulled out.
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
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(output | grep -c -- "$1" || true) < want )); do
        (( tries++ < 600 )) || fail "no \"$1\" (x$want) on the serial line"
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
# QEMU with the devices given, its serial line the shell's.
start() {
    rm -f "$work/input" "$work/monitor"
    mkfifo "$work/input"
    : > "$work/serial.txt"
    prompts=0
    qemu-system-x86_64 -machine q35 -m 1G -smp 2 -cpu max -display none -monitor "unix:$work/monitor,server,nowait" -no-reboot \
        -serial stdio -kernel "$work/brix.img" "$@" < "$work/input" > "$work/serial.txt" 2>&1 &
    qemu_pid=$!
    exec 3> "$work/input"
    for (( i = 0; i < 100; i++ )); do [[ -S $work/monitor ]] && break; sleep 0.1; done
}
stop() {
    exec 3>&-
    kill "$qemu_pid" 2> /dev/null || true
    wait "$qemu_pid" 2> /dev/null || true
    qemu_pid=
}
# The keys `a` and `b` typed on the USB keyboard QEMU types on (its last
# one) read from /devices/keyboard: their scancodes, pressed and released
# (1e 9e 30 b0), as base64. `$1`: how often that was seen before.
type_keys() {
    type_line 'head -c 4 /devices/keyboard | base64'
    sleep 1
    monitor "sendkey a"
    sleep 0.3
    monitor "sendkey b"
    wait_for '^Hp4wsA==$' "$1"
}

start -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0 -device usb-mouse,bus=xhci.0
wait_for '^usb: xHCI controller [0-9a-f:.]*, registers at 0x[0-9a-f]*$'
echo "ok   PCI finds the xHCI controller"
wait_for '^usb: /devices/keyboard (xHCI slot [0-9]* if [0-9]* dci [0-9]*)$'
wait_for '^usb: /devices/mouse (xHCI slot [0-9]* if [0-9]* dci [0-9]*)$'
echo "ok   the USB keyboard and mouse are enumerated over xHCI (slot, address, descriptors, config, boot protocol)"
wait_for '^usb: xHCI [0-9]* ports, context size [0-9]*, interrupts by MSI-X$'
type_keys 1
type_line 'sysinfo'
wait_for '^device interrupts: [1-9][0-9]*$'
echo "ok   the controller's interrupt comes as a message (MSI-X); a key typed on the USB keyboard is read from /devices/keyboard"

# A hub, then a keyboard behind it, while the system runs (the kernel's
# word about them is in its log, not on the console).
monitor "device_add usb-hub,bus=xhci.0,port=3,id=hub"
sleep 2
monitor "device_add usb-kbd,bus=xhci.0,port=3.1,id=behind"
sleep 2
type_line 'cat /devices/klog'
wait_for '^usb: port [0-9]*: a hub of 8 ports$'
wait_for '^usb: port [0-9]*\.1 -> slot [0-9]* speed [0-9]*$'
wait_for '^usb: /devices/keyboard (xHCI slot [0-9]* if [0-9]* dci [0-9]*)$' 3
type_keys 2
echo "ok   a hub plugged in while the system runs and a keyboard plugged into it are brought up; its keys arrive"

monitor "device_del hub"
sleep 2
type_line 'cat /devices/klog'
wait_for '^usb: port [0-9]*: gone (slot [0-9]*)$'
wait_for '^usb: port [0-9]*\.1: gone (slot [0-9]*)$'
# The slots are the controller's again: a keyboard plugged in gets the
# lowest, the one the hub had.
monitor "device_add usb-kbd,bus=xhci.0,port=4,id=again"
sleep 2
type_line 'cat /devices/klog'
wait_for '^usb: port [0-9]* -> slot 3 speed [0-9]*$' 3
type_keys 3
echo "ok   the hub pulled out takes the keyboard behind it along; their slots are given back and used again"
stop

# A controller with no message-signalled interrupt: the tick serves it.
start -device nec-usb-xhci,id=xhci,msi=off,msix=off -device usb-kbd,bus=xhci.0
wait_for '^usb: xHCI [0-9]* ports, context size [0-9]*, polled$'
wait_for '^usb: /devices/keyboard (xHCI slot [0-9]* if [0-9]* dci [0-9]*)$'
type_keys 1
type_line 'sysinfo'
wait_for '^device interrupts: 0$'
echo "ok   a controller that raises no message is served by the timer tick: the keyboard is read all the same"
stop
echo "PASS brixOS on Q35: USB keyboards and mice on xHCI, by interrupt and by the tick, plugged in and pulled out"
