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
# the wire); a ping to the subnet's address for everyone (to everyone's
# hardware address, nobody asked for one); the link's state
# as a program reads it from the kernel (/devices/net0's record): by
# DHCP, with the lease's time; then booted again without a DHCP server
# (no address: nothing is sent from one, nothing but DHCP's answers is
# taken, not what is sent to everyone either, no name server is written,
# DHCP goes on asking, and the machine still reaches itself as 127.0.0.1;
# a connection, a datagram and an echo to another machine are refused at
# once as unreachable, and wget says why),
# with a DHCP answer that comes late (the cable in after the boot: the
# kernel says the address then), on another network (what its DHCP
# server answers), with a fixed address in /system/settings/network on a
# card with a hardware address of its own (the first packet through its
# gateway arrives), with a file that has an address line alone (no
# gateway, no name server: the own subnet is reached, nothing beyond
# it, said at once by wget too), with a file whose address line says
# 0.0.0.0 (no address: DHCP asks), with a file whose netmask is none (a
# hole in its bits, or 0.0.0.0: the file is not used, said in the log),
# without a network card (ping says there is none, and the machine
# reaches itself all the same: ping, the socket calls, logserve asked by
# wget); and ping on
# a network of this check's own (tools/brix_lan.py on
# QEMU's socket network, for what the user network cannot play): the
# first packets to a name server and through a gateway never spoken to,
# a name looked up (also written with the root's dot, and when the first
# question is lost; a name server that cannot look a name up), replies
# later than a second, a reply that comes twice, one from another
# address, a neighbour slow to say its hardware address, a router's word
# that a host cannot be reached (also of one that answers all the same),
# a host and a gateway nobody answers for; there too what is sent to the
# subnet's address for everyone and what comes from it (a datagram or an
# echo: a stream's connect to such an address is refused), a name whose
# IPv6 question the name server drops (not asked), and a listener
# (logserve) that a connection asked of everyone does not reach (and
# nothing is answered that names everyone as where it comes from), while a
# real request is answered within two seconds behind 4 and behind 100
# connections that were begun and never finished.
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
"$compiler" --quiet $brix projects/brix/tools/logserve/main.mlx -o "$work/logserve"
for program in cat wc echo sha256sum sleep; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
# (A name of the hosts file for ping: QEMU's name server asks the host's
# resolver, which the machine this runs on may not have.)
printf '10.0.2.2 gateway.test\n' > "$work/hosts"
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "wget=$work/wget" "ping=$work/ping" "netcalls=$work/netcalls" "logserve=$work/logserve" "settings/hosts=$work/hosts" "$work/bin/"
# The same with a fixed address in the kernel's settings file (the name
# server beyond the subnet, behind the gateway), and with a file that has
# an address and nothing else.
printf 'address 10.0.2.77\nnetmask 255.255.254.0\ngateway 10.0.3.1\nnameserver 10.0.9.53\n' > "$work/network"
"$work/brix-image" "$work/kernel" "$work/static.img" "flux=$work/flux" "ping=$work/ping" "netcalls=$work/netcalls" "settings/network=$work/network" "$work/bin/"
printf 'address 10.0.2.77\n' > "$work/network-alone"
"$work/brix-image" "$work/kernel" "$work/alone.img" "flux=$work/flux" "wget=$work/wget" "ping=$work/ping" "netcalls=$work/netcalls" "settings/network=$work/network-alone" "$work/bin/"
# And one whose address line says none (0.0.0.0), with a gateway line.
printf 'address 0.0.0.0\nnetmask 255.255.0.0\ngateway 10.0.2.9\n' > "$work/network-none"
"$work/brix-image" "$work/kernel" "$work/none.img" "flux=$work/flux" "netcalls=$work/netcalls" "settings/network=$work/network-none" "$work/bin/"
# And two whose netmask line is no netmask: a hole in its bits; 0.0.0.0.
printf 'address 10.0.2.77\nnetmask 255.0.255.0\ngateway 10.0.2.9\n' > "$work/network-holes"
"$work/brix-image" "$work/kernel" "$work/holes.img" "flux=$work/flux" "netcalls=$work/netcalls" "settings/network=$work/network-holes" "$work/bin/"
printf 'address 10.0.2.77\nnetmask 0.0.0.0\ngateway 10.0.2.9\n' > "$work/network-zero"
"$work/brix-image" "$work/kernel" "$work/zero.img" "flux=$work/flux" "netcalls=$work/netcalls" "settings/network=$work/network-zero" "$work/bin/"
# And with an address of tools/brix_lan.py's network.
printf 'address 10.10.36.50\nnetmask 255.255.255.0\ngateway 10.10.36.1\nnameserver 10.10.36.2\n' > "$work/lan-network"
"$work/brix-image" "$work/kernel" "$work/lan.img" "flux=$work/flux" "ping=$work/ping" "netcalls=$work/netcalls" "logserve=$work/logserve" "settings/network=$work/lan-network" "$work/bin/"
echo "ok   the kernel, wget, ping, netcalls, logserve and a boot image build"

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
# What tools/brix_lan.py is told to do, and the line $1 of its output
# waited for ($2 seconds, 60 unless given): the last such line.
tell_lan() { python3 tools/brix_lan.py --tell "$lan_port" "$@"; }
lan_said() {
    local tries=0
    until grep -q -- "$1" "$work/lan.txt"; do
        (( tries++ < ${2:-60} * 10 )) || fail "the network of this check did not say \"$1\": $(tail -n 5 "$work/lan.txt" | tr '\n' ';')"
        sleep 0.1
    done
    grep -- "$1" "$work/lan.txt" | tail -n 1
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
# (frames_of, the frames QEMU captured on a network a line each, is
# tools/brix_ping_cases.sh's.)
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
# The subnet's address for everyone, 10.0.2.255: the echo requests go to
# everyone's hardware address and nobody is asked for one. (The kernel
# asked for the hardware address of 10.0.2.255 as of a host's, nobody
# answered, and ping said so; no request went.) Nobody answers on this
# network: ping says that alone.
type_line 'ping -c 2 10.0.2.255; echo to-everyone $?'
wait_for '^PING 10.0.2.255: 56 bytes of data$'
wait_for '^to-everyone 1$'
if output | grep -q '^no answer from 10.0.2.255 '; then fail "ping to the subnet's address for everyone: $(output | grep '^no answer from 10.0.2.255 ' | head -n 1)"; fi
asked=$(frames_of "$work/frames.pcap" | grep -c '^arp 10\.0\.2\.15>10\.0\.2\.255$' || true)
(( asked == 0 )) || fail "the kernel asked $asked times for the hardware address of 10.0.2.255, the subnet's address for everyone"
went=$(frames_of "$work/frames.pcap" everyone | grep -c '^ip 10\.0\.2\.15>10\.0\.2\.255$' || true)
(( went == 2 )) || fail "of two echo requests to 10.0.2.255, $went went to everyone's hardware address"
echo "ok   a ping to the subnet's address for everyone goes to everyone's hardware address: nobody is asked for one"
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
# (A request that never went is not one that was sent and lost: the
# summary once said "1 sent, 0 received, 100% lost" under that line.)
wait_for '^--- 0 sent, 0 received, 1 could not be sent$'
wait_for '^nowhere 1$'
# A name there: no name server is set, and why not (this machine has no
# address) is said with it, as it is before a request to an address.
type_line 'ping -c 1 gateway.lan; echo nameless $?'
wait_for '^ping: gateway.lan: no name server is set to look the name up$'
wait_for '^ping: this machine has no address yet (DHCP is asking)$' 2
wait_for '^nameless 1$'
type_line 'wc -c /system/settings/resolv.conf'
wait_for '^ *0 /system/settings/resolv.conf$'
type_line 'ping -c 1 127.0.0.1; echo home $?; netcalls itself'
wait_for '^64 bytes from 127.0.0.1: seq=1 time=[0-9.]* ms$'
wait_for '^home 0$'
# (Nothing stands in the way to the machine itself: the missing address
# was said twice above, and is not said of this request.)
(( $(output | grep -c '^ping: this machine has no address yet (DHCP is asking)$') == 2 )) || fail "ping says of a request to 127.0.0.1 that the machine has no address"
for expected in '^itself: the peer is 127.0.0.1, the client speaks from 127.0.0.1$' '^itself: read 5$' '^itself: datagram received 5 from 127.0.0.1$'; do
    wait_for "$expected"
done
wait_for '^done$' 2
# Nothing off the wire is taken either but DHCP's answers: the name
# server of this network calls everyone twice a second (a datagram to
# UDP port 4715, which a machine with an address hears: below), and a
# socket on that port hears nothing.
type_line 'netcalls hears; netcalls hears 4716'
wait_for '^hears: ' 2
(( $(output | grep -c '^hears: nothing$') == 2 )) || fail "without an address a socket hears what is sent to everyone: $(output | grep '^hears: ' | tr '\n' ';')"
calls=$(frames_of "$work/alone.pcap" | grep -c '^ip 10\.10\.36\.2>255\.255\.255\.255 udp 4715>4715$' || true)
(( calls >= 2 )) || fail "the network's calls to everyone did not reach the machine's card ($calls)"
# What is to go to another machine is refused at once, with the status
# that says it cannot leave this one (27, unreachable): a stream's
# connect (it was taken, and its SYN tried for over a minute), a
# datagram and an echo (they were told to try later). wget says so with
# the reason, and at once (it said "cannot connect" after thirty
# seconds).
type_line 'netcalls nowhere 10.10.36.1'
# (std's own write of such a datagram says it as Linux's layer does,
# error number 101, ENETUNREACH: it said 22, EINVAL.)
for expected in '^nowhere: connect 27$' '^nowhere: datagram 27$' "^nowhere: std's write 101\$" '^nowhere: echo 27$'; do
    wait_for "$expected"
done
type_line 'wget http://10.10.36.1/x; echo unreachable $?'
wait_within '^wget: cannot fetch http://10.10.36.1/x: cannot connect: this machine has no address$' 10 "wget to a host that cannot be reached, on a machine without an address"
wait_for '^unreachable 1$'
output | grep -q '^brixOS: net0 is ' && fail "the kernel says an address without a DHCP server"
asked=$(frames_of "$work/alone.pcap" | grep -c '^ip 0\.0\.0\.0>255\.255\.255\.255 udp 68>67$' || true)
(( asked >= 2 )) || fail "DHCP does not go on asking on a wire without a DHCP server ($asked questions)"
# (The capture has both ways: the network's calls aside, the machine's
# own frames are DHCP's questions alone.)
others=$(frames_of "$work/alone.pcap" | grep -v -e '^ip 0\.0\.0\.0>255\.255\.255\.255 udp 68>67$' -e '^ip 10\.10\.36\.2>255\.255\.255\.255 udp 4715>4715$' -e '^ip 10\.10\.36\.2>10\.10\.36\.255 udp 4716>4716$' | sort -u | tr '\n' ' ' || true)
[[ -z $others ]] || fail "without an address something else than DHCP's questions went onto the wire: $others"
echo "ok   without a DHCP server there is no address: the record says none and that DHCP is asking, ping says it cannot send, resolv.conf names no server, the wire has DHCP's questions alone ($asked), and what is sent to everyone is not heard; 127.0.0.1 is reached all the same; a connection, a datagram and an echo to another machine are unreachable at once, and wget says that this machine has no address"
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
wait_for '^--- 0 sent, 0 received, 1 could not be sent$'
wait_for '^beyond 1$'
type_line 'wc -c /system/settings/resolv.conf'
wait_for '^ *0 /system/settings/resolv.conf$'
# (And a connection beyond the subnet is refused at once, with the
# reason: it was tried for thirty seconds.)
type_line 'netcalls nowhere 192.168.76.2'
wait_for '^nowhere: connect 27$'
type_line 'wget http://192.168.76.2/x; echo beyond-wget $?'
wait_within "^wget: cannot fetch http://192.168.76.2/x: cannot connect: the host is not on this machine's network, and there is no gateway\$" 10 "wget to a host beyond the subnet, without a gateway"
wait_for '^beyond-wget 1$'
echo "ok   with an address line alone there is no gateway and no name server: the record and the boot say none, the own subnet is reached, nothing beyond it (ping and wget say why, at once)"
# A settings file whose address line says 0.0.0.0: that is no address,
# so nothing of the file is used (not its gateway either) and DHCP asks.
again "$work/none.img" -netdev user,id=n0 -device virtio-net-pci,netdev=n0
wait_for '^brixOS: net0 is 10.0.2.15, gateway 10.0.2.2, name server 10.0.2.3 (DHCP)$'
type_line 'netcalls links'
for expected in '^configured by DHCP$' '^address 10.0.2.15 mask 255.255.255.0$' '^gateway 10.0.2.2 name server 10.0.2.3$' '^done$'; do
    wait_for "$expected"
done
echo "ok   an address line of 0.0.0.0 is no address: DHCP asks, and nothing of the file is the configuration"
# A settings file whose netmask is no netmask: a hole in its bits, or
# 0.0.0.0. The kernel says so, uses nothing of the file, and DHCP asks.
# (Taken as typed, 255.0.255.0 made neighbours of hosts that are none,
# and 0.0.0.0 of every host there is.)
again "$work/holes.img" -netdev user,id=n0 -device virtio-net-pci,netdev=n0
wait_for '^brixOS: /system/settings/network is not used: 255\.0\.255\.0 is no netmask$'
wait_for '^brixOS: net0 is 10.0.2.15, gateway 10.0.2.2, name server 10.0.2.3 (DHCP)$'
type_line 'netcalls links'
for expected in '^configured by DHCP$' '^address 10.0.2.15 mask 255.255.255.0$' '^done$'; do
    wait_for "$expected"
done
again "$work/zero.img" -netdev user,id=n0 -device virtio-net-pci,netdev=n0
wait_for '^brixOS: /system/settings/network is not used: 0\.0\.0\.0 is no netmask$'
wait_for '^brixOS: net0 is 10.0.2.15, gateway 10.0.2.2, name server 10.0.2.3 (DHCP)$'
echo "ok   a settings file whose netmask is none (a hole in its bits; 0.0.0.0) is not used: the kernel's log says so, and DHCP asks"
# No network card at all: ping says so, of an address and with a name
# that cannot be looked up (no name server is set: there is no device
# one could be set for, which was not said).
again "$work/brix.img" -nic none
type_line 'ping -c 1 10.0.2.2; echo cardless $?'
wait_for '^ping: this machine has no network device$'
wait_for '^cardless 1$'
type_line 'ping -c 1 gateway.lan; echo cardless-name $?'
wait_for '^ping: gateway.lan: no name server is set to look the name up$'
wait_for '^ping: this machine has no network device$' 2
wait_for '^cardless-name 1$'
echo "ok   without a network card ping says that there is none, also with a name it cannot look up"
# The machine reaches itself all the same: 127.0.0.0/8 needs no card.
# (No socket could be opened there at all: ping said of 127.0.0.1 too
# that there is no network device.) An echo, the socket calls (a stream
# and a datagram), and a listener: logserve, asked by wget. What is to
# go to another machine is unreachable, and wget says why.
type_line 'ping -c 1 127.0.0.1; echo cardless-home $?'
wait_for '^64 bytes from 127.0.0.1: seq=1 time=[0-9.]* ms$'
wait_for '^cardless-home 0$'
type_line 'netcalls itself; netcalls nowhere 10.0.2.2'
for expected in '^itself: connected 2$' '^itself: the peer is 127.0.0.1, the client speaks from 127.0.0.1$' '^itself: received 5$' '^itself: read 5$' '^itself: datagram received 5 from 127.0.0.1$' '^nowhere: connect 27$' '^nowhere: datagram 27$' '^nowhere: echo 27$'; do
    wait_for "$expected"
done
type_line 'wget http://10.0.2.2/x; echo cardless-away $?'
wait_within '^wget: cannot fetch http://10.0.2.2/x: cannot connect: this machine has no network device$' 10 "wget to another machine, on a machine without a network card"
wait_for '^cardless-away 1$'
# (logserve goes on behind the shell: this boot's last line.)
type_line 'logserve & sleep 1; wget http://127.0.0.1:8080/system -O /temporary/system && cat /temporary/system; echo cardless-fetched $?'
wait_for '^  this machine has no network device: only this machine itself reaches it, at http://127.0.0.1:8080/all$'
wait_for '^127\.0\.0\.1 GET /system: 200, [0-9]* bytes$'
wait_for '^wget: /temporary/system: [0-9]* bytes$'
wait_for '^no network link (/devices/net0 is not there)$'
wait_for '^cardless-fetched 0$'
echo "ok   without a network card the machine reaches itself: ping 127.0.0.1 is answered, a stream and a datagram to 127.0.0.1 arrive, logserve listens and wget fetches a page from it; what is to go elsewhere is unreachable at once, and wget says that there is no network device"
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
# A name written with the root's dot at its end is the name (it was "no
# such name": no record of the answer was named with the dot). A name
# server that says it cannot look a name up has not said there is no
# such name. One that does not hear the first question is asked again
# within the time (it was "did not answer" after the one question).
type_line 'ping -c 1 gateway.lan.; echo dotted $?'
wait_for '^PING gateway.lan. (10.10.36.1): 56 bytes of data$'
wait_for '^dotted 0$'
type_line 'ping -c 1 broken.lan; echo broken $?'
wait_for '^ping: broken.lan: the name server answered that it cannot look the name up$'
wait_for '^broken 1$'
type_line 'ping -c 1 second.lan; echo second $?'
wait_for '^PING second.lan (10.10.36.1): 56 bytes of data$'
wait_for '^second 0$'
# (With an address, what the name server calls to everyone reaches a
# socket: the same call the machine without an address did not hear.)
type_line 'netcalls hears'
wait_for '^hears: a datagram of 7 bytes from 10.10.36.2$'
echo "ok   on a network of our own: a name is looked up and pinged with the first packets to the name server and through the gateway, also written with the root's dot and when the first question is lost; a name there is none of is said, and a name server that cannot look names up; a datagram to everyone is heard"
# Replies that come a second and a half after their requests: each is
# said late with its own request's number and time, none is lost and
# none is taken for the next request's. (Three requests, and a time of
# one to three seconds: on a loaded host without KVM a reply may be half
# a second later still. With two requests the first reply had to come
# within the half second before the count's end for the others to be
# waited for, which is where this step failed under load.)
type_line 'ping -c 3 10.10.36.20; echo late $?'
wait_for '^no answer for seq=1$'
wait_for '^64 bytes from 10.10.36.20: seq=1 time=[12][0-9]* ms (late)$'
wait_for '^no answer for seq=2$'
wait_for '^64 bytes from 10.10.36.20: seq=2 time=[12][0-9]* ms (late)$'
wait_for '^no answer for seq=3$'
wait_for '^64 bytes from 10.10.36.20: seq=3 time=[12][0-9]* ms (late)$'
wait_for '^--- 3 sent, 3 received, 0% lost$'
wait_for '^late 0$'
# A reply that comes twice is shown, and counted once.
type_line 'ping -c 2 10.10.36.21; echo twice $?'
wait_for '^64 bytes from 10.10.36.21: seq=1 time=[0-9.]* ms$'
wait_for '^64 bytes from 10.10.36.21: seq=1 time=[0-9.]* ms (again)$'
wait_for '^--- 2 sent, 2 received, 0% lost$'
wait_for '^twice 0$'
# A reply from another address than the one asked is shown as that.
type_line 'ping -c 1 10.10.36.23; echo other $?'
wait_for '^64 bytes from 10.10.36.24: seq=1 time=[0-9.]* ms (not the host asked)$'
wait_for '^other 0$'
# A neighbour that takes 0.3 s to say its hardware address, pinged by
# two programs at once: the first request of each reaches it all the
# same (both wait in the kernel for the one answer).
type_line 'ping -c 1 10.10.36.22 > /temporary/other & ping -c 1 10.10.36.22; slow=$?; wait; cat /temporary/other; echo slow-neighbour $slow'
wait_for '^64 bytes from 10.10.36.22: seq=1 time=[0-9.]* ms$' 2
wait_for '^slow-neighbour 0$'
echo "ok   replies later than a second are told by their own request, a reply that comes twice counts once, one from another address is said to be, a neighbour slow to say its hardware address gets the first request of two programs that ping it at once"
# The gateway's word that a host cannot be reached, in place of "no
# answer"; and a host nobody answers for.
type_line 'ping -c 1 192.0.2.9; echo unreachable $?'
wait_for '^from 10.10.36.1: seq=1 was not delivered: the host cannot be reached$'
wait_for '^--- 1 sent, 0 received, 100% lost$'
wait_for '^unreachable 1$'
# The same word of a host whose reply comes all the same, a fifth of a
# second later: the reply counts, and is not called late (which is said
# of a reply that comes after "no answer").
type_line 'ping -c 2 192.0.2.12; echo yet-there $?'
wait_for '^from 10.10.36.1: seq=2 was not delivered: the host cannot be reached$'
wait_for '^64 bytes from 192.0.2.12: seq=1 time=[0-9.]* ms$'
wait_for '^--- 2 sent, 1 received, 50% lost$'
wait_for '^yet-there 1$'
# (The kernel kept the request while it asked for the hardware address,
# and said itself that nobody answered: ping says whose it was, for each
# request within its second.)
type_line 'ping -c 2 10.10.36.9; echo absent $?'
wait_for '^no answer from 10.10.36.9 (its hardware address is not known: nobody answered for it)$' 2
wait_for '^--- 2 sent, 0 received, 100% lost$'
wait_for '^absent 1$'
# (The three bare ones on this serial line are the late host's, above.)
(( $(output | grep -c '^no answer for seq=') == 3 )) || fail "a request nobody's hardware address was said for got a bare \"no answer\""
echo "ok   a router's word that a host cannot be reached is said; for an address nobody has ping says, request by request, that nobody answered for its hardware address"
# The subnet's address for everyone, 10.10.36.255. An echo request there
# goes to everyone's hardware address, and this network's gateway
# answers it (from its own address); a datagram there goes the same way;
# nobody is asked for a hardware address. And what the name server calls
# to that address (UDP port 4716) reaches a socket, as its call to
# 255.255.255.255 does. (Such a datagram was not taken, and what was
# sent there never went: the kernel asked who has 10.10.36.255.)
type_line 'ping -c 1 10.10.36.255; echo subnet-everyone $?'
wait_for '^64 bytes from 10.10.36.1: seq=1 time=[0-9.]* ms (not the host asked)$'
wait_for '^subnet-everyone 0$'
type_line 'netcalls everyone 10.10.36.255; netcalls hears 4716'
wait_for '^everyone: datagram taken 5$'
wait_for '^hears: a datagram of 7 bytes from 10.10.36.2$' 2
# (The datagram, and one to 255.255.255.255, as the network saw them
# come: at everyone's hardware address. "taken" says only that the
# kernel took it.)
type_line 'netcalls everyone 255.255.255.255'
wait_for '^everyone: datagram taken 5$' 2
for to in 10.10.36.255 255.255.255.255; do
    came=$(lan_said "^a datagram for port 4717 to $to came to ")
    [[ $came == "a datagram for port 4717 to $to came to everyone's hardware address" ]] || fail "a datagram sent to $to: $came"
done
if grep -q 'which is everyone.s$' "$work/lan.txt"; then fail "the kernel asked for a hardware address of everyone's: $(grep -c 'which is everyone.s$' "$work/lan.txt") times"; fi
if grep -q "came to one host's hardware address" "$work/lan.txt"; then fail "an echo request to everyone went to one host's hardware address"; fi
echo "ok   the subnet's address for everyone: an echo request and a datagram go to everyone's hardware address (the gateway answers the echo), nobody is asked for one, and a datagram sent to it is heard"
# A stream is a connection to one host: a connect to an address that is
# everyone's is refused as invalid (status 10) and nothing of it goes
# onto the wire (RFC 1122, 4.2.3.10). (It was taken, 0, and its SYN went
# to everyone's hardware address, again and again for over a minute.
# The wire is looked at below, when the network has had its time.)
type_line 'netcalls nowhere 10.10.36.255; netcalls nowhere 255.255.255.255; echo no-stream-to-everyone'
wait_for '^no-stream-to-everyone$'
(( $(output | grep -c '^nowhere: connect 10$') == 2 )) || fail "a stream's connect to an address that is everyone's is not refused as invalid: $(output | grep '^nowhere: connect ' | tr '\n' ';')"
echo "ok   a stream's connect to an address that is everyone's (the subnet's, 255.255.255.255) is refused as invalid"
# A name whose IPv6 address the name server does not answer for (it
# drops such questions): brixOS speaks no IPv6, so that question is not
# asked, of this name or of any name above. (It was asked after each
# address found, and here the lookup waited five seconds for the answer
# that does not come.)
type_line 'ping -c 1 nosix.lan; echo nosix $?'
wait_within '^PING nosix.lan (10.10.36.1): 56 bytes of data$' 4 "the lookup of a name whose IPv6 question the name server drops"
wait_for '^nosix 0$'
if grep -q 'asked for the IPv6 address' "$work/lan.txt"; then fail "the name server was asked for IPv6 addresses, which brixOS does not speak: $(grep 'asked for the IPv6 address' "$work/lan.txt" | sort | uniq -c | tr '\n' ';')"; fi
echo "ok   a name is looked up without the question for its IPv6 address: a name server that drops such questions delays nothing"
# A listener (logserve, behind the shell: this boot's last line) and
# what is asked of it in raw frames. A connection asked of everyone
# (a SYN to 255.255.255.255, one to 10.10.36.255) is not answered (RFC
# 1122, 4.2.3.10; the listener answered the first). Then connections
# that are begun and never finished: 4 of them, and a real request (a
# page asked for as a browser does) is answered within two seconds; a
# hundred more, and so is the next. (4 were all the listener held, for
# 71 seconds: the request got no answer.)
type_line 'logserve &'
wait_for '^  Ctrl+C ends it$'
tell_lan everyone 8080
[[ "$(lan_said '^everyone 8080: ')" == "everyone 8080: no answer" ]] || fail "a connection asked of everyone was answered: $(lan_said '^everyone 8080: ')"
# And what names everyone, or a group, as where it comes from is from
# nobody (RFC 1122, 3.2.1.3): an echo request and a connection asked so
# (from 10.10.36.255, 255.255.255.255, 224.0.0.1) get no answer. (Each
# was answered, and the answers to the first two went to every host on
# the wire, to everyone's hardware address.)
tell_lan forged 8080
[[ "$(lan_said '^forged 8080: ')" == "forged 8080: no answer" ]] || fail "what named everyone or a group as where it came from was answered: $(lan_said '^forged 8080: ')"
answered_within() {
    local said
    said=$(lan_said "^get $1: " 45)
    [[ $said =~ ^get\ $1:\ HTTP/1\.1\ 200\ OK,\ [0-9]+\ bytes,\ in\ ([0-9.]+)\ s$ ]] || fail "behind $2 connections begun and never finished, a request: $said"
    python3 -c "import sys; sys.exit(0 if float(sys.argv[1]) < 2.0 else 1)" "${BASH_REMATCH[1]}" || fail "behind $2 connections begun and never finished, a request took ${BASH_REMATCH[1]} s"
    echo "${BASH_REMATCH[1]}"
}
tell_lan silent 8080 4
lan_said '^silent: 4 ' > /dev/null
sleep 1
tell_lan get 8080 /system 30
few=$(answered_within /system 4)
tell_lan silent 8080 100
lan_said '^silent: 100 ' > /dev/null
sleep 1
tell_lan get 8080 /klog 30
many=$(answered_within /klog 104)
echo "ok   a listener: a connection asked of everyone is not answered, nor what names everyone or a group as where it comes from; behind 4 connections that were begun and never finished a real request is answered in $few s, behind 100 more in $many s"
# (And no segment of a stream came to everyone's hardware address: the
# connects to everyone's addresses above put nothing on the wire.)
if grep -q "^a stream's segment to " "$work/lan.txt"; then fail "a stream's segment went to everyone's hardware address: $(grep "^a stream's segment to " "$work/lan.txt" | sort | uniq -c | tr '\n' ';')"; fi
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
