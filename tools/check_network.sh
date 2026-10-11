#!/usr/bin/env bash
# End-to-end check of projects/network (mlx-netd, mlx-net), in network
# namespaces of its own (unshare -rn: the machine's network is not
# touched). Two links come from a second namespace that plays the
# network: eth0 with a DHCP server (tools/network-test/dhcpd.py, a 12 s
# lease) behind it, eth1 configured statically. mlx-netd runs with
# MLXIPC (mlx-ipcd) as its system bus. The check:
#
#   - eth0 gets 10.7.0.50/24 by DHCP with its default route (metric 100),
#     eth1 10.8.0.5/24 with its own (metric 50, set in the
#     configuration); resolv.conf has eth0's search domain, then eth1's
#     name server, then eth0's;
#   - the lease is renewed at T1;
#   - eth0 losing its carrier is unconfigured (eth1's route stays), and
#     configured again when the carrier comes back;
#   - mlx-net lists both links over the bus, switches eth0 off (the lease
#     is released, the link put down) and on again;
#   - org.freedesktop.NetworkManager answers state() with 70 (connected)
#     when gdbus is there to ask;
#   - without a Wi-Fi card the Wi-Fi calls answer so (tools/check_wifi.sh
#     checks the Wi-Fi part itself);
#   - a server that refuses the renewal (NAK) sends it back to DISCOVER;
#   - SIGHUP reads the configuration again: eth1 set to off goes down.
#
#   tools/check_network.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
for tool in unshare nsenter ip python3; do
    command -v "$tool" > /dev/null || { echo "check_network.sh: $tool is not installed" >&2; exit 2; }
done
unshare -rn true 2> /dev/null || { echo "check_network.sh: no network namespaces here (unshare -rn)" >&2; exit 2; }

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
"$compiler" --quiet projects/network/main.mlx -o "$work/mlx-netd"
"$compiler" --quiet projects/network/tool.mlx -o "$work/mlx-net"
"$compiler" --quiet projects/desktop/ipc/main.mlx -o "$work/mlx-ipcd"

fail() { echo "FAIL $1" >&2; [[ -f "${2:-}" ]] && cat "$2" >&2; exit 1; }

cat > "$work/inner.sh" <<'SCRIPT'
W=$1
DHCPD=$2
cd "$W"
snapshot() {
    ip -4 -o addr show > "$W/$1-addresses.txt"
    ip -4 route > "$W/$1-routes.txt"
    ip -o link show > "$W/$1-links.txt"
    "$W/mlx-net" --bus "$W/bus" > "$W/$1-status.txt" 2>&1 || true
    cp "$W/resolv.conf" "$W/$1-resolv.txt" 2> /dev/null || true
}
ip link set lo up
ip link add eth0 type veth peer name peer0
ip link add eth1 type veth peer name peer1
unshare -n sleep 120 &
NETWORK=$!
sleep 0.3
ip link set peer0 netns "$NETWORK"
ip link set peer1 netns "$NETWORK"
inside() { nsenter -t "$NETWORK" -n "$@"; }
inside ip link set lo up
inside ip addr add 10.7.0.1/24 dev peer0
inside ip addr add 10.8.0.1/24 dev peer1
inside ip link set peer0 up
inside ip link set peer1 up
inside python3 "$DHCPD" peer0 --lease 12 > "$W/dhcpd.log" 2>&1 &
SERVER=$!
cat > "$W/network.conf" <<CONF
# The check's links.
hostname = mlx-test
eth1.method = static
eth1.address = 10.8.0.5/24
eth1.gateway = 10.8.0.1
eth1.dns = 10.8.0.53
eth1.metric = 50
CONF
"$W/mlx-ipcd" --address "$W/bus" > "$W/ipcd.log" 2>&1 &
IPCD=$!
for _ in $(seq 1 50); do [ -S "$W/bus" ] && break; sleep 0.1; done
sleep 0.3
"$W/mlx-netd" --config "$W/network.conf" --resolv "$W/resolv.conf" --bus "$W/bus" --verbose > "$W/netd.log" 2>&1 &
NETD=$!
sleep 4
snapshot bound
if command -v gdbus > /dev/null; then
    gdbus call --address "unix:path=$W/bus" --dest org.freedesktop.NetworkManager --object-path /org/freedesktop/NetworkManager --method org.freedesktop.NetworkManager.state > "$W/nm-state.txt" 2>&1 || true
