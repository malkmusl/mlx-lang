#!/usr/bin/env python3
"""A DHCPv4 server for tools/check_network.sh: one link, one address.

    dhcpd.py LINK [--lease SECONDS] [--refuse-renew]

It hands out 10.7.0.50/24 with router 10.7.0.1, name servers 10.7.0.53
and 10.7.0.54 and the domain test.lan (it is 10.7.0.1 itself), and says
on stdout what it got and sent ("discover", "offer", "request", "ack",
"renew", "release"). With --refuse-renew it answers a renewal with NAK.
"""
import socket
import struct
import sys

SERVER = bytes([10, 7, 0, 1])
YOURS = bytes([10, 7, 0, 50])


def options(data):
    found = {}
    at = 240
    while at < len(data):
        code = data[at]
        if code == 255:
            break
        if code == 0:
            at += 1
            continue
        size = data[at + 1]
        found[code] = data[at + 2:at + 2 + size]
        at += 2 + size
    return found


def answer(request, kind, lease):
    reply = bytearray(240)
    reply[0] = 2
    reply[1] = 1
    reply[2] = 6
    reply[4:8] = request[4:8]
    reply[10:12] = request[10:12]
    reply[12:16] = request[12:16]
    if kind != 6:
        reply[16:20] = YOURS
    reply[20:24] = SERVER
    reply[28:44] = request[28:44]
    reply[236:240] = bytes([99, 130, 83, 99])
    out = [(53, bytes([kind])), (54, SERVER)]
    if kind != 6:
        out += [(51, struct.pack('>I', lease)), (1, bytes([255, 255, 255, 0])), (3, SERVER),
                (6, bytes([10, 7, 0, 53, 10, 7, 0, 54])), (15, b'test.lan')]
    for code, value in out:
        reply += bytes([code, len(value)]) + value
    reply += bytes([255])
    while len(reply) < 300:
        reply += b'\0'
    return bytes(reply)


def main():
    link = sys.argv[1]
    lease = 3600
    refuse_renew = '--refuse-renew' in sys.argv
    if '--lease' in sys.argv:
        lease = int(sys.argv[sys.argv.index('--lease') + 1])
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, link.encode())
    sock.bind(('0.0.0.0', 67))
    print('listening', flush=True)
    while True:
        data, source = sock.recvfrom(2048)
        if len(data) < 240 or data[0] != 1:
            continue
        found = options(data)
        kind = found.get(53, b'\0')[0]
        ciaddr = data[12:16]
        if kind == 1:
            print('discover', flush=True)
            reply, name = answer(data, 2, lease), 'offer'
        elif kind == 3:
            renewing = ciaddr != bytes(4)
            print('renew' if renewing else 'request', flush=True)
            if renewing and refuse_renew:
                reply, name = answer(data, 6, lease), 'nak'
            else:
                reply, name = answer(data, 5, lease), 'ack'
        elif kind == 7:
            print('release', flush=True)
            continue
        else:
            continue
        if ciaddr != bytes(4) and name == 'ack':
            sock.sendto(reply, (socket.inet_ntoa(ciaddr), 68))
        else:
            sock.sendto(reply, ('255.255.255.255', 68))
        print(name, flush=True)


if __name__ == '__main__':
    main()
