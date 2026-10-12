#!/usr/bin/env bash
# Builds the boot splash (projects/desktop/splash: mlx-splash and
# mlx-splashctl) and installs it in place of Plymouth: the MLX picture
# while the system starts, powers off or restarts. It speaks Plymouth's
# protocol, so systemd tells it what starts as it would tell plymouthd.
# Nothing is switched over unless --enable says so.
#
# Installs (PREFIX is /usr unless --prefix says otherwise):
#   PREFIX/bin/mlx/mlx-splash, mlx-splashctl
#   PREFIX/bin/mlx on PATH: /etc/profile.d/mlx.sh, /etc/environment.d/50-mlx.conf,
#                                     /etc/systemd/system.conf.d/50-mlx-path.conf
#                                     (tools/lib/mlx_layout.sh)
#   /etc/systemd/system/mlx-splash.service (at boot), mlx-splash-quit.service
#                                     (ends it where no display manager does),
#                                     mlx-splash-poweroff.service,
#                                     mlx-splash-reboot.service
#   /etc/systemd/system/UNIT.d/mlx-splash.conf, UNIT the display manager's
#                                     (what display-manager.service names, else
#                                     display-manager.service itself): it ends
#                                     the splash leaving the last frame before
#                                     the display manager starts
#   /etc/mlx/init/services/mlx-splash.service (mlx-init)
#   /var/lib/mlx/splash/              (the last boot's length, for the bar)
#   PREFIX/share/doc/mlx/splash/README.md
#   with --enable: the four units enabled (sysinit.target, multi-user.target,
#   poweroff.target and halt.target, reboot.target and kexec.target), and
#   Plymouth's units masked (the symbolic links systemctl makes); what was at
#   those places before is kept under /var/lib/mlx/splash-installer
#   with --disable: the units disabled and Plymouth's as they were before
#   --enable; nothing is built or installed
#
# A Plymouth in the initramfs is asked by mlx-splash to quit leaving its
# picture, and mlx-splash takes over from it.
#
#   tools/install_splash.sh [--prefix DIR] [--destdir DIR] [--enable | --disable] [compiler]
#
# The programs come from ./mlx build (the splash group, only what is not
# current, built as the checkout's owner); a compiler named compiles them
# with it instead.
#
# --destdir stages everything under DIR (for packaging, or to look before
# installing). Without it this has to run as root. The installer never
# calls systemctl itself; it says what to run.
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
source tools/lib/mlx_layout.sh
source tools/lib/mlx_build.sh
prefix=$mlx_default_prefix
destdir=""
enable=false
disable=false
compiler=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix) prefix=$2; shift 2 ;;
        --destdir) destdir=$2; shift 2 ;;
        --enable) enable=true; shift ;;
        --disable) disable=true; shift ;;
        -h|--help) sed -n '2,/^set -euo/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 0 ;;
        *) compiler=$1; shift ;;
    esac
