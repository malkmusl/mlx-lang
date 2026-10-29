#!/usr/bin/env python3
"""The MP4 files tests/416_mp4_demux_runtime.mlx reads, built box by box
(no media tool): a plain file with sample tables (an AVC track of three
samples in two chunks with composition offsets and one sync sample, an
AAC track of two fixed-size samples with an edit list) and a fragmented
one (an initialization segment with trex defaults, two fragments whose
second has no tfdt). Prints them as Mlx byte arrays."""
import struct


def box(kind, payload=b''):
    return struct.pack('>I4s', 8 + len(payload), kind) + payload


def full(kind, version, flags, payload):
    return box(kind, struct.pack('>B', version) + flags.to_bytes(3, 'big') + payload)


def ftyp():
    return box(b'ftyp', b'isom' + struct.pack('>I', 512) + b'isomiso2avc1mp41')


def mvhd(timescale, duration):
    return full(b'mvhd', 0, 0, struct.pack('>IIII', 0, 0, timescale, duration)
                + struct.pack('>IH', 0x00010000, 0x0100) + b'\0' * 10
                + struct.pack('>9I', 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x40000000)
                + b'\0' * 24 + struct.pack('>I', 3))


def tkhd(track_id, duration, width, height):
    return full(b'tkhd', 0, 7, struct.pack('>IIII', 0, 0, track_id, 0) + struct.pack('>I', duration)
                + b'\0' * 8 + struct.pack('>HHHH', 0, 0, 0, 0)
                + struct.pack('>9I', 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x40000000)
                + struct.pack('>II', width << 16, height << 16))


def mdhd(timescale, duration):
    return full(b'mdhd', 0, 0, struct.pack('>IIII', 0, 0, timescale, duration) + struct.pack('>HH', 0x55c4, 0))


def hdlr(handler):
    return full(b'hdlr', 0, 0, struct.pack('>I', 0) + handler + b'\0' * 12 + b'Handler\0')


def elst(entries):
    return full(b'elst', 0, 0, struct.pack('>I', len(entries))
                + b''.join(struct.pack('>IIhh', d, m, 1, 0) for d, m in entries))


def avcc(sps, pps):
    return box(b'avcC', bytes([1, sps[1], sps[2], sps[3], 0xff, 0xe0 | 1]) + struct.pack('>H', len(sps)) + sps
               + bytes([1]) + struct.pack('>H', len(pps)) + pps)


def avc1(width, height, sps, pps):
    entry = (b'\0' * 6 + struct.pack('>H', 1) + b'\0' * 16 + struct.pack('>HH', width, height)
             + struct.pack('>IIIH', 0x480000, 0x480000, 0, 1) + b'\0' * 32 + struct.pack('>Hh', 24, -1))
    return box(b'avc1', entry + avcc(sps, pps))


def esds(asc):
    dsi = bytes([5, len(asc)]) + asc
    dcd = bytes([4, 13 + len(dsi), 0x40, 0x15]) + b'\0\0\0' + struct.pack('>II', 128000, 128000) + dsi
    es = bytes([3, 3 + len(dcd)]) + struct.pack('>HB', 1, 0) + dcd
    return full(b'esds', 0, 0, es)


def mp4a(channels, rate, asc):
    entry = (b'\0' * 6 + struct.pack('>H', 1) + b'\0' * 8 + struct.pack('>HHHH', channels, 16, 0, 0)
             + struct.pack('>I', rate << 16))
    return box(b'mp4a', entry + esds(asc))


def stsd(entry):
    return full(b'stsd', 0, 0, struct.pack('>I', 1) + entry)


def table(kind, entries, fmt):
    return full(kind, 0, 0, struct.pack('>I', len(entries)) + b''.join(struct.pack(fmt, *e) for e in entries))


def stsz(fixed, sizes, count=None):
    count = len(sizes) if count is None else count
    return full(b'stsz', 0, 0, struct.pack('>II', fixed, count) + b''.join(struct.pack('>I', s) for s in sizes))


SPS = bytes([0x67, 0x42, 0xc0, 0x1e, 0xd9, 0x00, 0x50, 0x05, 0xbb, 0x01, 0x10, 0x00, 0x00, 0x3e, 0x90, 0x00, 0x0b, 0xb8, 0x0f, 0x16, 0x2d, 0x96])
PPS = bytes([0x68, 0xcb, 0x83, 0xcb, 0x20])
ASC = bytes([0x12, 0x10])   # AAC-LC, 44100 Hz, 2 channels


