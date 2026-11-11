#!/usr/bin/env python3
"""A network of its own for tools/check_brix_net.sh, at the other end of
QEMU's `-netdev socket,udp=`: every Ethernet frame the guest sends comes
here as a datagram, and what is sent back is what the guest's card
receives. It plays what QEMU's user network cannot: a host whose replies
come late, one that answers twice, a neighbour slow to say its hardware
address, a router that says a host cannot be reached, and a name server
with names of its own.

The network is 10.10.36.0/24; the guest has an address of it from its
settings file.

  10.10.36.1    the gateway: answers echo; routes to 192.0.2.0/24:
                  192.0.2.10 answers echo
                  192.0.2.9  the gateway says: host unreachable
                  192.0.2.11 the gateway says: time exceeded
                  (the rest of it: silence)
  10.10.36.2    the name server (answers echo too): gateway.lan,
                slow.lan, far.lan (192.0.2.10) have addresses; quiet.lan
                gets no answer at all; any other name does not exist
  10.10.36.20   answers echo 1.5 s late
  10.10.36.21   answers every echo twice
  10.10.36.22   says its hardware address only 0.3 s after it is asked
  anything else: nobody (no hardware address, no answer)

    tools/brix_lan.py LISTEN_PORT QEMU_PORT [--silent-gateway]

(QEMU: -netdev socket,id=n0,udp=127.0.0.1:LISTEN_PORT,localaddr=127.0.0.1:QEMU_PORT.)
With --silent-gateway the gateway does not say its hardware address.
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
HOSTS = (GATEWAY, NAMESERVER, SLOW, TWICE, SLOW_NEIGHBOUR)
FAR = bytes([192, 0, 2, 10])
UNREACHABLE = bytes([192, 0, 2, 9])
TOO_FAR = bytes([192, 0, 2, 11])
NAMES = {b"gateway.lan": GATEWAY, b"slow.lan": SLOW, b"far.lan": FAR}
QUIET_NAME = b"quiet.lan"


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


def main():
    listen_port, qemu_port = int(sys.argv[1]), int(sys.argv[2])
    silent_gateway = "--silent-gateway" in sys.argv[3:]
    wire = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    wire.bind(("127.0.0.1", listen_port))
    later = []
    order = 0

    def send(frame, delay=0.0):
        nonlocal order
        order += 1
        heapq.heappush(later, (time.monotonic() + delay, order, frame))

    def to_guest(guest_mac, speaker, packet, delay=0.0):
        # (What is beyond the gateway comes through the gateway's card.)
        via = speaker if speaker in HOSTS else GATEWAY
        send(guest_mac + mac_of(via) + b"\x08\x00" + packet, delay)

    while True:
        now = time.monotonic()
        while later and later[0][0] <= now:
            wire.sendto(heapq.heappop(later)[2], ("127.0.0.1", qemu_port))
        wait = max(0.0, later[0][0] - now) if later else 1.0
        if not select.select([wire], [], [], wait)[0]:
            continue
        frame = wire.recv(4096)
        if len(frame) < 34:
            continue
        guest_mac = frame[6:12]
        kind = struct.unpack(">H", frame[12:14])[0]
        if kind == 0x0806:
            operation = struct.unpack(">H", frame[20:22])[0]
            asker_ip, asked = frame[28:32], frame[38:42]
            if operation != 1 or asked not in HOSTS or (asked == GATEWAY and silent_gateway):
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
        if protocol == 1 and len(payload) >= 8 and payload[0] == 8:
            echo = ip_packet(destination, source, 1, icmp(0, 0, payload[4:8], payload[8:]))
            if destination in (GATEWAY, NAMESERVER, SLOW_NEIGHBOUR, FAR):
                to_guest(guest_mac, destination, echo)
            elif destination == SLOW:
                to_guest(guest_mac, destination, echo, 1.5)
            elif destination == TWICE:
                to_guest(guest_mac, destination, echo)
                to_guest(guest_mac, destination, echo, 0.05)
            elif destination in (UNREACHABLE, TOO_FAR):
                # The router's word, with the IP header and the first 8
                # bytes of what it could not deliver.
                said = icmp(3, 1, bytes(4), packet[:header + 8]) if destination == UNREACHABLE else icmp(11, 0, bytes(4), packet[:header + 8])
                to_guest(guest_mac, GATEWAY, ip_packet(GATEWAY, source, 1, said))
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
            if name == QUIET_NAME:
                continue
            address = NAMES.get(name)
            if address is None:
                answer = query[:2] + struct.pack(">HHHHH", 0x8183, 1, 0, 0, 0) + question
            elif asked_for == 1:
                answer = query[:2] + struct.pack(">HHHHH", 0x8180, 1, 1, 0, 0) + question + struct.pack(">HHHIH", 0xC00C, 1, 1, 60, 4) + address
            else:
                answer = query[:2] + struct.pack(">HHHHH", 0x8180, 1, 0, 0, 0) + question
            datagram = struct.pack(">HHHH", 53, source_port, 8 + len(answer), 0) + answer
            to_guest(guest_mac, NAMESERVER, ip_packet(NAMESERVER, source, 17, datagram))


if __name__ == "__main__":
    main()
