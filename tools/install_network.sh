#!/usr/bin/env bash
# Builds mlx-netd and mlx-net (projects/network) and MLXIPC's bus
# (mlx-ipcd, projects/desktop/ipc) and installs them as the system's
# network service, in place of NetworkManager, on MLXIPC's system bus
# (/run/mlx/bus; no D-Bus). Nothing is switched over unless --enable says
# so.
#
# Installs:
#   PREFIX/bin/mlx-netd, PREFIX/bin/mlx-net, PREFIX/bin/mlx-ipcd
#   /etc/mlx/network.conf             (an example, all commented out; an
#                                     existing one stays)
#   /etc/systemd/system/mlx-ipcd.service (MLXIPC's system bus),
#   /etc/systemd/system/mlx-netd.service (needs it)
#   /etc/mlx/init/services/mlx-ipcd.service, mlx-netd.service (mlx-init)
#   /var/lib/mlx/network/             (mode 0700: the saved Wi-Fi networks)
#   PREFIX/share/doc/mlx/network/README.md
#   with --enable: mlx-ipcd.service and mlx-netd.service enabled,
#   NetworkManager.service, systemd-networkd.service and
#   wpa_supplicant.service masked (the symbolic links systemctl makes)
#
# Who may change the network is MLXIPC's permission network.control
# (/etc/mlx/ipc.conf): every unsandboxed app by default, no sandboxed one.
#
#   tools/install_network.sh [--prefix DIR] [--destdir DIR] [--enable] [compiler]
#
# --destdir stages everything under DIR (for packaging, or to look before
# installing). Without it this has to run as root.
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
prefix=/usr/local
destdir=""
enable=false
compiler=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix) prefix=$2; shift 2 ;;
        --destdir) destdir=$2; shift 2 ;;
        --enable) enable=true; shift ;;
        -h|--help) sed -n '2,30p' "$0"; exit 0 ;;
        *) compiler=$1; shift ;;
    esac
done
compiler=${compiler:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
if [[ -z "$destdir" && $(id -u) -ne 0 ]]; then
    echo "install_network.sh: run it as root (or stage it with --destdir DIR)" >&2
    exit 2
fi

build=$(mktemp -d)
trap 'rm -rf -- "$build"' EXIT
"$compiler" --quiet projects/network/main.mlx -o "$build/mlx-netd"
"$compiler" --quiet projects/network/tool.mlx -o "$build/mlx-net"
"$compiler" --quiet projects/desktop/ipc/main.mlx -o "$build/mlx-ipcd"
echo "built mlx-netd, mlx-net and mlx-ipcd"

bindir="$prefix/bin"
install -d "$destdir$bindir" "$destdir/etc/mlx" "$destdir/etc/systemd/system" "$destdir/etc/mlx/init/services" "$destdir$prefix/share/doc/mlx/network"
install -d -m 700 "$destdir/var/lib/mlx/network"
install -m 755 "$build/mlx-netd" "$build/mlx-net" "$build/mlx-ipcd" "$destdir$bindir/"
if [[ ! -e "$destdir/etc/mlx/network.conf" ]]; then
    install -m 644 projects/network/examples/network.conf "$destdir/etc/mlx/network.conf"
fi
sed "s|@BINDIR@|$bindir|g" projects/network/examples/mlx-netd.service > "$build/mlx-netd.service"
sed "s|@BINDIR@|$bindir|g" projects/network/examples/mlx-netd.init > "$build/mlx-netd.init"
sed "s|@BINDIR@|$bindir|g" projects/desktop/ipc/examples/mlx-ipcd.service > "$build/mlx-ipcd.service"
sed "s|@BINDIR@|$bindir|g" projects/desktop/ipc/examples/mlx-ipcd.init > "$build/mlx-ipcd.init"
install -m 644 "$build/mlx-netd.service" "$destdir/etc/systemd/system/mlx-netd.service"
install -m 644 "$build/mlx-ipcd.service" "$destdir/etc/systemd/system/mlx-ipcd.service"
install -m 644 "$build/mlx-netd.init" "$destdir/etc/mlx/init/services/mlx-netd.service"
install -m 644 "$build/mlx-ipcd.init" "$destdir/etc/mlx/init/services/mlx-ipcd.service"
install -m 644 projects/network/README.md "$destdir$prefix/share/doc/mlx/network/README.md"
echo "installed into ${destdir:-/}: $bindir/mlx-netd, $bindir/mlx-net, $bindir/mlx-ipcd, /etc/mlx/network.conf, the systemd units and mlx-init services of mlx-ipcd and mlx-netd"

if $enable; then
    # What systemctl enable and mask leave: links.
    install -d "$destdir/etc/systemd/system/multi-user.target.wants"
    ln -sf /etc/systemd/system/mlx-ipcd.service "$destdir/etc/systemd/system/multi-user.target.wants/mlx-ipcd.service"
    ln -sf /etc/systemd/system/mlx-netd.service "$destdir/etc/systemd/system/multi-user.target.wants/mlx-netd.service"
    # An alias the D-Bus mlx-netd of before left.
    if [[ "$(readlink "$destdir/etc/systemd/system/dbus-org.freedesktop.NetworkManager.service" 2> /dev/null)" == /etc/systemd/system/mlx-netd.service ]]; then
        rm -f "$destdir/etc/systemd/system/dbus-org.freedesktop.NetworkManager.service"
    fi
    for unit in NetworkManager.service systemd-networkd.service wpa_supplicant.service; do
        ln -sf /dev/null "$destdir/etc/systemd/system/$unit"
    done
    echo "enabled mlx-ipcd.service and mlx-netd.service; masked NetworkManager.service, systemd-networkd.service, wpa_supplicant.service"
    [[ -z "$destdir" ]] && echo "run: systemctl daemon-reload && systemctl start mlx-netd (it starts mlx-ipcd and stops NetworkManager)"
else
    echo "to switch over: tools/install_network.sh --enable"
fi
if [[ -z "$destdir" && -e /etc/dbus-1/system.d/org.mlx.Network.conf ]]; then
    rm -f /etc/dbus-1/system.d/org.mlx.Network.conf
    echo "removed the D-Bus policy an earlier install left (mlx-netd is on MLXIPC now)"
fi
