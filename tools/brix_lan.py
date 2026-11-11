#!/usr/bin/env python3
"""A network of its own for tools/check_brix_net.sh, at the other end of
QEMU's `-netdev socket,udp=`: every Ethernet frame the guest sends comes
here as a datagram, and what is sent back is what the guest's card
receives. It plays what QEMU's user network cannot: a host whose replies
come late, one that answers twice, one whose replies come from another
address, a neighbour slow to say its hardware address, a router that
says a host cannot be reached (also of one that answers all the same),
a name server with names of its own, which is deaf once, for some names
says that it cannot look names up and for one drops the question for
its IPv6 address, and hosts that open TCP connections to the guest from
raw frames: one that never finishes a handshake, one that asks for a
page as a browser would, and one that asks everyone for a connection.

The network is 10.10.36.0/24; the guest has an address of it from its
settings file. Nobody here gives addresses (DHCP's questions get no
answer): a guest without a settings file stays without an address, which
the check uses for what such a machine puts on the wire.

  10.10.36.1    the gateway: answers echo; routes to 192.0.2.0/24:
                  192.0.2.10 answers echo
                  192.0.2.9  the gateway says: host unreachable
                  192.0.2.11 the gateway says: time exceeded
                  192.0.2.12 the gateway says: host unreachable, and
                             0.2 s later the host's reply comes
                  (the rest of it: silence)
  10.10.36.2    the name server (answers echo too): gateway.lan,
                slow.lan, far.lan (192.0.2.10) have addresses; quiet.lan
                gets no answer at all; second.lan (the gateway) gets
                none the first time its address is asked for, and one
                from then on; of broken.lan the server says that it
                cannot look it up (a server failure); any other name
                does not exist; nosix.lan has an address (the
                gateway's), and the question for its IPv6 address gets
                no answer at all; every question for an IPv6 address is
                a line of the output; it calls everyone twice a second
                (UDP port 4715 to 255.255.255.255, port 4716 to the
                subnet's 10.10.36.255)
  10.10.36.20   answers echo 1.5 s late
  10.10.36.21   answers every echo twice
  10.10.36.22   says its hardware address only 0.3 s after it is asked
  10.10.36.23   its echo replies come from 10.10.36.24
  10.10.36.40   begins connections to the guest and never finishes one
  10.10.36.41   asks the guest for a page (HTTP/1.0), as a client does:
                its SYN again after 1, 2, 4, ... seconds without an answer
  10.10.36.42   asks everyone for a connection (a SYN to 255.255.255.255
                and one to 10.10.36.255, which no host may answer)
  10.10.36.255, 255.255.255.255
                everyone: an echo request sent there to everyone's
                hardware address is answered by the gateway; nobody says
                a hardware address for either (a question for one is a
                line of the output, and so is a TCP segment that comes
                to everyone's hardware address, and a datagram for UDP
                port 4717 with the hardware address it came to)
  anything else: nobody (no hardware address, no answer)

    tools/brix_lan.py LISTEN_PORT QEMU_PORT [--silent-gateway]
    tools/brix_lan.py --tell LISTEN_PORT WORDS...

(QEMU: -netdev socket,id=n0,udp=127.0.0.1:LISTEN_PORT,localaddr=127.0.0.1:QEMU_PORT.)
With --silent-gateway the gateway does not say its hardware address.

--tell hands the running network a line of what to do (a datagram to
its port); what comes of it is a line of the network's output:

  silent PORT COUNT    COUNT connections to the guest's PORT begun from
                       10.10.36.40 and left: "silent: COUNT connections
                       begun on port PORT"
  get PORT PATH [SECONDS]
                       the page asked for from 10.10.36.41, waited for
                       SECONDS (100): "get PATH: STATUS LINE, N bytes, in
                       T s" (T from the first SYN to the connection's
                       end; "cut short after N bytes" when the answer is
                       shorter than its head says), or "get PATH: no
                       answer in T s"
  everyone PORT        the two SYNs from 10.10.36.42; two seconds later
                       "everyone PORT: no answer", or "everyone PORT: the
                       guest answered (N segments)"
  forged PORT          an echo request and a SYN for PORT that name as
                       where they come from 10.10.36.255, then
                       255.255.255.255, then 224.0.0.1 (nobody speaks
                       from an address that is everyone's or a group's);
                       two seconds later "forged PORT: no answer", or
                       "forged PORT: the guest answered (N frames)"
"""
import heapq
import select
import socket
import struct
import sys
import time

