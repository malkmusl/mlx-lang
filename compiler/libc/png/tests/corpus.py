#!/usr/bin/env python3
"""Writes the PNG files tools/check_png.sh decodes with the system's libpng
and with mlx-libpng: every colour type at every bit depth, interlaced and
not, with transparency, the colour space chunks (gamma values that need
correction, sRGB, cHRM, iCCP profiles good and bad, sBIT), text chunks,
APNG's chunks, image data split over many IDAT chunks, and damaged files
(bad CRCs, a bad filter type, damaged deflate data, truncation, unknown
critical chunks, chunks out of place, data past the image). The images'
pixels come from a fixed pseudo-random sequence: the same files every run.

    corpus.py DIRECTORY
"""
import os
import struct
import sys
import zlib

PASSES = [(0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2)]
CHANNELS = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}


class Random:
    def __init__(self, seed):
        self.state = seed & 0xFFFFFFFF or 1

    def next(self):
        x = self.state
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= x >> 17
        x ^= (x << 5) & 0xFFFFFFFF
        self.state = x & 0xFFFFFFFF
        return self.state


def chunk(name, data):
    body = name + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def ihdr(width, height, depth, color, interlace=0):
    return chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, depth, color, 0, 0, interlace))


def pixels(width, height, depth, color, seed, smooth=True):
    """Rows of samples (each sample below 2**depth), a smooth gradient
    with noise so that every filter gets used."""
    rng = Random(seed)
    channels = CHANNELS[color]
    top = (1 << depth) - 1
    rows = []
    for y in range(height):
        row = []
        for x in range(width):
            for c in range(channels):
                if smooth:
                    base = ((x * 7 + y * 5 + c * 40) * (top + 1)) // (width * 7 + height * 5 + 120)
                    value = (base + rng.next() % max(1, (top + 1) // 16)) % (top + 1)
                else:
                    value = rng.next() % (top + 1)
                row.append(value)
        rows.append(row)
    return rows


def pack(samples, depth):
    if depth == 8:
        return bytes(samples)
    if depth == 16:
        return b"".join(struct.pack(">H", s) for s in samples)
    out = bytearray()
    per = 8 // depth
    for i in range(0, len(samples), per):
        byte = 0
        group = samples[i:i + per]
        for j, s in enumerate(group):
            byte |= s << (8 - depth * (j + 1))
        out.append(byte)
    return bytes(out)


def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def filtered(rows_bytes, bpp, choice):
    out = bytearray()
    previous = bytes(len(rows_bytes[0])) if rows_bytes else b""
    for index, row in enumerate(rows_bytes):
        kind = choice(index)
        out.append(kind)
        for i, value in enumerate(row):
            left = row[i - bpp] if i >= bpp else 0
            up = previous[i]
            corner = previous[i - bpp] if i >= bpp else 0
            predicted = [0, left, up, (left + up) // 2, paeth(left, up, corner)][kind]
            out.append((value - predicted) & 255)
        previous = row
    return bytes(out)


def image_data(width, height, depth, color, interlace, rows, choice=lambda i: i % 5):
    channels = CHANNELS[color]
    bpp = max(1, depth * channels // 8)
    if not interlace:
        return filtered([pack(r, depth) for r in rows], bpp, choice)
    out = b""
    for x0, y0, dx, dy in PASSES:
        sub = []
        for y in range(y0, height, dy):
            samples = []
            for x in range(x0, width, dx):
                samples.extend(rows[y][x * channels:(x + 1) * channels])
            if samples:
                sub.append(pack(samples, depth))
        if sub and sub[0]:
            out += filtered(sub, bpp, choice)
    return out


def idats(raw, level=6, pieces=1, rng=None):
    stream = zlib.compress(raw, level)
    if pieces <= 1:
        return chunk(b"IDAT", stream)
    out = b""
    cuts = sorted({(rng.next() % max(1, len(stream))) for _ in range(pieces - 1)})
    start = 0
    for cut in cuts + [len(stream)]:
        out += chunk(b"IDAT", stream[start:cut])
        start = cut
    return out


SIGNATURE = b"\x89PNG\r\n\x1a\n"


def png(width, height, depth, color, interlace=0, before=b"", after=b"", seed=1, level=6, pieces=1, rows=None, plte=None):
    if rows is None:
        rows = pixels(width, height, depth, color, seed)
    data = image_data(width, height, depth, color, interlace, rows)
    head = SIGNATURE + ihdr(width, height, depth, color, interlace) + before
    if color == 3:
        count = plte if plte is not None else (1 << depth)
        rng = Random(seed + 99)
        head += chunk(b"PLTE", bytes(rng.next() % 256 for _ in range(count * 3)))
    return head + idats(data, level, pieces, Random(seed + 7)) + after + chunk(b"IEND", b"")


def profile(space=b"RGB ", klass=b"mntr", size=200):
    header = bytearray(128)
    struct.pack_into(">I", header, 0, size)
    header[8] = 4
    header[12:16] = klass
    header[16:20] = space
    header[20:24] = b"XYZ "
    header[36:40] = b"acsp"
    header[68:80] = bytes([0, 0, 0xF6, 0xD6, 0, 1, 0, 0, 0, 0, 0xD3, 0x2D])
    body = bytes(header) + struct.pack(">I", 1) + b"desc" + struct.pack(">II", 144, size - 144)
    return body + bytes(size - len(body))


def iccp(name, data, level=0):
    return chunk(b"iCCP", name + b"\0\0" + zlib.compress(data, level))


def main():
    directory = sys.argv[1]
    os.makedirs(directory, exist_ok=True)
    files = {}
    sizes = [(1, 1), (3, 5), (32, 32), (33, 17), (7, 64), (64, 3), (17, 9), (2, 2)]
    n = 0
    for color, depths in [(0, [1, 2, 4, 8, 16]), (2, [8, 16]), (3, [1, 2, 4, 8]), (4, [8, 16]), (6, [8, 16])]:
        for depth in depths:
            for interlace in (0, 1):
                w, h = sizes[n % len(sizes)]
                n += 1
                files["type%d-depth%d-i%d.png" % (color, depth, interlace)] = png(w, h, depth, color, interlace, seed=n)
    # Transparency.
    for depth in (1, 2, 4, 8, 16):
        top = (1 << depth) - 1
        files["trns-gray%d.png" % depth] = png(19, 13, depth, 0, depth % 2, before=chunk(b"tRNS", struct.pack(">H", top // 3)), seed=depth)
    for depth in (8, 16):
        rows = pixels(16, 16, depth, 2, 5, smooth=False)
        rows[3][0:3] = [1, 2, 3]
        files["trns-rgb%d.png" % depth] = png(16, 16, depth, 2, before=chunk(b"tRNS", struct.pack(">HHH", 1, 2, 3)), rows=rows)
    files["trns-palette-short.png"] = png(23, 11, 8, 3, 1, before=b"", seed=3, plte=None).replace(b"IDAT", b"IDAT", 1)
    pal = SIGNATURE + ihdr(20, 20, 4, 3) + chunk(b"PLTE", bytes(range(48))) + chunk(b"tRNS", bytes([0, 128, 255, 7])) + idats(image_data(20, 20, 4, 3, 0, pixels(20, 20, 4, 3, 8))) + chunk(b"IEND", b"")
    files["trns-palette4.png"] = pal
    small = SIGNATURE + ihdr(9, 9, 8, 3, 1) + chunk(b"PLTE", bytes(range(30))) + chunk(b"tRNS", bytes([10, 20])) + idats(image_data(9, 9, 8, 3, 1, [[(x * y) % 16 for x in range(9)] for y in range(9)])) + chunk(b"IEND", b"")
    files["palette-index-out-of-range.png"] = small
    # Gamma, sBIT, the colour space.
    for gamma in (100000, 55556, 45455, 70000, 35000, 250000):
        for color, depth in ((0, 8), (2, 16), (3, 8), (6, 8), (0, 4)):
            files["gamma%d-type%d-depth%d.png" % (gamma, color, depth)] = png(13, 7, depth, color, before=chunk(b"gAMA", struct.pack(">I", gamma)), seed=gamma + color)
    files["gamma-sbit12.png"] = png(10, 10, 16, 2, before=chunk(b"gAMA", struct.pack(">I", 100000)) + chunk(b"sBIT", bytes([12, 10, 11])), seed=4)
    files["gamma-sbit5-gray.png"] = png(10, 10, 16, 0, before=chunk(b"gAMA", struct.pack(">I", 55556)) + chunk(b"sBIT", bytes([5])), seed=5)
    files["srgb.png"] = png(8, 8, 8, 2, before=chunk(b"sRGB", b"\0"), seed=6)
    files["srgb-gamma-mismatch.png"] = png(8, 8, 8, 2, before=chunk(b"gAMA", struct.pack(">I", 100000)) + chunk(b"sRGB", b"\1"), seed=7)
    chrm = struct.pack(">8I", 31270, 32900, 64000, 33000, 30000, 60000, 15000, 6000)
    files["chrm-srgb.png"] = png(8, 8, 8, 2, before=chunk(b"cHRM", chrm) + chunk(b"gAMA", struct.pack(">I", 45455)), seed=8)
    wide = struct.pack(">8I", 34570, 35850, 73470, 26530, 0, 100000, 1000, 1000)
    files["chrm-wide-then-srgb.png"] = png(8, 8, 8, 2, before=chunk(b"gAMA", struct.pack(">I", 70000)) + chunk(b"cHRM", wide) + chunk(b"sRGB", b"\0"), seed=9)
    files["chrm-invalid.png"] = png(8, 8, 8, 2, before=chunk(b"gAMA", struct.pack(">I", 70000)) + chunk(b"cHRM", struct.pack(">8I", 0, 0, 1, 1, 1, 1, 1, 1)), seed=10)
    files["iccp-rgb.png"] = png(8, 8, 8, 2, before=iccp(b"Display P3", profile()), seed=11)
    files["iccp-gray-on-rgb.png"] = png(8, 8, 8, 2, before=chunk(b"gAMA", struct.pack(">I", 70000)) + iccp(b"wrong", profile(b"GRAY")), seed=12)
    files["iccp-bad-length.png"] = png(8, 8, 8, 2, before=iccp(b"p", profile()[:150]), seed=13)
    files["iccp-truncated-stream.png"] = png(8, 8, 8, 2, before=chunk(b"iCCP", b"cut\0\0" + zlib.compress(profile(), 0)[:120]), seed=13)
    files["iccp-damaged-stream.png"] = png(8, 8, 8, 2, before=chunk(b"iCCP", b"bad\0\0\x78\x01" + bytes(range(120))), seed=13)
    files["iccp-short-stream.png"] = png(8, 8, 8, 2, before=chunk(b"iCCP", b"short\0\0" + zlib.compress(profile()[:100], 0)), seed=13)
    files["iccp-gray.png"] =png(8, 8, 8, 0, before=iccp(b"gray profile", profile(b"GRAY", b"scnr", 260)), seed=14)
    files["gamma-after-plte.png"] = png(8, 8, 8, 3, seed=15).replace(b"IDAT", b"IDAT")
    data = png(8, 8, 8, 3, seed=16)
    at = data.index(b"IDAT") - 4
    files["gamma-after-plte.png"] = data[:at] + chunk(b"gAMA", struct.pack(">I", 100000)) + data[at:]
    files["gamma-zero.png"] = png(8, 8, 8, 2, before=chunk(b"gAMA", struct.pack(">I", 0)), seed=17)
    # Text, time and the rest.
    text = chunk(b"tEXt", b"Title\0A PNG") + chunk(b"zTXt", b"Comment\0\0" + zlib.compress(b"compressed text" * 20)) + chunk(b"iTXt", b"XML:com.adobe.xmp\0\0\0\0\0<x:xmpmeta/>") + chunk(b"iTXt", b"Note\0\1\0en\0Note\0" + zlib.compress(b"an iTXt"))
    files["text.png"] = png(12, 12, 8, 6, before=text, after=chunk(b"tIME", struct.pack(">HBBBBB", 2024, 5, 6, 7, 8, 9)) + chunk(b"tEXt", b"After\0the image"), seed=18)
    files["text-bad.png"] = png(12, 12, 8, 2, before=chunk(b"zTXt", b"K\0\0notzlib") + chunk(b"tEXt", b"\0no keyword") + chunk(b"iTXt", b"K\0\2\0") + chunk(b"tIME", b"\0\0\0\0\0\0\0"), seed=19)
    files["phys-exif.png"] = png(12, 12, 8, 2, before=chunk(b"pHYs", struct.pack(">IIB", 2835, 2835, 1)) + chunk(b"eXIf", b"MM\0*\0\0\0\x08\0\0"), seed=20)
    files["bkgd-hist.png"] = png(12, 12, 8, 3, seed=21)
    # APNG: acTL, fcTL and fdAT, as WebKit reads them through the unknown
    # chunk function.
    rows0 = pixels(16, 16, 8, 6, 30)
    rows1 = pixels(8, 8, 8, 6, 31)
    actl = chunk(b"acTL", struct.pack(">II", 2, 0))
    fctl0 = chunk(b"fcTL", struct.pack(">IIIIIHHBB", 0, 16, 16, 0, 0, 1, 10, 0, 0))
    fctl1 = chunk(b"fcTL", struct.pack(">IIIIIHHBB", 1, 8, 8, 4, 4, 1, 10, 1, 1))
    frame = zlib.compress(image_data(8, 8, 8, 6, 0, rows1))
    fdat = chunk(b"fdAT", struct.pack(">I", 2) + frame)
    files["apng.png"] = SIGNATURE + ihdr(16, 16, 8, 6) + actl + fctl0 + idats(image_data(16, 16, 8, 6, 0, rows0)) + fctl1 + fdat + chunk(b"IEND", b"")
    # The image data in many pieces, and empty IDATs.
    files["idat-pieces.png"] = png(40, 30, 8, 2, 1, pieces=25, seed=22)
    zero = png(20, 20, 8, 0, seed=23)
    at = zero.index(b"IDAT") - 4
    files["idat-empty.png"] = zero[:at] + chunk(b"IDAT", b"") + zero[at:]
    for level in (0, 1, 9):
        files["level%d.png" % level] = png(31, 29, 8, 6, level=level, seed=24 + level)
    files["unknown-ancillary.png"] = png(9, 9, 8, 2, before=chunk(b"prVt", b"private") + chunk(b"vpAg", b"\0" * 9), after=chunk(b"zzZz", b"tail"), seed=27)
    # Damaged.
    good = png(16, 16, 8, 2, seed=28)
    def corrupt(data, name, offset, value):
        at = data.index(name) + offset
        return data[:at] + bytes([value]) + data[at + 1:]
    ancillary = png(16, 16, 8, 2, before=chunk(b"gAMA", struct.pack(">I", 70000)), seed=29)
    at = ancillary.index(b"gAMA") + 8
    files["crc-ancillary.png"] = ancillary[:at] + bytes([ancillary[at] ^ 1]) + ancillary[at + 1:]
    at = good.index(b"IEND") - 5
    files["crc-idat.png"] = good[:at] + bytes([good[at] ^ 0x55]) + good[at + 1:]
    raw = bytearray(image_data(16, 16, 8, 2, 0, pixels(16, 16, 8, 2, 28)))
    raw[49 * 3] = 9
    files["bad-filter.png"] = SIGNATURE + ihdr(16, 16, 8, 2) + chunk(b"IDAT", zlib.compress(bytes(raw))) + chunk(b"IEND", b"")
    stream = bytearray(zlib.compress(image_data(16, 16, 8, 2, 0, pixels(16, 16, 8, 2, 28))))
    stream[len(stream) // 2] ^= 0xFF
    files["bad-deflate.png"] = SIGNATURE + ihdr(16, 16, 8, 2) + chunk(b"IDAT", bytes(stream)) + chunk(b"IEND", b"")
    stream = bytearray(zlib.compress(image_data(16, 16, 8, 2, 0, pixels(16, 16, 8, 2, 28))))
    stream[-1] ^= 1
    files["bad-adler.png"] = SIGNATURE + ihdr(16, 16, 8, 2) + chunk(b"IDAT", bytes(stream)) + chunk(b"IEND", b"")
    files["truncated-half.png"] = good[:len(good) // 2]
    files["truncated-header.png"] = good[:30]
    files["too-much-data.png"] = SIGNATURE + ihdr(8, 8, 8, 0) + chunk(b"IDAT", zlib.compress(bytes(9 * 8) + b"extra" * 10)) + chunk(b"IEND", b"")
    files["too-little-data.png"] = SIGNATURE + ihdr(8, 8, 8, 0) + chunk(b"IDAT", zlib.compress(bytes(9 * 5))) + chunk(b"IEND", b"")
    files["unknown-critical.png"] = png(8, 8, 8, 2, before=chunk(b"CRIT", b"data"), seed=30)
    files["bad-ihdr-depth.png"] = SIGNATURE + chunk(b"IHDR", struct.pack(">IIBBBBB", 8, 8, 3, 0, 0, 0, 0)) + chunk(b"IEND", b"")
    files["bad-ihdr-zero.png"] = SIGNATURE + chunk(b"IHDR", struct.pack(">IIBBBBB", 0, 8, 8, 0, 0, 0, 0)) + chunk(b"IEND", b"")
    files["missing-plte.png"] = SIGNATURE + ihdr(8, 8, 8, 3) + idats(bytes(9 * 8)) + chunk(b"IEND", b"")
    files["not-png.png"] = b"GIF89a" + bytes(100)
    files["ascii-mangled.png"] = b"\x89PNG\n\x1a\n" + good[8:]
    files["bad-chunk-name.png"] = SIGNATURE + ihdr(8, 8, 8, 0) + chunk(b"ab1d", b"") + idats(bytes(9 * 8)) + chunk(b"IEND", b"")
    files["trns-after-idat.png"] = png(8, 8, 8, 2, after=chunk(b"tRNS", struct.pack(">HHH", 1, 2, 3)), seed=31)
    files["plte-twice.png"] = SIGNATURE + ihdr(4, 4, 8, 3) + chunk(b"PLTE", bytes(12)) + chunk(b"PLTE", bytes(12)) + idats(bytes(5 * 4)) + chunk(b"IEND", b"")
    files["after-iend.png"] = good + b"junk after the end"
    files["idat-after-chunk.png"] = png(8, 8, 8, 2, pieces=3, seed=32)
    data = files["idat-after-chunk.png"]
    second = data.index(b"IDAT", data.index(b"IDAT") + 4) - 4
    files["idat-after-chunk.png"] = data[:second] + chunk(b"teXt", b"x") + data[second:]
    big = png(300, 200, 8, 6, 1, seed=33, level=9)
    files["large-interlaced.png"] = big
    for name, data in sorted(files.items()):
        with open(os.path.join(directory, name), "wb") as out:
            out.write(data)


if __name__ == "__main__":
    main()