done
die() {
    echo "install_splash.sh: $*" >&2
    exit 2
}
$enable && $disable && die "--enable and --disable are mutually exclusive"
[[ -z "$destdir" || "$destdir" == /* ]] || die "--destdir must be an absolute path"
[[ "$destdir" == / ]] && destdir=""
destdir=${destdir%/}
if [[ -z "$destdir" && $(id -u) -ne 0 ]]; then
    die "run it as root (or stage it with --destdir DIR)"
fi

system_dir="$destdir/etc/systemd/system"
state_dir="$destdir/var/lib/mlx/splash-installer"
# Plymouth's units, masked by --enable.
replaced=(plymouth-start.service plymouth-read-write.service plymouth-quit.service plymouth-quit-wait.service
          plymouth-switch-root.service plymouth-poweroff.service plymouth-reboot.service plymouth-halt.service
          plymouth-kexec.service)
# Our units and the targets that want them.
wants=("sysinit.target:mlx-splash.service" "multi-user.target:mlx-splash-quit.service"
       "poweroff.target:mlx-splash-poweroff.service" "halt.target:mlx-splash-poweroff.service"
       "reboot.target:mlx-splash-reboot.service" "kexec.target:mlx-splash-reboot.service")

remove_path() {
    local path=$1
    [[ -d "$path" && ! -L "$path" ]] && die "refusing to replace directory $path"
    if [[ -e "$path" || -L "$path" ]]; then rm -f -- "$path"; fi
    return 0
}

# Keeps what is at `path` (nothing, a symbolic link, a file) as data under
# the state directory, to put back with restore_path.
capture_path() {
    local label=$1 path=$2
    rm -f -- "$state_dir/$label.kind" "$state_dir/$label.target" "$state_dir/$label.file"
    if [[ -L "$path" ]]; then
        readlink -- "$path" > "$state_dir/$label.target"
        echo symlink > "$state_dir/$label.kind"
    elif [[ -f "$path" ]]; then
        cp -a -- "$path" "$state_dir/$label.file"
        echo file > "$state_dir/$label.kind"
    elif [[ -e "$path" ]]; then
        die "cannot keep $path (not a file or link)"
    else
        echo absent > "$state_dir/$label.kind"
    fi
}

restore_path() {
    local label=$1 path=$2 kind
    [[ -f "$state_dir/$label.kind" ]] || return 0
    kind=$(< "$state_dir/$label.kind")
    remove_path "$path"
    case "$kind" in
        absent) ;;
        symlink) ln -s -- "$(< "$state_dir/$label.target")" "$path" ;;
        file) cp -a -- "$state_dir/$label.file" "$path" ;;
        *) die "invalid saved state for $path" ;;
    esac
}

disable_splash() {
    local unit pair target
    if [[ ! -f "$state_dir/enabled" ]]; then
        echo "the splash was not enabled by this installer; nothing changed"
        return
    fi
    for pair in "${wants[@]}"; do
        target=${pair%%:*}
        unit=${pair#*:}
        restore_path "wants-$target" "$system_dir/$target.wants/$unit"
    done
    for unit in "${replaced[@]}"; do restore_path "$unit" "$system_dir/$unit"; done
    rm -rf -- "$state_dir"
    echo "disabled the MLX splash; Plymouth's units are as they were before --enable"
    if [[ -z "$destdir" ]]; then echo "run: systemctl daemon-reload"; fi
    echo "to switch back: tools/install_splash.sh --enable"
}

if $disable; then
    disable_splash
    exit 0
fi
compiler=${compiler:-${MLX_COMPILER:-}}

build=$(mktemp -d)
trap 'rm -rf -- "$build"' EXIT
if [[ -n "$compiler" ]]; then
    "$compiler" --quiet projects/desktop/splash/main.mlx -o "$build/mlx-splash"
    "$compiler" --quiet projects/desktop/splash/tool.mlx -o "$build/mlx-splashctl"
else
    mlx_build splash
    cp mlx-out/bin/mlx-splash mlx-out/bin/mlx-splashctl "$build/"
fi
echo "built mlx-splash and mlx-splashctl"

prefix=${prefix%/}
bindir=$(mlx_bindir "$prefix")
docdir=$prefix/share/doc/mlx/splash
# The display manager's own unit, for its drop-in (display-manager.service
# is the alias systemctl enable makes for it).
manager=display-manager.service
if [[ -L "$system_dir/display-manager.service" ]]; then
    manager=$(basename -- "$(readlink -- "$system_dir/display-manager.service")")
fi
install -d "$destdir$bindir" "$system_dir" "$system_dir/$manager.d" "$destdir/etc/mlx/init/services" "$destdir$docdir"
install -d -m 755 "$destdir/var/lib/mlx/splash"
install -m 755 "$build/mlx-splash" "$build/mlx-splashctl" "$destdir$bindir/"
for unit in mlx-splash.service mlx-splash-quit.service mlx-splash-poweroff.service mlx-splash-reboot.service; do
    sed -e "s|@BINDIR@|$bindir|g" -e "s|@DOCDIR@|$docdir|g" "projects/desktop/splash/examples/$unit" > "$build/$unit"
    install -m 644 "$build/$unit" "$system_dir/$unit"
done
sed "s|@BINDIR@|$bindir|g" projects/desktop/splash/examples/mlx-splash-display-manager.conf > "$build/mlx-splash.conf"
install -m 644 "$build/mlx-splash.conf" "$system_dir/$manager.d/mlx-splash.conf"
sed "s|@BINDIR@|$bindir|g" projects/desktop/splash/examples/mlx-splash.init > "$build/mlx-splash.init"
install -m 644 "$build/mlx-splash.init" "$destdir/etc/mlx/init/services/mlx-splash.service"
install -m 644 projects/desktop/splash/README.md "$destdir$docdir/README.md"
echo "installed into ${destdir:-/}: $bindir/mlx-splash, $bindir/mlx-splashctl, the systemd units, the drop-in for $manager, the mlx-init service"
mlx_install_path_files "$destdir" "$bindir"
mlx_note_old_layout "$destdir" "$bindir"

if $enable; then
    # What --disable puts back, kept once (a second --enable keeps the
    # state of before the first).
    if [[ ! -f "$state_dir/enabled" ]]; then
        install -d -m 700 "$state_dir"
        for unit in "${replaced[@]}"; do capture_path "$unit" "$system_dir/$unit"; done
        for pair in "${wants[@]}"; do capture_path "wants-${pair%%:*}" "$system_dir/${pair%%:*}.wants/${pair#*:}"; done
        echo enabled > "$state_dir/enabled"
    fi
    # What systemctl enable and mask leave: links.
    for pair in "${wants[@]}"; do
        install -d "$system_dir/${pair%%:*}.wants"
        ln -sf "/etc/systemd/system/${pair#*:}" "$system_dir/${pair%%:*}.wants/${pair#*:}"
    done
    for unit in "${replaced[@]}"; do
        remove_path "$system_dir/$unit"
        ln -s /dev/null "$system_dir/$unit"
    done
    echo "enabled the MLX splash (boot, poweroff, reboot); masked Plymouth's units"
    if [[ -z "$destdir" ]]; then
        echo "run: systemctl daemon-reload (the splash shows from the next boot; mlx.splash=0 on the kernel's command line keeps it away)"
    fi
else
    echo "to switch over: tools/install_splash.sh --enable (and back: --disable)"
fi
