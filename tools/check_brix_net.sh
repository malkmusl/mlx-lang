#!/usr/bin/env bash
# brixOS on the network: QEMU's user network (-netdev user: the guest is
# 10.0.2.15, the host 10.0.2.2, a name server at 10.0.2.3) on a virtio
# network device; the kernel's stack (ARP, IPv4, ICMP, UDP, TCP), Linux's
# socket calls for std.net, and wget (std.http.client) fetching files
# from an HTTP server on the host: a small one, a large one (many
# segments, the window, acknowledgements), a missing one (404); ping as
# a person types it (straight after the boot to a neighbour not spoken
# to yet, to the gateway, to an address nobody has, to the machine
# itself, to a name; -c before and after the host; left running and
# ended with Ctrl+C; two at once; many in a row), QEMU's capture of the
# frames saying where the echo requests went; brixOS's own socket calls
# (netcalls: a stream connection to ourselves through the stack, a
# datagram, an echo; a stream and a datagram to 127.0.0.1, none of it on
# the wire); the link's state
# as a program reads it from the kernel (/devices/net0's record): by
# DHCP, with the lease's time; then booted again without a DHCP server
# (no address: nothing is sent from one, nothing but DHCP's answers is
# taken, not what is sent to everyone either, no name server is written,
# DHCP goes on asking, and the machine still reaches itself as 127.0.0.1),
# with a DHCP answer that comes late (the cable in after the boot: the
# kernel says the address then), on another network (what its DHCP
# server answers), with a fixed address in /system/settings/network on a
# card with a hardware address of its own (the first packet through its
# gateway arrives), with a file that has an address line alone (no
# gateway, no name server: the own subnet is reached, nothing beyond
# it), with a file whose address line says 0.0.0.0 (no address: DHCP
# asks); and ping on a network of this check's own (tools/brix_lan.py on
# QEMU's socket network, for what the user network cannot play): the
# first packets to a name server and through a gateway never spoken to,
# a name looked up, replies later than a second, a reply that comes
# twice, a neighbour slow to say its hardware address, a router's word
# that a host cannot be reached, a host and a gateway nobody answers for.
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
lan_pid=
cleanup() {
    exec 3>&- 2> /dev/null || true
    [[ -n "$qemu_pid" ]] && kill "$qemu_pid" 2> /dev/null || true
    [[ -n "$server_pid" ]] && kill "$server_pid" 2> /dev/null || true
    [[ -n "$lan_pid" ]] && kill "$lan_pid" 2> /dev/null || true
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
# (A name of the hosts file for ping: QEMU's name server asks the host's
# resolver, which the machine this runs on may not have.)
printf '10.0.2.2 gateway.test\n' > "$work/hosts"
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "wget=$work/wget" "ping=$work/ping" "netcalls=$work/netcalls" "settings/hosts=$work/hosts" "$work/bin/"
# The same with a fixed address in the kernel's settings file (the name
# server beyond the subnet, behind the gateway), and with a file that has
# an address and nothing else.
printf 'address 10.0.2.77\nnetmask 255.255.254.0\ngateway 10.0.3.1\nnameserver 10.0.9.53\n' > "$work/network"
"$work/brix-image" "$work/kernel" "$work/static.img" "flux=$work/flux" "ping=$work/ping" "netcalls=$work/netcalls" "settings/network=$work/network" "$work/bin/"
printf 'address 10.0.2.77\n' > "$work/network-alone"
"$work/brix-image" "$work/kernel" "$work/alone.img" "flux=$work/flux" "ping=$work/ping" "netcalls=$work/netcalls" "settings/network=$work/network-alone" "$work/bin/"
# And one whose address line says none (0.0.0.0), with a gateway line.
printf 'address 0.0.0.0\nnetmask 255.255.0.0\ngateway 10.0.2.9\n' > "$work/network-none"
"$work/brix-image" "$work/kernel" "$work/none.img" "flux=$work/flux" "netcalls=$work/netcalls" "settings/network=$work/network-none" "$work/bin/"
# And with an address of tools/brix_lan.py's network.
printf 'address 10.10.36.50\nnetmask 255.255.255.0\ngateway 10.10.36.1\nnameserver 10.10.36.2\n' > "$work/lan-network"
"$work/brix-image" "$work/kernel" "$work/lan.img" "flux=$work/flux" "ping=$work/ping" "netcalls=$work/netcalls" "settings/network=$work/lan-network" "$work/bin/"
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
    -kernel "$work/brix.img" -netdev user,id=n0 -device virtio-net-pci,netdev=n0 -object "filter-dump,id=d0,netdev=n0,file=$work/frames.pcap" < "$work/input" > "$work/serial.txt" 2>&1 &
qemu_pid=$!
exec 3> "$work/input"
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
# ping's cases on this network (ping_cases).
source tools/brix_ping_cases.sh
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
# the network the further arguments give (and its monitor on a socket);
# the serial line from its start.
again() {
    local image=$1
    shift
    exec 3>&-
    kill "$qemu_pid" 2> /dev/null || true
    wait "$qemu_pid" 2> /dev/null || true
    : > "$work/serial.txt"
    rm -f "$work/monitor"
    qemu-system-x86_64 -m 256M -display none -monitor "unix:$work/monitor,server,nowait" -no-reboot -cpu max -serial stdio \
        -kernel "$image" "$@" < "$work/input" > "$work/serial.txt" 2>&1 &
    qemu_pid=$!
    exec 3> "$work/input"
    prompts=0
}
# Commands to that QEMU's monitor.
monitor() {
    local tries=0
    until [[ -S "$work/monitor" ]]; do
        (( tries++ < 100 )) || fail "QEMU's monitor is not there"
        sleep 0.1
    done
    python3 - "$work/monitor" "$@" <<'PY'
import socket, sys, time
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sys.argv[1])
s.settimeout(0.5)
def drain():
    try:
        while s.recv(4096): pass
    except socket.timeout:
        pass
drain()
for command in sys.argv[2:]:
    s.sendall((command + "\n").encode())
    time.sleep(0.15)
    drain()
PY
}
# The frames QEMU captured on a network (the file $1), a line each: an
# ARP message as "arp SENDER>TARGET", an IPv4 packet as "ip
# SOURCE>DESTINATION", UDP's with its ports after it.
frames_of() {
    python3 - "$1" <<'PY'
import struct, sys
data = open(sys.argv[1], "rb").read()
at = 24
dotted = lambda raw: ".".join(str(part) for part in raw)
while at + 16 <= len(data):
    size = struct.unpack("<I", data[at + 8:at + 12])[0]
    frame = data[at + 16:at + 16 + size]
    at += 16 + size
    if len(frame) >= 42 and frame[12:14] == b"\x08\x06":
        print("arp %s>%s" % (dotted(frame[28:32]), dotted(frame[38:42])))
    elif len(frame) >= 34 and frame[12:14] == b"\x08\x00":
        header = (frame[14] & 15) * 4
        ports = ""
        if frame[23] == 17 and len(frame) >= 14 + header + 4:
            ports = " udp %d>%d" % struct.unpack(">HH", frame[14 + header:14 + header + 4])
        print("ip %s>%s%s" % (dotted(frame[26:30]), dotted(frame[30:34]), ports))
PY
}
# Those of them that name 127.0.0.0/8 (none may: it is the machine itself).
frames_naming_itself() { frames_of "$1" | grep '[ >]127\.' | sort -u | tr '\n' ' ' || true; }
wait_for '^brixOS: /devices/net0, [0-9a-f:]* (virtio, IRQ [0-9]*)$'
wait_for '^brixOS: net0 is 10.0.2.15, gateway 10.0.2.2, name server 10.0.2.3 (DHCP)$'
type_line 'cat /system/settings/resolv.conf'
wait_for '^nameserver 10.0.2.3$'
echo "ok   the virtio network device is found, the stack configured by DHCP from QEMU's user network, resolv.conf written"
# (ping's cases begin with the first packet to a neighbour nobody has
# asked for yet, which the kernel keeps until the hardware address is
# known, and have the address nobody has: the kernel's own word for it.)
ping_cases
type_line 'netcalls; echo netcalls $?'
for expected in '^socket kind 9$' '^connect 0$' '^client writable (connected) 2$' '^listener readable (a connection waits) 1$' '^the peer is ourselves$' '^received 5$' '^hello$' '^read 5$' '^client sees the end (peer gone) 4$' '^receive at the end 0$' '^datagram received 5$' '^from port 4712$' '^echo answered 1$' '^an echo reply$' '^echo from 10.0.2.2$' '^netcalls 0$'; do
    wait_for "$expected"
