import struct, sys, random
# Copies a font with a made-up MATH table (every part of it: constants with
# device tables, italics corrections, top accents, extended shapes, kerning
# with several heights, variants and assemblies with values past int16):
# mkmath.py in out
random.seed(3)
src, dst = sys.argv[1], sys.argv[2]

def u16(v): return struct.pack('>H', v & 0xFFFF)
def i16(v): return struct.pack('>h', v)
def u32(v): return struct.pack('>I', v)

class Blob:
    """A table with subtables placed after the head, offsets patched."""
    def __init__(self):
        self.data = bytearray()
    def add(self, b):
        at = len(self.data)
        self.data += b
        return at

def device(fmt):
    start, end = 10, 20
    n = end - start + 1
    per = {1: 8, 2: 4, 3: 2}[fmt]
    bits = 16 // per
    words = []
    for w in range((n + per - 1) // per):
        v = 0
        for k in range(per):
            d = random.randint(-(1 << (bits - 1)), (1 << (bits - 1)) - 1)
            v = (v << bits) | (d & ((1 << bits) - 1))
        words.append(v)
    return u16(start) + u16(end) + u16(fmt) + b''.join(u16(w) for w in words)

def coverage1(glyphs):
    return u16(1) + u16(len(glyphs)) + b''.join(u16(g) for g in glyphs)

def coverage2(ranges):
    out = u16(2) + u16(len(ranges))
    idx = 0
    for a, b in ranges:
        out += u16(a) + u16(b) + u16(idx)
        idx += b - a + 1
    return out

# A subtable made of a head of value records (value, device offset) whose
# devices follow it.
def records(head_prefix, values, with_devices=True):
    """head_prefix: bytes before the records; values: list of ints. Returns
    bytes with the device tables appended, offsets from the subtable start."""
    head_len = len(head_prefix) + 4 * len(values)
    tail = bytearray()
    recs = b''
    for v in values:
        off = 0
        if with_devices and random.random() < 0.4:
            off = head_len + len(tail)
            tail += device(random.choice([1, 2, 3]))
        recs += i16(v) + u16(off)
    return head_prefix + recs + bytes(tail)

# MathConstants: 2 int16, 2 uint16, 51 records, 1 int16.
values = [random.randint(-500, 1500) for _ in range(51)]
head = i16(80) + i16(60) + u16(1500) + u16(40000)
body = records(head, values)
# radicalDegreeBottomRaisePercent goes right after the records: rebuild.
recs_end = len(head) + 4 * 51
tail = body[recs_end:]
# device offsets must account for the extra 2 bytes
fixed = bytearray(head)
for i in range(51):
    v, off = struct.unpack('>hH', body[len(head) + 4 * i:len(head) + 4 * i + 4])
    if off: off += 2
    fixed += i16(v) + u16(off)
fixed += i16(65) + tail
constants = bytes(fixed)

def italics(glyphs):
    vals = [random.randint(-200, 300) for _ in glyphs]
    cov = coverage1(glyphs)
    body = records(u16(0) + u16(len(vals)), vals)
    return u16(len(body)) + body[2:] + cov

def sub_with_coverage(prefix_after_cov, cov):
    body = prefix_after_cov
    return u16(2 + len(body)) + body + cov

ic_glyphs = list(range(10, 60, 3))
ic_vals = [random.randint(-200, 300) for _ in ic_glyphs]
ic_body = records(u16(len(ic_vals)), ic_vals)
ic = sub_with_coverage(ic_body, coverage1(ic_glyphs))

ta_glyphs = list(range(30, 90, 2))
ta_vals = [random.randint(0, 600) for _ in ta_glyphs]
ta_body = records(u16(len(ta_vals)), ta_vals)
ta = sub_with_coverage(ta_body, coverage2([(30, 30), (32, 32)] + [(g, g) for g in ta_glyphs[2:]]) if False else coverage1(ta_glyphs))

ext = coverage2([(40, 45), (100, 120)])

def mathkern(n):
    heights = sorted(random.sample(range(-400, 900), n))
    kerns = [random.randint(-150, 150) for _ in range(n + 1)]
    return records(u16(n), heights + kerns)

kern_glyphs = list(range(20, 50))
kern_recs = []
kern_tables = bytearray()
base_len = 2 + 2 + 8 * len(kern_glyphs)  # coverage offset, count, records
for g in kern_glyphs:
    offs = []
    for corner in range(4):
        if random.random() < 0.6:
            offs.append(base_len + len(kern_tables))
            kern_tables += mathkern(random.randint(0, 4))
        else:
            offs.append(0)
    kern_recs.append(b''.join(u16(o) for o in offs))
cov = coverage1(kern_glyphs)
kerninfo = u16(base_len + len(kern_tables)) + u16(len(kern_glyphs)) + b''.join(kern_recs) + bytes(kern_tables) + cov

parts = [ic, ta, ext, kerninfo]
glyphinfo = bytearray()
offs = []
at = 8
for p in parts:
    offs.append(at)
    at += len(p)
glyphinfo = b''.join(u16(o) for o in offs) + b''.join(parts)

def assembly(nparts):
    head_len = 6 + 10 * nparts
    dev = b''
    off = 0
    if random.random() < 0.5:
        off = head_len
        dev = device(random.choice([1, 2, 3]))
    out = i16(random.randint(-50, 50)) + u16(off) + u16(nparts)
    for k in range(nparts):
        out += u16(random.randint(1, 300)) + u16(random.randint(0, 200)) + u16(random.randint(0, 200)) + u16(random.choice([300, 900, 40000])) + u16(random.choice([0, 1, 3]))
    return out + dev

def construction(nvar, with_assembly):
    head_len = 4 + 4 * nvar
    asm = assembly(random.randint(1, 5)) if with_assembly else b''
    out = u16(head_len if with_assembly else 0) + u16(nvar)
    for k in range(nvar):
        out += u16(random.randint(1, 300)) + u16(random.choice([500, 1200, 33000]))
    return out + asm

vert = [5, 6, 7, 50]
horiz = [8, 9, 50]
cons = [construction(random.randint(0, 4), random.random() < 0.7) for _ in vert + horiz]
head_len = 10 + 2 * len(cons)
blob = bytearray()
con_offs = []
for c in cons:
    con_offs.append(head_len + len(blob))
    blob += c
vcov_off = head_len + len(blob)
blob += coverage1(vert)
hcov_off = head_len + len(blob)
blob += coverage1(horiz)
variants = u16(0xFFF0) + u16(vcov_off) + u16(hcov_off) + u16(len(vert)) + u16(len(horiz)) + b''.join(u16(o) for o in con_offs) + bytes(blob)

math = u32(0x00010000)
at = 10
math_offs = []
for p in (constants, glyphinfo, variants):
    math_offs.append(at)
    at += len(p)
math += b''.join(u16(o) for o in math_offs) + constants + glyphinfo + variants

# Rebuild the font with MATH replaced (or added).
data = open(src, 'rb').read()
n = struct.unpack('>H', data[4:6])[0]
tables = {}
for i in range(n):
    rec = 12 + i * 16
    tag = data[rec:rec + 4]
    off, length = struct.unpack('>II', data[rec + 8:rec + 16])
    tables[tag] = data[off:off + length]
tables[b'MATH'] = bytes(math)
tags = sorted(tables)
out = data[:4] + u16(len(tags)) + data[6:12]
offset = 12 + 16 * len(tags)
body = b''
for t in tags:
    d = tables[t]
    out += t + u32(0) + u32(offset + len(body)) + u32(len(d))
    body += d + b'\0' * ((4 - len(d) % 4) % 4)
open(dst, 'wb').write(out + body)
print(len(math))
