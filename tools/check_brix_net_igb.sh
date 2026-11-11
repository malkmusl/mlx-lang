#!/usr/bin/env bash
# brixOS on the network over an Intel igb NIC (QEMU's `-device igb` on the
# Q35 machine, a stand-in for the Supermicro H11's on-board i210), as
# opposed to the virtio-net device the other net check uses. QEMU's user
# network (the guest is 10.0.2.15, the host 10.0.2.2, a name server at
# 10.0.2.3); the kernel's stack, Linux's socket calls for std.net, and
# wget fetching files from an HTTP server on the host; ping as a person
# types it (tools/brix_ping_cases.sh: the cases the virtio check goes
# through too, with QEMU's capture of the frames saying where the echo
# requests went); brixOS's own socket calls; the link's state as a
# program reads it from the kernel (/devices/net0's record: the cable
# and the speed from the device's status register, also once QEMU's
# monitor has pulled the cable, when ping says the link is down, and
# put it back: the address stays, its lease runs on). The device's
# frames come by its message-signalled interrupt (MSI); the timer tick
# looks at its ring as well. Then a boot with the cable out: no address,
# and when the cable comes in DHCP asks at once and the kernel says the
# address then.
#
#   tools/check_brix_net_igb.sh [compiler]
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
"$compiler" --quiet $brix projects/brix/tools/sysinfo/main.mlx -o "$work/sysinfo"
for program in cat wc echo sha256sum; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
# (A name of the hosts file for ping: QEMU's name server asks the host's
# resolver, which the machine this runs on may not have.)
printf '10.0.2.2 gateway.test\n' > "$work/hosts"
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "wget=$work/wget" "ping=$work/ping" "netcalls=$work/netcalls" "sysinfo=$work/sysinfo" "settings/hosts=$work/hosts" "$work/bin/"
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
qemu-system-x86_64 -machine q35 -m 256M -display none -monitor "unix:$work/monitor,server,nowait" -no-reboot -cpu max -serial stdio \
    -kernel "$work/brix.img" -netdev user,id=n0 -device igb,netdev=n0 -object "filter-dump,id=d0,netdev=n0,file=$work/frames.pcap" < "$work/input" > "$work/serial.txt" 2>&1 &
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
# Commands to QEMU's monitor.
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
wait_for '^brixOS: /devices/net0, [0-9a-f:]* (igb, message-signalled)$'
wait_for '^brixOS: net0 is 10.0.2.15, gateway 10.0.2.2, name server 10.0.2.3 (DHCP)$'
type_line 'cat /system/settings/resolv.conf'
wait_for '^nameserver 10.0.2.3$'
echo "ok   the igb network device is found, the stack configured by DHCP from QEMU's user network, resolv.conf written"
ping_cases
type_line 'netcalls; echo netcalls $?'
for expected in '^socket kind 9$' '^connect 0$' '^client writable (connected) 2$' '^listener readable (a connection waits) 1$' '^the peer is ourselves$' '^received 5$' '^hello$' '^read 5$' '^client sees the end (peer gone) 4$' '^receive at the end 0$' '^datagram received 5$' '^from port 4712$' '^echo answered 1$' '^an echo reply$' '^echo from 10.0.2.2$' '^netcalls 0$'; do
    wait_for "$expected"
done
echo "ok   the native socket calls over igb: a stream to ourselves, a datagram, an echo answered by the gateway it was sent to"
# The link's state, read natively from /devices/net0: the cable and the
# speed are the device's status register's.
for expected in '^link net0: igb 52:54:00:12:34:56 up$' '^speed 1000$' '^configured by DHCP$' '^address 10.0.2.15 mask 255.255.255.0$' '^gateway 10.0.2.2 name server 10.0.2.3$' '^frames received [1-9][0-9]* sent [1-9][0-9]*$' '^lease 86400 s, 86[0-9][0-9][0-9] s left$' '^no link net1: 6$' '^record: 72 bytes, its length word 72, its first 64 as they were, kind 5$'; do
    wait_for "$expected"
