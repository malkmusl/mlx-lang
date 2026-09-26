#!/usr/bin/env bash
# End-to-end check of the Vulkan Wayland examples: examples/wayland-compositor
# with both renderers (cpu, vulkan) showing examples/vulkan-wayland-client in
# both of the client's zero-copy paths:
#
#   shm-direct  the client's GPU renders into its wl_shm pool (host memory
#               imported into Vulkan);
#   dmabuf      the client hands its buffers over with linux-dmabuf (a memfd
#               offered as a dma-buf, --test-memfd-dmabuf, since lavapipe
#               cannot export dma-bufs); the compositor maps it (cpu) or
#               imports it into Vulkan (vulkan).
#
# The vulkan renderer runs once more with MLX_VULKAN_NO_HOST_IMPORT=1, as on
# drivers that cannot import the shared memory (RADV): the frame is copied
# into the output and the client's wl_shm buffers are uploaded.
#
# tools/wayland-test-host plays the session compositor and saves the frame
# it receives. Each frame is checked pixel by pixel: the client's window must
# hold the pattern shader's output for a single time value, with the
# client's label (std.truetype text drawn by the `text` shader) at the
# bottom, and everything outside it (background, focus frame, title bar
# with the window title) must match the CPU renderer exactly - so the
# compositor's GPU-drawn title equals its CPU-drawn one.
#
# Needs a Vulkan driver with VK_EXT_external_memory_host and
# VK_EXT_external_memory_dma_buf (lavapipe) and xkbcli. Usage:
#
#   tools/check_vulkan_wayland.sh [compiler] [lavapipe manifest]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
manifest=${2:-/usr/share/vulkan/icd.d/lvp_icd.json}
command -v xkbcli > /dev/null || { echo "check_vulkan_wayland.sh: xkbcli is not installed" >&2; exit 2; }
[[ -f "$manifest" ]] || { echo "check_vulkan_wayland.sh: no Vulkan driver manifest at $manifest" >&2; exit 2; }
export VK_DRIVER_FILES=$manifest

work=$(mktemp -d)
export XDG_RUNTIME_DIR="$work/runtime"
mkdir -m 700 "$XDG_RUNTIME_DIR"
trap 'rm -rf -- "$work"' EXIT

"$compiler" --quiet examples/wayland-compositor/main.mlx -o "$work/mlx-compositor"
"$compiler" --quiet examples/vulkan-wayland-client/main.mlx -o "$work/vulkan-client"
"$compiler" --quiet tools/wayland-test-host/main.mlx -o "$work/test-host"
xkbcli compile-keymap --layout us > "$work/us.xkb"

# --run takes a program without arguments: one wrapper per client mode.
printf '#!/bin/sh\nexec "%s" --mode shm-direct --verbose\n' "$work/vulkan-client" > "$work/client-shm"
printf '#!/bin/sh\nexec "%s" --mode dmabuf --test-memfd-dmabuf --verbose\n' "$work/vulkan-client" > "$work/client-dmabuf"
chmod +x "$work/client-shm" "$work/client-dmabuf"

cat > "$work/shot.script" <<SCRIPT
wait 3000
shot $work/frame.ppm
close
SCRIPT

# run NAME RENDERER CLIENT
run() {
    local name=$1 renderer=$2 client=$3
    rm -f "$work/frame.ppm"
    "$work/test-host" "host-$name" "$work/us.xkb" "$work/shot.script" > "$work/$name-host.log" 2>&1 &
    local host_pid=$!
    for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/host-$name" ]] && break; sleep 0.1; done
    WAYLAND_DISPLAY="host-$name" MLX_VULKAN_NO_HOST_IMPORT=${no_host_import:-} timeout 60 "$work/mlx-compositor" --verbose --renderer "$renderer" --size 640x480 \
        --socket "nested-$name" --run "$work/$client" > "$work/$name.log" 2>&1 || true
    wait "$host_pid" || { echo "FAIL $name: the test host failed" >&2; cat "$work/$name-host.log" "$work/$name.log" >&2; exit 1; }
    [[ -f "$work/frame.ppm" ]] || { echo "FAIL $name: no frame" >&2; cat "$work/$name.log" >&2; exit 1; }
    mv "$work/frame.ppm" "$work/$name.ppm"
}

check() {
    local name=$1 renderer=$2 client=$3 expect=$4
    run "$name" "$renderer" "$client"
    if [[ -n "$expect" ]] && ! grep -q "$expect" "$work/$name.log"; then
        echo "FAIL $name: expected '$expect' in the log" >&2
        cat "$work/$name.log" >&2
        exit 1
    fi
    python3 - "$work/$name.ppm" "$work/cpu-shm.ppm" <<'PY' || { cat "$work/$name.log" >&2; exit 1; }
import os, sys
def load(path):
    data = open(path, 'rb').read()
    _, size, _, pixels = data.split(b'\n', 3)
    width, height = map(int, size.split())
    return width, height, pixels
width, height, pixels = load(sys.argv[1])
def rgb(x, y):
    offset = (y * width + x) * 3
    return tuple(pixels[offset:offset + 3])
# The first window is placed at (24, 24); the client is 480x320.
left, top, w, h = 24, 24, 480, 320
t = rgb(left, top)[0]
for tt in range(t, 1024, 256):
    if all(rgb(left + x, top + y) == ((x + tt) & 255, (y + tt // 2) & 255, ((x ^ y) + 2 * tt) & 255)
           for x in range(0, w, 7) for y in range(0, h - 32, 5)):
        break
else:
    sys.exit("window does not hold the pattern: %r at its origin" % (rgb(left, top),))
focus = (0x5a, 0xa0, 0xff)
assert rgb(left - 1, top + 100) == focus and rgb(left + w + 1, top + 100) == focus, "no focus frame"
# The client's label (white text) in its bottom-left corner.
label = sum(1 for y in range(top + h - 30, top + h) for x in range(left + 8, left + 260) if rgb(x, y) == (255, 255, 255))
assert label > 40, "no label in the client window (%d white pixels)" % label
# The title bar above the frame: focus color with the title in white.
bar_top = top - 2 - 20
assert rgb(left + w - 4, bar_top + 3) == focus, "no title bar"
# (anti-aliased white over the bar: red well above the bar's 0x5a)
title = sum(1 for y in range(bar_top, top - 2) for x in range(left, left + 200) if rgb(x, y)[0] > 180)
assert title > 40, "no title text in the title bar (%d light pixels)" % title
reference = sys.argv[2]
if os.path.exists(reference) and reference != sys.argv[1]:
    _, _, expected = load(reference)
    for y in range(height):
        for x in range(width):
            if left <= x < left + w and top <= y < top + h:
                continue
            offset = (y * width + x) * 3
            if pixels[offset:offset + 3] != expected[offset:offset + 3]:
                sys.exit("differs from the CPU renderer at (%d, %d)" % (x, y))
PY
    echo "ok   $renderer renderer${no_host_import:+ (no host-memory import)}, client $client"
}

check cpu-shm cpu client-shm "mode: shm-direct"
check cpu-dmabuf cpu client-dmabuf "dmabuf buffer created"
check vulkan-shm vulkan client-shm "renderer: vulkan"
check vulkan-dmabuf vulkan client-dmabuf "dmabuf buffer created"
no_host_import=1
check vulkan-copy-shm vulkan client-shm "uploading changed buffers"
check vulkan-copy-dmabuf vulkan client-dmabuf "copying each frame"
