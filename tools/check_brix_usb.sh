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
#   tools/check_brix_usb.sh [compiler]
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
if ! command -v qemu-system-x86_64 > /dev/null; then
    echo "skip: no qemu-system-x86_64"
    exit 0
fi
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; [[ -f "$work/serial.txt" ]] && tr -d '\r' < "$work/serial.txt" | tail -n 40 >&2; exit 1; }

"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet --target=x86_64-brixos projects/shell/main.mlx -o "$work/flux"
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux"
echo "ok   the kernel and a Q35 boot image build"

timeout 40 qemu-system-x86_64 -machine q35 -m 1G -smp 2 -cpu max -display none -serial stdio -no-reboot \
    -kernel "$work/brix.img" \
    -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0 -device usb-mouse,bus=xhci.0 \
    < /dev/null > "$work/serial.txt" 2>&1 || true
out() { tr -d '\r' < "$work/serial.txt"; }

grep -qE '^usb: xHCI controller [0-9a-f:.]+, registers at 0x[0-9a-f]+$' <(out) || fail "the xHCI controller was not found"
echo "ok   PCI finds the xHCI controller"
grep -qE '^usb: /devices/keyboard \(xHCI slot [0-9]+ if [0-9]+ dci [0-9]+\)$' <(out) || fail "the USB keyboard was not brought up"
grep -qE '^usb: /devices/mouse \(xHCI slot [0-9]+ if [0-9]+ dci [0-9]+\)$' <(out) || fail "the USB mouse was not brought up"
echo "ok   the USB keyboard and mouse are enumerated over xHCI (slot, address, descriptors, config, boot protocol)"
echo "PASS brixOS on Q35: a USB keyboard and mouse on the xHCI controller"
