#!/usr/bin/env python3
"""The Settings app's icon (projects/desktop/settings/org.mlx.settings.png,
128x128): a silver gear with the desktop's accent blue at its hub on a
graphite rounded square, as the file manager's icon is a folder on blue;
drawn at 4x and scaled down.

    python3 tools/icons/settings.py projects/desktop/settings/org.mlx.settings.png
"""
import math, sys
from PIL import Image, ImageDraw, ImageFilter

S = 4
N = 128 * S

def lerp(a, b, t): return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(len(a)))

def gradient(size, top, bottom):
    image = Image.new('RGBA', (size, size))
    draw = ImageDraw.Draw(image)
    for y in range(size):
        draw.line([(0, y), (size, y)], fill=lerp(top, bottom, y / (size - 1)))
    return image

def mask_rounded(size, inset, radius):
    mask = Image.new('L', (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle([inset, inset, size - 1 - inset, size - 1 - inset], radius, fill=255)
    return mask

def gear(size, cx, cy, outer, inner, teeth, hole):
    """A gear's mask: `teeth` trapezoid teeth from `inner` to `outer`, a
    hole."""
    mask = Image.new('L', (size, size), 0)
    draw = ImageDraw.Draw(mask)
    pitch = 2 * math.pi / teeth
    points = []
    for tooth in range(teeth):
        middle = tooth * pitch - math.pi / 2
        for angle, r in ((middle - pitch * 0.30, inner), (middle - pitch * 0.19, outer), (middle + pitch * 0.19, outer), (middle + pitch * 0.30, inner)):
            points.append((cx + r * math.cos(angle), cy + r * math.sin(angle)))
    draw.polygon(points, fill=255)
    draw.ellipse([cx - inner, cy - inner, cx + inner, cy + inner], fill=255)
    draw.ellipse([cx - hole, cy - hole, cx + hole, cy + hole], fill=0)
    return mask

icon = Image.new('RGBA', (N, N), (0, 0, 0, 0))
# The tile: graphite, lighter at the top (as the files icon is blue).
tile = gradient(N, (126, 134, 151), (58, 64, 78))
icon.paste(tile, (0, 0), mask_rounded(N, 3 * S, 28 * S))
cx = cy = N / 2
outer, inner, hole = 47 * S, 37 * S, 14 * S
teeth = 8
shape = gear(N, cx, cy + 2 * S, outer, inner, teeth, hole)
# Its shadow, below and soft.
shadow = Image.new('RGBA', (N, N), (12, 16, 26, 0))
shadow.putalpha(shape.filter(ImageFilter.GaussianBlur(4 * S)).point(lambda v: v * 0.55))
offset = Image.new('RGBA', (N, N), (0, 0, 0, 0))
offset.paste(shadow, (0, 3 * S))
icon = Image.alpha_composite(icon, offset)
# The gear: silver, lighter at the top.
shape = gear(N, cx, cy, outer, inner, teeth, hole)
metal = gradient(N, (250, 251, 253), (198, 203, 214))
icon.paste(metal, (0, 0), shape)
# A groove ring on its face, and the accent blue in its hole.
ring = Image.new('L', (N, N), 0)
draw = ImageDraw.Draw(ring)
draw.ellipse([cx - 29 * S, cy - 29 * S, cx + 29 * S, cy + 29 * S], fill=255)
draw.ellipse([cx - 25 * S, cy - 25 * S, cx + 25 * S, cy + 25 * S], fill=0)
icon.paste(Image.new('RGBA', (N, N), (160, 167, 181, 255)), (0, 0), ring)
accent = Image.new('L', (N, N), 0)
ImageDraw.Draw(accent).ellipse([cx - hole, cy - hole, cx + hole, cy + hole], fill=255)
icon.paste(gradient(N, (120, 186, 255), (52, 120, 230)), (0, 0), accent)
inner_hole = Image.new('L', (N, N), 0)
ImageDraw.Draw(inner_hole).ellipse([cx - 6 * S, cy - 6 * S, cx + 6 * S, cy + 6 * S], fill=255)
icon.paste(Image.new('RGBA', (N, N), (244, 247, 252, 255)), (0, 0), inner_hole)

icon.resize((128, 128), Image.LANCZOS).save(sys.argv[1], optimize=True)
