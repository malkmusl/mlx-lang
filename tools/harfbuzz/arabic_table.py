import re
# Makes std/src/shaping/arabictable.mlx of HarfBuzz 8.3's
# hb-ot-shaper-arabic-table.hh: Arabic joining types, the presentation forms
# of the fallback shaper and its ligatures.
import os
# (tools/harfbuzz/generate_tables.sh runs it: SRC is HarfBuzz 8.3's src
# directory, WORK where it put the preprocessed tables.)
SRC = os.environ['HB_SRC']
WORK = os.environ.get('HB_WORK', '.')
src = open(SRC + '/hb-ot-shaper-arabic-table.hh').read()

def body(name):
    a = src.index(name)
    a = src.index('{', src.index('=', a))
    depth = 0
    for i in range(a, len(src)):
        if src[i] == '{': depth += 1
        elif src[i] == '}':
            depth -= 1
            if depth == 0:
                return src[a + 1:i]

def strip_comments(s):
    s = re.sub(r'/\*.*?\*/', '', s, flags=re.S)
    return re.sub(r'#define[^\n]*', '', s)

vals = {'A': 4, 'DR': 5, 'C': 3, 'D': 3, 'L': 1, 'R': 2, 'T': 7, 'U': 0, 'X': 8}
jt = strip_comments(body('joining_table[]'))
joining = [vals[t] for t in re.findall(r'\b(DR|[ACDLRTUX])\b', jt)]
offsets = {k.lower(): int(v) for k, v in re.findall(r'#define joining_offset_(0x[0-9a-fA-F]+)u (\d+)', src)}
ranges = re.findall(r'hb_in_range<hb_codepoint_t> \(u, (0x[0-9A-F]+)u, (0x[0-9A-F]+)u\)\) return joining_table\[u - 0x[0-9A-F]+u \+ joining_offset_(0x[0-9a-f]+)u\]', src)

def nums(s):
    return [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]+)u', strip_comments(s))]

shaping = nums(body('shaping_table[][4]'))
def groups(name, n, ncomp):
    text = strip_comments(body(name))
    out = []
    for first, inner in re.findall(r'\{\s*(0x[0-9A-Fa-f]+)u,\s*\{(.*?)\}\s*\}', text, re.S):
        entries = re.findall(r'\{\s*\{([^}]*)\},\s*(0x[0-9A-Fa-f]+)u\s*\}', inner)
        out.append(int(first, 16))
        k = 0
        for comps, l in entries:
            cs = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]+)u', comps)]
            out += cs + [int(l, 16)]
            k += 1
        out += [0] * ((n - k) * (ncomp + 1))
    return out
lig = groups('} ligature_table[]', 14, 1)
ligmark = groups('} ligature_mark_table[]', 5, 1)
lig3 = groups('} ligature_3_table[]', 3, 2)

def arr(name, typ, data, per=16):
    lines = []
    for i in range(0, len(data), per):
        lines.append('    ' + ', '.join('0x%X' % v if v > 9 else str(v) for v in data[i:i + per]) + ',')
    return 'pub var %s = [%d]%s{\n%s\n}\n' % (name, len(data), typ, '\n'.join(lines))

out = ['''// The Arabic shaper's data (of HarfBuzz 8.3's hb-ot-shaper-arabic-table.hh,
// from Unicode 15.1): joining types (0 U, 1 L, 2 R, 3 D, 4 Alaph, 5
// Dalath-Rish, 7 T, 8 X: by general category), the presentation forms of
// the letters U+0621..U+06D3 (initial, medial, final, isolated), and the
// ligatures of presentation forms the fallback shaper makes.
''']
out.append(arr('JOINING', 'u8', joining, 32))
rng = []
for a, b, off in ranges:
    rng += [int(a, 16), int(b, 16), offsets[off.lower()]]
out.append('// The ranges of JOINING: first, last, offset.\n' + arr('JOINING_RANGES', 'u32', rng, 3))
out.append('pub const SHAPING_FIRST: u32 = 0x0621\npub const SHAPING_LAST: u32 = 0x06D3\n')
out.append(arr('SHAPING', 'u16', shaping, 4))
out.append('// Two-glyph ligatures: a first glyph, then 14 pairs of second glyph and\n// ligature (0: none).\n' + arr('LIGATURES', 'u16', lig, 29))
out.append('// Mark ligatures: a first mark, then 5 pairs of second mark and ligature.\n' + arr('LIGATURES_MARK', 'u16', ligmark, 11))
out.append('// Three-glyph ligatures: a first glyph, then 3 triplets of second and\n// third glyph and ligature.\n' + arr('LIGATURES_3', 'u16', lig3, 10))
open('std/src/shaping/arabictable.mlx', 'w').write('\n'.join(out))
print(len(joining), len(rng) // 3, len(shaping) // 4, len(lig) // 29, len(ligmark) // 11, len(lig3) // 10)
