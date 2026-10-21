import struct, sys, random
# Copies a TrueType font as a variable one: fvar with two axes (wght,
# wdth), avar for wght, and gvar with made-up deltas for the first glyphs
# (shared and embedded peaks, intermediate regions, shared and private
# point numbers, zero, byte and word delta runs, phantom points), no HVAR
# so that advances come from the phantom points: mkvar.py in out [glyphs]
random.seed(4)
src, dst = sys.argv[1], sys.argv[2]
limit = int(sys.argv[3]) if len(sys.argv) > 3 else 400

def u16(v): return struct.pack('>H', v & 0xFFFF)
def i16(v): return struct.pack('>h', v)
def u32(v): return struct.pack('>I', v & 0xFFFFFFFF)

data = open(src, 'rb').read()
n = struct.unpack('>H', data[4:6])[0]
tables = {}
for i in range(n):
    rec = 12 + i * 16
    tag = data[rec:rec + 4]
    off, length = struct.unpack('>II', data[rec + 8:rec + 16])
    tables[tag] = data[off:off + length]
for t in (b'HVAR', b'VVAR', b'MVAR', b'gvar', b'fvar', b'avar', b'STAT'):
    tables.pop(t, None)

head = tables[b'head']
loc_format = struct.unpack('>h', head[50:52])[0]
num_glyphs = struct.unpack('>H', tables[b'maxp'][4:6])[0]
loca = tables[b'loca']
glyf = tables[b'glyf']
def glyph_range(g):
    if loc_format == 0:
        a, b = struct.unpack('>HH', loca[g * 2:g * 2 + 4]); return a * 2, b * 2
    return struct.unpack('>II', loca[g * 4:g * 4 + 8])
def point_count(g):
    a, b = glyph_range(g)
    if b - a < 10: return 0
    nc = struct.unpack('>h', glyf[a:a + 2])[0]
    if nc > 0:
        return struct.unpack('>H', glyf[a + 10 + 2 * (nc - 1):a + 12 + 2 * (nc - 1)])[0] + 1
    if nc == -1:
        p = a + 10; count = 0
        while True:
            flags = struct.unpack('>H', glyf[p:p + 2])[0]
            count += 1
            size = 4 + (4 if flags & 1 else 2)
            if flags & 8: size += 2
            elif flags & 0x40: size += 4
            elif flags & 0x80: size += 8
            p += size
            if not flags & 0x20: break
        return count
    return 0

# fvar: wght 100..400..900, wdth 50..100..200, two named instances.
axes = [(b'wght', 100, 400, 900), (b'wdth', 50, 100, 200)]
fvar = u16(1) + u16(0) + u16(16) + u16(2) + u16(len(axes)) + u16(20) + u16(2) + u16(4 + 4 * len(axes))
for tag, lo, df, hi in axes:
    fvar += tag + u32(lo << 16) + u32(df << 16) + u32(hi << 16) + u16(0) + u16(256)
for inst in ((700, 100), (300, 75)):
    fvar += u16(257) + u16(0) + b''.join(u32(v << 16) for v in inst)
# avar: wght bent, wdth identity.
def f2(v): return i16(int(round(v * 16384)))
avar = u16(1) + u16(0) + u16(0) + u16(2)
avar += u16(5) + f2(-1) + f2(-1) + f2(-0.5) + f2(-0.3) + f2(0) + f2(0) + f2(0.5) + f2(0.7) + f2(1) + f2(1)
avar += u16(3) + f2(-1) + f2(-1) + f2(0) + f2(0) + f2(1) + f2(1)

axis_count = len(axes)
shared = [(1.0, 0.0), (0.0, 1.0), (-1.0, 0.0), (1.0, 1.0), (0.0, -0.5)]

def pack_points(pts):
    out = bytearray()
    if len(pts) < 128: out += bytes([len(pts)])
    else: out += bytes([0x80 | (len(pts) >> 8), len(pts) & 0xFF])
    prev = 0; i = 0
    while i < len(pts):
        run = pts[i:i + random.randint(1, 20)]
        diffs = []
        for p in run:
            diffs.append(p - prev); prev = p
        words = any(d > 255 for d in diffs) or random.random() < 0.2
        out += bytes([(0x80 if words else 0) | (len(run) - 1)])
        for d in diffs: out += u16(d) if words else bytes([d])
        i += len(run)
    return bytes(out)

