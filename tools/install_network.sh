#!/usr/bin/env bash
# Builds nexus and mlx-net (projects/network) and MLXIPC's bus
# (synapse, projects/desktop/ipc) and installs them as the system's
# network service, in place of NetworkManager, on MLXIPC's system bus
# (/run/mlx/bus; no D-Bus). Nothing is switched over unless --enable says
# so.
#
# Installs (PREFIX is /usr unless --prefix says otherwise):
#   PREFIX/bin/mlx/nexus, mlx-net, synapse
#   PREFIX/bin/mlx on PATH: /etc/profile.d/mlx.sh, /etc/environment.d/50-mlx.conf,
#                                     /etc/systemd/system.conf.d/50-mlx-path.conf
#                                     (tools/lib/mlx_layout.sh)
#   /etc/mlx/network.conf             (an example, all commented out; an
#                                     existing one stays)
#   /etc/systemd/system/synapse.service (MLXIPC's system bus),
#   /etc/systemd/system/nexus.service (needs it)
#   /etc/mlx/init/services/synapse.service, nexus.service (impulse)
#   /var/lib/mlx/network/             (mode 0700: the saved Wi-Fi networks)
#   PREFIX/share/doc/mlx/network/README.md, PREFIX/share/doc/mlx/ipc/README.md
#   with --enable: synapse.service and nexus.service enabled,
#   NetworkManager.service, systemd-networkd.service and
#   wpa_supplicant.service masked (the symbolic links systemctl makes);
#   what was at those places before is kept under
#   /var/lib/mlx/network-installer
#   with --disable: nexus.service disabled and the three units as they
#   were before --enable (unmasked, when an older install kept nothing);
#   nothing is built or installed, and synapse.service stays enabled
#   (sentinel needs the bus)
#
# Who may change the network is MLXIPC's permission network.control
# (/etc/mlx/ipc.conf): every unsandboxed app by default, no sandboxed one.
#
#   tools/install_network.sh [--prefix DIR] [--destdir DIR] [--enable | --disable] [compiler]
#
# The programs come from ./mlx build (the network group, only what is not
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
    echo "install_network.sh: $*" >&2
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
netd_wants="$system_dir/multi-user.target.wants/nexus.service"
state_dir="$destdir/var/lib/mlx/network-installer"
# The services nexus stands in for, masked by --enable.
replaced=(NetworkManager.service systemd-networkd.service wpa_supplicant.service)

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

# Units of `replaced` enabled for some target (their own enable links,
# which masking leaves alone): what to start again.
enabled_replaced() {
    local unit found=()
    for unit in "${replaced[@]}"; do
        if compgen -G "$system_dir/*.wants/$unit" > /dev/null; then found+=("$unit"); fi
    done
    echo "${found[*]}"
}

disable_network() {
    local unit restored=() start
    if [[ -f "$state_dir/enabled" ]]; then
        for unit in "${replaced[@]}"; do restore_path "$unit" "$system_dir/$unit"; done
        restore_path nexus-wants "$netd_wants"
        rm -rf -- "$state_dir"
        echo "disabled nexus.service; ${replaced[*]} are as they were before --enable"
    else
        # Enabled by an installer that kept nothing: undo its masks, but
        # only while nexus is enabled (a mask on a machine not switched
        # over is its owner's).
        if [[ "$(readlink -- "$netd_wants" 2> /dev/null || true)" == /etc/systemd/system/nexus.service ]]; then
            remove_path "$netd_wants"
            for unit in "${replaced[@]}"; do
                if [[ "$(readlink -- "$system_dir/$unit" 2> /dev/null || true)" == /dev/null ]]; then
                    remove_path "$system_dir/$unit"
                    restored+=("$unit")
                fi
            done
            echo "disabled nexus.service; no saved state, unmasked: ${restored[*]:-nothing}"
        else
            echo "nexus.service was not enabled; nothing changed"
        fi
    fi
    echo "synapse.service stays enabled (MLXIPC's system bus, which sentinel needs)"
    start=$(enabled_replaced)
    if [[ -z "$destdir" ]]; then
        if [[ -n "$start" ]]; then
            echo "run: systemctl daemon-reload && systemctl stop nexus.service && systemctl start $start"
        else
            echo "run: systemctl daemon-reload && systemctl stop nexus.service (and start the network service you use: none of ${replaced[*]} is enabled)"
        fi
    else
        echo "to start after daemon-reload: ${start:-none of ${replaced[*]} is enabled}"
    fi
    echo "to switch back: tools/install_network.sh --enable"
}

if $disable; then
    disable_network
    exit 0
fi
compiler=${compiler:-${MLX_COMPILER:-}}

