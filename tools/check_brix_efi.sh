#!/usr/bin/env bash
# brixOS starts from UEFI: brix-image's .efi output (an EFI application
# that loads the Multiboot image as a Multiboot loader would: the memory
# map, ExitBootServices, out of long mode, the image to 1 MiB) as
# EFI/BOOT/BOOTX64.EFI on a FAT disk, started by OVMF in QEMU; the kernel
# says itself, the shell answers.
#
#   tools/check_brix_efi.sh [compiler]
set -euo pipefail
cd "$(dirname "$0")/.."
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
firmware=""
for candidate in /usr/share/ovmf/OVMF.fd /usr/share/qemu/OVMF.fd /usr/share/OVMF/OVMF.fd /usr/share/OVMF/OVMF_CODE_4M.fd /usr/share/OVMF/OVMF_CODE.fd /usr/share/edk2/ovmf/OVMF_CODE.fd /usr/share/edk2/x64/OVMF_CODE.4m.fd; do
    [[ -f $candidate ]] && firmware=$candidate && break
done
if ! command -v qemu-system-x86_64 > /dev/null || [[ -z $firmware ]]; then
    echo "skip: no qemu-system-x86_64 or OVMF"
    exit 0
fi
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; cat -v "$work/serial.txt" >&2 || true; exit 1; }

mkdir -p "$work/bin" "$work/esp/EFI/BOOT"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet --target=x86_64-brixos projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet --target=x86_64-brixos projects/coreutils/uname/main.mlx -o "$work/bin/uname"
"$work/brix-image" "$work/kernel" "$work/esp/EFI/BOOT/BOOTX64.EFI" "flux=$work/flux" "$work/bin/"
head -c 2 "$work/esp/EFI/BOOT/BOOTX64.EFI" | grep -q MZ || fail "BOOTX64.EFI is no PE file"
echo "ok   the kernel and BOOTX64.EFI build"

# Enough RAM that it spans the memory hole below 4 GiB and reaches past
# it: the kernel must count all of it from the EFI memory map (the loader
# passes it through as a Multiboot map), not only the RAM below the hole.
# (The firmware takes its time where the processor is emulated: the
# command is typed late and the machine given long enough.)
{
    sleep 45
    printf 'echo started by UEFI; uname -o\r'
    sleep 8
} | timeout 90 qemu-system-x86_64 -m 6G -cpu max -display none -serial stdio -net none \
    -bios "$firmware" -drive "format=raw,file=fat:rw:$work/esp" 2>&1 | tr -d '\r' > "$work/serial.txt" || true
for expected in '^brixOS: kernel at 0x200000' '^brixOS: display [0-9]*x[0-9]* pitch [0-9]* format [0-9]* at 0x[0-9a-f]* (the firmware.s)$' '^brixOS: [0-9]* MiB' '^started by UEFI$' '^brixOS$'; do
    grep -aq "$expected" "$work/serial.txt" || fail "no \"$expected\" on the serial line"
done
echo "ok   OVMF starts it; the kernel learns its memory from the EFI memory map and its display from GOP, the shell answers"
# The 6 GiB are all counted: more than the ~2.75 GiB below the hole, so
# the map's ranges above 4 GiB were passed through and mapped.
seen=$(grep -a '^brixOS: [0-9]* MiB' "$work/serial.txt" | head -1 | grep -ao '[0-9]*' | head -1)
[[ -n $seen && $seen -ge 5000 ]] || fail "the kernel saw only ${seen:-?} MiB of 6144 (RAM above the hole was lost)"
echo "ok   with 6 GiB the kernel counts ${seen} MiB: the RAM above the memory hole, from the EFI map"
echo PASS