def pack_deltas(ds):
    out = bytearray(); i = 0
    while i < len(ds):
        if ds[i] == 0:
            j = i
            while j < len(ds) and ds[j] == 0 and j - i < 64: j += 1
            out += bytes([0x80 | (j - i - 1)]); i = j; continue
        j = i
        while j < len(ds) and ds[j] != 0 and j - i < 64: j += 1
        run = ds[i:j]
        words = any(d < -128 or d > 127 for d in run)
        out += bytes([(0x40 if words else 0) | (len(run) - 1)])
        for d in run: out += i16(d) if words else struct.pack('>b', d)
        i = j
    return bytes(out)

def glyph_var_data(count):
    total = count + 4
    use_shared = random.random() < 0.4 and total > 2
    shared_pts = sorted(random.sample(range(total), random.randint(1, total))) if use_shared else None
    ntuples = random.randint(1, 4)
    headers = []
    blobs = []
    for t in range(ntuples):
        flags = 0
        peak = None
        if random.random() < 0.5:
            flags |= 0x8000
            peak = [random.choice([-1, -0.5, 0, 0.5, 1]) for _ in range(axis_count)]
            if all(p == 0 for p in peak): peak[0] = 1
            index = 0
        else:
            index = random.randrange(len(shared))
        interm = None
        if random.random() < 0.3:
            flags |= 0x4000
            pk = peak if peak else shared[index]
            interm = ([min(p, 0) if random.random() < 0.5 else min(p, 0) / 2 for p in pk],
                      [max(p, 0) if random.random() < 0.5 else (max(p, 0) + 1) / 2 for p in pk])
        private = None
        if not use_shared or random.random() < 0.5:
            if random.random() < 0.3:
                private = []  # all points
            else:
                private = sorted(random.sample(range(total), random.randint(1, total)))
            flags |= 0x2000
        pts = private if private is not None else shared_pts
        npts = total if (pts is not None and len(pts) == 0) or pts is None else len(pts)
        if pts is None: npts = total
        body = (pack_points(private) if private is not None else b'')
        xs = [random.choice([0, 0, random.randint(-60, 60), random.randint(-400, 400)]) for _ in range(npts)]
        ys = [random.choice([0, 0, random.randint(-60, 60), random.randint(-400, 400)]) for _ in range(npts)]
        body += pack_deltas(xs) + pack_deltas(ys)
        hdr = u16(len(body)) + u16(flags | index)
        if peak: hdr += b''.join(f2(p) for p in peak)
        if interm: hdr += b''.join(f2(p) for p in interm[0]) + b''.join(f2(p) for p in interm[1])
        headers.append(hdr); blobs.append(body)
    head_len = 4 + sum(len(h) for h in headers)
    serialized = (pack_points(shared_pts) if use_shared else b'') + b''.join(blobs)
    return u16((0x8000 if use_shared else 0) | ntuples) + u16(head_len) + b''.join(headers) + serialized

datas = []
for g in range(num_glyphs):
    if g < limit and g % 5 != 3:
        d = glyph_var_data(point_count(g))
    else:
        d = b''
    if len(d) % 2: d += b'\0'
    datas.append(d)
shared_tuples = b''.join(f2(v) for t in shared for v in t)
offsets = []
pos = 0
for d in datas:
    offsets.append(pos); pos += len(d)
offsets.append(pos)
gvar_head_len = 20 + 4 * (num_glyphs + 1)
gvar = u16(1) + u16(0) + u16(axis_count) + u16(len(shared)) + u32(gvar_head_len) + u16(num_glyphs) + u16(1)
gvar += u32(gvar_head_len + len(shared_tuples)) + b''.join(u32(o) for o in offsets) + shared_tuples + b''.join(datas)

tables[b'fvar'] = fvar
tables[b'avar'] = avar
tables[b'gvar'] = gvar
tags = sorted(tables)
out = data[:4] + u16(len(tags)) + data[6:12]
offset = 12 + 16 * len(tags)
body = b''
for t in tags:
    d = tables[t]
    out += t + u32(0) + u32(offset + len(body)) + u32(len(d))
    body += d + b'\0' * ((4 - len(d) % 4) % 4)
open(dst, 'wb').write(out + body)
print(len(gvar))
