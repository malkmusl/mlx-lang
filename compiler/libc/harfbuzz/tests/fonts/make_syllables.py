import struct, sys
# Builds a small TrueType test font (no outlines) for the syllable-based
# shapers: every character of some ranges gets a glyph, and GSUB has the
# scripts mym2, khmr, java, bali, tibt, sinh and dev2 with features whose
# lookups substitute some characters (single) and sequences (ligatures),
# so that reordering and per-syllable feature masks show in the output.
RANGES = [(0x0020, 0x007E), (0x0900, 0x097F), (0x0D80, 0x0DFF), (0x0F00, 0x0FFF), (0x1000, 0x109F),
          (0x1780, 0x17FF), (0x1B00, 0x1B7F), (0xA980, 0xA9DF), (0x200B, 0x200D), (0x25CC, 0x25CC),
          (0x034F, 0x034F), (0x00A0, 0x00A0)]
cps = []
for a, b in RANGES:
    cps += list(range(a, b + 1))
gid = {cp: i + 1 for i, cp in enumerate(cps)}
nchars = len(cps) + 1
alts = []

def alt():
    alts.append(len(alts))
    return nchars + len(alts) - 1

def g(cp):
    return gid[cp]

# Features: tag -> list of lookups; a lookup is ('single', {glyph: glyph})
# or ('liga', [([glyphs], glyph)]).
F = {}
def single(tag, cpl):
    m = {}
    for cp in cpl:
        m[g(cp)] = alt()
    F.setdefault(tag, []).append(('single', m))
def liga(tag, seqs):
    F.setdefault(tag, []).append(('liga', [([g(c) for c in s], alt()) for s in seqs]))

# Khmer
liga('pref', [[0x17D2, 0x179A]])
liga('blwf', [[0x17D2, c] for c in (0x1780, 0x1781, 0x1782, 0x1784, 0x1789, 0x178A, 0x1793, 0x1794, 0x179B, 0x179F)])
single('abvf', [0x17C6, 0x17CB])
single('pstf', [0x17B6])
single('cfar', [0x17C1, 0x17B6, 0x17BB, 0x17BC, 0x17BD, 0x17D2])
single('pres', [0x17C1, 0x17C2, 0x17C3])
single('abvs', [0x17B7, 0x17B8, 0x17B9, 0x17BA, 0x17CB, 0x17C9, 0x17CA])
single('blws', [0x17BB, 0x17BC, 0x17BD])
single('psts', [0x17C7])
liga('clig', [[0x1780, 0x17B6], [0x1794, 0x17B6]])
# Myanmar
liga('rphf', [[0x1004, 0x103A, 0x1039]])
single('pref', [0x103C])
liga('blwf', [[0x1039, c] for c in (0x1000, 0x1001, 0x1002, 0x1010, 0x1019, 0x101C)])
single('pstf', [0x103B])
single('pres', [0x1031, 0x103C])
single('abvs', [0x102D, 0x102E, 0x1032, 0x1036])
single('blws', [0x102F, 0x1030, 0x103D, 0x103E, 0x1037])
single('psts', [0x102B, 0x102C, 0x1038])
# Javanese, Balinese, Tibetan, Sinhala, Devanagari (USE and Indic)
liga('rphf', [[0xA982], [0x1B03], [0x0DBB, 0x0DCA, 0x200D], [0x0930, 0x094D]])
liga('pref', [[0xA9C0, 0xA9BF]])
liga('blwf', [[0xA9C0, c] for c in (0xA98F, 0xA9A0, 0xA9A4)] + [[0x1B44, c] for c in (0x1B13, 0x1B15, 0x1B2B)] + [[0x094D, 0x0930]])
liga('half', [[c, 0x094D] for c in (0x0915, 0x0924, 0x0928, 0x0938)] + [[0x1B13, 0x1B44]])
liga('pstf', [[0x1B44, 0x1B2D]])
liga('vatu', [[0x0924, 0x094D, 0x0930]])
liga('cjct', [[0x0915, 0x094D, 0x0937]])
single('abvs', [0xA9B6, 0xA9B7, 0x1B36, 0x0F72, 0x0F7A, 0x0947])
single('blws', [0xA9B8, 0x1B38, 0x0F71, 0x0F74, 0x0F90, 0x0F92])
single('pres', [0xA9BC, 0x1B3E, 0x093F])
single('psts', [0xA9B4, 0x1B35, 0x093E])
single('haln', [0xA9C0, 0x1B44, 0x094D])
single('init', [0x1B05])
single('abvf', [0x0F80])
single('rkrf', [0x0931])
single('nukt', [0x093C])
single('akhn', [0x0915])
single('locl', [0x0F40])
single('ccmp', [0x0F00])

numGlyphs = nchars + len(alts)

def u16(v): return struct.pack('>H', v)
def i16(v): return struct.pack('>h', v)
def u32(v): return struct.pack('>I', v)

def coverage(glyphs):
    glyphs = sorted(glyphs)
    return u16(1) + u16(len(glyphs)) + b''.join(u16(x) for x in glyphs)

