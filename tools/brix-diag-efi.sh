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
    # brixfs is a host tool here (format/read), the rest are brixOS programs.
    if [ "$name" = brixfs ]; then "$compiler" --quiet "$src" -o "$out/$name"
    elif [ "$name" = impulsectl ]; then "$compiler" --quiet $brix "projects/init/tool.mlx" -o "$out/$name"
    elif [ "$name" = impulse ]; then "$compiler" --quiet $brix "projects/init/brixos.mlx" -o "$out/$name"
    else "$compiler" --quiet $brix "$src" -o "$out/$name"; fi
done
for p in cat ls echo mkdir sync cp rm; do
    "$compiler" --quiet $brix "projects/coreutils/$p/main.mlx" -o "$out/coreutils/$p" 2>/dev/null || true
done

efi="$out/BOOTX64-diag.EFI"
mlx-out/bin/brix-image "$out/kernel" "$efi" \
    flux="$out/flux" sysinfo="$out/sysinfo" brixfs="$out/brixfs" klogsave="$out/klogsave" \
    impulse="$out/impulse" impulsectl="$out/impulsectl" \
    settings/init/services/serial-shell.service=projects/brix/settings/init/services/serial-shell.service \
    settings/init/services/klogsave.service=projects/brix/settings/init/services/klogsave.service \
    settings/users=projects/brix/settings/users \
    settings/groups=projects/brix/settings/groups \
    settings/shadow=projects/brix/settings/shadow \
    "$out/coreutils/"
echo "wrote $efi"
echo "Flash it to the ESP as /EFI/BOOT/BOOTX64.EFI to see the boot log on screen."