fi
sleep 9
inside ip link set peer0 down
sleep 1
snapshot unplugged
inside ip link set peer0 up
sleep 4
snapshot plugged
"$W/mlx-net" --bus "$W/bus" down eth0 > "$W/down.txt" 2>&1 || true
sleep 1
snapshot off
"$W/mlx-net" --bus "$W/bus" up eth0 > "$W/up.txt" 2>&1 || true
sleep 4
snapshot on
"$W/mlx-net" --bus "$W/bus" down eth9 > "$W/unknown.txt" 2>&1 && echo "accepted" >> "$W/unknown.txt" || true
"$W/mlx-net" --bus "$W/bus" wifi > "$W/wifi.txt" 2>&1 || echo "failed" >> "$W/wifi.txt"
"$W/mlx-net" --bus "$W/bus" scan > "$W/scan.txt" 2>&1 && echo "accepted" >> "$W/scan.txt" || true
"$W/mlx-net" --bus "$W/bus" connect Nowhere "a passphrase" > "$W/connect.txt" 2>&1 && echo "accepted" >> "$W/connect.txt" || true
# A server that refuses the renewal.
kill "$SERVER"
sleep 0.3
inside python3 "$DHCPD" peer0 --lease 12 --refuse-renew > "$W/dhcpd-refusing.log" 2>&1 &
SERVER=$!
sleep 9
snapshot refused
# eth1 off in the configuration, read again on SIGHUP.
sed -i 's/^eth1.method = static$/eth1.method = off/' "$W/network.conf"
kill -HUP "$NETD"
sleep 1
snapshot reloaded
kill "$NETD"
sleep 0.5
kill "$SERVER" "$IPCD" "$NETWORK" 2> /dev/null
wait 2> /dev/null
SCRIPT
timeout 120 unshare -rn bash "$work/inner.sh" "$work" "$repo_root/tools/network-test/dhcpd.py" > "$work/inner.log" 2>&1 || fail "the namespaces' script failed" "$work/inner.log"
log="$work/netd.log"

# Bound: both links, both routes, resolv.conf in metric order.
grep -q "eth0 *inet 10.7.0.50/24" "$work/bound-addresses.txt" || fail "eth0 did not get its DHCP address" "$log"
grep -q "eth1 *inet 10.8.0.5/24" "$work/bound-addresses.txt" || fail "eth1 did not get its static address" "$log"
grep -q "^default via 10.7.0.1 dev eth0 proto dhcp metric 100" "$work/bound-routes.txt" || fail "eth0 has no default route" "$work/bound-routes.txt"
grep -q "^default via 10.8.0.1 dev eth1 proto static metric 50" "$work/bound-routes.txt" || fail "eth1 has no default route" "$work/bound-routes.txt"
[[ "$(grep -v '^#' "$work/bound-resolv.txt" | tr '\n' ' ')" == "search test.lan nameserver 10.8.0.53 nameserver 10.7.0.53 nameserver 10.7.0.54 " ]] || fail "resolv.conf is not in the links' metric order" "$work/bound-resolv.txt"
grep -q "^eth0 *ethernet *connected *10.7.0.50/24 *via 10.7.0.1 *dns 10.7.0.53 10.7.0.54$" "$work/bound-status.txt" || fail "mlx-net does not list eth0" "$work/bound-status.txt"
grep -q "^eth1 *ethernet *connected *10.8.0.5/24 *via 10.8.0.1 *dns 10.8.0.53$" "$work/bound-status.txt" || fail "mlx-net does not list eth1" "$work/bound-status.txt"
grep -q "^netd: on the system bus (also as org.freedesktop.NetworkManager)$" "$log" || fail "mlx-netd is not on the bus" "$log"
grep -q "^netd: \(nl80211 is there (Wi-Fi)\|no nl80211 in this kernel (no Wi-Fi)\)$" "$log" || fail "mlx-netd did not say whether there is Wi-Fi" "$log"
if [[ -f "$work/nm-state.txt" ]]; then
    grep -q "(uint32 70,)" "$work/nm-state.txt" || fail "NetworkManager's state() is not 70 (connected)" "$work/nm-state.txt"
