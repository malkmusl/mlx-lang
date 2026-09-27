#!/usr/bin/env python3
"""Writes the PNG test images of tests/272_png_runtime.mlx.

Each NAME.png comes with NAME.argb: the pixels std.png must decode it to,
premultiplied 0xAARRGGBB as little-endian words (alpha rounded like std.png:
(channel * alpha + 127) // 255). The images cover every colour type and bit
depth, all five row filters (row r uses filter r % 5), and zlib streams with
stored (level 0), fixed and dynamic Huffman blocks. interlaced.png must be
refused. scaled.argb is rgba8.png shrunk to 5x3 by area averaging.

Run from the repository root:  python3 tests/support/png/make_pngs.py
"""
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
W, H = 37, 23


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)


def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def filtered(rows, bpp):
    out = b""
    previous = bytes(len(rows[0]))
    for r, row in enumerate(rows):
        kind = r % 5
        line = bytearray()
        for i, value in enumerate(row):
            left = row[i - bpp] if i >= bpp else 0
            up = previous[i]
            corner = previous[i - bpp] if i >= bpp else 0
            predictor = [0, left, up, (left + up) // 2, paeth(left, up, corner)][kind]
            line.append((value - predictor) & 255)
        out += bytes([kind]) + bytes(line)
        previous = row
    return out


def png(color_type, depth, rows, bpp, level=9, palette=None, trns=None, interlace=0, strategy=None):
    header = struct.pack(">IIBBBBB", W, H, depth, color_type, 0, 0, interlace)
    data = filtered(rows, bpp)
    if strategy is None:
        compressed = zlib.compress(data, level)
    else:
        compressor = zlib.compressobj(level, zlib.DEFLATED, 15, 9, strategy)
        compressed = compressor.compress(data) + compressor.flush()
    body = chunk(b"IHDR", header)
    if palette:
        body += chunk(b"PLTE", palette)
    if trns:
        body += chunk(b"tRNS", trns)
    # The image data split over two IDAT chunks.
    half = len(compressed) // 2
    body += chunk(b"IDAT", compressed[:half]) + chunk(b"IDAT", compressed[half:]) + chunk(b"IEND", b"")
    return b"\x89PNG\r\n\x1a\n" + body


def premultiply(a, r, g, b):
    if a == 255:
        return 0xff000000 | r << 16 | g << 8 | b
    return a << 24 | (r * a + 127) // 255 << 16 | (g * a + 127) // 255 << 8 | (b * a + 127) // 255


def rgba(x, y):
    return (x * 37 + y * 11) & 255, (x * 5 + y * 29) & 255, (x * y * 3) & 255, (x * 13 + y * 7 + 40) & 255


def pack_bits(values, depth):
    out, byte, used = bytearray(), 0, 0
    for value in values:
        byte = byte << depth | value
        used += depth
        if used == 8:
            out.append(byte)
            byte, used = 0, 0
    if used:
        out.append(byte << (8 - used))
    return bytes(out)


def save(name, data, pixels):
    open(os.path.join(HERE, name + ".png"), "wb").write(data)
    if pixels is not None:
        open(os.path.join(HERE, name + ".argb"), "wb").write(struct.pack("<%dI" % len(pixels), *pixels))


def main():
    # RGBA, 8 bits, dynamic Huffman.
    rows = [bytes(v for x in range(W) for v in rgba(x, y)) for y in range(H)]
    rgba8 = [premultiply(*(lambda c: (c[3], c[0], c[1], c[2]))(rgba(x, y))) for y in range(H) for x in range(W)]
    save("rgba8", png(6, 8, rows, 4), rgba8)
    # RGB, 8 bits, stored blocks.
    rows = [bytes(v for x in range(W) for v in rgba(x, y)[:3]) for y in range(H)]
    save("rgb8", png(2, 8, rows, 3, level=0), [premultiply(255, *rgba(x, y)[:3]) for y in range(H) for x in range(W)])
    # RGBA, 16 bits (std.png keeps the high byte), fixed Huffman.
    rows = [bytes(v for x in range(W) for c in rgba(x, y) for v in (c, (c * 7) & 255)) for y in range(H)]
    save("rgba16", png(6, 16, rows, 8, strategy=zlib.Z_FIXED), rgba8)
    # Greyscale, 1 bit.
    values = [[(x + y) % 3 == 0 and 1 or 0 for x in range(W)] for y in range(H)]
    rows = [pack_bits(row, 1) for row in values]
    save("grey1", png(0, 1, rows, 1), [premultiply(255, *(3 * [255 * v])) for row in values for v in row])
    # Greyscale, 16 bits.
    rows = [bytes(v for x in range(W) for v in (rgba(x, y)[0], 99)) for y in range(H)]
    save("grey16", png(0, 16, rows, 2), [premultiply(255, *(3 * [rgba(x, y)[0]])) for y in range(H) for x in range(W)])
    # Greyscale with alpha, 8 bits.
    rows = [bytes(v for x in range(W) for v in (rgba(x, y)[1], rgba(x, y)[3])) for y in range(H)]
    save("greyalpha8", png(4, 8, rows, 2), [premultiply(rgba(x, y)[3], *(3 * [rgba(x, y)[1]])) for y in range(H) for x in range(W)])
    # Palette, 4 bits, with transparency for the first entries.
    palette = bytes(v for i in range(16) for v in ((i * 16) & 255, (255 - i * 9) & 255, (i * 41) & 255))
    trns = bytes([0, 64, 128, 200])
    values = [[(x * 3 + y) % 16 for x in range(W)] for y in range(H)]
    rows = [pack_bits(row, 4) for row in values]
    alpha = lambda i: trns[i] if i < len(trns) else 255
    save("palette4", png(3, 4, rows, 1, palette=palette, trns=trns),
         [premultiply(alpha(i), palette[i * 3], palette[i * 3 + 1], palette[i * 3 + 2]) for row in values for i in row])
    # Interlaced: refused.
    rows = [bytes(v for x in range(W) for v in rgba(x, y)) for y in range(H)]
    save("interlaced", png(6, 8, rows, 4, interlace=1), None)
    # rgba8 shrunk to 5x3 by area averaging.
    scaled = []
    for y in range(3):
        top, bottom = y * H // 3, max((y + 1) * H // 3, y * H // 3 + 1)
        for x in range(5):
            left, right = x * W // 5, max((x + 1) * W // 5, x * W // 5 + 1)
            covered = [rgba8[r * W + c] for r in range(top, bottom) for c in range(left, right)]
            channel = lambda shift: sum((p >> shift) & 255 for p in covered) // len(covered)
            scaled.append(channel(24) << 24 | channel(16) << 16 | channel(8) << 8 | channel(0))
    open(os.path.join(HERE, "scaled.argb"), "wb").write(struct.pack("<%dI" % len(scaled), *scaled))


if __name__ == "__main__":
    main()
