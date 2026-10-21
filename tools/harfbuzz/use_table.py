import re
# Makes std/src/shaping/usetable.mlx of HarfBuzz 8.3's preprocessed
# hb-ot-shaper-use-table.hh (use-table.i): the Universal Shaping Engine's
# character categories as a five-level trie.
import os
# (tools/harfbuzz/generate_tables.sh runs it: SRC is HarfBuzz 8.3's src
# directory, WORK where it put the preprocessed tables.)
SRC = os.environ['HB_SRC']
WORK = os.environ.get('HB_WORK', '.')
src = open(WORK + '/use-table.i').read()

def array(name):
    a = src.index('%s[' % name)
    a = src.index('{', a)
    b = src.index('};', a)
    return [int(x) for x in re.findall(r'\d+', src[a:b])]

u8 = array('hb_use_u8')
u16 = array('hb_use_u16')
expr = re.search(r'hb_use_get_category \(unsigned u\)\n\{\n  return (.*?);\n\}', src).group(1)
assert expr == 'u<921600u?hb_use_u8[2809+(((hb_use_u8[593+(((hb_use_u16[((hb_use_u8[113+(((hb_use_b4(hb_use_u8,u>>1>>3>>3>>5))<<5)+((u>>1>>3>>3)&31u))])<<3)+((u>>1>>3)&7u)])<<3)+((u>>1)&7u))])<<1)+((u)&1u))]:0u', expr

def arr(name, typ, data, per=24):
    lines = []
    for i in range(0, len(data), per):
        lines.append('    ' + ', '.join(str(v) for v in data[i:i + per]) + ',')
    return 'pub var %s = [%d]%s{\n%s\n}\n' % (name, len(data), typ, '\n'.join(lines))

out = ['''// The Universal Shaping Engine's character categories (the data of
// HarfBuzz 8.3's hb-ot-shaper-use-table.hh, from the Unicode character
// database and the USE's overrides): a trie of five levels over the code
// point's bits.
''']
out.append(arr('U8', 'u8', u8))
out.append(arr('U16', 'u16', u16))
out.append('''fn b4(i: u32) -> u32 { return (@intCast(u32, U8[@intCast(usize, i >> 1)]) >> @intCast(u5, (i & 1) << 2)) & 15 }

fn u8at(i: u32) -> u32 { return @intCast(u32, U8[@intCast(usize, i)]) }

// The category of `u` (0: other).
pub fn category(u: u32) -> u32 {
    if u >= 921600 { return 0 }
    const l1 = b4(u >> 12)
    const l2 = u8at(113 + (l1 << 5) + ((u >> 7) & 31))
    const l3 = @intCast(u32, U16[@intCast(usize, (l2 << 3) + ((u >> 4) & 7))])
    const l4 = u8at(593 + (l3 << 3) + ((u >> 1) & 7))
    return u8at(2809 + (l4 << 1) + (u & 1))
}
''')
open('std/src/shaping/usetable.mlx', 'w').write('\n'.join(out))
print(len(u8), len(u16))
