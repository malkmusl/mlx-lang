# ping as a person types it, on QEMU's user network (the guest is
# 10.0.2.15, the gateway 10.0.2.2, the name server 10.0.2.3; nobody else
# has an address): the cases tools/check_brix_net.sh and
# tools/check_brix_net_igb.sh both go through, each on its card. Sourced
# by them: it uses their type_line, wait_for, output, fail and $work.
# QEMU captures the network's frames in $work/frames.pcap (-object
# filter-dump), and the image's hosts file names 10.0.2.2 gateway.test.
# The cases come straight after the boot: nothing has been sent to the
# name server yet.

# The addresses the guest's echo requests went to, by the frames QEMU
# captured on its network (sorted, a blank between).
echo_destinations() {
    python3 - "$work/frames.pcap" <<'PY'
import struct, sys
data = open(sys.argv[1], "rb").read()
at = 24
seen = set()
while at + 16 <= len(data):
    size = struct.unpack("<I", data[at + 8:at + 12])[0]
    frame = data[at + 16:at + 16 + size]
    at += 16 + size
    if len(frame) >= 42 and frame[12:14] == b"\x08\x00" and frame[23] == 1:
        header = (frame[14] & 15) * 4
        if len(frame) > 14 + header and frame[14 + header] == 8:
            seen.add(".".join(str(part) for part in frame[30:34]))
print(" ".join(sorted(seen)))
PY
}

