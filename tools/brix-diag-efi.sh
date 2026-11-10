#!/usr/bin/env bash
# Builds a diagnostic brixOS EFI (mlx-out/bin/brix/BOOTX64-diag.EFI): the
# same kernel and tools as the normal image, but with no graphical session
# (no splash, compositor, login or greeter). Nothing takes the framebuffer,
# so the kernel's own framebuffer console (fbcon) keeps the whole boot log
# -- the hardware bring-up, the disks it found, what klogsave wrote -- on
# screen, to read or photograph on a machine with no serial line.
#
# Flash it to the EFI System Partition as /EFI/BOOT/BOOTX64.EFI (keep the
# normal one aside first). klogsave still runs, so a brixfs data partition
# still gets boot.log; this just makes the same log visible on the screen.
#
#   tools/brix-diag-efi.sh [compiler]
set -euo pipefail
cd "$(dirname "$0")/.."
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
out=mlx-out/bin/brix
mkdir -p "$out/coreutils"

brix=--target=x86_64-brixos
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$out/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o mlx-out/bin/brix-image
for p in flux:projects/shell sysinfo:projects/brix/tools/sysinfo brixfs:projects/brix/tools/brixfs \
         klogsave:projects/brix/tools/klogsave impulse:projects/init/brixos impulsectl:projects/init/tool; do
    name=${p%%:*}; src=${p##*:}/main.mlx
    if [ "$name" = impulsectl ]; then "$compiler" --quiet $brix "projects/init/tool.mlx" -o "$out/$name"
    elif [ "$name" = impulse ]; then "$compiler" --quiet $brix "projects/init/brixos.mlx" -o "$out/$name"
    else "$compiler" --quiet $brix "$src" -o "$out/$name"; fi
done
for p in cat ls echo mkdir sync cp rm; do
    "$compiler" --quiet $brix "projects/coreutils/$p/main.mlx" -o "$out/coreutils/$p" 2>/dev/null || true
done

# Both images run on one processor (/system/settings/processors: 1): what
# they are for is finding out what is wrong, and the other processors are
# one thing less to be in doubt. The build's own image starts them.
services=$(mktemp -d)
trap 'rm -rf -- "$services"' EXIT
echo 1 > "$services/processors"

# The GPU's firmware, as the build's image has it (a driver's bring-up is
# what these images are looked at for).
firmware=()
for blob in projects/brix/firmware/amdgpu/*.bin; do
    [ -f "$blob" ] && firmware+=("firmware/amdgpu/$(basename "$blob")=$blob")
done

efi="$out/BOOTX64-diag.EFI"
mlx-out/bin/brix-image "$out/kernel" "$efi" \
    flux="$out/flux" sysinfo="$out/sysinfo" brixfs="$out/brixfs" klogsave="$out/klogsave" \
    impulse="$out/impulse" impulsectl="$out/impulsectl" \
    "settings/processors=$services/processors" "${firmware[@]}" \
    settings/init/services/serial-shell.service=projects/brix/settings/init/services/serial-shell.service \
    settings/init/services/klogsave.service=projects/brix/settings/init/services/klogsave.service \
    settings/users=projects/brix/settings/users \
    settings/groups=projects/brix/settings/groups \
    settings/shadow=projects/brix/settings/shadow \
    "$out/coreutils/"
echo "wrote $efi"
echo "Flash it to the ESP as /EFI/BOOT/BOOTX64.EFI to see the boot log on screen."

# A second one with a shell on the screen (mlx-out/bin/brix/BOOTX64-console.EFI):
# the same without a graphical session, but with mlx-console, the virtual
# console, started by the init (console.service, enabled here): flux on the
# framebuffer with the USB keyboard, for a machine with no serial line when
# the desktop does not come up. mlx-console takes the framebuffer, so the
# kernel's own log leaves the screen; `cat /devices/klog` shows it there.
font=
for candidate in /usr/share/fonts/truetype/dejavu /usr/share/fonts/dejavu /usr/share/fonts/TTF /usr/share/fonts/dejavu-sans-mono-fonts; do
    if [ -f "$candidate/DejaVuSansMono.ttf" ]; then font="$candidate/DejaVuSansMono.ttf"; break; fi
done
if [ -z "$font" ]; then
    echo "no DejaVuSansMono.ttf found: the console image is not built"
    exit 0
fi
"$compiler" --quiet $brix projects/console/brixos.mlx -o "$out/mlx-console"
sed '/^disabled = yes$/d' projects/brix/settings/init/services/console.service > "$services/console.service"
console="$out/BOOTX64-console.EFI"
mlx-out/bin/brix-image "$out/kernel" "$console" \
    flux="$out/flux" sysinfo="$out/sysinfo" brixfs="$out/brixfs" klogsave="$out/klogsave" \
    impulse="$out/impulse" impulsectl="$out/impulsectl" mlx-console="$out/mlx-console" \
    "fonts/DejaVuSansMono.ttf=$font" \
    settings/init/services/serial-shell.service=projects/brix/settings/init/services/serial-shell.service \
    settings/init/services/klogsave.service=projects/brix/settings/init/services/klogsave.service \
    "settings/init/services/console.service=$services/console.service" \
    "settings/processors=$services/processors" "${firmware[@]}" \
    settings/users=projects/brix/settings/users \
    settings/groups=projects/brix/settings/groups \
    settings/shadow=projects/brix/settings/shadow \
    "$out/coreutils/"
echo "wrote $console"
echo "Flash that one instead for a shell on the screen (USB keyboard); cat /devices/klog shows the boot log."
