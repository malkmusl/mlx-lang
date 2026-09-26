#!/usr/bin/env bash
# Installs the Mlx compositor as a desktop session, checks the installation
# and the machine, and shows the session log: the one script to run when
# the session does not come up.
#
#   ./install_compositor.sh              build, install into /usr/bin, check
#   ./install_compositor.sh --check      only check an installation and the
#                                        machine (no build)
#   ./install_compositor.sh --log        show the last session log
#   ./install_compositor.sh --uninstall  remove the installation
#
# Options:
#   --prefix DIR    programs go to DIR/bin (default /usr: /usr/bin, which
#                   is on every PATH; tools/install_compositor_session.sh
#                   defaults to /usr/local)
#   --sessions DIR  where GDM and SDDM read session entries
#                   (default /usr/share/wayland-sessions)
#   --compiler PATH the Mlx compiler (default: mlx-out/bin/compiler/mlx4,
#                   else zig-out/bin/mlx1, else built with zig)
#
# Installing into system directories asks for sudo when not run as root.
# The build and the copying are tools/install_compositor_session.sh's; this
# script adds the checks.
set -uo pipefail

repo_root=$(cd "$(dirname "$0")" && pwd)
cd "$repo_root"

prefix=/usr
sessions=/usr/share/wayland-sessions
compiler=""
mode=install
while [[ $# -gt 0 ]]; do
    case $1 in
    --prefix) prefix=$2; shift ;;
    --sessions) sessions=$2; shift ;;
    --compiler) compiler=$2; shift ;;
    --check) mode=check ;;
    --log) mode=log ;;
    --uninstall) mode=uninstall ;;
    -h|--help) sed -n '2,/^set -uo/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 0 ;;
    *) echo "install_compositor.sh: unknown option $1 (see --help)" >&2; exit 2 ;;
    esac
    shift
done

bindir="$prefix/bin"
log_file="${MLX_SESSION_LOG:-${XDG_CONFIG_HOME:-$HOME/.config}/mlx/compositor.log}"
problems=0

ok() { echo "ok    $*"; }
bad() { echo "FAIL  $*"; problems=$((problems + 1)); }
note() { echo "note  $*"; }

if [[ $mode == log ]]; then
    trace="/tmp/mlx-session-$(id -u).log"
    if [[ -f "$trace" ]]; then
        echo "== $trace (every start of the session launcher)"
        tail -n 20 "$trace"
        echo
    else
        echo "== no $trace: the session launcher (mlx-session) was never started for this user"
        echo
    fi
    if [[ -f "$log_file" ]]; then
        echo "== $log_file"
        cat "$log_file"
        [[ -f "$log_file.old" ]] && echo && echo "(the run before that is in $log_file.old)"
    else
        echo "== no $log_file"
        echo "The session launcher never got far enough to write it, and a compositor"
        echo "started by hand (mlx-compositor --backend drm) writes it only since the"
        echo "install that added this line. Reinstall with this script."
    fi
    if command -v journalctl > /dev/null 2>&1; then
        echo
        echo "== the display manager's journal lines about the session (this boot)"
        journalctl -b --no-pager -q -g 'mlx-session|mlx-compositor|wayland-sessions|Mlx' 2> /dev/null | tail -n 40 || echo "(not readable: try with sudo, or add yourself to the systemd-journal group)"
    fi
    exit 0
fi

if [[ $mode == uninstall ]]; then
    exec tools/install_compositor_session.sh --prefix "$prefix" --sessions "$sessions" --uninstall
fi

