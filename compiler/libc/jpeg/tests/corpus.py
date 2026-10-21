"""Writes JPEG test images into the directory given (with Pillow, and
ImageMagick's convert for the samplings Pillow does not write): every
sampling, baseline and progressive, sizes that are no multiple of the
blocks, grey, CMYK, qualities from 5 to 100, optimized Huffman tables,
restart intervals, EXIF orientation and an ICC profile in APP2 markers,
and damaged files (changed bytes in the scans, files cut short).

    python3 corpus.py DIRECTORY [small]

`small` writes the few small images tests/images has (decode.expected
is what the system's libjpeg-turbo prints for them)."""
import io, os, random, shutil, subprocess, sys
from PIL import Image, ImageDraw

out = sys.argv[1]
small = len(sys.argv) > 2 and sys.argv[2] == "small"
os.makedirs(out, exist_ok=True)


def picture(w, h, seed, mode="RGB"):
    rnd = random.Random(seed)
    im = Image.new("RGB", (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            px[x, y] = ((x * 255 // max(1, w - 1) + rnd.randint(0, 30)) % 256,
                        (y * 255 // max(1, h - 1) + rnd.randint(0, 30)) % 256,
                        ((x + y) * 7 + rnd.randint(0, 60)) % 256)
    d = ImageDraw.Draw(im)
    for i in range(6):
        x0, y0 = rnd.randint(0, w), rnd.randint(0, h)
        d.ellipse([x0, y0, x0 + rnd.randint(1, w), y0 + rnd.randint(1, h)],
                  fill=(rnd.randint(0, 255), rnd.randint(0, 255), rnd.randint(0, 255)))
    return im.convert(mode) if mode != "RGB" else im


def save(name, im, **options):
    im.save(os.path.join(out, name), "JPEG", **options)


def damage(name, source, seed, changes):
    data = bytearray(open(os.path.join(out, source), "rb").read())
    rnd = random.Random(seed)
    start = data.rfind(b"\xff\xda")
    for i in range(changes):
        p = rnd.randint(start + 20, len(data) - 3)
        data[p] = rnd.randint(0, 255)
    open(os.path.join(out, name), "wb").write(data)


def cut(name, source, fraction):
    data = open(os.path.join(out, source), "rb").read()
    open(os.path.join(out, name), "wb").write(data[: int(len(data) * fraction)])


exif = Image.Exif()
exif[0x112] = 6
profile = bytes(range(256)) * 9

if small:
    save("baseline420.jpg", picture(37, 21, 1), quality=80, subsampling=2)
    save("baseline444.jpg", picture(19, 13, 2), quality=90, subsampling=0)
    save("progressive422.jpg", picture(41, 29, 3), quality=75, subsampling=1, progressive=True)
    save("progressive420.jpg", picture(50, 34, 4), quality=60, progressive=True, exif=exif.tobytes(), icc_profile=profile)
    save("gray.jpg", picture(23, 17, 5, "L"), quality=85)
    save("grayprogressive.jpg", picture(23, 17, 6, "L"), quality=85, progressive=True)
    save("cmyk.jpg", picture(17, 11, 7, "CMYK"), quality=85)
    save("restart.jpg", picture(45, 33, 8), quality=80, restart_marker_blocks=3)
    damage("damaged.jpg", "restart.jpg", 9, 3)
    save("progressiverestart.jpg", picture(45, 33, 10), quality=80, progressive=True, restart_marker_rows=1)
    damage("damagedprogressive.jpg", "progressiverestart.jpg", 11, 4)
    cut("cut.jpg", "baseline420.jpg", 0.8)
    cut("cutprogressive.jpg", "progressive422.jpg", 0.7)
    sys.exit(0)

sizes = [(1, 1), (7, 5), (8, 8), (16, 16), (17, 9), (33, 47), (64, 31), (100, 75), (257, 3), (3, 129)]
for (w, h) in sizes:
    for sub in (0, 1, 2):
        for prog in (False, True):
            name = "s%dx%d_%d%s.jpg" % (w, h, sub, "p" if prog else "")
            save(name, picture(w, h, w * 31 + h + sub), quality=85 if not prog else 70, subsampling=sub, progressive=prog)
for q in (5, 50, 95, 100):
    save("q%d.jpg" % q, picture(61, 43, q), quality=q)
save("gray.jpg", picture(45, 37, 3, "L"), quality=80)
save("grayp.jpg", picture(45, 37, 4, "L"), quality=80, progressive=True)
save("cmyk.jpg", picture(40, 30, 5, "CMYK"), quality=80)
save("cmykp.jpg", picture(40, 30, 5, "CMYK"), quality=80, progressive=True)
save("opt.jpg", picture(120, 90, 6), quality=75, optimize=True)
big = picture(320, 240, 8)
save("big420.jpg", big, quality=90)
save("big420p.jpg", big, quality=90, progressive=True)
save("big444.jpg", big, quality=90, subsampling=0)
save("exif.jpg", picture(30, 20, 9), exif=exif.tobytes(), icc_profile=profile)
im = picture(90, 70, 12)
for blocks in (1, 3, 17):
    save("rst%d.jpg" % blocks, im, quality=80, restart_marker_blocks=blocks)
save("rstrows.jpg", im, quality=80, restart_marker_rows=1, progressive=True)
for base in ("rst3.jpg", "rstrows.jpg"):
    for k in range(4):
        damage("bad%d_%s" % (k, base), base, 100 + k, 3)
    for fraction in (0.3, 0.6, 0.95):
        cut("cut%d_%s" % (int(fraction * 100), base), base, fraction)
cut("cut_header.jpg", "rst3.jpg", 0.05)
if shutil.which("convert"):
    source = os.path.join(out, "source.png")
    picture(77, 53, 11).save(source)
    for factors in ("1x2", "2x1", "4x1", "1x4", "4x2", "2x4", "3x1", "1x1,2x2,1x1", "2x2,1x2,2x1"):
        name = "f" + factors.replace(",", "_") + ".jpg"
        subprocess.run(["convert", source, "-sampling-factor", factors, "-quality", "80", os.path.join(out, name)], check=True)
        subprocess.run(["convert", source, "-sampling-factor", factors, "-quality", "80", "-interlace", "JPEG", os.path.join(out, "p" + name)], check=True)
    os.remove(source)
