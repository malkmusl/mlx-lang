import re
# Makes std/src/shaping/nametable.mlx of HarfBuzz 8.3's
# hb-ot-name-language-static.hh: the languages of the Windows and Macintosh
# language codes of name records.
import os
# (tools/harfbuzz/generate_tables.sh runs it: SRC is HarfBuzz 8.3's src
# directory, WORK where it put the preprocessed tables.)
SRC = os.environ['HB_SRC']
WORK = os.environ.get('HB_WORK', '.')
src = open(SRC + '/hb-ot-name-language-static.hh').read()

def table(name):
    a = src.index(name + '[] =')
    b = src.index('};', a)
    pairs = re.findall(r'\{\s*(0x[0-9a-fA-F]+|\d+),\s*"([^"]*)"\}', src[a:b])
    return [(int(c, 0), l) for c, l in pairs]

def emit(name, pairs):
    codes = [c for c, _ in pairs]
    assert codes == sorted(codes) and len(set(codes)) == len(codes), name
    text = ''.join(l + '\\x00' for _, l in pairs)
    offsets = []
    pos = 0
    for _, l in pairs:
        offsets.append(pos)
        pos += len(l) + 1
    out = ['pub const %s_COUNT: u32 = %d\n' % (name, len(codes))]
    out.append('pub var %s_CODES = [%d]u16{\n' % (name, len(codes)))
    for i in range(0, len(codes), 16):
        out.append('    ' + ', '.join(str(c) for c in codes[i:i + 16]) + ',\n')
    out.append('}\n')
    out.append('pub var %s_OFFSETS = [%d]u16{\n' % (name, len(offsets)))
    for i in range(0, len(offsets), 16):
        out.append('    ' + ', '.join(str(c) for c in offsets[i:i + 16]) + ',\n')
    out.append('}\n')
    out.append('pub const %s_LANGUAGES: []const u8 = "%s"\n' % (name, text))
    return ''.join(out)

ms = table('_hb_ms_language_map')
mac = table('_hb_mac_language_map')
out = ['''// The languages of name records (HarfBuzz 8.3's
// hb-ot-name-language-static.hh): for each Windows (MS) and Macintosh
// language code, sorted, the language tag (zero-terminated, at its offset
// in the LANGUAGES string).

''']
out.append(emit('MS', ms))
out.append('\n')
out.append(emit('MAC', mac))
open('std/src/shaping/nametable.mlx', 'w').write(''.join(out))
print(len(ms), len(mac))
