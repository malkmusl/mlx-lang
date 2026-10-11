#!/usr/bin/env bash
# Builds mlx-netd and mlx-net (projects/network) and installs them as the
# system's network service, in place of NetworkManager. Nothing is
# switched over unless --enable says so.
#
# Installs:
#   PREFIX/bin/mlx-netd, PREFIX/bin/mlx-net
#   /etc/mlx/network.conf             (an example, all commented out; an
#                                     existing one stays)
#   /etc/systemd/system/mlx-netd.service
#   /etc/mlx/init/services/mlx-netd.service (for mlx-init)
#   /etc/dbus-1/system.d/org.mlx.Network.conf (D-Bus's policy)
#   /var/lib/mlx/network/             (mode 0700: the saved Wi-Fi networks)
#   PREFIX/share/doc/mlx/network/README.md
#   with --enable: mlx-netd.service enabled, NetworkManager.service,
#   systemd-networkd.service and wpa_supplicant.service masked (the
#   symbolic links systemctl makes)
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
        -h|--help) sed -n '2,24p' "$0"; exit 0 ;;
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
echo "built mlx-netd and mlx-net"

bindir="$prefix/bin"
install -d "$destdir$bindir" "$destdir/etc/mlx" "$destdir/etc/systemd/system" "$destdir/etc/mlx/init/services" "$destdir/etc/dbus-1/system.d" "$destdir$prefix/share/doc/mlx/network"
install -d -m 700 "$destdir/var/lib/mlx/network"
install -m 755 "$build/mlx-netd" "$build/mlx-net" "$destdir$bindir/"
if [[ ! -e "$destdir/etc/mlx/network.conf" ]]; then
    install -m 644 projects/network/examples/network.conf "$destdir/etc/mlx/network.conf"
fi
sed "s|@BINDIR@|$bindir|g" projects/network/examples/mlx-netd.service > "$build/mlx-netd.service"
sed "s|@BINDIR@|$bindir|g" projects/network/examples/mlx-netd.init > "$build/mlx-netd.init"
install -m 644 "$build/mlx-netd.service" "$destdir/etc/systemd/system/mlx-netd.service"
install -m 644 "$build/mlx-netd.init" "$destdir/etc/mlx/init/services/mlx-netd.service"
install -m 644 projects/network/examples/org.mlx.Network.conf "$destdir/etc/dbus-1/system.d/org.mlx.Network.conf"
install -m 644 projects/network/README.md "$destdir$prefix/share/doc/mlx/network/README.md"
echo "installed into ${destdir:-/}: $bindir/mlx-netd, $bindir/mlx-net, /etc/mlx/network.conf, the systemd unit, the mlx-init service, the D-Bus policy"

if $enable; then
    # What systemctl enable and mask leave: links.
    install -d "$destdir/etc/systemd/system/multi-user.target.wants"
    ln -sf /etc/systemd/system/mlx-netd.service "$destdir/etc/systemd/system/multi-user.target.wants/mlx-netd.service"
    ln -sf /etc/systemd/system/mlx-netd.service "$destdir/etc/systemd/system/dbus-org.freedesktop.NetworkManager.service"
    for unit in NetworkManager.service systemd-networkd.service wpa_supplicant.service; do
        ln -sf /dev/null "$destdir/etc/systemd/system/$unit"
    done
    echo "enabled mlx-netd.service; masked NetworkManager.service, systemd-networkd.service, wpa_supplicant.service"
    [[ -z "$destdir" ]] && echo "run: systemctl daemon-reload && systemctl stop NetworkManager && systemctl start mlx-netd"
else
    echo "to switch over: tools/install_network.sh --enable (or systemctl enable --now mlx-netd after stopping NetworkManager)"
fi
