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


def scheme(dark, name=None):
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
    c.save(name or ("scheme-dark-expected.png" if dark else "scheme-expected.png"))


def forced_dark_colour(colour):
    """A colour as forced dark paints it (MlxDarkMode.cpp's
    mlxForcedDarkColor): its OKLab lightness inverted between the dark
    background's (0.215) and the text's (0.945), its hue and chroma kept
    (less chroma until it is in sRGB)."""
    def linear(c):
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

    def encoded(c):
        return 12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055

    def to_rgb(l_, a_, b_):
        l3 = (l_ + 0.3963377774 * a_ + 0.2158037573 * b_) ** 3
        m3 = (l_ - 0.1055613458 * a_ - 0.0638541728 * b_) ** 3
        s3 = (l_ - 0.0894841775 * a_ - 1.2914855480 * b_) ** 3
        return (4.0767416621 * l3 - 3.3077115913 * m3 + 0.2309699292 * s3,
                -1.2684380046 * l3 + 2.6097574011 * m3 - 0.3413193965 * s3,
                -0.0041960863 * l3 - 0.7034186147 * m3 + 1.7076147010 * s3)

    r, g, b = (linear(c / 255) for c in colour)
    l = (0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b) ** (1 / 3)
    m = (0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b) ** (1 / 3)
    s = (0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b) ** (1 / 3)
    lab = (0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
           1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
           0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s)
    lightness = 0.215 + (0.945 - 0.215) * (1 - min(max(lab[0], 0), 1))
    inside = lambda rgb: all(-0.0001 <= c <= 1.0001 for c in rgb)
    rgb = to_rgb(lightness, lab[1], lab[2])
    if not inside(rgb):
        low, high = 0.0, 1.0
        for _ in range(16):
            middle = (low + high) / 2
            if inside(to_rgb(lightness, lab[1] * middle, lab[2] * middle)):
                low = middle
            else:
                high = middle
        rgb = to_rgb(lightness, lab[1] * low, lab[2] * low)
    return tuple(round(encoded(min(max(c, 0), 1)) * 255) for c in rgb)


def forcedark():
    """forcedark.html with --dark --force-dark: the white canvas, the black
    Ahem text and the boxes in forced dark's colours; the image (2x,
    nearest neighbour) with its own pixels, its translucent quadrant over
    the darkened canvas."""
    canvas = forced_dark_colour((255, 255, 255))
    c = Canvas(canvas)
    text = forced_dark_colour((0, 0, 0))
    for column in (0, 1, 3):
        c.fill(10 + column * 20, 10, 20, 20, text)
    c.fill(10, 50, 100, 50, forced_dark_colour((0x33, 0x66, 0xcc)))
    c.fill(130, 50, 100, 50, forced_dark_colour((0xff, 0xe0, 0xa0)))
    for y in range(40):
        for x in range(40):
            r, g, b, a = QUADRANTS[y][x]
            c.fill(10 + x * 2, 120 + y * 2, 2, 2, (r, g, b), a / 255)
    c.save("forcedark-forcedark-expected.png")


def mediadark():
    """mediadark.html with --dark --force-dark: its own dark colours."""
    c = Canvas((0x20, 0x20, 0x20))
    c.fill(10, 10, 100, 50, (0x80, 0xa0, 0xff))
    c.save("mediadark-forcedark-expected.png")


def composited(dark=False):
    """composited.html: layers translated, scaled, turned, translucent;
    with --dark --force-dark (DARK) each layer's colours in forced dark's
    (the layers' contents painted dark), composited over the darkened
    canvas."""
    colour = forced_dark_colour if dark else (lambda c: c)
    c = Canvas(colour((255, 255, 255)))
    c.fill(10, 20, 100, 50, colour((255, 0, 0)))
    c.fill(150, 10, 100, 80, colour((0, 160, 0)))
    c.fill(330, 10, 40, 100, colour((0, 0, 255)))
    c.fill(10, 200, 100, 100, colour((0, 0, 255)), 0.5)
    # (The group composited at half: the green box over the red one.)
    c.fill(200, 200, 80, 100, colour((255, 0, 0)), 0.5)
    c.fill(280, 200, 120, 100, colour((0, 128, 0)), 0.5)
    c.save("composited-forcedark-expected.png" if dark else "composited-expected.png")


def fixed():
    """fixed.html: the bands scrolled up by 250, the fixed box over them."""
    c = Canvas()
    c.fill(0, 50, 800, 100, (0, 0, 255))
    c.fill(0, 250, 800, 100, (0, 160, 0))
    c.fill(20, 20, 200, 60, (255, 0, 0))
    c.save("fixed-expected.png")


def overflow():
    """overflow.html: the box's rows from 150 to 350 of its contents."""
    c = Canvas()
    c.fill(50, 50, 300, 50, (0, 160, 0))
    c.fill(50, 100, 300, 100, (0, 0, 255))
    c.fill(50, 200, 300, 50, (255, 255, 0))
    c.save("overflow-expected.png")


def perspective():
    """perspective.html: the card twice its size about the stage's centre."""
    c = Canvas()
    c.fill(300, 200, 200, 200, (0, 0, 255))
    c.save("perspective-expected.png")


if __name__ == "__main__":
    write_png(os.path.join(HERE, "support", "quadrants.png"), 40, 40, QUADRANTS, True)
    boxes()
    borders()
    ahem()
    gradient()
    image()
    scheme(False)
    scheme(True)
    scheme(True, "scheme-forcedark-expected.png")
    forcedark()
    mediadark()
    composited()
    composited(True)
    fixed()
    overflow()
    perspective()
