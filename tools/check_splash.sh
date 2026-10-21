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

"$compiler" --quiet projects/desktop/splash/main.mlx -o "$work/mlx-splash"
"$compiler" --quiet projects/desktop/splash/tool.mlx -o "$work/mlx-splashctl"

# Pictures: one frame, a few in a row, the timing.
"$work/mlx-splash" --size 1280x720 --png "$work/frame.png" --at 3 --status "Starting network" >/dev/null || fail "--png"
[[ $(stat -c %s "$work/frame.png") -gt 100000 ]] || fail "the PNG frame is (nearly) empty"
mkdir "$work/frames"
"$work/mlx-splash" --size 640x360 --frames "$work/frames" --count 3 --mode shutdown || fail "--frames"
[[ -f "$work/frames/frame-0002.png" ]] || fail "--frames wrote no third frame"
"$work/mlx-splash" --size 1280x720 --bench 5 | grep -q "^set up: [0-9]* ms, frame: [0-9]* us, [0-9]* threads$" || fail "--bench"
echo "ok   pictures: --png, --frames, --bench"

# Initramfs can start before a DRM node exists. Hide the host's cards in
# this private mount namespace, create card0 only after the daemon is up,
# and verify that a later probe opens the newly appeared node.
if [[ -d /dev/dri ]] && command -v strace >/dev/null 2>&1; then
    mount -t tmpfs -o mode=755 tmpfs /dev/dri
    mounted_dri=true
    probe_socket="/mlx-splash-reprobe-$$"
    strace -f -qq -e trace=openat -o "$work/reprobe.strace" \
        "$work/mlx-splash" --no-tty --socket "$probe_socket" \
        --framebuffer "$work/no-framebuffer" --size 320x180 \
        >"$work/reprobe.log" 2>&1 &
    probe_pid=$!
    for _ in {1..100}; do
        "$work/mlx-splashctl" --socket "$probe_socket" ping >/dev/null 2>&1 && break
        kill -0 "$probe_pid" 2>/dev/null || { cat "$work/reprobe.log" >&2; fail "splash ended before delayed KMS probe"; }
        sleep 0.02
    done
    "$work/mlx-splashctl" --socket "$probe_socket" ping >/dev/null 2>&1 || fail "splash did not listen without an initial KMS card"
    : > /dev/dri/card0
    sleep 1.3
    "$work/mlx-splashctl" --socket "$probe_socket" quit >/dev/null
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
            etc/systemd/system/mlx-splash-switch-root.service etc/systemd/system/gdm.service.d/mlx-splash.conf \
            etc/mlx/init/services/mlx-splash.service usr/lib/initcpio/install/mlx-splash \
            usr/lib/initcpio/hooks/mlx-splash; do
    [[ -e "$stage/$path" ]] || fail "the installer left out $path"
done
[[ $(readlink "$units/sysinit.target.wants/mlx-splash.service") == /etc/systemd/system/mlx-splash.service ]] || fail "mlx-splash.service not enabled"
[[ $(readlink "$units/plymouth-start.service") == /dev/null && $(readlink "$units/plymouth-quit.service") == /dev/null ]] || fail "Plymouth's units not masked"
grep -q "^ExecStartPre=-/usr/bin/mlx/mlx-splashctl quit --retain-splash$" "$units/gdm.service.d/mlx-splash.conf" || fail "the display manager's drop-in"
grep -qx "ExecCondition=/usr/bin/mlx/mlx-splashctl absent" "$units/mlx-splash.service" || fail "real-root duplicate guard missing"
grep -qx "IgnoreOnIsolate=true" "$units/mlx-splash.service" || fail "initramfs service would be stopped at switch-root"
grep -qx "disabled = yes" "$stage/etc/mlx/init/services/mlx-splash.service" || fail "mlx-init would start a duplicate splash"
if grep -Rqs "ConditionKernelCommandLine=!plymouth.enable=0" "$units"/mlx-splash*.service; then
    fail "plymouth.enable=0 incorrectly disables mlx-splash"
fi
bash -n "$stage/usr/lib/initcpio/install/mlx-splash" "$stage/usr/lib/initcpio/hooks/mlx-splash" || fail "mkinitcpio hook syntax"
grep -q -- '--newroot=/sysroot' "$stage/usr/lib/initcpio/hooks/mlx-splash" || fail "classic initramfs root handoff missing"
grep -q -- '--newroot=/sysroot' "$units/mlx-splash-switch-root.service" || fail "systemd initramfs root handoff missing"
if grep -Rqs '@BINDIR@' "$stage/usr/lib/initcpio" "$units"/mlx-splash*.service; then
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
    source "$stage/usr/lib/initcpio/install/mlx-splash"
    build
) > "$classic_log"
grep -qx 'binary /usr/bin/mlx/mlx-splash' "$classic_log" || fail "classic hook omitted mlx-splash"
grep -qx 'binary /usr/bin/mlx/mlx-splashctl' "$classic_log" || fail "classic hook omitted mlx-splashctl"
grep -qx 'runscript' "$classic_log" || fail "classic hook omitted its runscript"

systemd_log="$work/initcpio-systemd.log"
(
    add_binary() { printf 'binary %s\n' "$*"; }
    add_file() { printf 'file %s\n' "$*"; }
    add_symlink() { printf 'link %s\n' "$*"; }
    add_runscript() { echo runscript; }
    add_udev_rule() { printf 'rule %s\n' "$*"; }
    add_systemd_unit() { :; }
    source "$stage/usr/lib/initcpio/install/mlx-splash"
    build
) > "$systemd_log"
grep -q '^file /etc/systemd/system/mlx-splash.service /usr/lib/systemd/system/mlx-splash.service$' "$systemd_log" || fail "systemd initramfs omitted the splash unit"
grep -q '^link /usr/lib/systemd/system/initrd-switch-root.target.wants/mlx-splash-switch-root.service ../mlx-splash-switch-root.service$' "$systemd_log" || fail "systemd initramfs omitted root handoff ordering"
if grep -qx runscript "$systemd_log"; then fail "systemd initramfs got the classic runscript"; fi
tools/install_splash.sh --destdir "$stage" --disable > "$work/install.log" 2>&1 || fail "install_splash.sh --disable"
[[ -f "$units/plymouth-quit.service" && ! -L "$units/plymouth-quit.service" && ! -e "$units/plymouth-start.service" ]] || fail "Plymouth's units not put back"
[[ ! -e "$units/sysinit.target.wants/mlx-splash.service" ]] || fail "mlx-splash.service still enabled"
echo "ok   the installer (staged): initramfs variants, duplicate guards, units, Plymouth masked and put back"