ping_cases() {
    local went
    # The name server first: a neighbour whose hardware address the
    # kernel has not asked for. The first request is answered all the
    # same (ping sends it again while the kernel asks, or the kernel
    # holds it), and the summary has the loss and the times.
    type_line 'ping 10.0.2.3 2; echo neighbour $?'
    wait_for '^PING 10.0.2.3: 56 bytes of data$'
    wait_for '^64 bytes from 10.0.2.3: seq=1 time=[0-9.]* ms$'
    wait_for '^64 bytes from 10.0.2.3: seq=2 time=[0-9.]* ms$'
    wait_for '^--- 2 sent, 2 received, 0% lost$'
    wait_for '^round trip: least [0-9.]* ms, mean [0-9.]* ms, greatest [0-9.]* ms$'
    wait_for '^neighbour 0$'
    type_line 'ping 10.0.2.2 2; echo ping $?'
    wait_for '^64 bytes from 10.0.2.2: seq=1 time=[0-9.]* ms$'
    wait_for '^64 bytes from 10.0.2.2: seq=2 time=[0-9.]* ms$'
    wait_for '^--- 2 sent, 2 received, 0% lost$' 2
    wait_for '^ping 0$'
    # Where the requests went: to the hosts that were pinged. (They all
    # went to 0.0.0.0 through the gateway once: the kernel took a ping
    # socket's destination by its port, which it does not have. QEMU's
    # host answers for 0.0.0.0, a real network does not.)
    went=$(echo_destinations)
    [[ $went == "10.0.2.2 10.0.2.3" ]] || fail "the echo requests to 10.0.2.2 and 10.0.2.3 went to: $went"
    echo "ok   ping: straight after the boot a neighbour not spoken to yet answers from the first request on; the gateway answers; the requests go to the host pinged; the summary has the loss and the least, mean and greatest time"
    # An address of the subnet nobody has: no answer, said for each
    # request (whose hardware address nobody answered for, where the
    # kernel does not send before it knows it), all lost, status 1.
    type_line 'ping -c 2 10.0.2.9; echo nobody $?'
    wait_for '^no answer \(from 10.0.2.9 (its hardware address is not known: nobody answered for it)\|for seq=[12]\)$' 2
    wait_for '^--- 2 sent, 0 received, 100% lost$'
    wait_for '^nobody 1$'
    echo "ok   ping to an address nobody has: no answer for each request, all lost"
    # The count before the host, after it, joined to its option; none at
    # all (the usage), none that is one, a host that is no address.
    type_line 'ping -c 1 10.0.2.2; echo count-before $?'
    wait_for '^--- 1 sent, 1 received, 0% lost$'
    wait_for '^count-before 0$'
    type_line 'ping 10.0.2.2 -c 1; echo count-after $?'
    wait_for '^--- 1 sent, 1 received, 0% lost$' 2
    wait_for '^count-after 0$'
    type_line 'ping -c1 10.0.2.2; echo count-joined $?'
    wait_for '^--- 1 sent, 1 received, 0% lost$' 3
    wait_for '^count-joined 0$'
    type_line 'ping; echo no-host $?'
    wait_for '^usage: ping \[-c COUNT\] HOST$'
    wait_for '^no-host 1$'
    type_line 'ping -c 0 10.0.2.2; echo no-count $?'
    wait_for '^ping: the count is a number from 1 on$'
    wait_for '^no-count 1$'
    type_line 'ping 10.0.2.300; echo no-address $?'
    wait_for '^ping: 10.0.2.300 is no address (four numbers up to 255 with dots between)$'
    wait_for '^no-address 1$'
    echo "ok   ping takes -c COUNT before and after the host and as -cCOUNT; the usage without a host; a count of 0 and a host that is no address are said"
    # The machine itself, by its address and as 127.0.0.1 and localhost:
    # answered (each reply once), and nothing of it on the wire.
    type_line 'ping -c 1 10.0.2.15; ping -c 1 127.0.0.1; ping -c 1 localhost; echo itself $?'
    wait_for '^64 bytes from 10.0.2.15: seq=1 time=[0-9.]* ms$' 3
    wait_for '^PING localhost (127.0.0.1): 56 bytes of data$'
    wait_for '^--- 1 sent, 1 received, 0% lost$' 6
    wait_for '^itself 0$'
    if output | grep -q '(again)$'; then fail "the machine's reply to itself came twice"; fi
    went=$(echo_destinations)
    [[ $went == "10.0.2.2 10.0.2.3" ]] || fail "echo requests to the machine itself went onto the wire: $went"
    # A name (the hosts file's).
    type_line 'ping -c 1 gateway.test; echo named $?'
    wait_for '^PING gateway.test (10.0.2.2): 56 bytes of data$'
    wait_for '^named 0$'
    echo "ok   ping to the machine's own address, 127.0.0.1 and localhost is answered once and stays off the wire; a name is looked up"
    # resolv.conf is still there once a program has read and closed it
    # (the kernel made it with no link counted: the first close freed it,
    # and no name was looked up after that).
    type_line 'cat /system/settings/resolv.conf; echo again $?'
    wait_for '^nameserver 10.0.2.3$' 2
    wait_for '^again 0$'
    echo "ok   resolv.conf is still there when it is read a second time"
    # Two at once: each gets its own three replies. Then forty in a row,
    # more than the kernel has sockets: each gets one and gives it back.
    type_line 'ping -c 3 10.0.2.2 > /temporary/first & ping -c 3 10.0.2.3 > /temporary/second; wait; cat /temporary/first /temporary/second; echo together $?'
    wait_for '^64 bytes from 10.0.2.2: seq=3 time=[0-9.]* ms$'
    wait_for '^64 bytes from 10.0.2.3: seq=3 time=[0-9.]* ms$'
    wait_for '^--- 3 sent, 3 received, 0% lost$' 2
    wait_for '^together 0$'
    if output | grep -q '(again)$'; then fail "one ping got the other's replies"; fi
    type_line 'n=0; while [ $n -lt 40 ]; do ping -c 1 10.0.2.2 > /temporary/one || echo failed-at $n; n=$((n+1)); done; echo in-a-row $n'
    wait_for '^in-a-row 40$'
    if output | grep -q '^failed-at '; then fail "one of forty pings in a row was not answered"; fi
    echo "ok   two pings at once get each its own replies; forty in a row are all answered"
    # Left running and ended with Ctrl+C: the summary still comes, and
    # the shell goes on. (A request sent in the moment of the Ctrl+C
    # may have no reply yet: the status says so.)
    type_line 'ping 10.0.2.2; echo interrupted $?'
    wait_for '^64 bytes from 10.0.2.2: seq=4 time=[0-9.]* ms$'
    printf '\003' >&3
    wait_for '^--- [4-9] sent, [3-9] received, [0-9]*% lost$'
    wait_for '^interrupted [01]$'
    echo "ok   ping without a count goes on until Ctrl+C, which ends it with the summary"
}
