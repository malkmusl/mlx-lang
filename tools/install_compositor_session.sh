#!/usr/bin/env bash
# Builds the Mlx compositor and terminal and installs them as a desktop
# session that GDM and SDDM offer at login ("Mlx Compositor").
#
# The session (examples/wayland-compositor/session/mlx-session) runs the
# compositor freestanding: it drives the monitor (DRM/KMS) at its preferred
# resolution and reads the input devices itself, with systemd-logind
# handing out the devices and switching VTs; libxkbcommon (installed on any
# desktop) makes the keymap. MLX_SESSION_HOST=cage|weston in the session's
# environment runs it nested in one of those hosts instead.
#
# Installs:
#   PREFIX/bin/mlx-compositor, PREFIX/bin/mlx-terminal, PREFIX/bin/mlx-session
#   SESSIONS/mlx-compositor.desktop   (read by GDM and SDDM)
#
# Usage: tools/install_compositor_session.sh [options]
#   --prefix DIR      programs go to DIR/bin (default /usr/local)
#   --sessions DIR    the session entry's directory
#                     (default /usr/share/wayland-sessions)
#   --destdir DIR     stage everything under DIR instead (packaging, tests)
#   --compiler PATH   the Mlx compiler (default: mlx-out/bin/compiler/mlx4,
#                     else zig-out/bin/mlx1, else built with `zig build mlx1`)
#   --build-only      build into mlx-out/session and stop
#   --uninstall       remove what an install put there
#
# Installing into system directories asks for sudo when not run as root.
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"

prefix=/usr/local
sessions=/usr/share/wayland-sessions
destdir=""
compiler=""
build_only=0
uninstall=0
while [[ $# -gt 0 ]]; do
    case $1 in
    --prefix) prefix=$2; shift ;;
    --sessions) sessions=$2; shift ;;
    --destdir) destdir=$2; shift ;;
    --compiler) compiler=$2; shift ;;
    --build-only) build_only=1 ;;
    --uninstall) uninstall=1 ;;
    -h|--help) sed -n '2,/^set -euo/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 0 ;;
    *) echo "install_compositor_session.sh: unknown option $1 (see --help)" >&2; exit 2 ;;
    esac
    shift
done

bindir="$prefix/bin"
programs=(mlx-compositor mlx-terminal mlx-session)

# Runs a command with sudo when the target is not writable by us.
as_owner() {
    local target=$1
    shift
    local probe=$target
    while [[ ! -e "$probe" ]]; do probe=$(dirname "$probe"); done
    if [[ -w "$probe" ]]; then
        "$@"
    else
        sudo "$@"
    fi
}

if [[ $uninstall -eq 1 ]]; then
    for program in "${programs[@]}"; do
        path="$destdir$bindir/$program"
        [[ -e "$path" ]] && as_owner "$path" rm -f -- "$path" && echo "removed $path"
    done
    path="$destdir$sessions/mlx-compositor.desktop"
    [[ -e "$path" ]] && as_owner "$path" rm -f -- "$path" && echo "removed $path"
    exit 0
fi

# The compiler.
if [[ -z "$compiler" ]]; then
    if [[ -x mlx-out/bin/compiler/mlx4 ]]; then
        compiler=mlx-out/bin/compiler/mlx4
    elif [[ -x zig-out/bin/mlx1 ]]; then
        compiler=zig-out/bin/mlx1
    elif command -v zig > /dev/null; then
        echo "building the Mlx compiler (zig build mlx1)"
        zig build mlx1
        compiler=zig-out/bin/mlx1
    else
        echo "install_compositor_session.sh: no Mlx compiler; build one (zig build mlx1) or pass --compiler" >&2
        exit 1
    fi
fi

# Build.
build=mlx-out/session
mkdir -p "$build"
echo "building with $compiler"
"$compiler" --quiet examples/wayland-compositor/main.mlx -o "$build/mlx-compositor"
"$compiler" --quiet examples/wayland-terminal/main.mlx -o "$build/mlx-terminal"
sed "s|@BINDIR@|$bindir|g" examples/wayland-compositor/session/mlx-compositor.desktop.in > "$build/mlx-compositor.desktop"
echo "built $build/mlx-compositor and $build/mlx-terminal"
[[ $build_only -eq 1 ]] && exit 0

# Install.
as_owner "$destdir$bindir" install -d "$destdir$bindir"
as_owner "$destdir$bindir" install -m 755 "$build/mlx-compositor" "$build/mlx-terminal" examples/wayland-compositor/session/mlx-session "$destdir$bindir/"
as_owner "$destdir$sessions" install -d "$destdir$sessions"
as_owner "$destdir$sessions" install -m 644 "$build/mlx-compositor.desktop" "$destdir$sessions/"
for program in "${programs[@]}"; do echo "installed $destdir$bindir/$program"; done
echo "installed $destdir$sessions/mlx-compositor.desktop"

if [[ -z "$destdir" ]]; then
    echo
    echo "Log out and pick \"Mlx Compositor\": in GDM with the gear button after"
    echo "choosing your user, in SDDM in the session menu. Alt+Enter opens a"
    echo "terminal, Ctrl+Alt+F1..F12 switch VTs, Alt+Shift+Q ends the session."
    echo "Its log is"
    echo "${XDG_STATE_HOME:-$HOME/.local/state}/mlx-compositor/session.log."
fi
