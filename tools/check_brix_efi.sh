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

# The monitor's EDID. A card's firmware has its monitor's EDID (the EDID
# Active protocol on the display's handle); the loader reads the
# monitor's own mode from it and passes the EDID itself on to the kernel,
# whose GPU driver takes the monitor's own timing from it. OVMF has no
# EDID for QEMU's display, so a test image's loader gives the firmware one
# first (brix-image --test-edid): two blocks written here from the
# standard's layout, 3840x1080 at 120 Hz preferred in the base block and
# the same size at 60 Hz in a CTA-861 extension. The kernel must say the
# monitor's own mode (the loader's reading of the first timing) and the
# timing of 60 Hz (its own reading of both blocks: the extension came
# too).
# The bytes $2... (two hex digits each) as a block of 128 bytes, zeros
# after them and the checksum last, appended to the file $1.
edid_block() {
    local file=$1 sum=0 count=0 byte
    shift
    for byte in "$@"; do
        printf "\\x$byte" >> "$file"
        sum=$(( (sum + 0x$byte) & 255 ))
        count=$(( count + 1 ))
    done
    while (( count < 127 )); do
        printf '\0' >> "$file"
        count=$(( count + 1 ))
    done
    printf "\\x$(printf '%02x' $(( (256 - sum) & 255 )))" >> "$file"
}
: > "$work/edid.bin"
# The header, a maker and a product, EDID 1.4, a digital input of 120 by
# 34 cm; no established or standard timings; the preferred timing (549.12
# MHz, 3840 of 4000, 1080 of 1144, porches 48 and 3, pulses 32 high and 10
# low), the name, two empty descriptors; one extension block.
edid_block "$work/edid.bin" 00 ff ff ff ff ff ff 00 4c 2d 12 34 00 00 00 00 01 21 01 04 a5 78 22 78 3a \
    00 00 00 00 00 00 00 00 00 00 00 00 00 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 \
    80 d6 00 a0 f0 38 40 40 30 20 3a 00 b0 50 41 00 00 1a \
    00 00 00 fc 00 57 49 44 45 20 54 45 53 54 0a 20 20 20 \
    00 00 00 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 \
    00 00 00 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 \
    01
# A CTA-861 extension (revision 3, its timings from byte 4): the same
# size at 60 Hz (266.64 MHz, 3840 of 4000, 1080 of 1111).
edid_block "$work/edid.bin" 02 03 04 01 28 68 00 a0 f0 38 1f 40 30 20 3a 00 b0 50 41 00 00 1a
[[ $(wc -c < "$work/edid.bin") -eq 256 ]] || fail "the test EDID is not two blocks"
"$work/brix-image" "--test-edid=$work/edid.bin" "$work/kernel" "$work/esp/EFI/BOOT/BOOTX64.EFI" "flux=$work/flux" "$work/bin/"
start 1G
wait_for '^brixOS: display [0-9]*x[0-9]* '
wait_for '^brixOS: [0-9]* MiB'
stop
got=$(output | grep -a '^brixOS: display [0-9]*x[0-9]* ' | head -1 | grep -ao '[0-9][0-9]*x[0-9][0-9]*' | head -1)
[[ $got == 1920x1080 ]] || fail "with a monitor of 3840x1080, which the firmware lacks, the image came up in $got, not in the nearest below it (1920x1080)"
output | grep -aq "^brixOS: the loader set this mode, the nearest the firmware has to the monitor's own; the firmware's was $default\$" || fail "the kernel does not say that the loader set the nearest mode to the monitor's own"
output | grep -aq "^brixOS: the monitor's own mode is 3840x1080, which was not set" || fail "the kernel does not say the monitor's own mode (the loader's reading of the EDID)"
output | grep -aq "^gpu: the monitor's own timing is 3840x1080 at 60.00 Hz: pixel clock 266640 kHz, h 3840 4000 (front 48 sync 32 +), v 1080 1111 (front 3 sync 10 -) (the least demanding of 2 of that size)\$" || fail "the kernel does not say the monitor's own timing from the EDID the loader passed on (both blocks of it)"
echo "ok   with an EDID from the firmware the loader takes the monitor's own mode from it and passes the EDID on: the kernel says the monitor's own timing, the least demanding of the two the base block and the extension hold"
echo PASS
