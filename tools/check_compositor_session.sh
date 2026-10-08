#!/usr/bin/env bash
# Checks the "Mlx Compositor" desktop session end to end, without a
# display manager or a monitor: tools/install_compositor_session.sh stages
# an install under a temporary DESTDIR, then the staged mlx-session is
# started the way GDM or SDDM would start it, in its nested variants
# (MLX_SESSION_HOST) with each host that is installed (cage, weston) on its
# headless backend. (The default, freestanding compositor is checked by
# tools/check_compositor_drm.sh.) The compositor must come
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
tools/install_compositor_session.sh --destdir "$work/root" --replace-sound-servers "${compiler_args[@]}" > "$work/install.log"
bindir="$work/root/usr/local/bin"
entry="$work/root/usr/share/wayland-sessions/mlx-compositor.desktop"
for program in mlx-compositor mlx-terminal mlx-session mlx-dock mlx-topbar mlx-launcher mlx-settings mlx-files mlx-observatory mlx-codemap mlx-profile mlx-capture mlx-ipcd mlx-ipc mlx-audiod mlx-audio mlx-permissions; do
    [[ -x "$bindir/$program" ]] || { echo "not installed: $program" >&2; cat "$work/install.log" >&2; exit 1; }
done
grep -qx "Exec=/usr/local/bin/mlx-session" "$entry" || { echo "session entry without the launcher:" >&2; cat "$entry" >&2; exit 1; }
grep -qx "Exec=/usr/local/bin/mlx-audiod" "$work/root/usr/local/share/mlx/dbus-1/services/org.mlx.Audio.service" || { echo "MLX Audio's service file is missing" >&2; exit 1; }
grep -qx "Exec=/usr/local/bin/mlx-permissions" "$work/root/usr/local/share/mlx/dbus-1/services/org.mlx.PermissionAgent.service" || { echo "the permission agent's service file is missing" >&2; exit 1; }
grep -qx "Exec=/usr/local/bin/mlx-capture --portal" "$work/root/usr/local/share/mlx/dbus-1/services/org.freedesktop.portal.Desktop.service" || { echo "the screen-sharing portal's service file is missing" >&2; exit 1; }
for library in libpulse.so.0 libpulse-simple.so.0 libpulse-mainloop-glib.so.0 libasound_module_pcm_pulse.so libasound_module_ctl_pulse.so; do
    [[ -f "$work/root/usr/local/lib/mlx-audio/$library" ]] || { echo "MLX Audio's $library is not installed" >&2; cat "$work/install.log" >&2; exit 1; }
done
grep -q "type pulse" "$work/root/etc/alsa/conf.d/99-zz-mlx-audio.conf" && grep -qF 'lib "/usr/local/lib/mlx-audio/libasound_module_pcm_pulse.so"' "$work/root/etc/alsa/conf.d/99-zz-mlx-audio.conf" && grep -qx "autospawn = no" "$work/root/etc/pulse/client.conf.d/50-mlx-audio.conf" || { echo "--replace-sound-servers did not stage the ALSA and libpulse settings" >&2; cat "$work/install.log" >&2; exit 1; }
if command -v systemctl > /dev/null; then
    [[ "$(readlink "$work/root/etc/systemd/user/pipewire.socket")" == /dev/null && "$(readlink "$work/root/etc/systemd/user/pulseaudio.service")" == /dev/null ]] || { echo "--replace-sound-servers did not mask PipeWire and PulseAudio" >&2; cat "$work/install.log" >&2; exit 1; }
fi
tools/install_compositor_session.sh --destdir "$work/root" --restore-sound-servers > "$work/restore.log"
[[ ! -e "$work/root/etc/systemd/user/pipewire.socket" ]] || { echo "--restore-sound-servers left PipeWire masked" >&2; cat "$work/restore.log" >&2; exit 1; }
[[ ! -e "$work/root/etc/alsa/conf.d/99-zz-mlx-audio.conf" && ! -e "$work/root/etc/pulse/client.conf.d/50-mlx-audio.conf" ]] || { echo "--restore-sound-servers left the settings" >&2; cat "$work/restore.log" >&2; exit 1; }
files_entry="$work/root/usr/local/share/applications/org.mlx.files.desktop"
grep -qx "Exec=/usr/local/bin/mlx-files %U" "$files_entry" && [[ -f "$work/root/usr/local/share/icons/hicolor/128x128/apps/org.mlx.files.png" ]] || { echo "the file manager's entry or icon is missing" >&2; exit 1; }
observatory_entry="$work/root/usr/local/share/applications/org.mlx.observatory.desktop"
grep -qx "Exec=/usr/local/bin/mlx-observatory $repo_root" "$observatory_entry" && [[ -f "$work/root/usr/local/share/icons/hicolor/128x128/apps/org.mlx.observatory.png" ]] || { echo "MLX Observatory's entry or icon is missing" >&2; exit 1; }
echo "ok   staged install: programs in PREFIX/bin, MLX Audio's libraries in PREFIX/lib/mlx-audio, session entry in wayland-sessions; the sound servers replaced and restored"

