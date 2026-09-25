#!/usr/bin/env bash
# Wire-compatibility check of std.wayland against established Wayland
# implementations (spec/06-wayland/wayland.xml <Conformance>):
#
#   1. examples/wayland-client (Mlx) opens a window on weston (libwayland
#      server) in headless mode;
#   2. examples/wayland-server (Mlx) serves wayland-info and
#      weston-simple-shm (libwayland clients);
#   3. the Mlx client talks to the Mlx compositor, which verifies the pixels
#      it receives through shared memory.
#
# Needs weston, wayland-info and weston-simple-shm on PATH (Debian/Ubuntu:
# apt-get install weston wayland-utils). Usage:
#
#   tools/check_wayland_interop.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
for tool in weston wayland-info weston-simple-shm; do
    if ! command -v "$tool" > /dev/null; then
        echo "check_wayland_interop.sh: $tool is not installed" >&2
        exit 2
    fi
done

work=$(mktemp -d)
export XDG_RUNTIME_DIR="$work/runtime"
mkdir -m 700 "$XDG_RUNTIME_DIR"
pids=()
cleanup() {
    for pid in "${pids[@]}"; do kill "$pid" 2> /dev/null || true; done
    rm -rf -- "$work"
}
trap cleanup EXIT

"$compiler" --quiet examples/wayland-client/main.mlx -o "$work/mlx-client"
"$compiler" --quiet examples/wayland-server/main.mlx -o "$work/mlx-compositor"

wait_for_socket() {
    for _ in $(seq 1 50); do
        [[ -S "$XDG_RUNTIME_DIR/$1" ]] && return 0
        sleep 0.1
    done
    echo "socket $1 did not appear" >&2
    return 1
}

# 1. Mlx client on weston.
weston --backend=headless --socket=weston-test --idle-time=0 > "$work/weston.log" 2>&1 &
pids+=($!)
wait_for_socket weston-test
WAYLAND_DISPLAY=weston-test timeout 20 "$work/mlx-client"
echo "ok   Mlx client presented frames on weston"

# 2. libwayland clients on the Mlx compositor.
"$work/mlx-compositor" mlx-test > "$work/compositor.log" 2>&1 &
pids+=($!)
wait_for_socket mlx-test
WAYLAND_DISPLAY=mlx-test timeout 10 wayland-info > "$work/info.log"
for interface in wl_compositor wl_shm xdg_wm_base; do
    grep -q "'$interface'" "$work/info.log" || { echo "wayland-info did not list $interface" >&2; exit 1; }
done
grep -q "AR24" "$work/info.log" || { echo "wayland-info did not list the shm formats" >&2; exit 1; }
echo "ok   wayland-info enumerated the Mlx compositor"

set +e
WAYLAND_DISPLAY=mlx-test timeout 2 weston-simple-shm > /dev/null 2>&1
set -e
sleep 0.2
commits=$(grep -c "commit buffer 250x250" "$work/compositor.log" || true)
if [[ "$commits" -lt 10 ]]; then
    echo "weston-simple-shm committed only $commits buffers" >&2
    exit 1
fi
echo "ok   weston-simple-shm committed $commits frames to the Mlx compositor"

# 3. Mlx client on the Mlx compositor; the pixel checksums are those of the
#    client's gradient (see examples/wayland-client/main.mlx).
WAYLAND_DISPLAY=mlx-test timeout 20 "$work/mlx-client"
sleep 0.2
for checksum in 3516967216 315644208 1409288496; do
    grep -q "commit buffer 320x240 checksum $checksum" "$work/compositor.log" || { echo "missing frame checksum $checksum" >&2; exit 1; }
done
echo "ok   Mlx client and Mlx compositor exchanged verified frames"