done
echo "ok   a program reads the igb link's state from /devices/net0: up at 1000 Mbit/s, the address by DHCP, gateway, name server, frames"
type_line "wget http://10.0.2.2:$port/hello.txt -O /temporary/hello.txt && cat /temporary/hello.txt"
wait_for '^wget: /temporary/hello.txt: 20 bytes$'
wait_for '^hello from the host$'
type_line "wget http://10.0.2.2:$port/large.txt -O /temporary/large.txt && sha256sum /temporary/large.txt"
wait_for "^$sum "
type_line "wget http://10.0.2.2:$port/missing.txt; echo status \$?"
wait_for '^wget: http://10.0.2.2:[0-9]*/missing.txt: the server answered 404$'
wait_for '^status 1$'
grep -q 'GET /hello.txt HTTP/1.1" 200' "$work/server.txt" || fail "the host's server did not see the request"
echo "ok   wget fetches a small and a large file over TCP from the host via igb, and reports a 404"
# The frames came by the device's interrupt (a message to the processor,
# MSI), not only by the timer tick's look at the ring.
type_line 'sysinfo'
wait_for '^device interrupts: [1-9][0-9]*$'
echo "ok   the device's message-signalled interrupt brings its frames"
# The cable pulled through QEMU's monitor (set_link): the device's status
# register says so, and the link's record with it.
monitor "set_link n0 off"
type_line 'netcalls links'
wait_for '^link net0: igb 52:54:00:12:34:56 no cable$'
echo "ok   with the cable pulled the igb link's record says no cable"
# ping says so ahead of its requests, which nobody answers.
type_line 'ping -c 1 10.0.2.2; echo unplugged $?'
wait_for '^ping: the network link is down (no cable, or it is still coming up)$'
wait_for '^no answer for seq=1 (the network link is down)$'
wait_for '^--- 1 sent, 0 received, 100% lost$'
wait_for '^unplugged 1$'
# (And of a neighbour not spoken to yet, whose request the kernel kept
# and gave up: that the link is down, not that nobody answered for the
# hardware address, which nobody could.)
type_line 'ping -c 1 10.0.2.9; echo unplugged-stranger $?'
wait_for '^no answer from 10.0.2.9 (the network link is down)$'
wait_for '^unplugged-stranger 1$'
echo "ok   with the cable pulled ping says the link is down, ahead and with the request that has no answer"
# The cable back in: the link is up again, and the address, whose lease
# runs on, is still the link's (nothing is said: nothing changed).
monitor "set_link n0 on"
dones=$(output | grep -c '^done$' || true)
for (( i = 0; i < 100; i++ )); do
    type_line 'netcalls links'
    dones=$((dones + 1))
    wait_for '^done$' "$dones"
    (( $(output | grep -c '^link net0: igb 52:54:00:12:34:56 up$') >= 2 )) && break
    sleep 0.3
done
wait_for '^link net0: igb 52:54:00:12:34:56 up$' 2
type_line 'ping -c 1 10.0.2.2; echo back $?'
wait_for '^back 0$'
(( $(output | grep -c '^brixOS: net0 is ') == 1 )) || fail "the kernel says the address again though nothing changed"
echo "ok   with the cable back the link is up, the address is kept and the gateway answers"
# The cable pulled while a ping runs: each request from then on says
# that the link is down (the link as it is when the request stays without
# an answer, not as it was at the start: these lines were bare "no
# answer", as from a host that is silent); back in, the replies come
# again, and Ctrl+C ends the run with what was lost.
type_line 'ping 10.0.2.2; echo during $?'
replies=$(output | grep -c '^64 bytes from 10.0.2.2: seq=[0-9]* time=[0-9.]* ms$' || true)
wait_for '^64 bytes from 10.0.2.2: seq=[0-9]* time=[0-9.]* ms$' $((replies + 2))
monitor "set_link n0 off"
wait_for '^no answer for seq=[0-9]* (the network link is down)$' 3
monitor "set_link n0 on"
replies=$(output | grep -c '^64 bytes from 10.0.2.2: seq=[0-9]* time=[0-9.]* ms$' || true)
wait_for '^64 bytes from 10.0.2.2: seq=[0-9]* time=[0-9.]* ms$' $((replies + 2))
printf '\003' >&3
wait_for '^during 1$'
echo "ok   the cable pulled while a ping runs: the requests without an answer say that the link is down, and the replies come again when it is back"
# The DHCP server's answer comes late: QEMU starts with the cable out
# (set_link off before the machine runs: no frame gets through), the
# boot says there is no address yet, and when the cable is in (the
# card's status register says so) the kernel asks at once and says the
# address then. (QEMU's igb says "link up" in its status register after
# the start whatever set_link said before it, though it passes no frame:
# the cable is pulled once more with the machine running, which the
# register does follow.)
exec 3>&-
kill "$qemu_pid" 2> /dev/null || true
wait "$qemu_pid" 2> /dev/null || true
: > "$work/serial.txt"
rm -f "$work/monitor"
qemu-system-x86_64 -machine q35 -m 256M -display none -monitor "unix:$work/monitor,server,nowait" -no-reboot -cpu max -serial stdio \
    -kernel "$work/brix.img" -S -netdev user,id=n0 -device igb,netdev=n0 < "$work/input" > "$work/serial.txt" 2>&1 &
qemu_pid=$!
exec 3> "$work/input"
prompts=0
monitor "set_link n0 off" "cont"
wait_for '^brixOS: net0 has no address yet (DHCP is asking)$'
wait_for '^root@brix:[^#]*# $'
monitor "set_link n0 off"
type_line 'netcalls links'
for expected in '^link net0: igb 52:54:00:12:34:56 no cable$' '^not configured (DHCP is asking)$' '^gateway none name server none$' '^done$'; do
    wait_for "$expected"
done
output | grep -q '^brixOS: net0 is ' && fail "the kernel says an address with the cable out"
monitor "set_link n0 on"
wait_for '^brixOS: net0 is 10.0.2.15, gateway 10.0.2.2, name server 10.0.2.3 (DHCP)$'
type_line 'netcalls links'
for expected in '^link net0: igb 52:54:00:12:34:56 up$' '^configured by DHCP$' '^address 10.0.2.15 mask 255.255.255.0$' '^gateway 10.0.2.2 name server 10.0.2.3$'; do
    wait_for "$expected"
done
type_line 'ping -c 1 10.0.2.2; echo late $?'
wait_for '^late 0$'
echo "ok   a DHCP answer that comes after the boot, on igb (the cable in late): the kernel says the address then, and the record and a ping follow"
echo PASS
