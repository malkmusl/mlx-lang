#!/usr/bin/env bash
# mlx-splash (projects/desktop/splash) end to end: pictures without a
# screen, the Plymouth protocol and mlx-splashctl, and the screen on an
# emulated KMS card (tools/check_splash.py). Nothing of the machine's is
# touched: no card, framebuffer, terminal or plymouthd socket.
#
#   tools/check_splash.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }

"$compiler" --quiet projects/desktop/splash/main.mlx -o "$work/mlx-splash"
"$compiler" --quiet projects/desktop/splash/tool.mlx -o "$work/mlx-splashctl"

# Pictures: one frame, a few in a row, the timing.
"$work/mlx-splash" --size 1280x720 --png "$work/frame.png" --at 3 --status "Starting network" >/dev/null || fail "--png"
[[ $(stat -c %s "$work/frame.png") -gt 100000 ]] || fail "the PNG frame is (nearly) empty"
mkdir "$work/frames"
"$work/mlx-splash" --size 640x360 --frames "$work/frames" --count 3 --mode shutdown || fail "--frames"
[[ -f "$work/frames/frame-0002.png" ]] || fail "--frames wrote no third frame"
"$work/mlx-splash" --size 1280x720 --bench 5 | grep -q "^set up: [0-9]* ms, frame: [0-9]* us$" || fail "--bench"
echo "ok   pictures: --png, --frames, --bench"

# The fake card (tools/fake_drm_session.py) is loaded with the compositor's
# check's Python, which has GObject introspection.
python=""
for candidate in python3 /usr/bin/python3 /usr/bin/python3.13 /usr/bin/python3.12 /usr/bin/python3.11; do
    if command -v "$candidate" >/dev/null 2>&1 && "$candidate" -c "import gi" >/dev/null 2>&1; then
        python=$candidate
        break
    fi
done
[[ -n "$python" ]] || { echo "check_splash.sh: needs Python with GObject introspection (python3-gi)" >&2; exit 2; }
"$python" tools/check_splash.py "$work/mlx-splash" "$work/mlx-splashctl" "$work"