fi
echo "ok   DHCP on eth0 and the static eth1: addresses, routes by metric, resolv.conf, mlx-net and NetworkManager's state over the bus"

grep -q "^renew$" "$work/dhcpd.log" && grep -q "^netd: eth0: dhcp renewed 10.7.0.50$" "$log" || fail "the lease was not renewed" "$work/dhcpd.log"
echo "ok   the lease is renewed at T1"

! grep -q "10.7.0.50" "$work/unplugged-addresses.txt" || fail "eth0 kept its address without a carrier" "$work/unplugged-addresses.txt"
! grep -q "dev eth0" "$work/unplugged-routes.txt" && grep -q "^default via 10.8.0.1 dev eth1" "$work/unplugged-routes.txt" || fail "the routes are wrong without eth0's carrier" "$work/unplugged-routes.txt"
grep -q "^eth0 *ethernet *no-carrier" "$work/unplugged-status.txt" || fail "mlx-net does not say eth0 has no carrier" "$work/unplugged-status.txt"
grep -q "eth0 *inet 10.7.0.50/24" "$work/plugged-addresses.txt" || fail "eth0 was not configured again with its carrier back" "$log"
echo "ok   a carrier lost unconfigures eth0, its return configures it again"

grep -q "^release$" "$work/dhcpd.log" || fail "switching eth0 off did not release its lease" "$work/dhcpd.log"
grep -q "^eth0 *ethernet *off" "$work/off-status.txt" && grep -q "eth0@.*state DOWN" "$work/off-links.txt" || fail "eth0 switched off is not off and down" "$work/off-status.txt"
grep -q "eth0 *inet 10.7.0.50/24" "$work/on-addresses.txt" || fail "eth0 switched on again got no address" "$log"
grep -q "no such managed link" "$work/unknown.txt" && ! grep -q "accepted" "$work/unknown.txt" || fail "mlx-net down of an unknown link was not refused" "$work/unknown.txt"
echo "ok   mlx-net switches eth0 off (the lease released, the link down) and on again"

[[ ! -s "$work/wifi.txt" ]] || fail "mlx-net wifi lists networks without a Wi-Fi card" "$work/wifi.txt"
grep -q "no Wi-Fi card" "$work/scan.txt" && ! grep -q "accepted" "$work/scan.txt" || fail "a scan without a Wi-Fi card was not refused" "$work/scan.txt"
grep -q "no such network in sight" "$work/connect.txt" && ! grep -q "accepted" "$work/connect.txt" || fail "joining a network nobody sees was not refused" "$work/connect.txt"
echo "ok   without a Wi-Fi card: no networks, Scan and Connect refused with a reason"

grep -q "^nak$" "$work/dhcpd-refusing.log" && grep -q "^netd: eth0: dhcp refused by 10.7.0.1$" "$log" || fail "a refused renewal was not taken" "$work/dhcpd-refusing.log"
grep -q "^discover$" "$work/dhcpd-refusing.log" && grep -q "eth0 *inet 10.7.0.50/24" "$work/refused-addresses.txt" || fail "after the NAK eth0 did not ask again and get its address" "$log"
echo "ok   a refused renewal (NAK) starts over with DISCOVER"

! grep -q "10.8.0.5" "$work/reloaded-addresses.txt" && grep -q "^eth1 *ethernet *off" "$work/reloaded-status.txt" || fail "eth1 set to off was not taken down on SIGHUP" "$work/reloaded-status.txt"
grep -q "^netd: ending$" "$log" || fail "mlx-netd did not end on SIGTERM" "$log"
echo "ok   SIGHUP reads the configuration again; SIGTERM ends mlx-netd"
