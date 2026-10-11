#!/usr/bin/env python3
"""Writes the messages of an nl80211 GET_SCAN dump, in the kernel's
layout, for tools/check_wifi.sh (projects/network/nl80211.mlx reads it):

    scan_results.py FILE SEQUENCE

The networks: Home (WPA2-PSK, two access points, the stronger kept),
Cafe (open), Modern (WPA3 alone), Mixed (WPA2/WPA3 transition), Office
(802.1X), Old (WEP), a hidden one, Strict (PSK with PMF required), Tkip
(a TKIP group cipher) and Joined (the access point joined is kept though
weaker).
"""
import struct
import sys

FAMILY = 28
NEW_SCAN_RESULTS = 34
NESTED = 0x8000


def attribute(kind, data):
    out = struct.pack('<HH', 4 + len(data), kind) + data
    while len(out) % 4:
        out += b'\0'
    return out


def rsn(group=4, pairwise=(4,), akms=(2,), capabilities=0):
    body = struct.pack('<H', 1) + bytes([0, 15, 172, group]) + struct.pack('<H', len(pairwise))
    body += b''.join(bytes([0, 15, 172, p]) for p in pairwise)
    body += struct.pack('<H', len(akms)) + b''.join(bytes([0, 15, 172, a]) for a in akms)
    body += struct.pack('<H', capabilities)
    return bytes([48, len(body)]) + body


def bss(last, ssid, frequency, dbm, elements=b'', privacy=False, joined=False):
    ies = bytes([0, len(ssid)]) + ssid.encode() + bytes([1, 4, 0x82, 0x84, 0x8b, 0x96]) + elements
    inner = attribute(1, bytes([2, 0, 0, 0, 0, last]))
    inner += attribute(2, struct.pack('<I', frequency))
    inner += attribute(3, struct.pack('<Q', 123456))
    inner += attribute(4, struct.pack('<H', 100))
    inner += attribute(5, struct.pack('<H', 0x0411 if privacy else 0x0401))
    inner += attribute(6, ies)
    inner += attribute(7, struct.pack('<i', dbm * 100))
    inner += attribute(10, struct.pack('<I', 40))
    if joined:
        inner += attribute(9, struct.pack('<I', 1))
    return inner


def message(sequence, payload, kind=FAMILY, flags=2):
    return struct.pack('<IHHII', 16 + len(payload), kind, flags, sequence, 4242) + payload


def main():
    path, sequence = sys.argv[1], int(sys.argv[2])
    entries = [
        bss(1, 'Home', 2412, -70, rsn(), True),
        bss(2, 'Home', 2437, -45, rsn(), True),
        bss(3, 'Cafe', 5180, -60),
        bss(4, 'Modern', 5200, -55, rsn(akms=(8,), capabilities=0xC0), True),
        bss(5, 'Mixed', 2462, -65, rsn(akms=(2, 8), capabilities=0x80), True),
        bss(6, 'Office', 5240, -50, rsn(akms=(1,)), True),
        bss(7, 'Old', 2412, -80, b'', True),
        bss(8, '', 2412, -40, rsn(), True),
        bss(9, 'Strict', 5745, -58, rsn(capabilities=0xC0), True),
        bss(10, 'Tkip', 2422, -75, rsn(group=2, pairwise=(4, 2)), True),
        bss(11, 'Joined', 2437, -72, rsn(), True, True),
        bss(12, 'Joined', 5180, -50, rsn(), True),
    ]
    out = b''
    for entry in entries:
        payload = bytes([NEW_SCAN_RESULTS, 1, 0, 0]) + attribute(46, struct.pack('<I', 7)) + attribute(3, struct.pack('<I', 5)) + attribute(47 | NESTED, entry)
        out += message(sequence, payload)
    # Another request's answer between them is not taken.
    out = message(sequence + 1, bytes([NEW_SCAN_RESULTS, 1, 0, 0]) + attribute(47 | NESTED, bss(13, 'Stray', 2412, -30))) + out
    out += message(sequence, struct.pack('<i', 0), kind=3)
    with open(path, 'wb') as handle:
        handle.write(out)


if __name__ == '__main__':
    main()
