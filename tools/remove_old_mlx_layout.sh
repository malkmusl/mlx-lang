#!/usr/bin/env bash
# Shows, and with --remove removes, what installs from before the
# PREFIX/bin/mlx layout (tools/lib/mlx_layout.sh) left: the programs in
# /usr/local/bin and /usr/bin, and the data and libraries of a /usr/local
# install (desktop entries, icons, MLXIPC service files, MLX Audio's and
# MLX Capture's libraries, the documentation, the session entry). A file
# goes only when its counterpart in the new layout is there, so a removal
# never takes a program away; reinstall first (./install_compositor.sh,
# tools/install_login.sh, tools/install_network.sh).
#
# With --remove it also moves the marker of --replace-sound-servers to the
# new prefix and points the ALSA setting of MLX Audio at the new library
# directory. Units and mlx-init services that still name an old path are
# only reported: the installers write them anew.
#
# Usage: tools/remove_old_mlx_layout.sh [--remove] [--prefix DIR] [--destdir DIR]
#   --remove         remove (default: only show what would go)
#   --prefix DIR     the new layout's prefix (default /usr: /usr/bin/mlx)
#   --destdir DIR    look below DIR instead of / (tests)
#
# Removing without --destdir has to run as root.
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
source tools/lib/mlx_layout.sh

prefix=$mlx_default_prefix
destdir=""
remove=false

