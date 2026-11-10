#!/usr/bin/env bash
# brixOS on the network: QEMU's user network (-netdev user: the guest is
# 10.0.2.15, the host 10.0.2.2, a name server at 10.0.2.3) on a virtio
# network device; the kernel's stack (ARP, IPv4, ICMP, UDP, TCP), Linux's
# socket calls for std.net, and wget (std.http.client) fetching files
# from an HTTP server on the host: a small one, a large one (many
# segments, the window, acknowledgements), a missing one (404); ping to
# the gateway; brixOS's own socket calls (netcalls: a stream connection
# to ourselves through the stack, a datagram, an echo); the link's state
# as a program reads it from the kernel (/devices/net0's record): by
# DHCP, then booted again without a DHCP server (no address yet), on
# another network (what its DHCP server answers, none of it what the
# stack starts with) and with a fixed address in /system/settings/network
# on a card with a hardware address of its own.
#
#   tools/check_brix_net.sh [compiler]
set -euo pipefail
cd "$(dirname "$0")/.."
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
if ! command -v qemu-system-x86_64 > /dev/null; then
    echo "skip: no qemu-system-x86_64"
    exit 0
fi
if ! command -v python3 > /dev/null; then
    echo "skip: no python3 for the host's HTTP server"
    exit 0
fi
work=$(mktemp -d)
qemu_pid=
server_pid=
cleanup() {
    exec 3>&- 2> /dev/null || true
    [[ -n "$qemu_pid" ]] && kill "$qemu_pid" 2> /dev/null || true
    [[ -n "$server_pid" ]] && kill "$server_pid" 2> /dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
fail() { echo "FAIL $1" >&2; [[ -f "$work/serial.txt" ]] && cat -v "$work/serial.txt" | tail -n 30 >&2; exit 1; }

brix=--target=x86_64-brixos
mkdir -p "$work/bin" "$work/www"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet $brix projects/wget/main.mlx -o "$work/wget"
"$compiler" --quiet $brix projects/ping/main.mlx -o "$work/ping"
"$compiler" --quiet $brix projects/brix/tools/netcalls/main.mlx -o "$work/netcalls"
for program in cat wc echo sha256sum; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "wget=$work/wget" "ping=$work/ping" "netcalls=$work/netcalls" "$work/bin/"
# The same with a fixed address in the kernel's settings file (no line of
# it what the stack starts with: the record has each from the file).
printf 'address 10.0.2.77\nnetmask 255.255.254.0\ngateway 10.0.3.1\nnameserver 10.0.3.53\n' > "$work/network"
"$work/brix-image" "$work/kernel" "$work/static.img" "flux=$work/flux" "netcalls=$work/netcalls" "settings/network=$work/network" "$work/bin/"
echo "ok   the kernel, wget, ping, netcalls and a boot image build"

# The host's HTTP server on a port of its own.
echo "hello from the host" > "$work/www/hello.txt"
head -c 300000 /dev/urandom | base64 > "$work/www/large.txt"
sum=$(sha256sum "$work/www/large.txt" | cut -d' ' -f1)
port=$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()')
(cd "$work/www" && python3 -m http.server --bind 127.0.0.1 "$port" > "$work/server.txt" 2>&1) &
server_pid=$!
for i in $(seq 1 50); do
    python3 -c "import socket,sys; s=socket.socket(); s.settimeout(0.2); sys.exit(0 if s.connect_ex(('127.0.0.1',$port))==0 else 1)" && break
    sleep 0.1
done

rm -f "$work/input"
mkfifo "$work/input"
: > "$work/serial.txt"
qemu-system-x86_64 -m 256M -display none -monitor none -no-reboot -cpu max -serial stdio \
    -kernel "$work/brix.img" -netdev user,id=n0 -device virtio-net-pci,netdev=n0 < "$work/input" > "$work/serial.txt" 2>&1 &
qemu_pid=$!
exec 3> "$work/input"
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(output | grep -c -- "$1" || true) < want )); do
        (( tries++ < 600 )) || fail "no \"$1\" (x$want) on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
prompts=0
type_line() {
    prompts=$((prompts + 1))
    wait_for '^root@brix:[^#]*# $' "$prompts"
    printf '%s\r' "$1" >&3
}
# brixOS once more: this QEMU ended, another started on the image $1 with
# the network the further arguments give; the serial line from its start.
again() {
    local image=$1
    shift
    exec 3>&-
    kill "$qemu_pid" 2> /dev/null || true
    wait "$qemu_pid" 2> /dev/null || true
    : > "$work/serial.txt"
    qemu-system-x86_64 -m 256M -display none -monitor none -no-reboot -cpu max -serial stdio \
        -kernel "$image" "$@" < "$work/input" > "$work/serial.txt" 2>&1 &
    qemu_pid=$!
    exec 3> "$work/input"
    prompts=0
}
wait_for '^brixOS: /devices/net0, [0-9a-f:]* (virtio, IRQ [0-9]*)$'
wait_for '^brixOS: net0 is 10.0.2.15, gateway 10.0.2.2, name server 10.0.2.3 (DHCP)$'
type_line 'cat /system/settings/resolv.conf'
wait_for '^nameserver 10.0.2.3$'
echo "ok   the virtio network device is found, the stack configured by DHCP from QEMU's user network, resolv.conf written"
type_line 'ping 10.0.2.2 2; echo ping $?'
wait_for '^[0-9]* bytes from 10.0.2.2: seq=1 time=[0-9.]* ms$'
wait_for '^--- 2 sent, 2 received$'
wait_for '^ping 0$'
type_line 'netcalls; echo netcalls $?'
for expected in '^socket kind 9$' '^connect 0$' '^client writable (connected) 2$' '^listener readable (a connection waits) 1$' '^the peer is ourselves$' '^received 5$' '^hello$' '^read 5$' '^client sees the end (peer gone) 4$' '^receive at the end 0$' '^datagram received 5$' '^from port 4712$' '^echo answered 1$' '^an echo reply$' '^netcalls 0$'; do
    wait_for "$expected"
