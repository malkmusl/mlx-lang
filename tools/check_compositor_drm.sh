#!/usr/bin/env bash
# Checks the freestanding compositor (examples/wayland-compositor with
# --backend drm) end to end against an emulated kernel and systemd-logind:
# tools/fake_drm_session.py plays the DRM card, the input devices and
# logind on a private dbus-daemon, and drives the scenario (modeset, typing
# into real Wayland terminals, mouse, touchpad, hotplug, a VT switch, quit
# with the CRTC restored).
#
# Needs root (mknod for the device nodes, /proc/PID/mem for the pointers in
# ioctl arguments), dbus-daemon and Python with GObject introspection (Gio).
#
# Usage: tools/check_compositor_drm.sh [compiler] [--screenshot PNG]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}
screenshot=()
while [[ $# -gt 0 ]]; do
    case $1 in
    --screenshot) screenshot=(--screenshot "$(realpath -m "$2")"); shift ;;
    *) compiler=$1 ;;
    esac
    shift
done

[[ $(id -u) -eq 0 ]] || { echo "check_compositor_drm.sh: needs root (mknod, /proc/PID/mem)" >&2; exit 2; }
command -v dbus-daemon > /dev/null || { echo "check_compositor_drm.sh: dbus-daemon is not installed" >&2; exit 2; }
python=""
for candidate in python3 /usr/bin/python3 /usr/bin/python3.13 /usr/bin/python3.12 /usr/bin/python3.11; do
    if command -v "$candidate" > /dev/null && "$candidate" -c "import gi; gi.require_version('Gio', '2.0'); from gi.repository import Gio" 2> /dev/null; then
        python=$candidate
        break
    fi
done
[[ -n "$python" ]] || { echo "check_compositor_drm.sh: needs Python with GObject introspection (python3-gi)" >&2; exit 2; }

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
"$compiler" --quiet examples/wayland-compositor/main.mlx -o "$work/mlx-compositor"
"$compiler" --quiet examples/wayland-terminal/main.mlx -o "$work/mlx-terminal"
"$python" tools/fake_drm_session.py "$work/mlx-compositor" "$work/mlx-terminal" "${screenshot[@]}"