def plain():
    # Video samples (length-prefixed NAL units): 3 samples, chunk 1 holds
    # two, chunk 2 one. Audio: 2 samples of 4 bytes in one chunk.
    video = [b'\0\0\0\x01\x65', b'\0\0\0\x02\x41\x9a', b'\0\0\0\x03\x41\x9b\x00']
    audio = [b'\x21\x00\x03\x40', b'\x21\x00\x03\x41']
    mdat_payload = video[0] + video[1] + audio[0] + audio[1] + video[2]
    header_size = None
    # Offsets are file offsets: ftyp + moov come first; compute moov twice.
    def build(mdat_offset):
        v_stbl = box(b'stbl', stsd(avc1(64, 48, SPS, PPS))
                     + table(b'stts', [(3, 3000)], '>II')
                     + table(b'ctts', [(1, 0), (1, 6000), (1, 3000)], '>II')
                     + table(b'stss', [(1,)], '>I')
                     + table(b'stsc', [(1, 2, 1), (2, 1, 1)], '>III')
                     + stsz(0, [len(s) for s in video])
                     + table(b'stco', [(mdat_offset,), (mdat_offset + 5 + 6 + 8,)], '>I'))
        a_stbl = box(b'stbl', stsd(mp4a(2, 44100, ASC))
                     + table(b'stts', [(2, 1024)], '>II')
                     + table(b'stsc', [(1, 2, 1)], '>III')
                     + stsz(4, [], 2)
                     + table(b'stco', [(mdat_offset + 5 + 6,)], '>I'))
        v_trak = box(b'trak', tkhd(1, 100, 64, 48) + box(b'mdia', mdhd(90000, 9000) + hdlr(b'vide')
                     + box(b'minf', box(b'vmhd', b'\0' * 12) + v_stbl)))
        a_trak = box(b'trak', tkhd(2, 46, 0, 0) + box(b'edts', elst([(46, 2112)]))
                     + box(b'mdia', mdhd(44100, 2048) + hdlr(b'soun') + box(b'minf', box(b'smhd', b'\0' * 8) + a_stbl)))
        return ftyp() + box(b'moov', mvhd(1000, 100) + v_trak + a_trak)
    head = build(0)
    head = build(len(head) + 8)
    return head + box(b'mdat', mdat_payload)


def fragmented():
    v_stbl = box(b'stbl', stsd(avc1(64, 48, SPS, PPS)) + table(b'stts', [], '>II')
                 + table(b'stsc', [], '>III') + stsz(0, []) + table(b'stco', [], '>I'))
    a_stbl = box(b'stbl', stsd(mp4a(2, 44100, ASC)) + table(b'stts', [], '>II')
                 + table(b'stsc', [], '>III') + stsz(0, []) + table(b'stco', [], '>I'))
    v_trak = box(b'trak', tkhd(1, 0, 64, 48) + box(b'mdia', mdhd(90000, 0) + hdlr(b'vide')
                 + box(b'minf', box(b'vmhd', b'\0' * 12) + v_stbl)))
    a_trak = box(b'trak', tkhd(2, 0, 0, 0) + box(b'mdia', mdhd(44100, 0) + hdlr(b'soun')
                 + box(b'minf', box(b'smhd', b'\0' * 8) + a_stbl)))
    mvex = box(b'mvex', full(b'trex', 0, 0, struct.pack('>IIIII', 1, 1, 3000, 0, 0x10000))
               + full(b'trex', 0, 0, struct.pack('>IIIII', 2, 1, 1024, 4, 0x2000000)))
    init = ftyp() + box(b'moov', mvhd(1000, 0) + mvex + v_trak + a_trak)

    def fragment(sequence, tfdt, video, audio):
        # Video run: sizes, flags and composition offsets per sample;
        # audio run: trex defaults (duration, size, flags), sizes given.
        v_entries = [(len(s), f, c) for s, f, c in video]
        a_entries = [(len(s),) for s in audio]
        def moof_of(v_offset, a_offset):
            v_traf = box(b'traf', full(b'tfhd', 0, 0x20000, struct.pack('>I', 1))
                         + (full(b'tfdt', 1, 0, struct.pack('>Q', tfdt)) if tfdt is not None else b'')
                         + full(b'trun', 1, 0x1 | 0x200 | 0x400 | 0x800,
                                struct.pack('>Ii', len(v_entries), v_offset)
                                + b''.join(struct.pack('>IIi', *e) for e in v_entries)))
            a_traf = box(b'traf', full(b'tfhd', 0, 0x20000, struct.pack('>I', 2))
                         + (full(b'tfdt', 0, 0, struct.pack('>I', tfdt * 44100 // 90000)) if tfdt is not None else b'')
                         + full(b'trun', 0, 0x1 | 0x200, struct.pack('>Ii', len(a_entries), a_offset)
                                + b''.join(struct.pack('>I', *e) for e in a_entries)))
            return box(b'moof', full(b'mfhd', 0, 0, struct.pack('>I', sequence)) + v_traf + a_traf)
        moof = moof_of(0, 0)
        v_data = b''.join(s for s, _, _ in video)
        a_data = b''.join(audio)
        moof = moof_of(len(moof) + 8, len(moof) + 8 + len(v_data))
        return moof + box(b'mdat', v_data + a_data)

    first = fragment(1, 0, [(b'\0\0\0\x01\x65', 0x2000000, 0), (b'\0\0\0\x02\x41\x9a', 0x1010000, 6000),
                            (b'\0\0\0\x03\x41\x9b\x00', 0x1010000, 3000)],
                     [b'\x21\x00\x03\x40', b'\x21\x00\x03\x41'])
    second = fragment(2, None, [(b'\0\0\0\x01\x65', 0x2000000, 0)], [b'\x21\x00\x03\x42'])
    return init, first, second


def dump(name, data):
    print('var %s = [%d]u8{' % (name, len(data)))
    for i in range(0, len(data), 24):
        print('    ' + ', '.join(str(b) for b in data[i:i + 24]) + ',')
    print('}')
    print()


if __name__ == '__main__':
    import sys
    plain_file = plain()
    init, first, second = fragmented()
    if len(sys.argv) > 1:
        open(sys.argv[1] + '/plain.mp4', 'wb').write(plain_file)
        open(sys.argv[1] + '/fragmented.mp4', 'wb').write(init + first + second)
    dump('plainFile', plain_file)
    dump('initSegment', init)
    dump('firstFragment', first)
    dump('secondFragment', second)
