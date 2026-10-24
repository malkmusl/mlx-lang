import re
# Makes std/src/shaping/indictable.mlx of HarfBuzz 8.3's preprocessed
# hb-ot-shaper-indic-table.cc: the categories (low byte) and positions
# (high byte) of characters for the Indic, Khmer and Myanmar shapers.
import os
# (tools/harfbuzz/generate_tables.sh runs it: SRC is HarfBuzz 8.3's src
# directory, WORK where it put the preprocessed tables.)
SRC = os.environ['HB_SRC']
WORK = os.environ.get('HB_WORK', '.')
pos = ['POS_START', 'POS_RA_TO_BECOME_REPH', 'POS_PRE_M', 'POS_PRE_C', 'POS_BASE_C', 'POS_AFTER_MAIN', 'POS_ABOVE_C', 'POS_BEFORE_SUB', 'POS_BELOW_C', 'POS_AFTER_SUB', 'POS_BEFORE_POST', 'POS_POST_C', 'POS_AFTER_POST', 'POS_SMVD', 'POS_END']
src = open(WORK + '/indic-table.i').read()

def ev(expr):
    e = expr
    for n, p in enumerate(pos):
        e = re.sub(r'\b%s\b' % p, str(n), e)
    e = re.sub(r'(\d+)u\b', r'\1', e)
    return eval(e)

a = src.index('static const uint16_t indic_table[] = {')
b = src.index('};', a)
items = re.findall(r'\(\(\d+u\) \| \(\(\w+\) << 8\)\)', src[a:b])
table = [ev(x) for x in items]
f = src[src.index('hb_indic_get_categories (hb_codepoint_t u)\n{'):]
f = f[:f.index('\n}\n')]
singles = [(int(u, 16), ev(v)) for u, v in re.findall(r'if \(\(u == 0x([0-9A-F]+)u\)\) return (\(\(\d+u\) \| \(\(\w+\) << 8\)\));', f)]
ranges = [(int(x, 16), int(y, 16), int(o)) for x, y, o in re.findall(r'hb_in_range<hb_codepoint_t> \(u, 0x([0-9A-F]+)u, 0x([0-9A-F]+)u\)\) return indic_table\[u - 0x[0-9A-F]+u \+ (\d+)\];', f)]
default = ev(re.search(r'return (\(\(\d+u\) \| \(\(\w+\) << 8\)\));\s*$', f.strip() + ';').group(1)) if False else None
m = re.findall(r'  return (\(\(\d+u\) \| \(\(\w+\) << 8\)\));', f)
default = ev(m[-1])

def arr(name, typ, data, per=16):
    lines = []
    for i in range(0, len(data), per):
        lines.append('    ' + ', '.join(str(v) for v in data[i:i + per]) + ',')
    return 'pub var %s = [%d]%s{\n%s\n}\n' % (name, len(data), typ, '\n'.join(lines))

out = ['''// The Indic, Khmer and Myanmar shapers' character categories (low byte)
// and positions (high byte), of HarfBuzz 8.3's hb-ot-shaper-indic-table.cc.
''']
out.append(arr('TABLE', 'u16', table))
flat = []
for x, y, o in ranges: flat += [x, y, o]
out.append('// Ranges of TABLE: first, last, offset.\n' + arr('RANGES', 'u32', flat, 3))
flat = []
for u, v in singles: flat += [u, v]
out.append('// Single characters: character, value.\n' + arr('SINGLES', 'u32', flat, 2))
out.append('pub const DEFAULT: u32 = %d\n' % default)
open('std/src/shaping/indictable.mlx', 'w').write('\n'.join(out))
print(len(table), len(ranges), singles, default)
