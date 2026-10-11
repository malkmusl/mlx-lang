#!/usr/bin/env python3
"""The access point's side of WPA2-Personal for tools/check_wifi.sh.

    authenticator.py LINK STATION PASSPHRASE SSID [--mode MODE]

It runs the 4-way handshake (IEEE 802.11-2020, 12.7.6) with STATION
(aa:bb:..) over LINK's EAPOL frames, then a group key handshake with a
new GTK, using key descriptor version 2 (HMAC-SHA1-128, the AES key wrap
of OpenSSL's libcrypto, libcrypto.py). It prints what it derived and saw:

    msg2 ok | msg2 bad-mic | msg2 bad-element
    msg4 ok
    ptk TK          the TK it derived
    gtk N GTK       each group key it handed over
    group ok        the station answered the group handshake
    gave up         no good message 2 after three messages 1

Modes: normal; forged (message 3 is first sent with a broken MIC, which
the station must ignore); downgrade (message 3 carries another RSN
element than the beacon's, which the station must refuse).
"""
import hashlib
import hmac
import os
import select
import socket
import struct
import sys
import time

import libcrypto

ETH_P_PAE = 0x888E
# The access point's RSN element, as its beacon carries it: CCMP group and
# pairwise, PSK, no capabilities beyond the default.
BEACON_RSN = bytes.fromhex('30140100000fac040100000fac040100000fac020000')
# What a downgrading access point would put into message 3: TKIP pairwise.
OTHER_RSN = bytes.fromhex('30140100000fac040100000fac020100000fac020000')
# The station's element (projects/network/wpa.mlx ownElement).
STATION_RSN = bytes.fromhex('30120100000fac040100000fac040100000fac02')

INFO_PAIRWISE = 0x0008
INFO_INSTALL = 0x0040
INFO_ACK = 0x0080
INFO_MIC = 0x0100
INFO_SECURE = 0x0200
INFO_ENCRYPTED = 0x1000
VERSION = 2


def prf(key, label, data, length):
    out = b''
    counter = 0
    while len(out) < length:
        out += hmac.new(key, label + b'\0' + data + bytes([counter]), hashlib.sha1).digest()
        counter += 1
    return out[:length]


def ptk_of(pmk, aa, spa, anonce, snonce):
    data = min(aa, spa) + max(aa, spa) + min(anonce, snonce) + max(anonce, snonce)
    return prf(pmk, b'Pairwise key expansion', data, 48)


def frame(info, replay, nonce=bytes(32), rsc=bytes(8), data=b'', kck=None, key_length=16):
    body = bytes([2]) + struct.pack('>HH', info, key_length) + struct.pack('>Q', replay)
    body += nonce + bytes(16) + rsc + bytes(8) + bytes(16) + struct.pack('>H', len(data)) + data
    out = bytearray(bytes([2, 3]) + struct.pack('>H', len(body)) + body)
    if kck is not None:
        out[81:97] = hmac.new(kck, bytes(out), hashlib.sha1).digest()[:16]
    return bytes(out)


def mic_good(kck, received):
    copy = bytearray(received)
    copy[81:97] = bytes(16)
    return hmac.compare_digest(hmac.new(kck, bytes(copy), hashlib.sha1).digest()[:16], received[81:97])


def wrapped(kek, plain):
    if len(plain) % 8 != 0 or len(plain) < 16:
        plain += b'\xdd'
        while len(plain) % 8 != 0 or len(plain) < 16:
            plain += b'\0'
    return libcrypto.wrap(kek, plain)


def gtk_kde(index, gtk):
    return bytes([0xdd, 6 + len(gtk), 0x00, 0x0f, 0xac, 0x01, index & 3, 0]) + gtk