build=$(mktemp -d)
trap 'rm -rf -- "$build"' EXIT
if [[ -n "$compiler" ]]; then
    "$compiler" --quiet projects/network/main.mlx -o "$build/nexus"
    "$compiler" --quiet projects/network/tool.mlx -o "$build/mlx-net"
    "$compiler" --quiet projects/desktop/ipc/main.mlx -o "$build/synapse"
else
    mlx_build network
    cp mlx-out/bin/nexus mlx-out/bin/mlx-net mlx-out/bin/synapse "$build/"
fi
echo "built nexus, mlx-net and synapse"

prefix=${prefix%/}
bindir=$(mlx_bindir "$prefix")
install -d "$destdir$bindir" "$destdir/etc/mlx" "$destdir/etc/systemd/system" "$destdir/etc/mlx/init/services" "$destdir$prefix/share/doc/mlx/network" "$destdir$prefix/share/doc/mlx/ipc"
install -d -m 700 "$destdir/var/lib/mlx/network"
install -m 755 "$build/nexus" "$build/mlx-net" "$build/synapse" "$destdir$bindir/"
if [[ ! -e "$destdir/etc/mlx/network.conf" ]]; then
    install -m 644 projects/network/examples/network.conf "$destdir/etc/mlx/network.conf"
fi
sed -e "s|@BINDIR@|$bindir|g" -e "s|@DOCDIR@|$prefix/share/doc/mlx/network|g" projects/network/examples/nexus.service > "$build/nexus.service"
sed "s|@BINDIR@|$bindir|g" projects/network/examples/nexus.init > "$build/nexus.init"
sed -e "s|@BINDIR@|$bindir|g" -e "s|@DOCDIR@|$prefix/share/doc/mlx/ipc|g" projects/desktop/ipc/examples/synapse.service > "$build/synapse.service"
sed "s|@BINDIR@|$bindir|g" projects/desktop/ipc/examples/synapse.init > "$build/synapse.init"
install -m 644 "$build/nexus.service" "$destdir/etc/systemd/system/nexus.service"
install -m 644 "$build/synapse.service" "$destdir/etc/systemd/system/synapse.service"
install -m 644 "$build/nexus.init" "$destdir/etc/mlx/init/services/nexus.service"
install -m 644 "$build/synapse.init" "$destdir/etc/mlx/init/services/synapse.service"
install -m 644 projects/network/README.md "$destdir$prefix/share/doc/mlx/network/README.md"
install -m 644 projects/desktop/ipc/README.md "$destdir$prefix/share/doc/mlx/ipc/README.md"
echo "installed into ${destdir:-/}: $bindir/nexus, $bindir/mlx-net, $bindir/synapse, /etc/mlx/network.conf, the systemd units and impulse services of synapse and nexus"
mlx_install_path_files "$destdir" "$bindir"
mlx_note_old_layout "$destdir" "$bindir"

if $enable; then
    # What --disable puts back, kept once (a second --enable keeps the
    # state of before the first).
    if [[ ! -f "$state_dir/enabled" ]]; then
        install -d -m 700 "$state_dir"
        for unit in "${replaced[@]}"; do capture_path "$unit" "$system_dir/$unit"; done
        capture_path nexus-wants "$netd_wants"
        echo enabled > "$state_dir/enabled"
    fi
    # What systemctl enable and mask leave: links.
    install -d "$destdir/etc/systemd/system/multi-user.target.wants"
    ln -sf /etc/systemd/system/synapse.service "$destdir/etc/systemd/system/multi-user.target.wants/synapse.service"
    ln -sf /etc/systemd/system/nexus.service "$destdir/etc/systemd/system/multi-user.target.wants/nexus.service"
    # An alias the D-Bus nexus of before left.
    if [[ "$(readlink "$destdir/etc/systemd/system/dbus-org.freedesktop.NetworkManager.service" 2> /dev/null)" == /etc/systemd/system/nexus.service ]]; then
        rm -f "$destdir/etc/systemd/system/dbus-org.freedesktop.NetworkManager.service"
    fi
    for unit in "${replaced[@]}"; do
        remove_path "$system_dir/$unit"
        ln -s /dev/null "$system_dir/$unit"
    done
    echo "enabled synapse.service and nexus.service; masked NetworkManager.service, systemd-networkd.service, wpa_supplicant.service"
    [[ -z "$destdir" ]] && echo "run: systemctl daemon-reload && systemctl start nexus (it starts synapse and stops NetworkManager)"
else
    echo "to switch over: tools/install_network.sh --enable (and back: --disable)"
fi
if [[ -z "$destdir" && -e /etc/dbus-1/system.d/org.mlx.Network.conf ]]; then
    rm -f /etc/dbus-1/system.d/org.mlx.Network.conf
    echo "removed the D-Bus policy an earlier install left (nexus is on MLXIPC now)"
fi