# A systemctl that has PipeWire's user units and logs what it is told:
# the session must stop and mask them for itself and start them again.
mkdir -p "$work/fakebin"
cat > "$work/fakebin/systemctl" <<SCRIPT
#!/bin/sh
echo "\$*" >> "$work/systemctl.log"
case "\$*" in
*list-unit-files*) printf 'pipewire.socket enabled enabled\npipewire.service disabled enabled\nwireplumber.service enabled enabled\n' ;;
esac
SCRIPT
chmod +x "$work/fakebin/systemctl"

# Starts the staged session with host $1; the compositor quits after two
# seconds. Echoes the session log.
run_session() {
    local host=$1
    local home="$work/home-$host"
    mkdir -p "$home" "$work/runtime-$host"
    chmod 700 "$work/runtime-$host"
    env -i PATH="$work/fakebin:$PATH" HOME="$home" XDG_RUNTIME_DIR="$work/runtime-$host" \
        XKB_DEFAULT_LAYOUT=de MLX_SESSION_HOST="$host" MLX_SESSION_BACKEND=headless \
        MLX_COMPOSITOR_ARGS="--timeout 2" \
        timeout 30 "$bindir/mlx-session" || true
    cat "$home/.config/mlx/compositor.log"
}

checked=0
if command -v cage > /dev/null; then
    log=$(run_session cage)
    grep -q "^output: 1280x720 (monitor 1280x720)" <<< "$log" || { echo "cage: the compositor did not take the monitor's resolution" >&2; echo "$log" >&2; exit 1; }
    grep -q "^mlx-session: keyboard de" <<< "$log" || { echo "cage: keyboard layout not passed on" >&2; exit 1; }
    grep -q "^launch: .*/mlx-dock$" <<< "$log" || { echo "cage: the session did not start the dock" >&2; echo "$log" >&2; exit 1; }
    echo "ok   cage session: fullscreen at the monitor's 1280x720, keyboard de, the dock started"
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
    grep -q "^launch: .*/mlx-dock$" <<< "$log" || { echo "weston: the session did not start the dock" >&2; echo "$log" >&2; exit 1; }
    grep -q "^mlx-session: session bus: mlx-ipcd" <<< "$log" || { echo "weston: the session is not on mlx-ipcd" >&2; echo "$log" >&2; exit 1; }
    grep -q "^mlx-session: sound: MLX Audio has the card; stopped for the session: pipewire.socket pipewire.service wireplumber.service" <<< "$log" || { echo "weston: PipeWire was not stopped" >&2; echo "$log" >&2; exit 1; }
    grep -q "^mlx-session: sound: given back" <<< "$log" && grep -qx -- "--user mask --runtime pipewire.socket pipewire.service wireplumber.service" "$work/systemctl.log" && grep -qx -- "--user unmask --runtime pipewire.socket pipewire.service wireplumber.service" "$work/systemctl.log" && grep -qx -- "--user start pipewire.socket" "$work/systemctl.log" || { echo "weston: PipeWire was not given back" >&2; cat "$work/systemctl.log" >&2; echo "$log" >&2; exit 1; }
    echo "ok   weston session (kiosk shell): fullscreen at the monitor's ${size#* }, keyboard de, the dock started, on mlx-ipcd, PipeWire stopped and given back"
    checked=$((checked + 1))
else
    echo "skip weston is not installed"
fi
[[ $checked -gt 0 ]] || { echo "neither cage nor weston is installed" >&2; exit 2; }
tools/install_compositor_session.sh --destdir "$work/root" --uninstall > "$work/uninstall.log"
[[ ! -e "$bindir/mlx-audiod" && ! -e "$work/root/usr/local/lib/mlx-audio" && ! -e "$entry" ]] || { echo "--uninstall left files behind" >&2; cat "$work/uninstall.log" >&2; exit 1; }
echo "ok   staged uninstall: the programs, MLX Audio's libraries and the session entry removed"
