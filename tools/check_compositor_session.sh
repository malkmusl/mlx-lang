#!/usr/bin/env bash
# Checks the "Mlx Compositor" desktop session end to end, without a
# display manager or a monitor: tools/install_compositor_session.sh stages
# an install under a temporary DESTDIR, then the staged mlx-session is
# started the way GDM or SDDM would start it, with each host that is
# installed (cage, weston) on its headless backend. The compositor must come
# up fullscreen at the host's monitor resolution (the headless outputs are
# 1280x720 in cage and 1024x640 in weston, not the compositor's 1024x768
# window size), with the keyboard layout the session picked up.
#
# Usage: tools/check_compositor_session.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

compiler_args=()
[[ $# -gt 0 ]] && compiler_args=(--compiler "$1")
tools/install_compositor_session.sh --destdir "$work/root" "${compiler_args[@]}" > "$work/install.log"
bindir="$work/root/usr/local/bin"
entry="$work/root/usr/share/wayland-sessions/mlx-compositor.desktop"
for program in mlx-compositor mlx-terminal mlx-session; do
    [[ -x "$bindir/$program" ]] || { echo "not installed: $program" >&2; cat "$work/install.log" >&2; exit 1; }
done
grep -qx "Exec=/usr/local/bin/mlx-session" "$entry" || { echo "session entry without the launcher:" >&2; cat "$entry" >&2; exit 1; }
echo "ok   staged install: programs in PREFIX/bin, session entry in wayland-sessions"

# Starts the staged session with host $1; the compositor quits after two
# seconds. Echoes the session log.
run_session() {
    local host=$1
    local home="$work/home-$host"
    mkdir -p "$home" "$work/runtime-$host"
    chmod 700 "$work/runtime-$host"
    env -i PATH="$PATH" HOME="$home" XDG_RUNTIME_DIR="$work/runtime-$host" \
        XKB_DEFAULT_LAYOUT=de MLX_SESSION_HOST="$host" MLX_SESSION_BACKEND=headless \
        MLX_COMPOSITOR_ARGS="--timeout 2" \
        timeout 30 "$bindir/mlx-session" || true
    cat "$home/.local/state/mlx-compositor/session.log"
}

checked=0
if command -v cage > /dev/null; then
    log=$(run_session cage)
    grep -q "^output: 1280x720 (monitor 1280x720)" <<< "$log" || { echo "cage: the compositor did not take the monitor's resolution" >&2; echo "$log" >&2; exit 1; }
    grep -q "^mlx-session: keyboard de" <<< "$log" || { echo "cage: keyboard layout not passed on" >&2; exit 1; }
    echo "ok   cage session: fullscreen at the monitor's 1280x720, keyboard de"
    checked=$((checked + 1))
else
    echo "skip cage is not installed"
fi
if command -v weston > /dev/null; then
    log=$(run_session weston)
    grep -q "^output: [0-9]*x[0-9]* (monitor " <<< "$log" || { echo "weston: the compositor did not start fullscreen" >&2; echo "$log" >&2; exit 1; }
    size=$(sed -n 's/^output: \([0-9]*x[0-9]*\) (monitor \([0-9]*x[0-9]*\).*/\1 \2/p' <<< "$log")
    [[ "${size% *}" == "${size#* }" ]] || { echo "weston: output ${size% *} is not the monitor's ${size#* }" >&2; exit 1; }
    grep -q "^mlx-session: keyboard de" <<< "$log" || { echo "weston: keyboard layout not passed on" >&2; exit 1; }
    echo "ok   weston session (kiosk shell): fullscreen at the monitor's ${size#* }, keyboard de"
    checked=$((checked + 1))
else
    echo "skip weston is not installed"
fi
[[ $checked -gt 0 ]] || { echo "neither cage nor weston is installed" >&2; exit 2; }
