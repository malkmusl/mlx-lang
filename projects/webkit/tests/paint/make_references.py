#!/usr/bin/env python3
"""The reference paintings of the paint tests (tools/check_webkit_mlx_paint.sh)
and the image they load, made from what each page says it draws: no
browser's output. Each reference is the 800x600 view, opaque RGB, on
white; a gradient's pixel is its colour at the pixel's centre; a
translucent colour is composited over white (the check allows a small
difference per channel for rounding and dithering).

    projects/webkit/tests/paint/make_references.py   # rewrites *-expected.png, support/quadrants.png
"""
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
WIDTH, HEIGHT = 800, 600


def write_png(path, width, height, rows, alpha):
    """ROWS: lists of (r, g, b[, a]) tuples."""
    raw = bytearray()
    for row in rows:
        raw.append(0)
        for pixel in row:
            raw.extend(pixel)

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, 8, 6 if alpha else 2, 0, 0, 0)
    with open(path, "wb") as out:
        out.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


class Canvas:
    def __init__(self, background=(255, 255, 255)):
        self.pixels = [[background] * WIDTH for _ in range(HEIGHT)]

    def fill(self, x, y, w, h, colour, alpha=1.0):
        for row in range(y, y + h):
            line = self.pixels[row]
            for column in range(x, x + w):
                if alpha >= 1.0:
                    line[column] = colour
                else:
                    line[column] = tuple(round(c * alpha + b * (1 - alpha)) for c, b in zip(colour, line[column]))

    def frame(self, x, y, w, h, top, right, bottom, left, colour):
        """A border of one colour: the outer box less the padding box."""
        self.fill(x, y, w, top, colour)
        self.fill(x, y + h - bottom, w, bottom, colour)
        self.fill(x, y, left, h, colour)
        self.fill(x + w - right, y, right, h, colour)

    def save(self, name):
        write_png(os.path.join(HERE, name), WIDTH, HEIGHT, self.pixels, False)


# support/quadrants.png: 40x40, red, green / blue, yellow at half alpha.
QUADRANTS = [[(255, 0, 0, 255) if x < 20 and y < 20 else (0, 160, 0, 255) if y < 20 else (0, 0, 255, 255) if x < 20 else (255, 255, 0, 128)
              for x in range(40)] for y in range(40)]


def boxes():
    c = Canvas()
    c.fill(10, 10, 100, 50, (255, 0, 0))
    c.fill(130, 10, 50, 120, (0, 160, 0))
    c.fill(10, 80, 100, 100, (0, 0, 255))
    c.fill(35, 105, 50, 50, (255, 255, 0))
    c.fill(200, 150, 80, 80, (0, 128, 128))
    c.fill(240, 190, 80, 80, (128, 0, 128))
    c.fill(400, 10, 100, 100, (255, 0, 0), 0.5)
    c.save("boxes-expected.png")


def borders():
    c = Canvas()
    c.fill(20, 20, 120, 80, (204, 204, 204))
    c.frame(20, 20, 120, 80, 10, 10, 10, 10, (0, 0, 0))
    c.frame(200, 20, 104, 96, 4, 8, 12, 16, (0, 0, 255))
    c.fill(40, 160, 100, 50, (255, 204, 0))
    c.frame(30, 150, 120, 70, 5, 5, 5, 5, (0, 128, 0))
    c.fill(300, 150, 20, 60, (128, 0, 0))
    c.save("borders-expected.png")


def ahem():
    c = Canvas()

    def text(x, y, size, lines, colour):
        for row, line in enumerate(lines):
            for column, character in enumerate(line):
                if character != " ":
                    c.fill(x + column * size, y + row * size, size, size, colour)

    text(10, 10, 20, ["XXX XX"], (0, 0, 128))
    text(10, 50, 40, ["X X"], (0, 128, 0))
    text(200, 10, 10, ["XX", "X X", "XX"], (192, 0, 0))
    c.save("ahem-expected.png")


def gradient():
    c = Canvas()
    for x in range(256):
        v = round(255 * (x + 0.5) / 256)
        c.fill(x, 0, 1, 50, (v, v, v))
    for y in range(256):
        t = (y + 0.5) / 256
        c.fill(300, y, 50, 1, (round(255 * (1 - t)), 0, round(255 * t)))
    c.save("gradient-expected.png")


def image():
    c = Canvas()
    for scale, left in ((1, 20), (2, 100)):
        for y in range(40):
            for x in range(40):
                r, g, b, a = QUADRANTS[y][x]
                c.fill(left + x * scale, 30 + y * scale, scale, scale, (r, g, b), a / 255)
    c.save("image-expected.png")


# std.ui.theme's palettes (std/src/ui/theme.mlx), as the port's theme
# (RenderThemeMlx.cpp, ControlFactoryMlx.cpp) gives them to pages (a
# light page's text black, as pages expect it).
LIGHT = dict(canvas=(255, 255, 255), text=(0, 0, 0), field=(255, 255, 255), control=(150, 154, 166), button=(226, 228, 234))
DARK = dict(canvas=(26, 28, 36), text=(235, 238, 245), field=(40, 43, 54), control=(96, 101, 118), button=(52, 56, 68))
ACCENT = (90, 160, 255)
SELECTION_ALPHA = round(0.38 * 255) / 255


def scheme(dark):
    """scheme.html light (as a page is drawn by default) and with the
    desktop's dark appearance (webcore-dump --dark), in the port's theme:
    the Canvas, CanvasText and ButtonFace colours, the prefers-color-scheme
    box; the text field (Field inside a 1-pixel outline of the control
    colour), the button (ButtonFace), the checked check box (the accent)
    and the select (ButtonFace); the selected Ahem text, its glyphs
    CanvasText on the accent's translucent selection. The rounded corners,
    the check mark and the select's chevron are the pixels pages.txt
    allows."""
    p = DARK if dark else LIGHT
    c = Canvas(p["canvas"])
    c.fill(10, 10, 100, 100, (0, 128, 255) if dark else (255, 128, 0))
    c.fill(130, 10, 100, 100, p["text"])
    c.fill(250, 10, 100, 100, p["button"])
    c.fill(10, 130, 200, 40, p["field"])
    c.frame(10, 130, 200, 40, 1, 1, 1, 1, p["control"])
    c.fill(250, 130, 100, 40, p["button"])
    c.fill(370, 130, 20, 20, ACCENT)
    c.fill(250, 190, 120, 30, p["button"])
    c.fill(10, 190, 60, 20, ACCENT, SELECTION_ALPHA)
    c.fill(10, 190, 20, 20, p["text"])
    c.fill(50, 190, 20, 20, p["text"])
    c.save("scheme-dark-expected.png" if dark else "scheme-expected.png")


if __name__ == "__main__":
    write_png(os.path.join(HERE, "support", "quadrants.png"), 40, 40, QUADRANTS, True)
    boxes()
    borders()
    ahem()
    gradient()
    image()
    scheme(False)
    scheme(True)