GATEWAY = bytes([10, 10, 36, 1])
NAMESERVER = bytes([10, 10, 36, 2])
SLOW = bytes([10, 10, 36, 20])
TWICE = bytes([10, 10, 36, 21])
SLOW_NEIGHBOUR = bytes([10, 10, 36, 22])
TWO_ADDRESSES = bytes([10, 10, 36, 23])
ITS_OTHER = bytes([10, 10, 36, 24])
HOSTS = (GATEWAY, NAMESERVER, SLOW, TWICE, SLOW_NEIGHBOUR, TWO_ADDRESSES)
GUEST = bytes([10, 10, 36, 50])
SILENT = bytes([10, 10, 36, 40])
CALLER = bytes([10, 10, 36, 41])
STRANGER = bytes([10, 10, 36, 42])
# The hosts that speak TCP to the guest: they say their hardware address
# and answer nothing else.
CLIENTS = (SILENT, CALLER, STRANGER)
SUBNET_EVERYONE = bytes([10, 10, 36, 255])
EVERYONE = bytes([255] * 4)
EVERYONES_MAC = b"\xff" * 6
# Addresses nobody speaks from: everyone's two, and a group's.
NOBODYS = (SUBNET_EVERYONE, EVERYONE, bytes([224, 0, 0, 1]))
FAR = bytes([192, 0, 2, 10])
UNREACHABLE = bytes([192, 0, 2, 9])
TOO_FAR = bytes([192, 0, 2, 11])
REFUSED_YET_THERE = bytes([192, 0, 2, 12])
NAMES = {b"gateway.lan": GATEWAY, b"slow.lan": SLOW, b"far.lan": FAR, b"second.lan": GATEWAY, b"nosix.lan": GATEWAY}
QUIET_NAME = b"quiet.lan"
DEAF_ONCE_NAME = b"second.lan"
BROKEN_NAME = b"broken.lan"
NO_SIX_NAME = b"nosix.lan"
FIN, SYN, RST, PSH, ACK = 1, 2, 4, 8, 16


def mac_of(ip):
    return bytes([2, 0]) + ip


