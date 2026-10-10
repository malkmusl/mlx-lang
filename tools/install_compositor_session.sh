#!/usr/bin/env bash
# Builds the Mlx compositor, terminal, dock and launcher and installs them
# as a desktop session that GDM and SDDM offer at login ("Mlx Compositor").
#
# The session (projects/desktop/compositor/session/mlx-session) runs the
# compositor freestanding: it drives the monitor (DRM/KMS) at its preferred
# resolution and reads the input devices itself, with systemd-logind
# handing out the devices and switching VTs; std.xkb makes the keymap from
# the XKB data (installed on any desktop). MLX_SESSION_HOST=cage|weston in
# the session's environment runs it nested in one of those hosts instead.
#
# Installs:
#   PREFIX/bin/mlx-compositor, PREFIX/bin/mlx-terminal, PREFIX/bin/mlx-sh, PREFIX/bin/mlx-console, PREFIX/bin/mlx-session,
#   PREFIX/bin/mlx-dock, PREFIX/bin/mlx-topbar, PREFIX/bin/mlx-launcher,
#   PREFIX/bin/mlx-settings, PREFIX/bin/mlx-files, PREFIX/bin/mlx-codemap,
#                                     PREFIX/bin/mlx-profile, PREFIX/bin/mlx-capture,
#                                     PREFIX/bin/mlx-ipcd, PREFIX/bin/mlx-ipc (MLXIPC,
#                                     the session bus), PREFIX/bin/mlx-audiod,
#                                     PREFIX/bin/mlx-audio (MLX Audio),
#                                     PREFIX/bin/mlx-permissions (the
#                                     permission agent and its dialog)
#   PREFIX/share/mlx/dbus-1/services/org.mlx.Audio.service,
#                                     org.mlx.PermissionAgent.service,
#                                     org.freedesktop.portal.Desktop.service
#                                     (MLX Audio, the agent and the
#                                     screen-sharing portal, mlx-capture
#                                     --portal, start on the first use, on
#                                     MLXIPC only)
#   PREFIX/share/applications/mlx-settings.desktop, org.mlx.files.desktop,
#                                     org.mlx.observatory.desktop (the launcher
#                                     lists them; MLX Observatory shows this
#                                     repository)
#   PREFIX/share/icons/hicolor/128x128/apps/org.mlx.files.png,
#                                     org.mlx.observatory.png
#   PREFIX/lib/mlx-audio/libpulse.so.0, libpulse-simple.so.0,
#                                     libpulse-mainloop-glib.so.0,
#                                     libasound_module_pcm_pulse.so,
#                                     libasound_module_ctl_pulse.so (MLX
#                                     Audio's client libraries, Mlx's own
#                                     libpulse and ALSA pulse plugin:
#                                     projects/desktop/libpulse)
#   PREFIX/lib/mlx-capture/libpipewire-0.3.so.0 (MLX Capture's client
#                                     library, Mlx's own libpipewire for the
#                                     programs that take a screen share:
#                                     projects/desktop/libpipewire)
#   SESSIONS/mlx-compositor.desktop   (read by GDM and SDDM)
#   ~/.local/lib/mlx-compositor/libmlx-shell.so, libmlx-render.so (for the
#   user running this; the session loads them, and loads them again when
#   tools/build_compositor_modules.sh rebuilds them)
#   ~/.config/obs-studio/plugins/mlx-capture/bin/64bit/mlx-capture.so (the
#   OBS source "MLX Capture", for the user running this)
#   with --replace-sound-servers: /etc/alsa/conf.d/99-zz-mlx-audio.conf,
#   /etc/pulse/client.conf.d/50-mlx-audio.conf, the sound servers' user
#   units masked in /etc/systemd/user
#   with --replace-libpulse: the package mlx-audio-libs (built into
#   mlx-out/session/libpulse when dpkg-deb is there), in place of libpulse0,
#   libpulse-mainloop-glib0 and libasound2-plugins
#   with --replace-libpipewire: the package mlx-capture-libs (built into
#   mlx-out/session/libpipewire when dpkg-deb is there), in place of
#   libpipewire-0.3-0 (libpipewire-0.3-0t64)
#
# Usage: tools/install_compositor_session.sh [options]
#   --prefix DIR      programs go to DIR/bin (default /usr/local)
#   --sessions DIR    the session entry's directory
#                     (default /usr/share/wayland-sessions)
#   --destdir DIR     stage everything under DIR instead (packaging, tests)
#   --compiler PATH   the Mlx compiler (default: mlx-out/bin/compiler/mlx4,
#                     else zig-out/bin/mlx1, else built with `zig build mlx1`)
#   --build-only      build into mlx-out/session and stop
#   --uninstall       remove what an install put there (and restore the
#                     sound servers when they were replaced)
#   --replace-sound-servers
#                     MLX Audio becomes the system's sound server: PipeWire,
#                     WirePlumber and PulseAudio are masked for every user
#                     (systemctl --global; from the next login), libpulse
#                     no longer starts a PulseAudio of its own, and ALSA
#                     programs play through MLX Audio by its own plugin
#                     (/etc/alsa/conf.d/99-zz-mlx-audio.conf). Their packages
#                     may then be removed, and libasound2-plugins too.
#                     Other desktops have no sound server then.
#   --replace-libpulse
#                     installs mlx-audio-libs with apt: MLX Audio's libpulse
#                     takes the place of PulseAudio's (libpulse0,
#                     libpulse-mainloop-glib0) for every program, and its
#                     ALSA plugin that of alsa-plugins' (libasound2-plugins).
#                     pulseaudio-utils (pactl, pacat) goes with libpulse0;
#                     mlx-audio does their work.
#   --replace-libpipewire
#                     installs mlx-capture-libs with apt: MLX Capture's
#                     libpipewire takes the place of PipeWire's
#                     (libpipewire-0.3-0, libpipewire-0.3-0t64) for every
#                     program (browsers, OBS, GStreamer). The PipeWire
#                     daemon's packages (pipewire, pipewire-pulse,
#                     wireplumber) need that library and go with it; in the
#                     MLX session mlx-capture and MLX Audio do their work.
#   --restore-sound-servers
#                     undoes --replace-sound-servers (PulseAudio's libpulse
#                     comes back with apt install libpulse0)
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
replace_sound=0
replace_libpulse=0
replace_libpipewire=0
restore_sound=0
while [[ $# -gt 0 ]]; do
    case $1 in
    --prefix) prefix=$2; shift ;;
    --sessions) sessions=$2; shift ;;
    --destdir) destdir=$2; shift ;;
    --compiler) compiler=$2; shift ;;
    --build-only) build_only=1 ;;
    --uninstall) uninstall=1 ;;
    --replace-sound-servers) replace_sound=1 ;;
    --replace-libpulse) replace_libpulse=1 ;;
    --replace-libpipewire) replace_libpipewire=1 ;;
    --restore-sound-servers) restore_sound=1 ;;
    -h|--help) sed -n '2,/^set -euo/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 0 ;;
    *) echo "install_compositor_session.sh: unknown option $1 (see --help)" >&2; exit 2 ;;
    esac
    shift
