#!/usr/bin/env bash
# ignition (projects/desktop/splash) end to end: pictures without a
# screen, the Plymouth protocol and ignitionctl, and the screen on an
# emulated KMS card (tools/check_splash.py). Nothing of the machine's is
# touched: no card, framebuffer, terminal or plymouthd socket.
#
#   tools/check_splash.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}

# The protocol deliberately accepts control requests from root only.
# A disposable user+mount namespace gives the check those capabilities
# without granting it any privilege over the host.
if [[ ${MLX_SPLASH_CHECK_NAMESPACE:-0} != 1 ]]; then
    exec unshare --user --map-root-user --mount --propagation private \
        env MLX_SPLASH_CHECK_NAMESPACE=1 "$0" "$@"
fi

work=$(mktemp -d)
mounted_dri=false
cleanup() {
    if $mounted_dri; then umount /dev/dri 2>/dev/null || true; fi
    rm -rf -- "$work"
}
trap cleanup EXIT
fail() { echo "FAIL $1" >&2; exit 1; }

"$compiler" --quiet projects/desktop/splash/main.mlx -o "$work/ignition"
"$compiler" --quiet projects/desktop/splash/tool.mlx -o "$work/ignitionctl"

# Pictures: one frame, a few in a row, the timing.
"$work/ignition" --size 1280x720 --png "$work/frame.png" --at 3 --status "Starting network" >/dev/null || fail "--png"
[[ $(stat -c %s "$work/frame.png") -gt 100000 ]] || fail "the PNG frame is (nearly) empty"
mkdir "$work/frames"
"$work/ignition" --size 640x360 --frames "$work/frames" --count 3 --mode shutdown || fail "--frames"
[[ -f "$work/frames/frame-0002.png" ]] || fail "--frames wrote no third frame"
"$work/ignition" --size 1280x720 --bench 5 | grep -q "^set up: [0-9]* ms, frame: [0-9]* us, [0-9]* threads$" || fail "--bench"
echo "ok   pictures: --png, --frames, --bench"

# Initramfs can start before a DRM node exists. Hide the host's cards in
# this private mount namespace, create card0 only after the daemon is up,
# and verify that a later probe opens the newly appeared node.
if [[ -d /dev/dri ]] && command -v strace >/dev/null 2>&1; then
    mount -t tmpfs -o mode=755 tmpfs /dev/dri
    mounted_dri=true
    probe_socket="/ignition-reprobe-$$"
    strace -f -qq -e trace=openat -o "$work/reprobe.strace" \
        "$work/ignition" --no-tty --socket "$probe_socket" \
        --framebuffer "$work/no-framebuffer" --size 320x180 \
        >"$work/reprobe.log" 2>&1 &
    probe_pid=$!
    for _ in {1..100}; do
        "$work/ignitionctl" --socket "$probe_socket" ping >/dev/null 2>&1 && break
        kill -0 "$probe_pid" 2>/dev/null || { cat "$work/reprobe.log" >&2; fail "splash ended before delayed KMS probe"; }
        sleep 0.02
    done
    "$work/ignitionctl" --socket "$probe_socket" ping >/dev/null 2>&1 || fail "splash did not listen without an initial KMS card"
    : > /dev/dri/card0
    sleep 1.3
    "$work/ignitionctl" --socket "$probe_socket" quit >/dev/null
    wait "$probe_pid" || fail "splash failed after delayed KMS probe"
    grep -Eq '"/dev/dri/card0".*= [0-9]+' "$work/reprobe.strace" || fail "the splash did not reopen a KMS node that appeared later"
    umount /dev/dri
    mounted_dri=false
    echo "ok   initramfs timing: no initial KMS node, a later card0 is probed"
else
    echo "skip delayed KMS-node probe (/dev/dri or strace unavailable)"
fi

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
"$python" tools/check_splash.py "$work/ignition" "$work/ignitionctl" "$work"

# The installer, staged only (--destdir): units, the display manager's
# drop-in, Plymouth masked with --enable and put back with --disable.
stage="$work/stage"
mkdir -p "$stage/etc/systemd/system"
ln -s /usr/lib/systemd/system/gdm.service "$stage/etc/systemd/system/display-manager.service"
echo "[Unit]" > "$stage/etc/systemd/system/plymouth-quit.service"
tools/install_splash.sh --destdir "$stage" --enable "$compiler" > "$work/install.log" 2>&1 || { cat "$work/install.log" >&2; fail "install_splash.sh --enable"; }
units="$stage/etc/systemd/system"
for path in usr/bin/mlx/ignition usr/bin/mlx/ignitionctl etc/systemd/system/ignition.service \
            etc/systemd/system/ignition-switch-root.service etc/systemd/system/gdm.service.d/ignition.conf \
            etc/mlx/init/services/ignition.service usr/lib/initcpio/install/ignition \
            usr/lib/initcpio/hooks/ignition; do
    [[ -e "$stage/$path" ]] || fail "the installer left out $path"
