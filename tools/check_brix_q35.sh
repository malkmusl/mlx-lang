#!/usr/bin/env bash
# brixOS on the Q35 platform (a stand-in for real hardware: ECAM chipset,
# AHCI/SATA, an Intel NIC and xHCI, as on a Supermicro H11 + EPYC board),
# as opposed to the i440fx + virtio machine the other checks use. The
# kernel enumerates PCI across every bus (pci.mlx) and finds the AHCI
# controller, the Ethernet controller and the xHCI controller by their
# class, the ground the real-hardware drivers stand on; ACPI and SMP still
# bring up the second processor on the new chipset.
#
# Then the real test of the real-hardware disk drivers: the disk on the
# AHCI controller is /devices/disk0 (ahci.mlx) and the NVMe namespace is
# /devices/disk1 (nvme.mlx). Each is formatted as a brixfs volume and a
# file written and committed (the write and flush paths), and after
# another boot both files are read back (the read path) -- the system on
# the SATA disk, a file on the NVMe one, neither driven by virtio.
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
truncate -s 128M "$work/nvme.img"
echo "ok   the kernel and a Q35 boot image build"

# disk0 sits on the Q35 machine's built-in AHCI controller (ide.0); disk1
# is an NVMe namespace.
boot() {
    rm -f "$work/input"
    mkfifo "$work/input"
    : > "$work/serial.txt"
    qemu-system-x86_64 -machine q35 -m 1G -smp 2 -cpu max -display none -monitor none -no-reboot \
        -serial stdio -kernel "$work/brix.img" \
        -drive id=d0,file="$work/disk.img",format=raw,if=none -device ide-hd,drive=d0,bus=ide.0 \
        -drive id=nvm,file="$work/nvme.img",format=raw,if=none -device nvme,serial=brixnvme,drive=nvm \
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
grep -qE '^ahci: controller 00:1f\.2, registers at 0x[0-9a-f]+, CAP 0x[0-9a-f]+, ports 0x[0-9a-f]+' <(output) || fail "the AHCI (SATA) controller was not found"
wait_for '^brixOS: /devices/net0, [0-9a-f:]* (igb)$'
wait_for '^usb: xHCI controller [0-9a-f:.]*, registers at 0x[0-9a-f]*$'
echo "ok   PCI enumerates the AHCI, Ethernet and xHCI controllers by class across the buses"

# disk0 on the AHCI controller, disk1 the NVMe namespace, each by its driver.
wait_for '^brixOS: /devices/disk0, 65536 KiB (AHCI)$'
wait_for '^brixOS: /devices/disk1, 131072 KiB (NVMe)$'
echo "ok   the SATA disk is /devices/disk0 (AHCI) and the NVMe namespace is /devices/disk1 (NVMe)"

# Format both, install the system on disk0, write a file on each, commit:
# the AHCI and NVMe write and flush paths.
type_line 'brixfs format /devices/disk0 && brixfs format /devices/disk1 && install && echo hello-ahci > /volumes/disk0/note && echo hello-nvme > /volumes/disk1/note && sync && echo written'
wait_for '^brixfs: mounted at /volumes/disk0$'
wait_for '^brixfs: mounted at /volumes/disk1$'
wait_for '^installed on /volumes/disk0: it is the system from the next boot on$'
wait_for '^written$'
stop
echo "ok   both disks formatted, the system installed on the SATA disk and a file committed on each"

# Second boot: the files are read back (the AHCI and NVMe read paths), and
# the system itself now comes from the SATA disk.
boot
wait_for '^brixOS: /volumes/disk0 (brixfs, generation [0-9]*)$'
wait_for '^brixOS: /volumes/disk1 (brixfs, generation [0-9]*)$'
wait_for '^brixOS: / from /volumes/disk0'
type_line 'cat /volumes/disk0/note /volumes/disk1/note'
wait_for '^hello-ahci$'
wait_for '^hello-nvme$'
stop
echo "ok   after another boot both files are read back and the system boots from the SATA disk"
echo "PASS brixOS on Q35: ACPI/SMP, PCI enumeration, and brixfs volumes on the SATA (AHCI) and NVMe disks"