done
if [[ $replace_libpulse -eq 1 && -n "$destdir" ]]; then
    echo "install_compositor_session.sh: --replace-libpulse installs a package, not into --destdir" >&2
    exit 2
fi
if [[ $replace_libpipewire -eq 1 && -n "$destdir" ]]; then
    echo "install_compositor_session.sh: --replace-libpipewire installs a package, not into --destdir" >&2
    exit 2
fi

bindir="$prefix/bin"
programs=(mlx-compositor mlx-terminal mlx-sh mlx-console mlx-session mlx-dock mlx-topbar mlx-launcher mlx-settings mlx-files mlx-observatory mlx-codemap mlx-profile mlx-capture mlx-ipcd mlx-ipc mlx-audiod mlx-audio mlx-permissions)
applications="$prefix/share/applications"
icons="$prefix/share/icons/hicolor/128x128/apps"
services="$prefix/share/mlx/dbus-1/services"
audio_libs="$prefix/lib/mlx-audio"
audio_lib_files=(libpulse.so.0 libpulse-simple.so.0 libpulse-mainloop-glib.so.0 libasound_module_pcm_pulse.so libasound_module_ctl_pulse.so)
capture_libs="$prefix/lib/mlx-capture"

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

# PipeWire's, WirePlumber's and PulseAudio's user units; what replacing
# them leaves.
sound_units=(pipewire.socket pipewire.service pipewire-pulse.socket pipewire-pulse.service wireplumber.service pulseaudio.socket pulseaudio.service)
alsa_default="/etc/alsa/conf.d/99-zz-mlx-audio.conf"
pulse_client="/etc/pulse/client.conf.d/50-mlx-audio.conf"
sound_marker="$prefix/share/mlx/sound-servers-replaced"