def single_subtable(m):
    keys = sorted(m)
    cov = coverage(keys)
    head = 6 + 2 * len(keys)
    return u16(2) + u16(head) + u16(len(keys)) + b''.join(u16(m[k]) for k in keys) + cov

def liga_subtable(seqs):
    firsts = sorted(set(s[0][0] for s in seqs))
    sets = []
    for f in firsts:
        ligs = [(s, r) for s, r in seqs if s[0] == f]
        ligs.sort(key=lambda x: -len(x[0]))
        tables = [u16(r) + u16(len(s)) + b''.join(u16(x) for x in s[1:]) for s, r in ligs]
        off = 2 + 2 * len(tables)
        offs = []
        body = b''
        for t in tables:
            offs.append(off + len(body)); body += t
        sets.append(u16(len(tables)) + b''.join(u16(o) for o in offs) + body)
    head = 6 + 2 * len(sets)
    body = b''
    offs = []
    for s in sets:
        offs.append(head + len(body)); body += s
    cov = coverage(firsts)
    covoff = head + len(body)
    return u16(1) + u16(covoff) + u16(len(sets)) + b''.join(u16(o) for o in offs) + body + cov

lookups = []
features = []
for tag in sorted(F):
    idx = []
    for kind, data in F[tag]:
        idx.append(len(lookups))
        lookups.append((1, single_subtable(data)) if kind == 'single' else (4, liga_subtable(data)))
    features.append((tag, idx))

def lookup_table(kind, sub):
    return u16(kind) + u16(0) + u16(1) + u16(8) + sub

lt = [lookup_table(k, s) for k, s in lookups]
head = 2 + 2 * len(lt)
lookup_list = u16(len(lt))
body = b''
for t in lt:
    lookup_list += u16(head + len(body)); body += t
lookup_list += body

ft = [u16(0) + u16(len(idx)) + b''.join(u16(i) for i in idx) for tag, idx in features]
head = 2 + 6 * len(ft)
feature_list = u16(len(ft))
body = b''
for (tag, idx), t in zip(features, ft):
    feature_list += tag.encode() + u16(head + len(body)); body += t
feature_list += body

langsys = u16(0) + u16(0xFFFF) + u16(len(features)) + b''.join(u16(i) for i in range(len(features)))
script = u16(4) + u16(0) + langsys
scripts = sorted(['DFLT', 'bali', 'dev2', 'java', 'khmr', 'mym2', 'sinh', 'tibt'])
head = 2 + 6 * len(scripts)
script_list = u16(len(scripts))
for i in range(len(scripts)):
    script_list += scripts[i].encode() + u16(head + i * len(script))
script_list += script * len(scripts)

gsub = u32(0x00010000) + u16(10) + u16(10 + len(script_list)) + u16(10 + len(script_list) + len(feature_list)) + script_list + feature_list + lookup_list

# cmap format 12
groups = []
for cp in cps:
    if groups and groups[-1][1] == cp - 1 and groups[-1][2] + (cp - groups[-1][0]) == gid[cp]:
        groups[-1][1] = cp
    else:
        groups.append([cp, cp, gid[cp]])
sub12 = u16(12) + u16(0) + u32(16 + 12 * len(groups)) + u32(0) + u32(len(groups)) + b''.join(u32(a) + u32(b) + u32(c) for a, b, c in groups)
cmap = u16(0) + u16(1) + u16(3) + u16(10) + u32(12) + sub12

upem = 1000
headt = u32(0x00010000) + u32(0x00010000) + u32(0) + u32(0x5F0F3CF5) + u16(0) + u16(upem) + b'\0' * 16 + i16(0) + i16(-200) + i16(1000) + i16(800) + u16(0) + u16(8) + i16(2) + i16(0) + i16(0)
hhea = u32(0x00010000) + i16(800) + i16(-200) + i16(0) + u16(1000) + i16(0) + i16(0) + i16(1000) + i16(1) + i16(0) + i16(0) + b'\0' * 8 + i16(0) + u16(numGlyphs)
maxp = u32(0x00005000) + u16(numGlyphs)
hmtx = b''.join(u16(500 + (i % 7) * 10) + i16(0) for i in range(numGlyphs))
post = u32(0x00030000) + u32(0) + i16(-100) + i16(50) + u32(0) + u32(0) + u32(0) + u32(0) + u32(0)
loca = b''.join(u16(0) for i in range(numGlyphs + 1))
glyf = b''

tables = {'GSUB': gsub, 'cmap': cmap, 'head': headt, 'hhea': hhea, 'maxp': maxp, 'hmtx': hmtx, 'post': post, 'loca': loca, 'glyf': glyf}
tags = sorted(tables)
out = u32(0x00010000) + u16(len(tags)) + u16(128) + u16(3) + u16(len(tags) * 16 - 128)
offset = 12 + 16 * len(tags)
data = b''
for t in tags:
    d = tables[t]
    out += t.encode() + u32(0) + u32(offset + len(data)) + u32(len(d))
    data += d + b'\0' * ((4 - len(d) % 4) % 4)
open(sys.argv[1], 'wb').write(out + data)
print(numGlyphs, len(lookups), len(features))