def checksum(data):
    if len(data) % 2:
        data += b"\0"
    total = sum(struct.unpack(">%dH" % (len(data) // 2), data))
    while total >> 16:
        total = (total & 0xFFFF) + (total >> 16)
    return (~total) & 0xFFFF


def ip_packet(source, destination, protocol, payload):
    header = struct.pack(">BBHHHBBH4s4s", 0x45, 0, 20 + len(payload), 0, 0x4000, 64, protocol, 0, source, destination)
    header = header[:10] + struct.pack(">H", checksum(header)) + header[12:]
    return header + payload


def icmp(kind, code, rest, data):
    message = struct.pack(">BBH", kind, code, 0) + rest + data
    return message[:2] + struct.pack(">H", checksum(message)) + message[4:]


def tcp_segment(source, source_port, destination, port, seq, ack, flags, data=b""):
    # (A SYN says the largest segment it takes.)
    options = struct.pack(">BBH", 2, 4, 1460) if flags & SYN else b""
    header = struct.pack(">HHIIBBHHH", source_port, port, seq & 0xFFFFFFFF, ack & 0xFFFFFFFF, ((20 + len(options)) // 4) << 4, flags, 65535, 0, 0) + options
    pseudo = source + destination + struct.pack(">BBH", 0, 6, len(header) + len(data))
    header = header[:16] + struct.pack(">H", checksum(pseudo + header + data)) + header[18:]
    return ip_packet(source, destination, 6, header + data)


def say(text):
    print(text, flush=True)


class Call:
    """10.10.36.41's connection to the guest: a page asked for (HTTP/1.0),
    read to the connection's end."""

    def __init__(self, source_port, port, path, patience, now):
        self.source_port, self.port, self.path, self.patience = source_port, port, path, patience
        self.started = now
        self.iss = 0x10000000 + source_port * 4096
        self.request = b"GET " + path.encode() + b" HTTP/1.0\r\nHost: brix\r\n\r\n"
        self.connected = False
        self.over = False
        self.theirs = 0
        self.acked = 0
        self.body = b""
        # When what was sent last and not answered goes again, and the
        # pause after that (it doubles).
        self.again_at = now + 1.0
        self.pause = 2.0

    def segment(self, seq, ack, flags, data=b""):
        return tcp_segment(CALLER, self.source_port, GUEST, self.port, seq, ack, flags, data)

    def first(self):
        return self.segment(self.iss, 0, SYN)

    def end(self, now, why=None):
        self.over = True
        took = "%.2f" % (now - self.started)
        if why is not None:
            say("get %s: %s in %s s" % (self.path, why, took))
            return
        status = self.body.split(b"\r\n", 1)[0].decode("latin-1")
        # (Whole: as many bytes after the head as the head says.)
        head, _, rest = self.body.partition(b"\r\n\r\n")
        said = [line.split(b":", 1)[1] for line in head.split(b"\r\n") if line.lower().startswith(b"content-length:")]
        if said and len(rest) != int(said[0]):
            say("get %s: %s, cut short after %d bytes, in %s s" % (self.path, status, len(self.body), took))
            return
        say("get %s: %s, %d bytes, in %s s" % (self.path, status, len(self.body), took))

    # The clock: the frames to send now (the SYN or the request once
    # more), or the end of the patience.
    def tick(self, now):
        if self.over:
            return []
        if now - self.started >= self.patience:
            self.end(now, "no answer")
            return []
        if now < self.again_at:
            return []
        self.again_at = now + self.pause
        self.pause *= 2
        if not self.connected:
            return [self.first()]
        if self.acked < len(self.request):
            return [self.segment(self.iss + 1, self.theirs, PSH | ACK, self.request)]
        return []

    # A segment from the guest: the frames that answer it.
    def take(self, seq, ack, flags, data, now):
        if self.over:
            return []
        if flags & RST:
            self.end(now, "the guest reset the connection")
            return []
        if not self.connected:
            if flags & (SYN | ACK) != SYN | ACK or ack != (self.iss + 1) & 0xFFFFFFFF:
                return []
            self.connected = True
            self.theirs = (seq + 1) & 0xFFFFFFFF
            self.again_at = now + 1.0
            self.pause = 2.0
            # The handshake's end and the request in one segment.
            return [self.segment(self.iss + 1, self.theirs, PSH | ACK, self.request)]
        if flags & ACK:
            self.acked = max(self.acked, (ack - self.iss - 1) & 0xFFFFFFFF)
        if flags & SYN:
            # (The guest's SYN+ACK once more: our answer was lost.)
            return [self.segment(self.iss + 1, self.theirs, PSH | ACK, self.request)]
        mine = self.iss + 1 + len(self.request)
        if data and seq == self.theirs:
            self.body += data
            self.theirs = (self.theirs + len(data)) & 0xFFFFFFFF
        if flags & FIN and (seq + len(data)) & 0xFFFFFFFF == self.theirs:
            # The guest's end: ours with the acknowledgement.
            self.theirs = (self.theirs + 1) & 0xFFFFFFFF
            self.end(now)
            return [self.segment(mine, self.theirs, FIN | ACK)]
        return [self.segment(mine, self.theirs, ACK)] if data or flags & FIN else []


def tell(port, words):
    socket.socket(socket.AF_INET, socket.SOCK_DGRAM).sendto(" ".join(words).encode(), ("127.0.0.1", port))


def main():
    if sys.argv[1] == "--tell":
        tell(int(sys.argv[2]), sys.argv[3:])
        return
    listen_port, qemu_port = int(sys.argv[1]), int(sys.argv[2])
    silent_gateway = "--silent-gateway" in sys.argv[3:]
    wire = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    wire.bind(("127.0.0.1", listen_port))
    later = []
    order = 0
    heard_once = False
    # The guest's hardware address (QEMU's own until a frame says it).
    guest_mac = bytes([0x52, 0x54, 0x00, 0x12, 0x34, 0x56])
    # The connections 10.10.36.41 has to the guest, by their port here;
    # the ports the next connections come from; how many segments the
    # guest sent 10.10.36.42, and when that is said.
    calls = {}
    silent_port = 20000
    call_port = 40000
    stranger_heard = 0
    stranger_says = []
    # The answers the guest gave to what named everyone, or a group, as
    # where it came from, until when they are counted, and when that is
    # said.
    forged_heard = 0
    forged_until = 0.0
    forged_says = []

    def send(frame, delay=0.0):
        nonlocal order
        order += 1
        heapq.heappush(later, (time.monotonic() + delay, order, frame))

    def to_guest(guest_mac, speaker, packet, delay=0.0):
        # (What is beyond the gateway comes through the gateway's card.)
        via = speaker if speaker in HOSTS + CLIENTS else GATEWAY
        send(guest_mac + mac_of(via) + b"\x08\x00" + packet, delay)

    # The name server calls everyone twice a second (a datagram to UDP
    # port 4715): a guest with an address hears it at a socket on that
    # port (netcalls hears), one without an address must not.
    # The same to the subnet's own address for everyone (port 4716).
    call = EVERYONES_MAC + mac_of(NAMESERVER) + b"\x08\x00" + ip_packet(NAMESERVER, EVERYONE, 17, struct.pack(">HHHH", 4715, 4715, 8 + 7, 0) + b"anyone?")
    subnet_call = EVERYONES_MAC + mac_of(NAMESERVER) + b"\x08\x00" + ip_packet(NAMESERVER, SUBNET_EVERYONE, 17, struct.pack(">HHHH", 4716, 4716, 8 + 7, 0) + b"anyone?")
    next_call = time.monotonic()

    # A line of what to do (--tell).
    def told(words, now):
        nonlocal silent_port, call_port, stranger_heard, forged_heard, forged_until
        if words[:1] == ["silent"] and len(words) == 3:
            port, count = int(words[1]), int(words[2])
            for k in range(count):
                send(guest_mac + mac_of(SILENT) + b"\x08\x00" + tcp_segment(SILENT, silent_port, GUEST, port, 0x20000000 + silent_port, 0, SYN), k * 0.002)
                silent_port = silent_port + 1 if silent_port < 39999 else 20000
            say("silent: %d connections begun on port %d" % (count, port))
        elif words[:1] == ["get"] and len(words) in (3, 4):
            made = Call(call_port, int(words[1]), words[2], float(words[3]) if len(words) == 4 else 100.0, now)
            calls[call_port] = made
            call_port += 1
            send(guest_mac + mac_of(CALLER) + b"\x08\x00" + made.first())
        elif words[:1] == ["everyone"] and len(words) == 2:
            port = int(words[1])
            stranger_heard = 0
            send(EVERYONES_MAC + mac_of(STRANGER) + b"\x08\x00" + tcp_segment(STRANGER, 50000, EVERYONE, port, 0x30000000, 0, SYN))
            send(EVERYONES_MAC + mac_of(STRANGER) + b"\x08\x00" + tcp_segment(STRANGER, 50001, SUBNET_EVERYONE, port, 0x30000000, 0, SYN), 0.2)
            stranger_says.append((now + 2.0, port))
        elif words[:1] == ["forged"] and len(words) == 2:
            port = int(words[1])
            forged_heard = 0
            forged_until = now + 2.0
            for k, source in enumerate(NOBODYS):
                echo = icmp(8, 0, struct.pack(">HH", 0x4242, k + 1), b"who answers everyone?")
                send(guest_mac + mac_of(STRANGER) + b"\x08\x00" + ip_packet(source, GUEST, 1, echo), k * 0.1)
                send(guest_mac + mac_of(STRANGER) + b"\x08\x00" + tcp_segment(source, 50010 + k, GUEST, port, 0x30000000, 0, SYN), k * 0.1 + 0.05)
            forged_says.append((forged_until, port))
        else:
            say("told what this network does not do: " + " ".join(words))

    while True:
        now = time.monotonic()
        if now >= next_call:
            wire.sendto(call, ("127.0.0.1", qemu_port))
            wire.sendto(subnet_call, ("127.0.0.1", qemu_port))
            next_call = now + 0.5
        for made in list(calls.values()):
            for packet in made.tick(now):
                send(guest_mac + mac_of(CALLER) + b"\x08\x00" + packet)
            if made.over:
                del calls[made.source_port]
        while stranger_says and stranger_says[0][0] <= now:
            port = stranger_says.pop(0)[1]
            say("everyone %d: %s" % (port, "the guest answered (%d segments)" % stranger_heard if stranger_heard else "no answer"))
        while forged_says and forged_says[0][0] <= now:
            port = forged_says.pop(0)[1]
            say("forged %d: %s" % (port, "the guest answered (%d frames)" % forged_heard if forged_heard else "no answer"))
        while later and later[0][0] <= now:
            wire.sendto(heapq.heappop(later)[2], ("127.0.0.1", qemu_port))
        wait = min(max(0.0, later[0][0] - now) if later else 1.0, max(0.0, next_call - now), 0.05 if calls or stranger_says or forged_says else 1.0)
        if not select.select([wire], [], [], wait)[0]:
            continue
        frame, sender = wire.recvfrom(4096)
        if sender[1] != qemu_port:
            told(frame.decode().split(), time.monotonic())
            continue
        if len(frame) < 34:
            continue
        guest_mac = frame[6:12]
        kind = struct.unpack(">H", frame[12:14])[0]
        if kind == 0x0806:
            operation = struct.unpack(">H", frame[20:22])[0]
            asker_ip, asked = frame[28:32], frame[38:42]
            if operation == 1 and asked in (SUBNET_EVERYONE, EVERYONE):
                say("asked for the hardware address of %s, which is everyone's" % ".".join(str(part) for part in asked))
            if operation != 1 or asked not in HOSTS + CLIENTS or (asked == GATEWAY and silent_gateway):
                continue
            reply = guest_mac + mac_of(asked) + b"\x08\x06" + struct.pack(">HHBBH", 1, 0x0800, 6, 4, 2) + mac_of(asked) + asked + guest_mac + asker_ip
            send(reply + bytes(18), 0.3 if asked == SLOW_NEIGHBOUR else 0.0)
            continue
        if kind != 0x0800:
            continue
        packet = frame[14:]
        header = (packet[0] & 15) * 4
        total = struct.unpack(">H", packet[2:4])[0]
        protocol, source, destination = packet[9], packet[12:16], packet[16:20]
        payload = packet[header:total]
        if destination in NOBODYS and now < forged_until and (protocol == 6 or (protocol == 1 and payload[:1] == b"\0")):
            # (An answer to what named everyone, or a group, as where
            # it came from: it goes to every host on the wire, or to
            # the gateway for the group.)
            forged_heard += 1
            continue
        if protocol == 6 and frame[0:6] == EVERYONES_MAC:
            # (A stream is a connection to one host: no segment of one
            # is for everyone.)
            say("a stream's segment to %s came to everyone's hardware address" % ".".join(str(part) for part in destination))
            continue
        if protocol == 6 and len(payload) >= 20 and destination in CLIENTS:
            source_port, port, seq, ack, offset, flags = struct.unpack(">HHIIBB", payload[:14])
            if destination == STRANGER:
                stranger_heard += 1
            elif destination == CALLER and port in calls:
                for answer in calls[port].take(seq, ack, flags, payload[(offset >> 4) * 4:], time.monotonic()):
                    to_guest(guest_mac, CALLER, answer)
            # (10.10.36.40 hears and says nothing.)
            continue
        if protocol == 17 and len(payload) >= 8 and struct.unpack(">H", payload[2:4])[0] == 4717:
            # (netcalls everyone: where its datagram came.)
            say("a datagram for port 4717 to %s came to %s hardware address" % (".".join(str(part) for part in destination), "everyone's" if frame[0:6] == EVERYONES_MAC else "one host's"))
            continue
        if protocol == 1 and len(payload) >= 8 and payload[0] == 8:
            echo = ip_packet(destination, source, 1, icmp(0, 0, payload[4:8], payload[8:]))
            if destination in (SUBNET_EVERYONE, EVERYONE):
                # To everyone: answered by the gateway, from its own
                # address, when it came to everyone's hardware address.
                if frame[0:6] == EVERYONES_MAC:
                    to_guest(guest_mac, GATEWAY, ip_packet(GATEWAY, source, 1, icmp(0, 0, payload[4:8], payload[8:])))
                else:
                    say("an echo request to everyone came to one host's hardware address")
            elif destination in (GATEWAY, NAMESERVER, SLOW_NEIGHBOUR, FAR):
                to_guest(guest_mac, destination, echo)
            elif destination == SLOW:
                to_guest(guest_mac, destination, echo, 1.5)
            elif destination == TWICE:
                to_guest(guest_mac, destination, echo)
                to_guest(guest_mac, destination, echo, 0.05)
            elif destination == TWO_ADDRESSES:
                # The reply from the host's other address (through the
                # card that was asked).
                to_guest(guest_mac, destination, ip_packet(ITS_OTHER, source, 1, icmp(0, 0, payload[4:8], payload[8:])))
            elif destination in (UNREACHABLE, TOO_FAR, REFUSED_YET_THERE):
                # The router's word, with the IP header and the first 8
                # bytes of what it could not deliver.
                said = icmp(11, 0, bytes(4), packet[:header + 8]) if destination == TOO_FAR else icmp(3, 1, bytes(4), packet[:header + 8])
                to_guest(guest_mac, GATEWAY, ip_packet(GATEWAY, source, 1, said))
                if destination == REFUSED_YET_THERE:
                    to_guest(guest_mac, destination, echo, 0.2)
        elif protocol == 17 and destination == NAMESERVER and len(payload) >= 8 + 12 and struct.unpack(">H", payload[2:4])[0] == 53:
            source_port = struct.unpack(">H", payload[0:2])[0]
            query = payload[8:]
            labels, at = [], 12
            while at < len(query) and query[at] != 0:
                labels.append(query[at + 1:at + 1 + query[at]])
                at += 1 + query[at]
            name = b".".join(labels).lower()
            question = query[12:at + 5]
            asked_for = struct.unpack(">H", query[at + 1:at + 3])[0] if at + 5 <= len(query) else 0
            if asked_for == 28:
                say("name server: asked for the IPv6 address of %s" % name.decode("latin-1"))
            if name == QUIET_NAME or (name == NO_SIX_NAME and asked_for == 28):
                continue
            if name == DEAF_ONCE_NAME and asked_for == 1 and not heard_once:
                # (The first question for its address is lost; who asks
                # again is answered.)
                heard_once = True
                continue
            address = NAMES.get(name)
            if name == BROKEN_NAME:
                # A server failure (code 2): the server cannot look the
                # name up, which says nothing about the name.
                answer = query[:2] + struct.pack(">HHHHH", 0x8182, 1, 0, 0, 0) + question
            elif address is None:
                answer = query[:2] + struct.pack(">HHHHH", 0x8183, 1, 0, 0, 0) + question
            elif asked_for == 1:
                answer = query[:2] + struct.pack(">HHHHH", 0x8180, 1, 1, 0, 0) + question + struct.pack(">HHHIH", 0xC00C, 1, 1, 60, 4) + address
            else:
                answer = query[:2] + struct.pack(">HHHHH", 0x8180, 1, 0, 0, 0) + question
            datagram = struct.pack(">HHHH", 53, source_port, 8 + len(answer), 0) + answer
            to_guest(guest_mac, NAMESERVER, ip_packet(NAMESERVER, source, 17, datagram))


if __name__ == "__main__":
    main()