# MLX Audio as the sound server for good (and back).
replace_sound_servers() {
    as_owner "$destdir$alsa_default" install -d "$(dirname "$destdir$alsa_default")"
    sed "s|@ALSA_PLUGIN@|$audio_libs/libasound_module_pcm_pulse.so|" projects/desktop/audio/asound.conf.in > "$build/99-zz-mlx-audio.conf"
    as_owner "$destdir$alsa_default" install -m 644 "$build/99-zz-mlx-audio.conf" "$destdir$alsa_default"
    echo "installed $destdir$alsa_default (ALSA programs play through MLX Audio)"
    as_owner "$destdir$pulse_client" install -d "$(dirname "$destdir$pulse_client")"
    printf '# MLX Audio answers PulseAudio apps; libpulse starts no PulseAudio.\nautospawn = no\n' > "$build/50-mlx-audio.conf"
    as_owner "$destdir$pulse_client" install -m 644 "$build/50-mlx-audio.conf" "$destdir$pulse_client"
    echo "installed $destdir$pulse_client (libpulse starts no PulseAudio)"
    # Staged (--destdir): masked inside the staged tree, every one of them.
    local masked=() root=()
    [[ -n "$destdir" ]] && root=(--root="$destdir")
    if command -v systemctl > /dev/null; then
        for unit in "${sound_units[@]}"; do
            if [[ -n "$destdir" || -n "$(systemctl --global list-unit-files --no-legend "$unit" 2> /dev/null)" ]]; then
                as_owner "$destdir/etc/systemd/user" systemctl "${root[@]}" --global mask "$unit" > /dev/null 2>&1 && masked+=("$unit")
            fi
        done
        [[ ${#masked[@]} -gt 0 ]] && echo "masked for every user (from the next login): ${masked[*]}"
    fi
    as_owner "$destdir$sound_marker" install -d "$(dirname "$destdir$sound_marker")"
    printf '%s\n' "${masked[@]}" > "$build/sound-servers-replaced"
    as_owner "$destdir$sound_marker" install -m 644 "$build/sound-servers-replaced" "$destdir$sound_marker"
    echo
    echo "MLX Audio is the sound server now. PipeWire, WirePlumber and PulseAudio"
    echo "no longer start; their packages (pipewire, pipewire-pulse, pipewire-alsa,"
    echo "wireplumber, pulseaudio) may be removed, and libasound2-plugins: ALSA"
    echo "programs use MLX Audio's plugin ($audio_libs)."
    if [[ -z "$destdir" ]] && ! libpulse_replaced; then
        echo "PulseAudio's libpulse0 stays until --replace-libpulse puts MLX Audio's"
        echo "in its place."
    fi
    echo "Other desktops (GNOME, KDE) have no sound now; --restore-sound-servers"
    echo "gives it back."
}

restore_sound_servers() {
    local path root=()
    [[ -n "$destdir" ]] && root=(--root="$destdir")
    if [[ -f "$destdir$sound_marker" ]] && command -v systemctl > /dev/null; then
        while read -r unit; do
            [[ -n "$unit" ]] && as_owner "$destdir/etc/systemd/user" systemctl "${root[@]}" --global unmask "$unit" > /dev/null 2>&1 && echo "unmasked $unit"
        done < "$destdir$sound_marker"
    fi
    for path in "$destdir$alsa_default" "$destdir$pulse_client" "$destdir$sound_marker"; do
        [[ -e "$path" ]] && as_owner "$path" rm -f -- "$path" && echo "removed $path"
    done
    if [[ -z "$destdir" ]] && libpulse_replaced; then
        echo "MLX Audio's libpulse (mlx-audio-libs) stays; PulseAudio's comes back with"
        echo "sudo apt install libpulse0 libpulse-mainloop-glib0 libasound2-plugins"
    fi
    return 0
}

# Whether mlx-audio-libs is installed.
libpulse_replaced() {
    dpkg-query -W -f '${Status}' mlx-audio-libs 2> /dev/null | grep -q "ok installed"
}

# Whether mlx-capture-libs is installed.
libpipewire_replaced() {
    dpkg-query -W -f '${Status}' mlx-capture-libs 2> /dev/null | grep -q "ok installed"
}

# MLX Capture's client library for every program: the package in place of
# libpipewire-0.3-0 (libpipewire-0.3-0t64).
replace_libpipewire() {
    local package
    package=$(ls "$build"/libpipewire/mlx-capture-libs_*.deb 2> /dev/null | head -n 1)
    [[ -n "$package" ]] || { echo "install_compositor_session.sh: --replace-libpipewire needs dpkg-deb (to build mlx-capture-libs)" >&2; exit 1; }
    command -v apt-get > /dev/null || { echo "install_compositor_session.sh: --replace-libpipewire needs apt-get" >&2; exit 1; }
    as_owner /var/lib/dpkg apt-get install -y "$repo_root/$package"
    echo "installed $package: MLX Capture's libpipewire for every program"
    echo "(apt install libpipewire-0.3-0t64, or libpipewire-0.3-0, brings PipeWire's"
    echo "back, with its daemon's packages)"
}

# MLX Audio's client libraries for every program: the package in place of
# libpulse0, libpulse-mainloop-glib0 and libasound2-plugins.
replace_libpulse() {
    local package
    package=$(ls "$build"/libpulse/mlx-audio-libs_*.deb 2> /dev/null | head -n 1)
    [[ -n "$package" ]] || { echo "install_compositor_session.sh: --replace-libpulse needs dpkg-deb (to build mlx-audio-libs)" >&2; exit 1; }
    command -v apt-get > /dev/null || { echo "install_compositor_session.sh: --replace-libpulse needs apt-get" >&2; exit 1; }
    as_owner /var/lib/dpkg apt-get install -y "$repo_root/$package"
    echo "installed $package: MLX Audio's libpulse and ALSA plugin for every program"
    echo "(apt install libpulse0 libpulse-mainloop-glib0 libasound2-plugins brings"
    echo "PulseAudio's back)"
}

build=mlx-out/session
mkdir -p "$build"
if [[ $restore_sound -eq 1 ]]; then
    restore_sound_servers
    exit 0
fi

if [[ $uninstall -eq 1 ]]; then
    [[ -f "$destdir$sound_marker" ]] && restore_sound_servers
    for program in "${programs[@]}"; do
        path="$destdir$bindir/$program"
        [[ -e "$path" ]] && as_owner "$path" rm -f -- "$path" && echo "removed $path"
    done
    path="$destdir$sessions/mlx-compositor.desktop"
    [[ -e "$path" ]] && as_owner "$path" rm -f -- "$path" && echo "removed $path"
    for library in "${audio_lib_files[@]}"; do
        path="$destdir$audio_libs/$library"
        [[ -e "$path" ]] && as_owner "$path" rm -f -- "$path" && echo "removed $path"
    done
    [[ -d "$destdir$audio_libs" ]] && as_owner "$destdir$audio_libs" rmdir --ignore-fail-on-non-empty -- "$destdir$audio_libs"
    path="$destdir$capture_libs/libpipewire-0.3.so.0"
    [[ -e "$path" ]] && as_owner "$path" rm -f -- "$path" && echo "removed $path"
    [[ -d "$destdir$capture_libs" ]] && as_owner "$destdir$capture_libs" rmdir --ignore-fail-on-non-empty -- "$destdir$capture_libs"
    for path in "$destdir$applications/mlx-settings.desktop" "$destdir$applications/org.mlx.files.desktop" "$destdir$applications/org.mlx.observatory.desktop" "$destdir$icons/org.mlx.files.png" "$destdir$icons/org.mlx.observatory.png" "$destdir$services/org.mlx.Audio.service" "$destdir$services/org.mlx.PermissionAgent.service" "$destdir$services/org.freedesktop.portal.Desktop.service"; do
        [[ -e "$path" ]] && as_owner "$path" rm -f -- "$path" && echo "removed $path"
    done
    exit 0
fi

# The compiler: the canonical one (tools/ensure_compiler.sh builds or
# updates mlx-out/bin/compiler/mlx4 when needed).
[[ -n "$compiler" ]] || compiler=$(tools/ensure_compiler.sh)

# Build.
echo "building with $compiler"
"$compiler" --quiet projects/desktop/compositor/main.mlx -o "$build/mlx-compositor"
"$compiler" --quiet projects/desktop/terminal/main.mlx -o "$build/mlx-terminal"
"$compiler" --quiet projects/shell/main.mlx -o "$build/mlx-sh"
"$compiler" --quiet projects/console/main.mlx -o "$build/mlx-console"
"$compiler" --quiet projects/desktop/dock/main.mlx -o "$build/mlx-dock"
"$compiler" --quiet projects/desktop/topbar/main.mlx -o "$build/mlx-topbar"
"$compiler" --quiet projects/desktop/launcher/main.mlx -o "$build/mlx-launcher"
"$compiler" --quiet projects/desktop/settings/main.mlx -o "$build/mlx-settings"
"$compiler" --quiet projects/desktop/files/main.mlx -o "$build/mlx-files"
"$compiler" --quiet projects/observatory/main.mlx -o "$build/mlx-observatory"
"$compiler" --quiet tools/profile/main.mlx -o "$build/mlx-profile"
"$compiler" --quiet projects/desktop/capture/main.mlx -o "$build/mlx-capture"
"$compiler" --quiet --plugin projects/desktop/capture/obs.mlx -o "$build/mlx-capture.so"
"$compiler" --quiet projects/desktop/ipc/main.mlx -o "$build/mlx-ipcd"
"$compiler" --quiet projects/desktop/ipc/tool.mlx -o "$build/mlx-ipc"
"$compiler" --quiet projects/desktop/audio/main.mlx -o "$build/mlx-audiod"
"$compiler" --quiet projects/desktop/audio/tool.mlx -o "$build/mlx-audio"
"$compiler" --quiet projects/desktop/permissions/main.mlx -o "$build/mlx-permissions"
deb=()
command -v dpkg-deb > /dev/null && deb=(--deb)
tools/build_libpulse.sh --compiler "$compiler" --out "$build/libpulse" "${deb[@]}" > /dev/null
tools/build_libpipewire.sh --compiler "$compiler" --out "$build/libpipewire" "${deb[@]}" > /dev/null
sed "s|@BINDIR@|$bindir|g" projects/desktop/audio/org.mlx.Audio.service.in > "$build/org.mlx.Audio.service"
sed "s|@BINDIR@|$bindir|g" projects/desktop/permissions/org.mlx.PermissionAgent.service.in > "$build/org.mlx.PermissionAgent.service"
sed "s|@BINDIR@|$bindir|g" projects/desktop/capture/org.freedesktop.portal.Desktop.service.in > "$build/org.freedesktop.portal.Desktop.service"
sed "s|@BINDIR@|$bindir|g" projects/desktop/settings/mlx-settings.desktop.in > "$build/mlx-settings.desktop"
sed "s|@BINDIR@|$bindir|g" projects/desktop/files/org.mlx.files.desktop.in > "$build/org.mlx.files.desktop"
sed "s|@BINDIR@|$bindir|g; s|@ROOT@|$repo_root|g" projects/observatory/org.mlx.observatory.desktop.in > "$build/org.mlx.observatory.desktop"
sed "s|@BINDIR@|$bindir|g" projects/desktop/compositor/session/mlx-compositor.desktop.in > "$build/mlx-compositor.desktop"
echo "built $build/mlx-compositor, mlx-terminal, mlx-sh, mlx-console, mlx-dock, mlx-topbar, mlx-launcher, mlx-settings, mlx-files, mlx-observatory, mlx-profile, mlx-capture and its OBS plugin, mlx-ipcd and mlx-ipc, mlx-audiod and mlx-audio, mlx-permissions"
echo "built $build/libpulse: MLX Audio's libpulse, libpulse-simple, libpulse-mainloop-glib and ALSA plugin${deb:+, the package mlx-audio-libs}"
echo "built $build/libpipewire: MLX Capture's libpipewire${deb:+, the package mlx-capture-libs}"
[[ $build_only -eq 1 ]] && exit 0

# Install.
as_owner "$destdir$bindir" install -d "$destdir$bindir"
as_owner "$destdir$bindir" install -m 755 "$build/mlx-compositor" "$build/mlx-terminal" "$build/mlx-sh" "$build/mlx-console" "$build/mlx-dock" "$build/mlx-topbar" "$build/mlx-launcher" "$build/mlx-settings" "$build/mlx-files" "$build/mlx-observatory" "$build/mlx-profile" "$build/mlx-capture" "$build/mlx-ipcd" "$build/mlx-ipc" "$build/mlx-audiod" "$build/mlx-audio" "$build/mlx-permissions" projects/desktop/compositor/session/mlx-session "$destdir$bindir/"
# The command line of the code map under its own name (the same program).
as_owner "$destdir$bindir" ln -sf mlx-observatory "$destdir$bindir/mlx-codemap"
as_owner "$destdir$sessions" install -d "$destdir$sessions"
as_owner "$destdir$sessions" install -m 644 "$build/mlx-compositor.desktop" "$destdir$sessions/"
as_owner "$destdir$applications" install -d "$destdir$applications"
as_owner "$destdir$applications" install -m 644 "$build/mlx-settings.desktop" "$build/org.mlx.files.desktop" "$build/org.mlx.observatory.desktop" "$destdir$applications/"
as_owner "$destdir$icons" install -d "$destdir$icons"
as_owner "$destdir$icons" install -m 644 projects/desktop/files/org.mlx.files.png projects/observatory/org.mlx.observatory.png "$destdir$icons/"
as_owner "$destdir$services" install -d "$destdir$services"
as_owner "$destdir$services" install -m 644 "$build/org.mlx.Audio.service" "$build/org.mlx.PermissionAgent.service" "$build/org.freedesktop.portal.Desktop.service" "$destdir$services/"
as_owner "$destdir$audio_libs" install -d "$destdir$audio_libs"
as_owner "$destdir$audio_libs" install -m 644 "${audio_lib_files[@]/#/$build/libpulse/}" "$destdir$audio_libs/"
as_owner "$destdir$capture_libs" install -d "$destdir$capture_libs"
as_owner "$destdir$capture_libs" install -m 644 "$build/libpipewire/libpipewire-0.3.so.0" "$destdir$capture_libs/"
for program in "${programs[@]}"; do echo "installed $destdir$bindir/$program"; done
echo "installed $destdir$sessions/mlx-compositor.desktop"
echo "installed $destdir$audio_libs: ${audio_lib_files[*]}"
echo "installed $destdir$capture_libs/libpipewire-0.3.so.0"
[[ $replace_libpulse -eq 1 ]] && replace_libpulse
[[ $replace_libpipewire -eq 1 ]] && replace_libpipewire
[[ $replace_sound -eq 1 ]] && replace_sound_servers
# The shell and renderer modules, for the user running this (not when
# staging a package).
if [[ -z "$destdir" ]]; then
    tools/build_compositor_modules.sh --compiler "$compiler"
    # OBS's per-user plugin directory.
    obs_plugin="${XDG_CONFIG_HOME:-$HOME/.config}/obs-studio/plugins/mlx-capture/bin/64bit"
    mkdir -p "$obs_plugin"
    install -m 755 "$build/mlx-capture.so" "$obs_plugin/mlx-capture.so"
    echo "installed $obs_plugin/mlx-capture.so"
fi

if [[ -z "$destdir" ]]; then
    echo
    echo "Log out and pick \"Mlx Compositor\": in GDM with the gear button after"
    echo "choosing your user, in SDDM in the session menu. Super opens the app"
    echo "launcher, the top bar shows the clock, the dock is at the bottom,"
    echo "mlx-files (Files in the launcher) browses the files, MLX Observatory"
    echo "flies through this repository's code (mlx-codemap PATH: another),"
    echo "Alt+Enter opens a terminal,"
    echo "Ctrl+Alt+F1..F12 switch VTs, Alt+Shift+Q ends the session. Pin apps"
    echo "in ~/.config/mlx/dock (one desktop entry id per line)."
    echo "Its log is"
    echo "${XDG_CONFIG_HOME:-$HOME/.config}/mlx/compositor.log."
    echo "tools/build_compositor_modules.sh [--watch] rebuilds the shell and the"
    echo "renderer; a running session loads them within a second."
fi
