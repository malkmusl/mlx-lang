#!/usr/bin/env bash
# brixOS starts from UEFI: brix-image's .efi output (an EFI application
# that loads the Multiboot image as a Multiboot loader would: the memory
# map, ExitBootServices, out of long mode, the image to 1 MiB) as
# EFI/BOOT/BOOTX64.EFI on a FAT disk, started by OVMF in QEMU; the kernel
# says itself, the shell answers. And the display's mode: the one the
# image names is set when the firmware has it.
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
qemu_pid=
cleanup() {
    exec 3>&- 2> /dev/null || true
    [[ -n "$qemu_pid" ]] && kill "$qemu_pid" 2> /dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
fail() { echo "FAIL $1" >&2; cat -v "$work/serial.txt" >&2 || true; exit 1; }

# OVMF and the image on its FAT disk in QEMU with `$1` of memory, the
# serial line the shell's; and its end.
start() {
    rm -f "$work/input"
    mkfifo "$work/input"
    : > "$work/serial.txt"
    qemu-system-x86_64 -m "$1" -cpu max -display none -monitor none -no-reboot -serial stdio -net none \
        -bios "$firmware" -drive "format=raw,file=fat:rw:$work/esp" < "$work/input" > "$work/serial.txt" 2>&1 &
    qemu_pid=$!
    exec 3> "$work/input"
}
stop() {
    exec 3>&-
    kill "$qemu_pid" 2> /dev/null || true
    wait "$qemu_pid" 2> /dev/null || true
    qemu_pid=
}
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
# Waits (the firmware takes its time where the processor is emulated: up
# to three minutes) until the output has a line matching $1.
wait_for() {
    local tries=0
    while ! output | grep -aq -- "$1"; do
        (( tries++ < 1800 )) || fail "no \"$1\" on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}

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
start 6G
wait_for '^brixOS: kernel at 0x200000'
wait_for '^brixOS: display [0-9]*x[0-9]* pitch [0-9]* format [0-9]* at 0x[0-9a-f]* (the firmware.s)$'
wait_for '^brixOS: [0-9]* MiB'
wait_for '^root@brix:[^#]*# $'
printf 'echo started by UEFI; uname -o\r' >&3
wait_for '^started by UEFI$'
wait_for '^brixOS$'
stop
echo "ok   OVMF starts it; the kernel learns its memory from the EFI memory map and its display from GOP, the shell answers"
# The 6 GiB are all counted: more than the ~2.75 GiB below the hole, so
# the map's ranges above 4 GiB were passed through and mapped.
seen=$(output | grep -a '^brixOS: [0-9]* MiB' | head -1 | grep -ao '[0-9]*' | head -1)
[[ -n $seen && $seen -ge 5000 ]] || fail "the kernel saw only ${seen:-?} MiB of 6144 (RAM above the hole was lost)"
echo "ok   with 6 GiB the kernel counts ${seen} MiB: the RAM above the memory hole, from the EFI map"

# The display's mode. The loader does not take whatever mode the firmware
# was left in: it sets the monitor's own (its EDID's) when the firmware
# has it, or the one the image names in settings/display. OVMF starts in
# 1280x800; an image built for 1920x1080 comes up in that, one that names
# a mode the firmware does not have comes up in the nearest below it, or
# stays in the firmware's when that is no larger picture.
default=$(output | grep -a '^brixOS: display [0-9]*x[0-9]* ' | head -1 | grep -ao '[0-9][0-9]*x[0-9][0-9]*' | head -1)
# The image built for the display mode $1, started; `got`: the mode the
# kernel says it has.
got=
mode_with() {
    printf '%s\n' "$1" > "$work/display"
    "$work/brix-image" "$work/kernel" "$work/esp/EFI/BOOT/BOOTX64.EFI" "flux=$work/flux" "settings/display=$work/display" "$work/bin/"
    start 1G
    wait_for '^brixOS: display [0-9]*x[0-9]* '
    wait_for '^brixOS: [0-9]* MiB'
    stop
    got=$(output | grep -a '^brixOS: display [0-9]*x[0-9]* ' | head -1 | grep -ao '[0-9][0-9]*x[0-9][0-9]*' | head -1)
}
mode_with 1920x1080
[[ $got == 1920x1080 ]] || fail "an image built for 1920x1080 came up in $got (the firmware's own was $default)"
output | grep -aq "^brixOS: the loader set this mode, the one the image names (settings/display); the firmware's was $default\$" || fail "the kernel does not say that the loader set the image's mode and what the firmware's was"
output | grep -aq "^brixOS: the firmware's modes: .* 1920x1080 " || fail "the kernel does not say the firmware's modes"
# A mode the firmware lacks: the nearest below it when that is a larger
# picture than the firmware's (3840x1080, a wide monitor's: 1920x1080), and
# the firmware's own when it is not (1234x567: nothing larger fits).
mode_with 3840x1080
[[ $got == 1920x1080 ]] || fail "an image built for 3840x1080, which the firmware lacks, came up in $got, not in the nearest below it (1920x1080)"
output | grep -aq "^brixOS: the loader set this mode, the nearest the firmware has to the one the image names; the firmware's was $default\$" || fail "the kernel does not say that the loader set the nearest mode"
mode_with 1234x567
[[ $got == "$default" ]] || fail "an image built for a mode the firmware lacks came up in $got, not the firmware's $default"
output | grep -aq '^brixOS: the loader set this mode' && fail "the kernel says the loader set a mode where the firmware's stayed"
echo "ok   the loader sets the display mode the image names when the firmware has it (1920x1080 instead of $default), the nearest below one it lacks, and leaves the firmware's otherwise; the kernel says which and lists the firmware's modes"
echo PASS