done
echo "ok   the native socket calls: a stream to ourselves, a datagram, an echo answered by the gateway it was sent to"
# The link's state, read natively from /devices/net0 (netcalls, with
# std.os.brix's networkLink): what the boot said, as a record.
# (QEMU's user network gives a lease of a day; its length cannot be set,
# so the renewal itself is tests/455_dhcp_lease_runtime.mlx's.)
for expected in '^link net0: virtio 52:54:00:12:34:56 up$' '^speed 0$' '^configured by DHCP$' '^address 10.0.2.15 mask 255.255.255.0$' '^gateway 10.0.2.2 name server 10.0.2.3$' '^frames received [1-9][0-9]* sent [1-9][0-9]*$' '^lease 86400 s, 86[0-9][0-9][0-9] s left$' '^no link net1: 6$' '^record: 72 bytes, its length word 72, its first 64 as they were, kind 5$' '^read at 32 4$' '^there: 10.0.2.15$' '^read past the end 0$' '^write refused 2$'; do
    wait_for "$expected"
done
echo "ok   a program reads the link's state from /devices/net0: driver, hardware address, cable, the address by DHCP, gateway, name server, frames, the lease; a reader of the record's first 64 bytes gets them as before"
# The machine spoken to as 127.0.0.1, over TCP and UDP as ping's cases
# did with an echo: both ends know each other by that name, and no frame
# on the wire names it.
type_line 'netcalls itself'
for expected in '^itself: connect 0$' '^itself: connected 2$' '^itself: the peer is 127.0.0.1, the client speaks from 127.0.0.1$' '^itself: received 5$' '^itself: read 5$' '^itself: datagram received 5 from 127.0.0.1$'; do
    wait_for "$expected"