done
[[ $(readlink "$units/sysinit.target.wants/ignition.service") == /etc/systemd/system/ignition.service ]] || fail "ignition.service not enabled"
[[ $(readlink "$units/plymouth-start.service") == /dev/null && $(readlink "$units/plymouth-quit.service") == /dev/null ]] || fail "Plymouth's units not masked"
grep -q "^ExecStartPre=-/usr/bin/mlx/ignitionctl quit --retain-splash$" "$units/gdm.service.d/ignition.conf" || fail "the display manager's drop-in"
grep -qx "ExecCondition=/usr/bin/mlx/ignitionctl absent" "$units/ignition.service" || fail "real-root duplicate guard missing"
grep -qx "IgnoreOnIsolate=true" "$units/ignition.service" || fail "initramfs service would be stopped at switch-root"
grep -qx "disabled = yes" "$stage/etc/mlx/init/services/ignition.service" || fail "impulse would start a duplicate splash"
if grep -Rqs "ConditionKernelCommandLine=!plymouth.enable=0" "$units"/ignition*.service; then
    fail "plymouth.enable=0 incorrectly disables ignition"
fi
bash -n "$stage/usr/lib/initcpio/install/ignition" "$stage/usr/lib/initcpio/hooks/ignition" || fail "mkinitcpio hook syntax"
grep -q -- '--newroot=/sysroot' "$stage/usr/lib/initcpio/hooks/ignition" || fail "classic initramfs root handoff missing"
grep -q -- '--newroot=/sysroot' "$units/ignition-switch-root.service" || fail "systemd initramfs root handoff missing"
if grep -Rqs '@BINDIR@' "$stage/usr/lib/initcpio" "$units"/ignition*.service; then
    fail "an initramfs file still has an unrendered bindir"
fi

# Exercise the mkinitcpio install hook's two branches with its API stubbed:
# classic udev gets a runscript; systemd gets units and target links.
classic_log="$work/initcpio-classic.log"
(
    add_binary() { printf 'binary %s\n' "$*"; }
    add_file() { printf 'file %s\n' "$*"; }
    add_symlink() { printf 'link %s\n' "$*"; }
    add_runscript() { echo runscript; }
    add_udev_rule() { printf 'rule %s\n' "$*"; }
    source "$stage/usr/lib/initcpio/install/ignition"
    build
) > "$classic_log"
grep -qx 'binary /usr/bin/mlx/ignition' "$classic_log" || fail "classic hook omitted ignition"
grep -qx 'binary /usr/bin/mlx/ignitionctl' "$classic_log" || fail "classic hook omitted ignitionctl"
grep -qx 'runscript' "$classic_log" || fail "classic hook omitted its runscript"

systemd_log="$work/initcpio-systemd.log"
(
    add_binary() { printf 'binary %s\n' "$*"; }
    add_file() { printf 'file %s\n' "$*"; }
    add_symlink() { printf 'link %s\n' "$*"; }
    add_runscript() { echo runscript; }
    add_udev_rule() { printf 'rule %s\n' "$*"; }
    add_systemd_unit() { :; }
    source "$stage/usr/lib/initcpio/install/ignition"
    build
) > "$systemd_log"
grep -q '^file /etc/systemd/system/ignition.service /usr/lib/systemd/system/ignition.service$' "$systemd_log" || fail "systemd initramfs omitted the splash unit"
grep -q '^link /usr/lib/systemd/system/initrd-switch-root.target.wants/ignition-switch-root.service ../ignition-switch-root.service$' "$systemd_log" || fail "systemd initramfs omitted root handoff ordering"
if grep -qx runscript "$systemd_log"; then fail "systemd initramfs got the classic runscript"; fi
tools/install_splash.sh --destdir "$stage" --disable > "$work/install.log" 2>&1 || fail "install_splash.sh --disable"
[[ -f "$units/plymouth-quit.service" && ! -L "$units/plymouth-quit.service" && ! -e "$units/plymouth-start.service" ]] || fail "Plymouth's units not put back"
[[ ! -e "$units/sysinit.target.wants/ignition.service" ]] || fail "ignition.service still enabled"
echo "ok   the installer (staged): initramfs variants, duplicate guards, units, Plymouth masked and put back"
