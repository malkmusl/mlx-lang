#!/usr/bin/env bash
# Builds the Mlx compositor, terminal, dock and launcher and installs them
# as a desktop session that GDM and SDDM offer at login ("Mlx Compositor").
#
# The session (examples/wayland-compositor/session/mlx-session) runs the
# compositor freestanding: it drives the monitor (DRM/KMS) at its preferred
# resolution and reads the input devices itself, with systemd-logind
# handing out the devices and switching VTs; libxkbcommon (installed on any
# desktop) makes the keymap. MLX_SESSION_HOST=cage|weston in the session's
# environment runs it nested in one of those hosts instead.
#
# Installs:
#   PREFIX/bin/mlx-compositor, PREFIX/bin/mlx-terminal, PREFIX/bin/mlx-session,
#   PREFIX/bin/mlx-dock, PREFIX/bin/mlx-topbar, PREFIX/bin/mlx-launcher,
#   PREFIX/bin/mlx-settings
#   PREFIX/share/applications/mlx-settings.desktop  (the launcher lists it)
#   SESSIONS/mlx-compositor.desktop   (read by GDM and SDDM)
#   ~/.local/lib/mlx-compositor/libmlx-shell.so, libmlx-render.so (for the
#   user running this; the session loads them, and loads them again when
#   tools/build_compositor_modules.sh rebuilds them)
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
programs=(mlx-compositor mlx-terminal mlx-session mlx-dock mlx-topbar mlx-launcher mlx-settings)
applications="$prefix/share/applications"

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
    path="$destdir$applications/mlx-settings.desktop"
    [[ -e "$path" ]] && as_owner "$path" rm -f -- "$path" && echo "removed $path"
    exit 0
fi

# The compiler: the canonical one (tools/ensure_compiler.sh builds or
# updates mlx-out/bin/compiler/mlx4 when needed).
[[ -n "$compiler" ]] || compiler=$(tools/ensure_compiler.sh)

# Build.
build=mlx-out/session
mkdir -p "$build"
echo "building with $compiler"
"$compiler" --quiet examples/wayland-compositor/main.mlx -o "$build/mlx-compositor"
"$compiler" --quiet examples/wayland-terminal/main.mlx -o "$build/mlx-terminal"
"$compiler" --quiet examples/mlx-dock/main.mlx -o "$build/mlx-dock"
"$compiler" --quiet examples/mlx-topbar/main.mlx -o "$build/mlx-topbar"
"$compiler" --quiet examples/mlx-launcher/main.mlx -o "$build/mlx-launcher"
"$compiler" --quiet examples/mlx-settings/main.mlx -o "$build/mlx-settings"
sed "s|@BINDIR@|$bindir|g" examples/mlx-settings/mlx-settings.desktop.in > "$build/mlx-settings.desktop"
sed "s|@BINDIR@|$bindir|g" examples/wayland-compositor/session/mlx-compositor.desktop.in > "$build/mlx-compositor.desktop"
echo "built $build/mlx-compositor, mlx-terminal, mlx-dock, mlx-topbar, mlx-launcher and mlx-settings"
[[ $build_only -eq 1 ]] && exit 0

# Install.
as_owner "$destdir$bindir" install -d "$destdir$bindir"
as_owner "$destdir$bindir" install -m 755 "$build/mlx-compositor" "$build/mlx-terminal" "$build/mlx-dock" "$build/mlx-topbar" "$build/mlx-launcher" "$build/mlx-settings" examples/wayland-compositor/session/mlx-session "$destdir$bindir/"
as_owner "$destdir$sessions" install -d "$destdir$sessions"
as_owner "$destdir$sessions" install -m 644 "$build/mlx-compositor.desktop" "$destdir$sessions/"
as_owner "$destdir$applications" install -d "$destdir$applications"
as_owner "$destdir$applications" install -m 644 "$build/mlx-settings.desktop" "$destdir$applications/"
for program in "${programs[@]}"; do echo "installed $destdir$bindir/$program"; done
echo "installed $destdir$sessions/mlx-compositor.desktop"
# The shell and renderer modules, for the user running this (not when
# staging a package).
if [[ -z "$destdir" ]]; then
    tools/build_compositor_modules.sh --compiler "$compiler"
fi

if [[ -z "$destdir" ]]; then
    echo
    echo "Log out and pick \"Mlx Compositor\": in GDM with the gear button after"
    echo "choosing your user, in SDDM in the session menu. Super opens the app"
    echo "launcher, the top bar shows the clock, the dock is at the bottom,"
    echo "Alt+Enter opens a terminal,"
    echo "Ctrl+Alt+F1..F12 switch VTs, Alt+Shift+Q ends the session. Pin apps"
    echo "in ~/.config/mlx/dock (one desktop entry id per line)."
    echo "Its log is"
    echo "${XDG_CONFIG_HOME:-$HOME/.config}/mlx/compositor.log."
    echo "tools/build_compositor_modules.sh [--watch] rebuilds the shell and the"
    echo "renderer; a running session loads them within a second."
fi