if [[ $mode == install ]]; then
    # System directories need root: sudo, unless we are root already.
    if [[ $(id -u) -ne 0 ]] && { [[ ! -w "$bindir" && ! -w "$(dirname "$bindir")" ]] || [[ ! -w "$sessions" && ! -w "$(dirname "$sessions")" ]]; }; then
        if command -v sudo > /dev/null 2>&1; then
            echo "== Installing into $bindir and $sessions needs root: sudo will ask for your password"
            sudo -v || { echo "install_compositor.sh: sudo failed; run the script as root instead (su -c ./install_compositor.sh)" >&2; exit 1; }
        else
            echo "install_compositor.sh: $bindir is not writable and sudo is not installed; run the script as root" >&2
            exit 1
        fi
    fi
    compiler_args=()
    [[ -n "$compiler" ]] && compiler_args=(--compiler "$compiler")
    echo "== Building and installing"
    if ! tools/install_compositor_session.sh --prefix "$prefix" --sessions "$sessions" "${compiler_args[@]}"; then
        echo "install_compositor.sh: the build or the installation failed (above)" >&2
        exit 1
    fi
    echo
    echo "== Installed files"
    ls -l "$bindir"/mlx-compositor "$bindir"/mlx-terminal "$bindir"/mlx-session "$sessions"/mlx-compositor.desktop 2>&1
    echo
fi

echo "== The installation"
for program in mlx-compositor mlx-terminal mlx-session; do
    if [[ -x "$bindir/$program" ]]; then
        ok "$bindir/$program"
    else
        bad "$bindir/$program is missing or not executable"
    fi
done
if [[ -x "$bindir/mlx-compositor" ]]; then
    if head -c 4 "$bindir/mlx-compositor" | grep -q ELF; then
        ok "mlx-compositor is a program ($(sha256sum "$bindir/mlx-compositor" | cut -c1-16))"
    else
        bad "$bindir/mlx-compositor is not an ELF program"
    fi
    # It parses its arguments and quits at once on an unknown option.
    if "$bindir/mlx-compositor" --no-such-option > /dev/null 2>&1; then
        bad "mlx-compositor accepted an unknown option"
    else
        ok "mlx-compositor runs (it rejects an unknown option)"
    fi
fi
found=$(command -v mlx-compositor 2> /dev/null || true)
if [[ -n "$found" ]]; then
    if [[ "$found" == "$bindir/mlx-compositor" ]]; then
        ok "mlx-compositor is on PATH ($found)"
    else
        note "PATH finds another mlx-compositor first: $found (the session uses $bindir/mlx-compositor)"
    fi
else
    note "$bindir is not on this shell's PATH (the session does not need it: its entry uses absolute paths)"
fi
entry="$sessions/mlx-compositor.desktop"
if [[ -f "$entry" ]]; then
    ok "$entry"
    exec_path=$(sed -n 's/^Exec=//p' "$entry")
    try_path=$(sed -n 's/^TryExec=//p' "$entry")
    if [[ -x "$exec_path" && -x "$try_path" ]]; then
        ok "its Exec and TryExec point at an executable ($exec_path)"
    else
        bad "its Exec ($exec_path) or TryExec ($try_path) is not executable; GDM and SDDM hide such entries"
    fi
    if grep -q '^DesktopNames=' "$entry"; then ok "it names its desktop"; fi
else
    bad "$entry is missing: GDM and SDDM offer no Mlx Compositor session"
fi
for other in /usr/local/share/wayland-sessions /usr/share/wayland-sessions; do
    if [[ "$other" != "$sessions" && -f "$other/mlx-compositor.desktop" ]]; then
        note "another entry at $other/mlx-compositor.desktop (from an earlier install?); remove it if its paths are stale"
    fi
done
if [[ -x "$bindir/mlx-session" ]] && ! sh -n "$bindir/mlx-session" 2> /dev/null; then
    bad "$bindir/mlx-session has a syntax error"
fi

