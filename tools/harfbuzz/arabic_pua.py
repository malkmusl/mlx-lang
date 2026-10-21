import re
# Makes std/src/shaping/arabicpua.mlx of HarfBuzz 8.3's
# hb-ot-shaper-arabic-pua.hh: the Arabic characters' private-use code
# points in the simplified and traditional Arabic symbol fonts.
import os
# (tools/harfbuzz/generate_tables.sh runs it: SRC is HarfBuzz 8.3's src
# directory, WORK where it put the preprocessed tables.)
SRC = os.environ['HB_SRC']
WORK = os.environ.get('HB_WORK', '.')
src = open(SRC + '/hb-ot-shaper-arabic-pua.hh').read()

def array(name):
    a = src.index('%s[' % name)
    a = src.index('{', a)
    b = src.index('};', a)
    return [int(x) for x in re.findall(r'\d+', src[a:b])]

u8 = array('_hb_arabic_u8')
u16 = array('_hb_arabic_u16')
simp = re.search(r'_hb_arabic_pua_simp_map \(unsigned u\)\n\{\n  return (.*?);', src).group(1)
trad = re.search(r'_hb_arabic_pua_trad_map \(unsigned u\)\n\{\n  return (.*?);', src).group(1)
assert simp == 'u<65277u?_hb_arabic_u16[((_hb_arabic_u8[40+(((_hb_arabic_b4(8+_hb_arabic_u8,((_hb_arabic_b2(_hb_arabic_u8,u>>3>>4>>4))<<4)+((u>>3>>4)&15u)))<<4)+((u>>3)&15u))])<<3)+((u)&7u)]:0', simp
assert trad == 'u<65277u?_hb_arabic_u16[320+(((_hb_arabic_u8[208+(((_hb_arabic_b4(168+_hb_arabic_u8,((_hb_arabic_b4(136+_hb_arabic_u8,u>>2>>4>>4))<<4)+((u>>2>>4)&15u)))<<4)+((u>>2)&15u))])<<2)+((u)&3u))]:0', trad

def arr(name, typ, data, per=20):
    lines = []
    for i in range(0, len(data), per):
        lines.append('    ' + ', '.join(str(v) for v in data[i:i + per]) + ',')
    return 'pub var %s = [%d]%s{\n%s\n}\n' % (name, len(data), typ, '\n'.join(lines))

out = ['''// The private-use code points of Arabic characters in the simplified
// (OS/2 font page 0xB300) and traditional (0xB400) Arabic symbol fonts
// (the data of HarfBuzz 8.3's hb-ot-shaper-arabic-pua.hh): tries over the
// code point's bits.
''']
out.append(arr('U8', 'u8', u8))
out.append(arr('U16', 'u16', u16))
out.append('''fn u8at(i: u32) -> u32 { return @intCast(u32, U8[@intCast(usize, i)]) }
fn b2(base: u32, i: u32) -> u32 { return (u8at(base + (i >> 2)) >> @intCast(u5, (i & 3) << 1)) & 3 }
fn b4(base: u32, i: u32) -> u32 { return (u8at(base + (i >> 1)) >> @intCast(u5, (i & 1) << 2)) & 15 }

// The code point of `u` in a simplified Arabic symbol font (0: none).
pub fn simpMap(u: u32) -> u32 {
    if u >= 65277 { return 0 }
    const l1 = b2(0, u >> 11)
    const l2 = b4(8, (l1 << 4) + ((u >> 7) & 15))
    const l3 = u8at(40 + (l2 << 4) + ((u >> 3) & 15))
    return @intCast(u32, U16[@intCast(usize, (l3 << 3) + (u & 7))])
}

// The code point of `u` in a traditional Arabic symbol font (0: none).
pub fn tradMap(u: u32) -> u32 {
    if u >= 65277 { return 0 }
    const l1 = b4(136, u >> 10)
    const l2 = b4(168, (l1 << 4) + ((u >> 6) & 15))
    const l3 = u8at(208 + (l2 << 4) + ((u >> 2) & 15))
    return @intCast(u32, U16[@intCast(usize, 320 + (l3 << 2) + (u & 3))])
}
''')
open('std/src/shaping/arabicpua.mlx', 'w').write('\n'.join(out))

# Check the transcription against the C expressions over every input.
def b2c(a, i): return (u8[a + (i >> 2)] >> ((i & 3) << 1)) & 3
def b4c(a, i): return (u8[a + (i >> 1)] >> ((i & 1) << 2)) & 15
def simp_c(u):
    return u16[((u8[40+(((b4c(8,((b2c(0,u>>3>>4>>4))<<4)+((u>>3>>4)&15)))<<4)+((u>>3)&15))])<<3)+((u)&7)] if u < 65277 else 0
def trad_c(u):
    return u16[320+(((u8[208+(((b4c(168,((b4c(136,u>>2>>4>>4))<<4)+((u>>2>>4)&15)))<<4)+((u>>2)&15))])<<2)+((u)&3))] if u < 65277 else 0
def simp_m(u):
    if u >= 65277: return 0
    l1 = b2c(0, u >> 11); l2 = b4c(8, (l1 << 4) + ((u >> 7) & 15)); l3 = u8[40 + (l2 << 4) + ((u >> 3) & 15)]
    return u16[(l3 << 3) + (u & 7)]
def trad_m(u):
    if u >= 65277: return 0
    l1 = b4c(136, u >> 10); l2 = b4c(168, (l1 << 4) + ((u >> 6) & 15)); l3 = u8[208 + (l2 << 4) + ((u >> 2) & 15)]
    return u16[320 + (l3 << 2) + (u & 3)]
assert all(simp_c(u) == simp_m(u) and trad_c(u) == trad_m(u) for u in range(70000))
print(len(u8), len(u16))