done
wait_for '^done$' 2
strays=$(frames_naming_itself "$work/frames.pcap")
[[ -z $strays ]] || fail "frames that name 127.0.0.0/8 went onto the wire: $strays"
echo "ok   a stream and a datagram to 127.0.0.1 stay in the machine: each end sees the other as 127.0.0.1, and nothing of it is on the wire"
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
# No DHCP server on the wire: a network of our own (tools/brix_lan.py at
# the other end of QEMU's socket network: every frame of the guest's is
# a datagram to it), which has hosts that would answer and nobody who
# gives addresses. (A hub with nothing else on it will not do here: QEMU
# takes the first frame from the card and no more, so its capture would
# not show what the machine sends.) The link is up and has no address,
# DHCP goes on asking, and the record says so. The stack has none
# either: nothing can be sent (ping says why), no name server is written
# for the programs that look names up, and the only frames on the wire
# are DHCP's questions, from 0.0.0.0 to everyone (no ARP, nothing from
# an address the machine does not have). The machine still reaches
# itself as 127.0.0.1.
ports=$(python3 -c 'import socket
a = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); a.bind(("127.0.0.1", 0))
b = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); b.bind(("127.0.0.1", 0))
print(a.getsockname()[1], b.getsockname()[1])')
lan_port=${ports% *}
guest_port=${ports#* }
python3 tools/brix_lan.py "$lan_port" "$guest_port" > "$work/lan.txt" 2>&1 &
lan_pid=$!
again "$work/brix.img" -netdev "socket,id=n0,udp=127.0.0.1:$lan_port,localaddr=127.0.0.1:$guest_port" -device virtio-net-pci,netdev=n0 -object "filter-dump,id=d0,netdev=n0,file=$work/alone.pcap"
wait_for '^brixOS: net0 has no address yet (DHCP is asking)$'
type_line 'netcalls links'
for expected in '^link net0: virtio 52:54:00:12:34:56 up$' '^not configured (DHCP is asking)$' '^address 0.0.0.0 mask 0.0.0.0$' '^gateway none name server none$' '^no lease$' '^done$'; do
    wait_for "$expected"
done
type_line 'ping -c 1 10.0.2.2; echo nowhere $?'
wait_for '^ping: this machine has no address yet (DHCP is asking)$'
wait_for '^seq=1 could not be sent: this machine has no address$'
wait_for '^--- 1 sent, 0 received, 100% lost$'
wait_for '^nowhere 1$'
type_line 'wc -c /system/settings/resolv.conf'
wait_for '^ *0 /system/settings/resolv.conf$'
type_line 'ping -c 1 127.0.0.1; echo home $?; netcalls itself'
wait_for '^64 bytes from 127.0.0.1: seq=1 time=[0-9.]* ms$'
wait_for '^home 0$'
for expected in '^itself: the peer is 127.0.0.1, the client speaks from 127.0.0.1$' '^itself: read 5$' '^itself: datagram received 5 from 127.0.0.1$'; do
    wait_for "$expected"
done
wait_for '^done$' 2
# Nothing off the wire is taken either but DHCP's answers: the name
# server of this network calls everyone twice a second (a datagram to
# UDP port 4715, which a machine with an address hears: below), and a
# socket on that port hears nothing.
type_line 'netcalls hears'
wait_for '^hears: '
output | grep -q '^hears: nothing$' || fail "without an address a socket hears what is sent to everyone: $(output | grep '^hears: ')"
calls=$(frames_of "$work/alone.pcap" | grep -c '^ip 10\.10\.36\.2>255\.255\.255\.255 udp 4715>4715$' || true)
(( calls >= 2 )) || fail "the network's calls to everyone did not reach the machine's card ($calls)"
output | grep -q '^brixOS: net0 is ' && fail "the kernel says an address without a DHCP server"
asked=$(frames_of "$work/alone.pcap" | grep -c '^ip 0\.0\.0\.0>255\.255\.255\.255 udp 68>67$' || true)
(( asked >= 2 )) || fail "DHCP does not go on asking on a wire without a DHCP server ($asked questions)"
# (The capture has both ways: the network's calls aside, the machine's
# own frames are DHCP's questions alone.)
others=$(frames_of "$work/alone.pcap" | grep -v -e '^ip 0\.0\.0\.0>255\.255\.255\.255 udp 68>67$' -e '^ip 10\.10\.36\.2>255\.255\.255\.255 udp 4715>4715$' | sort -u | tr '\n' ' ' || true)
[[ -z $others ]] || fail "without an address something else than DHCP's questions went onto the wire: $others"
echo "ok   without a DHCP server there is no address: the record says none and that DHCP is asking, ping says it cannot send, resolv.conf names no server, the wire has DHCP's questions alone ($asked), and what is sent to everyone is not heard; 127.0.0.1 is reached all the same"
# The DHCP server's answer comes late: QEMU starts with the cable out
# (set_link off before the machine runs), the boot says there is no
# address yet, and when the cable is in the kernel asks at once and says
# the address in the same words the boot has for it; the record follows.
again "$work/brix.img" -S -netdev user,id=n0 -device virtio-net-pci,netdev=n0
monitor "set_link n0 off" "cont"
wait_for '^brixOS: net0 has no address yet (DHCP is asking)$'
type_line 'netcalls links'
for expected in '^link net0: virtio 52:54:00:12:34:56 no cable$' '^not configured (DHCP is asking)$' '^gateway none name server none$' '^done$'; do
    wait_for "$expected"
done
output | grep -q '^brixOS: net0 is ' && fail "the kernel says an address with the cable out"
monitor "set_link n0 on"
wait_for '^brixOS: net0 is 10.0.2.15, gateway 10.0.2.2, name server 10.0.2.3 (DHCP)$'
type_line 'netcalls links'
for expected in '^link net0: virtio 52:54:00:12:34:56 up$' '^configured by DHCP$' '^address 10.0.2.15 mask 255.255.255.0$' '^gateway 10.0.2.2 name server 10.0.2.3$' '^lease 86400 s, 86[0-9][0-9][0-9] s left$'; do
    wait_for "$expected"
done
type_line 'cat /system/settings/resolv.conf; ping 10.0.2.2 1; echo late $?'
wait_for '^nameserver 10.0.2.3$'
wait_for '^late 0$'
(( $(output | grep -c '^brixOS: net0 is ') == 1 )) || fail "the kernel says the address more than once"
echo "ok   a DHCP answer that comes after the boot (the cable in late): the kernel says the address then, once, on a line of its own; the record, resolv.conf and a ping follow"
# Another network (QEMU's user network as 192.168.76.0/26, as a machine's
# own network is not 10.0.2.0/24): the address, the netmask, the gateway
# and the name server of the record are what that DHCP server answered.
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
# QEMU's network here is 10.0.0.0/16 with its host at 10.0.3.1, the
# file's gateway, and its name server at 10.0.9.53, which the file's
# netmask puts beyond the subnet: the first packet this stack sends at
# all goes there, through a gateway whose hardware address nobody has
# asked for, and is answered.
again "$work/static.img" -netdev user,id=n0,net=10.0.0.0/16,host=10.0.3.1,dns=10.0.9.53 -device virtio-net-pci,netdev=n0,mac=52:54:00:ab:cd:ef
wait_for '^brixOS: net0 is 10.0.2.77, gateway 10.0.3.1, name server 10.0.9.53$'
type_line 'ping -c 1 10.0.9.53; echo through $?'
wait_for '^64 bytes from 10.0.9.53: seq=1 time=[0-9.]* ms$'
wait_for '^--- 1 sent, 1 received, 0% lost$'
wait_for '^through 0$'
echo "ok   the first packet through the gateway of a static configuration is answered (kept until the gateway's hardware address is known)"
type_line 'netcalls links'
for expected in '^link net0: virtio 52:54:00:ab:cd:ef up$' '^configured from the settings$' '^address 10.0.2.77 mask 255.255.254.0$' '^gateway 10.0.3.1 name server 10.0.9.53$' '^no lease$' '^done$'; do
    wait_for "$expected"
done
type_line 'cat /system/settings/resolv.conf'
wait_for '^nameserver 10.0.9.53$'
echo "ok   with an address in /system/settings/network the record has it with its netmask, gateway and name server, from the settings; the hardware address is the card's"
# A settings file with an address line and nothing else: no gateway and
# no name server (none of QEMU's, which answer on this wire all the
# same), the netmask of the address's class. The own subnet is reached,
# nothing beyond it (ping says why), and no name server is written.
again "$work/alone.img" -netdev user,id=n0 -device virtio-net-pci,netdev=n0
wait_for '^brixOS: net0 is 10.0.2.77, gateway none, name server none$'
type_line 'netcalls links'
for expected in '^link net0: virtio 52:54:00:12:34:56 up$' '^configured from the settings$' '^address 10.0.2.77 mask 255.0.0.0$' '^gateway none name server none$' '^no lease$' '^done$'; do
    wait_for "$expected"
done
type_line 'ping -c 1 10.0.2.2; echo subnet $?'
wait_for '^64 bytes from 10.0.2.2: seq=1 time=[0-9.]* ms$'
wait_for '^subnet 0$'
type_line 'ping -c 1 192.168.76.2; echo beyond $?'
wait_for "^seq=1 could not be sent: 192.168.76.2 is not on this machine's network, and there is no gateway\$"
wait_for '^beyond 1$'
type_line 'wc -c /system/settings/resolv.conf'
wait_for '^ *0 /system/settings/resolv.conf$'
echo "ok   with an address line alone there is no gateway and no name server: the record and the boot say none, the own subnet is reached, nothing beyond it"
# A settings file whose address line says 0.0.0.0: that is no address,
# so nothing of the file is used (not its gateway either) and DHCP asks.
again "$work/none.img" -netdev user,id=n0 -device virtio-net-pci,netdev=n0
wait_for '^brixOS: net0 is 10.0.2.15, gateway 10.0.2.2, name server 10.0.2.3 (DHCP)$'
type_line 'netcalls links'
for expected in '^configured by DHCP$' '^address 10.0.2.15 mask 255.255.255.0$' '^gateway 10.0.2.2 name server 10.0.2.3$' '^done$'; do
    wait_for "$expected"
done
echo "ok   an address line of 0.0.0.0 is no address: DHCP asks, and nothing of the file is the configuration"
# The network of our own again (tools/brix_lan.py: 10.10.36.0/24 with a
# gateway, a name server, and hosts that answer late, twice, or only
# after a while say their hardware address). The guest's address is its
# settings file's; it has spoken to nobody when the first ping comes.
again "$work/lan.img" -netdev "socket,id=n0,udp=127.0.0.1:$lan_port,localaddr=127.0.0.1:$guest_port" -device virtio-net-pci,netdev=n0
wait_for '^brixOS: net0 is 10.10.36.50, gateway 10.10.36.1, name server 10.10.36.2$'
# A name first: the query is the first packet to the name server, the
# echo request the first through the gateway (neither's hardware address
# is known yet), and both arrive.
type_line 'ping -c 1 far.lan; echo far $?'
wait_for '^PING far.lan (192.0.2.10): 56 bytes of data$'
wait_for '^64 bytes from 192.0.2.10: seq=1 time=[0-9.]* ms$'
wait_for '^far 0$'
type_line 'ping -c 1 nothere.lan; echo unknown $?'
wait_for '^ping: nothere.lan: the name server knows no such name$'
wait_for '^unknown 1$'
# (With an address, what the name server calls to everyone reaches a
# socket: the same call the machine without an address did not hear.)
type_line 'netcalls hears'
wait_for '^hears: a datagram of 7 bytes from 10.10.36.2$'
echo "ok   on a network of our own: a name is looked up and pinged with the first packets to the name server and through the gateway; a name there is none of is said; a datagram to everyone is heard"
# Replies that come a second and a half after their requests: each is
# said late with its own request's number and time, none is lost and
# none is taken for the next request's.
type_line 'ping -c 2 10.10.36.20; echo late $?'
wait_for '^no answer for seq=1$'
wait_for '^64 bytes from 10.10.36.20: seq=1 time=1[0-9]* ms (late)$'
wait_for '^no answer for seq=2$'
wait_for '^64 bytes from 10.10.36.20: seq=2 time=1[0-9]* ms (late)$'
wait_for '^--- 2 sent, 2 received, 0% lost$'
wait_for '^late 0$'
# A reply that comes twice is shown, and counted once.
type_line 'ping -c 2 10.10.36.21; echo twice $?'
wait_for '^64 bytes from 10.10.36.21: seq=1 time=[0-9.]* ms$'
wait_for '^64 bytes from 10.10.36.21: seq=1 time=[0-9.]* ms (again)$'
wait_for '^--- 2 sent, 2 received, 0% lost$' 2
wait_for '^twice 0$'
# A neighbour that takes 0.3 s to say its hardware address, pinged by
# two programs at once: the first request of each reaches it all the
# same (both wait in the kernel for the one answer).
type_line 'ping -c 1 10.10.36.22 > /temporary/other & ping -c 1 10.10.36.22; slow=$?; wait; cat /temporary/other; echo slow-neighbour $slow'
wait_for '^64 bytes from 10.10.36.22: seq=1 time=[0-9.]* ms$' 2
wait_for '^slow-neighbour 0$'
echo "ok   replies later than a second are told by their own request, a reply that comes twice counts once, a neighbour slow to say its hardware address gets the first request of two programs that ping it at once"
# The gateway's word that a host cannot be reached, in place of "no
# answer"; and a host nobody answers for.
type_line 'ping -c 1 192.0.2.9; echo unreachable $?'
wait_for '^from 10.10.36.1: seq=1 was not delivered: the host cannot be reached$'
wait_for '^--- 1 sent, 0 received, 100% lost$'
wait_for '^unreachable 1$'
# (The kernel kept the request while it asked for the hardware address,
# and said itself that nobody answered: ping says whose it was, for each
# request within its second.)
type_line 'ping -c 2 10.10.36.9; echo absent $?'
wait_for '^no answer from 10.10.36.9 (its hardware address is not known: nobody answered for it)$' 2
wait_for '^--- 2 sent, 0 received, 100% lost$'
wait_for '^absent 1$'
# (The two bare ones on this serial line are the late host's, above.)
(( $(output | grep -c '^no answer for seq=') == 2 )) || fail "a request nobody's hardware address was said for got a bare \"no answer\""
echo "ok   a router's word that a host cannot be reached is said; for an address nobody has ping says, request by request, that nobody answered for its hardware address"
# The same network with a gateway that does not say its hardware address:
# what lies beyond it is not reached, and ping says whose address is
# missing; the own subnet's name server still answers.
kill "$lan_pid" 2> /dev/null || true
wait "$lan_pid" 2> /dev/null || true
python3 tools/brix_lan.py "$lan_port" "$guest_port" --silent-gateway > "$work/lan.txt" 2>&1 &
lan_pid=$!
again "$work/lan.img" -netdev "socket,id=n0,udp=127.0.0.1:$lan_port,localaddr=127.0.0.1:$guest_port" -device virtio-net-pci,netdev=n0
wait_for '^brixOS: net0 is 10.10.36.50, gateway 10.10.36.1, name server 10.10.36.2$'
type_line 'ping -c 1 192.0.2.10; echo beyond $?'
wait_for '^no answer from 192.0.2.10 (the hardware address of the gateway 10.10.36.1 is not known: nobody answered for it)$'
wait_for '^beyond 1$'
type_line 'ping -c 1 10.10.36.2; echo near $?'
wait_for '^64 bytes from 10.10.36.2: seq=1 time=[0-9.]* ms$'
wait_for '^near 0$'
echo "ok   a gateway nobody answers for: ping says that its hardware address is not known; the own subnet is still reached"
echo PASS
