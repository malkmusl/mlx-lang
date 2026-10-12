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

# The installer, staged only (--destdir): units, the display manager's
# drop-in, Plymouth masked with --enable and put back with --disable.
stage="$work/stage"
mkdir -p "$stage/etc/systemd/system"
ln -s /usr/lib/systemd/system/gdm.service "$stage/etc/systemd/system/display-manager.service"
echo "[Unit]" > "$stage/etc/systemd/system/plymouth-quit.service"
tools/install_splash.sh --destdir "$stage" --enable "$compiler" > "$work/install.log" 2>&1 || { cat "$work/install.log" >&2; fail "install_splash.sh --enable"; }
units="$stage/etc/systemd/system"
for path in usr/bin/mlx/mlx-splash usr/bin/mlx/mlx-splashctl etc/systemd/system/mlx-splash.service \
            etc/systemd/system/gdm.service.d/mlx-splash.conf etc/mlx/init/services/mlx-splash.service; do
    [[ -e "$stage/$path" ]] || fail "the installer left out $path"
done
[[ $(readlink "$units/sysinit.target.wants/mlx-splash.service") == /etc/systemd/system/mlx-splash.service ]] || fail "mlx-splash.service not enabled"
[[ $(readlink "$units/plymouth-start.service") == /dev/null && $(readlink "$units/plymouth-quit.service") == /dev/null ]] || fail "Plymouth's units not masked"
grep -q "^ExecStartPre=-/usr/bin/mlx/mlx-splashctl quit --retain-splash$" "$units/gdm.service.d/mlx-splash.conf" || fail "the display manager's drop-in"
tools/install_splash.sh --destdir "$stage" --disable > "$work/install.log" 2>&1 || fail "install_splash.sh --disable"
[[ -f "$units/plymouth-quit.service" && ! -L "$units/plymouth-quit.service" && ! -e "$units/plymouth-start.service" ]] || fail "Plymouth's units not put back"
[[ ! -e "$units/sysinit.target.wants/mlx-splash.service" ]] || fail "mlx-splash.service still enabled"
echo "ok   the installer (staged): units, the display manager's drop-in, Plymouth masked and put back"
