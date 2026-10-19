#!/usr/bin/env python3
# Source pictures for the WebP corpus (tools/check_webp.sh): raw RGBA
# files with their sizes in the name (name_WxH.rgba), synthetic and
# photographic, opaque and not, into the directory given.
#
#   sources.py OUT-DIRECTORY [PHOTO.jpg]
import os, random, sys
from PIL import Image, ImageDraw

OUT = sys.argv[1] + "/"
os.makedirs(OUT, exist_ok=True)
random.seed(7)


def save(name, img):
    img = img.convert("RGBA")
    w, h = img.size
    open(OUT + "%s_%dx%d.rgba" % (name, w, h), "wb").write(img.tobytes())


def gradient(w, h, alpha=False):
    img = Image.new("RGBA", (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            a = 255 if not alpha else (x * 255 // max(1, w - 1)) ^ (y & 0x3f)
            px[x, y] = ((x * 7) & 255, (y * 5) & 255, ((x + y) * 3) & 255, a & 255)
    return img


def noise(w, h, alpha=False):
    img = Image.new("RGBA", (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            px[x, y] = (random.randrange(256), random.randrange(256), random.randrange(256), random.randrange(256) if alpha else 255)
    return img


def shapes(w, h, alpha=False):
    img = Image.new("RGBA", (w, h), (255, 255, 255, 0 if alpha else 255))
    d = ImageDraw.Draw(img)
    for k in range(12):
        x0, y0 = random.randrange(w), random.randrange(h)
        x1, y1 = x0 + random.randrange(w // 2 + 1), y0 + random.randrange(h // 2 + 1)
        colour = (random.randrange(256), random.randrange(256), random.randrange(256), random.randrange(80, 256) if alpha else 255)
        if k % 2:
            d.ellipse([x0, y0, x1, y1], fill=colour)
        else:
            d.rectangle([x0, y0, x1, y1], fill=colour)
    for k in range(6):
        d.line([random.randrange(w), random.randrange(h), random.randrange(w), random.randrange(h)], fill=(0, 0, 0, 255), width=2)
    return img


def palette(w, h, colours):
    img = Image.new("RGBA", (w, h))
    px = img.load()
    cols = [(random.randrange(256), random.randrange(256), random.randrange(256), 255) for _ in range(colours)]
    for y in range(h):
        for x in range(w):
            px[x, y] = cols[((x // 3) + (y // 2) * 5) % colours]
    return img


save("grad", gradient(64, 48))
save("grada", gradient(37, 29, True))
save("noise", noise(33, 17))
save("noisea", noise(20, 21, True))
save("shapes", shapes(120, 90))
save("shapesa", shapes(97, 61, True))
save("pal2", palette(41, 23, 2))
save("pal4", palette(30, 30, 4))
save("pal16", palette(53, 19, 16))
save("pal200", palette(70, 40, 200))
save("tiny", gradient(1, 1))
save("thin", gradient(1, 37))
save("wide", gradient(77, 1))
save("odd", shapes(17, 33, True))
if len(sys.argv) > 2:
    photo = Image.open(sys.argv[2]).convert("RGB")
    save("photo", photo.resize((160, 120)))
    p2 = photo.resize((131, 97)).convert("RGBA")
    mask = gradient(131, 97, True).split()[3]
    p2.putalpha(mask)
    save("photoa", p2)
