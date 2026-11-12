#!/usr/bin/env bash
# Builds mlx-out/bin/brix/esp/EFI/BOOT/BOOTX64-native.EFI and
# BOOTX64-rehearsal.EFI: the build's own EFI image (BOOTX64.EFI beside
# them; the same kernel, programs and settings) with one file more in its
# archive, settings/gpu, with the word "native" or "rehearsal". That file
# asks the kernel's driver for an AMD Radeon (Polaris) to write to the
# card's display:
#
# - "native": to set the monitor's own mode itself where the card's
#   firmware only has smaller ones: the plan the kernel says at every boot
#   ("gpu: plan ...") is carried out, each write read back, and the
#   firmware's state put back when anything fails;
# - "rehearsal": on an HDMI or DVI output, to set the firmware's own mode
#   again by the same steps, the card's own firmware tables among them
#   (the pixel clock's PLL, the transmitter). The screen goes dark for a
#   moment and comes back as it was: nothing new is asked of the monitor,
#   and it shows that the card's tables run here before the monitor's own
#   mode is tried. Try this one first.
#
# Without the file no register of the display is written and no table of
# the card's is run; on a machine without such a card the images are the
# normal one.
#
# Copy them to the EFI System Partition beside the normal image (as
# /EFI/BOOT/BOOTX64-rehearsal.EFI and /EFI/BOOT/BOOTX64-native.EFI) and
# start one from the firmware's boot menu or its shell. If the screen
# stays dark, wait until the disk is quiet and start the normal image
# again: klogsave keeps the log of the boot before as boot.previous.log on
# the brixfs volumes, which says what the driver did.
#
# When tools/brix-diag-efi.sh has made its diagnostic image
# (mlx-out/bin/brix/BOOTX64-diag.EFI: no graphical session, the kernel's
# log stays on the screen), the same is made of it:
# mlx-out/bin/brix/BOOTX64-diag-native.EFI and BOOTX64-diag-rehearsal.EFI,
# on which what the driver says after it has set the mode can be read and
# photographed.
#
#   tools/brix-native-efi.sh [compiler]
set -euo pipefail
cd "$(dirname "$0")/.."
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
esp=mlx-out/bin/brix/esp/EFI/BOOT
normal=$esp/BOOTX64.EFI
if [ ! -f "$normal" ]; then
    echo "no $normal: build the image first (./mlx build brix)" >&2
    exit 1
fi
# (An image whose kernel is older than the mode setting would take the
# file and do nothing with it; one from before the card's tables are run
# knows no rehearsal.)
if ! grep -a -q 'settings/gpu' "$normal"; then
    echo "$normal has a kernel from before the GPU driver's mode setting: build the image again (./mlx build brix)" >&2
    exit 1
fi
if ! grep -a -q "the firmware's own mode set again" "$normal"; then
    echo "$normal has a kernel from before the card's tables are run: build the image again (./mlx build brix)" >&2
    exit 1
fi
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
diag=mlx-out/bin/brix/BOOTX64-diag.EFI
for word in rehearsal native; do
    printf '%s\n' "$word" > "$work/gpu"
    "$work/brix-image" --add "$normal" "$esp/BOOTX64-$word.EFI" "settings/gpu=$work/gpu"
    echo "wrote $esp/BOOTX64-$word.EFI"
    if [ -f "$diag" ] && grep -a -q "the firmware's own mode set again" "$diag"; then
        "$work/brix-image" --add "$diag" "mlx-out/bin/brix/BOOTX64-diag-$word.EFI" "settings/gpu=$work/gpu"
        echo "wrote mlx-out/bin/brix/BOOTX64-diag-$word.EFI (the log stays on the screen)"
    fi
done
echo "Copy them to the ESP beside the normal image. Start BOOTX64-rehearsal.EFI first (the firmware's own mode set again: the screen goes dark for a moment and comes back), then BOOTX64-native.EFI (the monitor's own mode)."
echo "If the screen stays dark, wait until the disk is quiet and start the normal image: boot.previous.log on the brixfs volumes is that boot's log."