echo
echo "== Display managers"
found_dm=0
for dm in gdm gdm3 sddm lightdm greetd; do
    if command -v "$dm" > /dev/null 2>&1 || [[ -d /etc/$dm ]] || [[ -d /etc/$dm.d ]]; then
        found_dm=1
        case $dm in
        gdm|gdm3) ok "$dm: pick the session with the gear button after choosing your user" ;;
        sddm) ok "sddm: pick the session in the session menu (bottom left)" ;;
        lightdm) note "lightdm: reads /usr/share/wayland-sessions only with a Wayland-capable greeter" ;;
        greetd) note "greetd: its greeter must offer $entry itself" ;;
        esac
    fi
done
[[ $found_dm -eq 0 ]] && note "no display manager found; the session can also be started from a text console: $bindir/mlx-session"
if command -v systemctl > /dev/null 2>&1; then
    active=$(systemctl is-active display-manager.service 2> /dev/null || true)
    [[ -n "$active" ]] && note "display-manager.service is $active"
fi

echo
echo "== This machine"
if [[ -S /run/dbus/system_bus_socket ]]; then
    ok "system bus (logind hands out the devices, no root needed)"
else
    bad "no system bus at /run/dbus/system_bus_socket: without logind the devices are opened directly, which needs the video and input groups"
fi
if command -v loginctl > /dev/null 2>&1; then
    if loginctl list-seats --no-legend 2> /dev/null | grep -q seat0; then ok "logind seat0"; else note "logind lists no seat0"; fi
fi
cards=$(ls /dev/dri/card* 2> /dev/null || true)
if [[ -n "$cards" ]]; then
    ok "DRM cards: $(echo "$cards" | tr '\n' ' ')"
    for card in $cards; do
        name=$(basename "$card")
        status=$(cat /sys/class/drm/"$name"-*/status 2> /dev/null | grep -c '^connected' || true)
        [[ "$status" -gt 0 ]] && ok "$name has $status connected connector(s)"
    done
else
    bad "no /dev/dri/card*: no DRM card to drive"
fi
groups_now=$(id -Gn 2> /dev/null || true)
for group in video input; do
    if grep -qw "$group" <<< "$groups_now"; then ok "you are in the $group group"; else note "you are not in the $group group (only needed without logind)"; fi
done
icds=$(ls /usr/share/vulkan/icd.d/*.json /etc/vulkan/icd.d/*.json 2> /dev/null | xargs -n1 basename 2> /dev/null | tr '\n' ' ' || true)
if [[ -n "$icds" ]]; then
    ok "Vulkan drivers: $icds"
    grep -q lvp <<< "$icds" && note "lavapipe (lvp) is a CPU device; the compositor prefers a driver with a GPU and takes lavapipe only when none works"
else
    note "no Vulkan driver manifests: the compositor composes on the CPU (mesa-vulkan-drivers provides RADV, ANV and NVK)"
fi
if ldconfig -p 2> /dev/null | grep -q libxkbcommon.so.0; then ok "libxkbcommon (the keymap)"; else bad "libxkbcommon.so.0 not found: keyboards will have no keymap"; fi
if [[ -f "$log_file" ]]; then
    note "last session log: $log_file ($(date -r "$log_file" '+%Y-%m-%d %H:%M')); show it with: $0 --log"
    if grep -q "first frame on screen" "$log_file"; then ok "the last session showed a frame"; fi
    if grep -q "modeset failed" "$log_file"; then bad "the last session could not set the mode: $(grep -m1 'modeset failed' "$log_file")"; fi
    if grep -q "crashed:" "$log_file"; then bad "the last session crashed: $(grep -m1 'crashed:' "$log_file")"; fi
    if grep -q "killed by signal" "$log_file"; then bad "$(grep -m1 'killed by signal' "$log_file")"; fi
else
    note "no session log yet at $log_file (written when the session starts)"
fi

echo
if [[ $problems -eq 0 ]]; then
    echo "Everything checks out. Log out, pick \"Mlx Compositor\" at the login screen,"
    echo "and if the screen stays black, switch to a text console (Ctrl+Alt+F3),"
    echo "log in, and run: $0 --log"
else
    echo "$problems problem(s) above."
    exit 1
fi