class Link:
    def __init__(self, name, station):
        self.name = name
        self.station = station
        self.sock = socket.socket(socket.AF_PACKET, socket.SOCK_DGRAM, socket.htons(ETH_P_PAE))
        self.sock.bind((name, ETH_P_PAE))
        self.own = self.sock.getsockname()[4][:6]

    def send(self, data):
        self.sock.sendto(data, (self.name, ETH_P_PAE, 0, 0, self.station))

    def receive(self, seconds):
        ready, _, _ = select.select([self.sock], [], [], seconds)
        if not ready:
            return None
        data, address = self.sock.recvfrom(2048)
        if address[4][:6] != self.station:
            return None
        return data


def say(text):
    print(text, flush=True)


def main():
    name, station, passphrase, ssid = sys.argv[1:5]
    mode = sys.argv[sys.argv.index('--mode') + 1] if '--mode' in sys.argv else 'normal'
    link = Link(name, bytes.fromhex(station.replace(':', '')))
    pmk = hashlib.pbkdf2_hmac('sha1', passphrase.encode(), ssid.encode(), 4096, 32)
    replay = 1
    anonce = os.urandom(32)
    ptk = None
    time.sleep(1.0)
    for attempt in range(3):
        link.send(frame(VERSION | INFO_PAIRWISE | INFO_ACK, replay, anonce))
        reply = link.receive(2.0)
        if reply is None:
            continue
        snonce = reply[17:49]
        candidate = ptk_of(pmk, link.own, link.station, anonce, snonce)
        if not mic_good(candidate[:16], reply):
            say('msg2 bad-mic')
            replay += 1
            continue
        length = struct.unpack('>H', reply[97:99])[0]
        if reply[99:99 + length] != STATION_RSN:
            say('msg2 bad-element')
            return
        say('msg2 ok')
        ptk = candidate
        break
    if ptk is None:
        say('gave up')
        return
    kck, kek, tk = ptk[:16], ptk[16:32], ptk[32:48]
    say('ptk ' + tk.hex())
    gtk = os.urandom(16)
    element = OTHER_RSN if mode == 'downgrade' else BEACON_RSN
    data = wrapped(kek, element + gtk_kde(1, gtk))
    replay += 1
    info = VERSION | INFO_PAIRWISE | INFO_INSTALL | INFO_ACK | INFO_MIC | INFO_SECURE | INFO_ENCRYPTED
    rsc = bytes([7, 0, 0, 0, 0, 0, 0, 0])
    good = frame(info, replay, anonce, rsc, data, kck)
    if mode == 'forged':
        forged = bytearray(good)
        forged[90] ^= 1
        link.send(bytes(forged))
        if link.receive(1.5) is not None:
            say('msg4 to a forged msg3')
            return
        say('forged msg3 ignored')
    link.send(good)
    say('gtk 1 ' + gtk.hex())
    reply = link.receive(2.0)
    if reply is None:
        say('no msg4')
        return
    info4 = struct.unpack('>H', reply[5:7])[0]
    if not mic_good(kck, reply) or not info4 & INFO_SECURE or struct.unpack('>Q', reply[9:17])[0] != replay:
        say('msg4 bad')
        return
    say('msg4 ok')
    # A new group key.
    time.sleep(0.5)
    gtk = os.urandom(16)
    replay += 1
    data = wrapped(kek, gtk_kde(2, gtk))
    link.send(frame(VERSION | INFO_ACK | INFO_MIC | INFO_SECURE | INFO_ENCRYPTED, replay, bytes(32), rsc, data, kck))
    say('gtk 2 ' + gtk.hex())
    reply = link.receive(2.0)
    if reply is None or not mic_good(kck, reply) or struct.unpack('>Q', reply[9:17])[0] != replay:
        say('group bad')
        return
    say('group ok')
    # The same frame again: a replay the station must not answer.
    link.send(frame(VERSION | INFO_ACK | INFO_MIC | INFO_SECURE | INFO_ENCRYPTED, replay, bytes(32), rsc, data, kck))
    say('replay ignored' if link.receive(1.0) is None else 'replay answered')


if __name__ == '__main__':
    main()
