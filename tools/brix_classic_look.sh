#!/usr/bin/env bash
# Sourced by the brixOS desktop checks that find windows by their colours:
# the look of before the desktop took its colours from its background
# (opaque blue title bars #5aa0ff, grey for the others, the blue accent, a
# flat dark wallpaper), so their pixels are known.
#
#   . tools/brix_classic_look.sh
#   classic_look "$work"        # writes $work/classic/... and sets:
#   ${classic_entries[@]}       # the boot image's entries for it
#   $classic_env                # the desktop service's line for it
classic_look() {
    local work=$1
    mkdir -p "$work/classic/mlx"
    printf 'border-color = #5aa0ff\ninactive-border-color = #505060\ntitle-opacity = 100\naccent-color = #5aa0ff\nwallpaper = /system/settings/classic/flat.png\n' > "$work/classic/mlx/compositor.conf"
    # The scheme the apps follow, as the compositor would write it for that
    # (so the first frames of the apps are in it already).
    printf 'accent-color = #5aa0ff\n' > "$work/classic/mlx/appearance.conf"
    python3 - "$work/classic/flat.png" <<'PY'
import struct, sys, zlib
def chunk(kind, data): return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
rows = b''.join(b'\x00' + bytes((24, 26, 32)) * 16 for _ in range(16))
open(sys.argv[1], 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 16, 16, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))
PY
    classic_entries=("settings/classic/mlx/compositor.conf=$work/classic/mlx/compositor.conf" "settings/classic/mlx/appearance.conf=$work/classic/mlx/appearance.conf" "settings/classic/flat.png=$work/classic/flat.png")
    classic_env='env = XDG_CONFIG_HOME=/system/settings/classic'
}