done
echo "ok   ping gets its echoes; the native socket calls: a stream to ourselves, a datagram, an echo"
# The link's state, read natively from /devices/net0 (netcalls, with
# std.os.brix's networkLink): what the boot said, as a record.
for expected in '^link net0: virtio 52:54:00:12:34:56 up$' '^speed 0$' '^configured by DHCP$' '^address 10.0.2.15 mask 255.255.255.0$' '^gateway 10.0.2.2 name server 10.0.2.3$' '^frames received [1-9][0-9]* sent [1-9][0-9]*$' '^no link net1: 6$' '^record: 64 bytes, its length word 64, kind 5$' '^read at 32 4$' '^there: 10.0.2.15$' '^read past the end 0$' '^write refused 2$'; do
    wait_for "$expected"
done
echo "ok   a program reads the link's state from /devices/net0: driver, hardware address, cable, the address by DHCP, gateway, name server, frames"
type_line "wget http://10.0.2.2:$port/hello.txt -O /temporary/hello.txt && cat /temporary/hello.txt"
wait_for '^wget: /temporary/hello.txt: 20 bytes$'
wait_for '^hello from the host$'
type_line "wget http://10.0.2.2:$port/large.txt -O /temporary/large.txt && sha256sum /temporary/large.txt"
wait_for "^$sum "
type_line "wget http://10.0.2.2:$port/missing.txt; echo status \$?"
wait_for '^wget: http://10.0.2.2:[0-9]*/missing.txt: the server answered 404$'
wait_for '^status 1$'
grep -q 'GET /hello.txt HTTP/1.1" 200' "$work/server.txt" || fail "the host's server did not see the request"
echo "ok   wget fetches a small and a large file over TCP from the host, and reports a 404"
# No DHCP server on the wire (a hub with nothing else on it): the link is
# up and has no address yet, DHCP goes on asking, and the record says so.
again "$work/brix.img" -netdev hubport,id=n0,hubid=0 -device virtio-net-pci,netdev=n0
wait_for '^brixOS: net0 has no address yet (DHCP is asking)$'
type_line 'netcalls links'
for expected in '^link net0: virtio 52:54:00:12:34:56 up$' '^not configured (DHCP is asking)$' '^address 0.0.0.0 mask 0.0.0.0$' '^gateway 0.0.0.0 name server 0.0.0.0$' '^done$'; do
    wait_for "$expected"
done
echo "ok   without a DHCP server the link's record has no address yet and says DHCP is asking"
# Another network (QEMU's user network as 192.168.76.0/26, as a machine's
# own network is not 10.0.2.0/24): the address, the netmask, the gateway
# and the name server of the record are what that DHCP server answered,
# not what the stack starts with.
again "$work/brix.img" -netdev user,id=n0,net=192.168.76.0/26 -device virtio-net-pci,netdev=n0
wait_for '^brixOS: net0 is 192.168.76.15, gateway 192.168.76.2, name server 192.168.76.3 (DHCP)$'
type_line 'netcalls links'
for expected in '^link net0: virtio 52:54:00:12:34:56 up$' '^configured by DHCP$' '^address 192.168.76.15 mask 255.255.255.192$' '^gateway 192.168.76.2 name server 192.168.76.3$' '^done$'; do
    wait_for "$expected"
done
echo "ok   on another network the record has what its DHCP server answered: address, netmask, gateway, name server"
# A fixed address in /system/settings/network: the kernel takes it, does
# not ask by DHCP, and the record says where the address is from. (The
# card with a hardware address of its own: the record's is the device's.)
again "$work/static.img" -netdev user,id=n0 -device virtio-net-pci,netdev=n0,mac=52:54:00:ab:cd:ef
wait_for '^brixOS: net0 is 10.0.2.77, gateway 10.0.3.1, name server 10.0.3.53$'
type_line 'netcalls links'
for expected in '^link net0: virtio 52:54:00:ab:cd:ef up$' '^configured from the settings$' '^address 10.0.2.77 mask 255.255.254.0$' '^gateway 10.0.3.1 name server 10.0.3.53$' '^done$'; do
    wait_for "$expected"
done
echo "ok   with an address in /system/settings/network the record has it with its netmask, gateway and name server, from the settings; the hardware address is the card's"
echo PASS
