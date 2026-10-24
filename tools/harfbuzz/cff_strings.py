import re
# Makes std/src/shaping/cfftable.mlx of HarfBuzz 8.3's hb-ot-cff1-std-str.hh
# and hb-ot-cff1-table.cc: the 391 standard strings of CFF, the standard
# encoding (code to string id) and the expert charset (glyph to string id).
import os
# (tools/harfbuzz/generate_tables.sh runs it: SRC is HarfBuzz 8.3's src
# directory, WORK where it put the preprocessed tables.)
SRC = os.environ['HB_SRC']
WORK = os.environ.get('HB_WORK', '.')
w = SRC + '/'
strs = re.findall(r'_S\("([^"]*)"\)', open(w + 'hb-ot-cff1-std-str.hh').read())
assert len(strs) == 391, len(strs)
cc = open(w + 'hb-ot-cff1-table.cc').read()

def array(name):
    a = cc.index(name + ' [] =')
    a = cc.index('{', a)
    b = cc.index('};', a)
    return [int(x) for x in re.findall(r'\d+', cc[a + 1:b])]

std_enc = array('standard_encoding_to_sid')
assert len(std_enc) == 256
expert = array('expert_charset_to_sid')
subset = array('expert_subset_charset_to_sid')
# The sorted (sid, gid) pairs are the inverses of the expert charsets.
for charset, name in ((expert, 'expert_charset_sid_to_gid'), (subset, 'expert_subset_charset_sid_to_gid')):
    pairs = array(name)
    assert len(pairs) % 2 == 0 and len(pairs) // 2 == len(charset) - 1
    for sid, gid in zip(pairs[0::2], pairs[1::2]):
        assert charset[gid] == sid
    assert pairs[0::2] == sorted(set(pairs[0::2]))

def arr(name, typ, data, per=20):
    lines = []
    for i in range(0, len(data), per):
        lines.append('    ' + ', '.join(str(v) for v in data[i:i + per]) + ',')
    return 'pub var %s = [%d]%s{\n%s\n}\n' % (name, len(data), typ, '\n'.join(lines))

offsets = [0]
for s in strs:
    offsets.append(offsets[-1] + len(s))
text = ''.join(strs)
out = ['''// The data of CFF (HarfBuzz 8.3's hb-ot-cff1-std-str.hh and
// hb-ot-cff1-table.cc): the standard strings, the standard encoding and
// the expert charsets.

// The 391 standard strings, one after the other; string i is
// STRINGS[STRING_OFFSETS[i]..STRING_OFFSETS[i + 1]].
pub const STRING_COUNT: u32 = 391
pub const STRINGS: []const u8 = "%s"
''' % text]
out.append(arr('STRING_OFFSETS', 'u16', offsets))
out.append('// The string id of each code of the standard encoding (0: none).')
out.append(arr('STANDARD_ENCODING_TO_SID', 'u8', std_enc))
out.append('// The string id of each glyph of the expert charset.')
out.append(arr('EXPERT_CHARSET_TO_SID', 'u16', expert))
out.append('// The string id of each glyph of the expert subset charset.')
out.append(arr('EXPERT_SUBSET_CHARSET_TO_SID', 'u16', subset))
out.append('''// The string id of `code` in the standard encoding, 0xFFFFFFFF past it.
pub fn standardEncodingSid(code: u32) -> u32 {
    if code >= 256 { return 0xFFFFFFFF }
    return @intCast(u32, STANDARD_ENCODING_TO_SID[@intCast(usize, code)])
}

// The string id of `glyph` in the expert charset (0: none).
pub fn expertCharsetSid(glyph: u32) -> u32 {
    if glyph >= %d { return 0 }
    return @intCast(u32, EXPERT_CHARSET_TO_SID[@intCast(usize, glyph)])
}

// The glyph of string id `sid` in the expert charset (0: none).
pub fn expertCharsetGlyph(sid: u32) -> u32 {
    if sid == 0 { return 0 }
    var g: u32 = 1
    while g < %d {
        if @intCast(u32, EXPERT_CHARSET_TO_SID[@intCast(usize, g)]) == sid { return g }
        g += 1
    }
    return 0
}

// The string id of `glyph` in the expert subset charset (0: none).
pub fn expertSubsetCharsetSid(glyph: u32) -> u32 {
    if glyph >= %d { return 0 }
    return @intCast(u32, EXPERT_SUBSET_CHARSET_TO_SID[@intCast(usize, glyph)])
}

// The glyph of string id `sid` in the expert subset charset (0: none).
pub fn expertSubsetCharsetGlyph(sid: u32) -> u32 {
    if sid == 0 { return 0 }
    var g: u32 = 1
    while g < %d {
        if @intCast(u32, EXPERT_SUBSET_CHARSET_TO_SID[@intCast(usize, g)]) == sid { return g }
        g += 1
    }
    return 0
}
''' % (len(expert), len(expert), len(subset), len(subset)))
open('std/src/shaping/cfftable.mlx', 'w').write('\n'.join(out))
