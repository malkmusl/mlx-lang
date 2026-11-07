#!/usr/bin/env bash
# brixOS on the Q35 platform (a stand-in for real hardware: ECAM chipset,
# AHCI/SATA, an Intel NIC and xHCI, as on a Supermicro H11 + EPYC board),
# as opposed to the i440fx + virtio machine the other checks use. The
# kernel enumerates PCI across every bus (pci.mlx) and finds the AHCI
# controller, the Ethernet controller and the xHCI controller by their
# class, the ground the real-hardware drivers stand on; ACPI and SMP still
# bring up the second processor on the new chipset.
#
# Then the real test of the AHCI (SATA) driver (ahci.mlx): the disk on the
# AHCI controller is /devices/disk0; it is formatted as a brixfs volume, a
# file is written and committed (WRITE_DMA_EXT and FLUSH_EXT), and after
# another boot that file is read back (READ_DMA_EXT) -- the disk driven by
# AHCI, not virtio.
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
qemu_pid=
cleanup() {
    exec 3>&- 2> /dev/null || true
    [[ -n "$qemu_pid" ]] && kill "$qemu_pid" 2> /dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
fail() { echo "FAIL $1" >&2; [[ -f "$work/serial.txt" ]] && cat -v "$work/serial.txt" | tail -n 40 >&2; exit 1; }

brix=--target=x86_64-brixos
mkdir -p "$work/bin"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet $brix projects/brix/tools/brixfs/main.mlx -o "$work/brixfs"
for program in ls cat sync echo cp mkdir chmod; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "brixfs=$work/brixfs" "install=projects/brix/tools/install.sh" "$work/bin/"
truncate -s 64M "$work/disk.img"
echo "ok   the kernel and a Q35 boot image build"

# The disk sits on the Q35 machine's built-in AHCI controller (ide.0).
boot() {
    rm -f "$work/input"
    mkfifo "$work/input"
    : > "$work/serial.txt"
    qemu-system-x86_64 -machine q35 -m 1G -smp 2 -cpu max -display none -monitor none -no-reboot \
        -serial stdio -kernel "$work/brix.img" \
        -drive id=d0,file="$work/disk.img",format=raw,if=none -device ide-hd,drive=d0,bus=ide.0 \
        -device igb -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0 \
        < "$work/input" > "$work/serial.txt" 2>&1 &
    qemu_pid=$!
    exec 3> "$work/input"
    prompts=0
}
stop() {
    exec 3>&-
    kill "$qemu_pid" 2> /dev/null || true
    wait "$qemu_pid" 2> /dev/null || true
    qemu_pid=
}
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(output | grep -c -- "$1" || true) < want )); do
        (( tries++ < 300 )) || fail "no \"$1\" (x$want) on the serial line"
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

# First boot: enumeration, and the AHCI disk found, formatted and written.
boot
wait_for '^brixOS: 2 processors, 1 more started$' || true
grep -q '^brixOS: 2 processors, 1 more started$' <(output) || fail "ACPI/SMP did not bring up the second processor on Q35"
echo "ok   ACPI and SMP bring up the second processor on the Q35 chipset"

# PCI across every bus: the AHCI, Ethernet and xHCI controllers the real
# hardware drivers will drive, found by their class code.
grep -qE '^pci: 00:1f\.2 .* 0x10601 AHCI$' <(output) || fail "the AHCI (SATA) controller was not enumerated"
grep -qE '^pci: [0-9a-f:.]+ .* 0x20000 Ethernet$' <(output) || fail "the Ethernet controller was not enumerated"
grep -qE '^pci: [0-9a-f:.]+ .* 0xc0330 xHCI$' <(output) || fail "the xHCI (USB) controller was not enumerated"
echo "ok   PCI enumerates the AHCI, Ethernet and xHCI controllers by class across the buses"

# The disk on the AHCI controller is /devices/disk0, driven by AHCI.
wait_for '^brixOS: /devices/disk0, 65536 KiB (AHCI)$'
echo "ok   the SATA disk is /devices/disk0, driven by the AHCI driver"

# Format it, install the system, write a file, commit it: the AHCI driver's
# write path (WRITE_DMA_EXT) and flush (FLUSH_EXT).
type_line 'brixfs format /devices/disk0 && install && echo hello-ahci > /volumes/disk0/note && sync && echo written'
wait_for '^brixfs: mounted at /volumes/disk0$'
wait_for '^installed on /volumes/disk0: it is the system from the next boot on$'
wait_for '^written$'
stop
echo "ok   the AHCI disk is formatted, the system installed and a file committed"

# Second boot: the file is read back off the AHCI disk (READ_DMA_EXT), and
# the system itself now comes from it.
boot
wait_for '^brixOS: /volumes/disk0 (brixfs, generation [0-9]*)$'
wait_for '^brixOS: / from /volumes/disk0'
type_line 'cat /volumes/disk0/note'
wait_for '^hello-ahci$'
stop
echo "ok   after another boot the file is read back off the AHCI disk and the system boots from it"
echo "PASS brixOS on Q35: ACPI/SMP, PCI enumeration, and a brixfs volume on the SATA (AHCI) disk"
