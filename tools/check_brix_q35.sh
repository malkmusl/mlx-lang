#!/usr/bin/env bash
# brixOS on the Q35 platform (a stand-in for real hardware: ECAM chipset,
# AHCI/SATA, an Intel NIC and xHCI, as on a Supermicro H11 + EPYC board),
# as opposed to the i440fx + virtio machine the other checks use. The
# kernel enumerates PCI across every bus (pci.mlx) and finds the AHCI
# controller, the Ethernet controller and the xHCI controller by their
# class, the ground the real-hardware drivers stand on; ACPI and SMP still
# bring up the second processor on the new chipset.
#
#   tools/check_brix_q35.sh [compiler]
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
truncate -s 64M "$work/disk.img"
echo "ok   the kernel and a Q35 boot image build"

timeout 40 qemu-system-x86_64 -machine q35 -m 1G -smp 2 -cpu max -display none -serial stdio -no-reboot \
    -kernel "$work/brix.img" \
    -drive id=d0,file="$work/disk.img",format=raw,if=none -device ide-hd,drive=d0,bus=ide.0 \
    -device igb -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0 \
    < /dev/null > "$work/serial.txt" 2>&1 || true
out() { tr -d '\r' < "$work/serial.txt"; }

grep -q '^brixOS: 2 processors, 1 more started$' <(out) || fail "ACPI/SMP did not bring up the second processor on Q35"
echo "ok   ACPI and SMP bring up the second processor on the Q35 chipset"

# PCI across every bus: the AHCI, Ethernet and xHCI controllers the real
# hardware drivers will drive, found by their class code.
grep -qE '^pci: 00:1f\.2 .* 0x10601 AHCI$' <(out) || fail "the AHCI (SATA) controller was not enumerated"
grep -qE '^pci: [0-9a-f:.]+ .* 0x20000 Ethernet$' <(out) || fail "the Ethernet controller was not enumerated"
grep -qE '^pci: [0-9a-f:.]+ .* 0xc0330 xHCI$' <(out) || fail "the xHCI (USB) controller was not enumerated"
echo "ok   PCI enumerates the AHCI, Ethernet and xHCI controllers by class across the buses"
echo "PASS brixOS on Q35: ACPI/SMP and PCI enumeration of the real-hardware controllers"