die() {
    echo "remove_old_mlx_layout.sh: $*" >&2
    exit 2
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --remove) remove=true; shift ;;
        --prefix)
            [[ $# -ge 2 && -n "$2" ]] || die "--prefix needs a directory"
            prefix=$2
            shift 2
            ;;
        --destdir)
            [[ $# -ge 2 && -n "$2" ]] || die "--destdir needs a directory"
            destdir=$2
            shift 2
            ;;
        -h|--help)
            sed -n '2,/^set -euo/p' "$0" | sed '$d; s/^# \{0,1\}//'
            exit 0
            ;;
        *) die "unknown option $1 (see --help)" ;;
    esac
done
[[ "$prefix" == /* ]] || die "--prefix must be an absolute path"
[[ -z "$destdir" || "$destdir" == /* ]] || die "--destdir must be an absolute path"
[[ "$prefix" == / ]] || prefix=${prefix%/}
[[ "$destdir" == / ]] && destdir=""
destdir=${destdir%/}
if $remove && [[ -z "$destdir" && $(id -u) -ne 0 ]]; then
    die "run it as root to remove (without --remove it only shows)"
fi

bindir=$(mlx_bindir "$prefix")
old=/usr/local
found=0
kept=0

# OLD goes when NEW (both below DESTDIR) is there.
retire() {
    local old_path=$1 new_path=$2
    [[ -e "$destdir$old_path" || -L "$destdir$old_path" ]] || return 0
    found=$((found + 1))
    if [[ ! -e "$destdir$new_path" ]]; then
        echo "kept    $old_path (no $new_path yet: reinstall first)"
        kept=$((kept + 1))
    elif $remove; then
        rm -f -- "$destdir$old_path"
        echo "removed $old_path"
    else
        echo "old     $old_path (now $new_path)"
    fi
}

# Removes DIR below DESTDIR when it is empty, and its empty parents up to
# STOP.
prune() {
    local dir=$1 stop=$2
    $remove || return 0
    while [[ "$dir" != "$stop" && "$dir" != / ]]; do
        rmdir -- "$destdir$dir" 2> /dev/null || return 0
        dir=$(dirname "$dir")
    done
}

for dir in /usr/local/bin /usr/bin; do
    [[ "$dir" == "$bindir" ]] && continue
    for program in "${mlx_all_programs[@]}"; do
        retire "$dir/$program" "$bindir/$program"
    done
done

if [[ "$prefix" != "$old" ]]; then
    for name in mlx-settings.desktop org.mlx.files.desktop org.mlx.observatory.desktop; do
        retire "$old/share/applications/$name" "$prefix/share/applications/$name"
    done
    for name in org.mlx.files.png org.mlx.settings.png org.mlx.observatory.png; do
        retire "$old/share/icons/hicolor/128x128/apps/$name" "$prefix/share/icons/hicolor/128x128/apps/$name"
    done
    for name in org.mlx.Audio.service org.mlx.PermissionAgent.service org.freedesktop.portal.Desktop.service org.freedesktop.Notifications.service; do
        retire "$old/share/mlx/dbus-1/services/$name" "$prefix/share/mlx/dbus-1/services/$name"
    done
    prune "$old/share/mlx/dbus-1/services" "$old/share"
    for name in libpulse.so.0 libpulse-simple.so.0 libpulse-mainloop-glib.so.0 libasound_module_pcm_pulse.so libasound_module_ctl_pulse.so; do
        retire "$old/lib/mlx-audio/$name" "$prefix/lib/mlx-audio/$name"
    done
    prune "$old/lib/mlx-audio" "$old/lib"
    retire "$old/lib/mlx-capture/libpipewire-0.3.so.0" "$prefix/lib/mlx-capture/libpipewire-0.3.so.0"
    prune "$old/lib/mlx-capture" "$old/lib"
    for part in login network; do
        retire "$old/share/doc/mlx/$part/README.md" "$prefix/share/doc/mlx/$part/README.md"
        prune "$old/share/doc/mlx/$part" "$old/share/doc"
    done
    retire "$old/share/wayland-sessions/mlx-compositor.desktop" "$prefix/share/wayland-sessions/mlx-compositor.desktop"
    prune "$old/share/wayland-sessions" "$old/share"

    # The masked sound servers' list: --restore-sound-servers reads it from
    # the new prefix.
    marker=share/mlx/sound-servers-replaced
    if [[ -f "$destdir$old/$marker" ]]; then
        found=$((found + 1))
        if [[ -e "$destdir$prefix/$marker" ]]; then
            if $remove; then rm -f -- "$destdir$old/$marker"; echo "removed $old/$marker"; else echo "old     $old/$marker (now $prefix/$marker)"; fi
        elif $remove; then
            install -d "$destdir$prefix/share/mlx"
            mv -- "$destdir$old/$marker" "$destdir$prefix/$marker"
            echo "moved   $old/$marker to $prefix/$marker"
        else
            echo "old     $old/$marker (moves to $prefix/$marker)"
        fi
        prune "$old/share/mlx" "$old/share"
    fi

    # ALSA's way to MLX Audio names the library directory.
    alsa=/etc/alsa/conf.d/99-zz-mlx-audio.conf
    if [[ -f "$destdir$alsa" ]] && grep -qF "$old/lib/mlx-audio/" "$destdir$alsa"; then
        found=$((found + 1))
        if [[ ! -e "$destdir$prefix/lib/mlx-audio/libasound_module_pcm_pulse.so" ]]; then
            echo "kept    $alsa (names $old/lib/mlx-audio; no $prefix/lib/mlx-audio yet: reinstall first)"
            kept=$((kept + 1))
        elif $remove; then
            sed -i "s|$old/lib/mlx-audio/|$prefix/lib/mlx-audio/|g" "$destdir$alsa"
            echo "changed $alsa: MLX Audio's ALSA plugin from $prefix/lib/mlx-audio"
        else
            echo "old     $alsa names $old/lib/mlx-audio (changes to $prefix/lib/mlx-audio)"
        fi
    fi
fi

# Units and services that still start a program from an old place.
for file in "$destdir"/etc/systemd/system/mlx-*.service "$destdir"/etc/mlx/init/services/*.service; do
    [[ -f "$file" ]] || continue
    if grep -Eq '(/usr/local/bin|/usr/bin)/mlx-' "$file"; then
        echo "stale   ${file#"$destdir"} starts a program from an old place: reinstall with the installer it came from"
    fi
done

if [[ $found -eq 0 ]]; then
    echo "nothing left from before $bindir"
elif ! $remove; then
    echo "remove them with: sudo tools/remove_old_mlx_layout.sh --remove"
elif [[ $kept -gt 0 ]]; then
    echo "$kept kept until their new counterparts are installed"
fi
exit 0
