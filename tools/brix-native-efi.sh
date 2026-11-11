#!/usr/bin/env bash
# Builds mlx-out/bin/brix/esp/EFI/BOOT/BOOTX64-native.EFI: the build's own
# EFI image (BOOTX64.EFI beside it; the same kernel, programs and
# settings) with one file more in its archive, settings/gpu with the word
# "native". That file asks the kernel's driver for an AMD Radeon (Polaris)
# to set the monitor's own mode itself where the card's firmware only has
# smaller ones: the plan the kernel says at every boot ("gpu: plan ...")
# is carried out, each write read back, and the firmware's state put back
# when anything fails. Without the file no register of the display is
# written; on a machine without such a card the image is the normal one.
#
# Copy it to the EFI System Partition beside the normal image (as
# /EFI/BOOT/BOOTX64-native.EFI) and start it from the firmware's boot menu
# or its shell. If the screen stays dark, start the normal image again:
# klogsave keeps the log of the boot before as boot.previous.log on the
# brixfs volumes, which says what the driver did.
#
# When tools/brix-diag-efi.sh has made its diagnostic image
# (mlx-out/bin/brix/BOOTX64-diag.EFI: no graphical session, the kernel's
# log stays on the screen), the same is made of it:
# mlx-out/bin/brix/BOOTX64-diag-native.EFI, on which what the driver says
# while it sets the mode can be read and photographed.
#
#   tools/brix-native-efi.sh [compiler]
set -euo pipefail
cd "$(dirname "$0")/.."
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
esp=mlx-out/bin/brix/esp/EFI/BOOT
normal=$esp/BOOTX64.EFI
native=$esp/BOOTX64-native.EFI
if [ ! -f "$normal" ]; then
    echo "no $normal: build the image first (./mlx build brix)" >&2
    exit 1
fi
# (An image whose kernel is older than the mode setting would take the
# file and do nothing with it.)
if ! grep -a -q 'settings/gpu' "$normal"; then
    echo "$normal has a kernel from before the GPU driver's mode setting: build the image again (./mlx build brix)" >&2
    exit 1
fi
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
printf 'native\n' > "$work/gpu"
"$work/brix-image" --add "$normal" "$native" "settings/gpu=$work/gpu"
echo "wrote $native"
diag=mlx-out/bin/brix/BOOTX64-diag.EFI
if [ -f "$diag" ] && grep -a -q 'settings/gpu' "$diag"; then
    "$work/brix-image" --add "$diag" mlx-out/bin/brix/BOOTX64-diag-native.EFI "settings/gpu=$work/gpu"
    echo "wrote mlx-out/bin/brix/BOOTX64-diag-native.EFI (the log stays on the screen)"
fi
echo "Copy it to the ESP beside the normal image and start it from the firmware's boot menu."
echo "If the screen stays dark, start the normal image: boot.previous.log on the brixfs volumes is that boot's log."
