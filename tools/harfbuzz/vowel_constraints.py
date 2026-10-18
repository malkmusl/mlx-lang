import re
# Makes std/src/shaping/voweltable.mlx of HarfBuzz 8.3's generated
# hb-ot-shaper-vowel-constraints.cc: per script, the vowel sequences that
# look like another vowel and get a dotted circle inserted between them.
import os
# (tools/harfbuzz/generate_tables.sh runs it: SRC is HarfBuzz 8.3's src
# directory, WORK where it put the preprocessed tables.)
SRC = os.environ['HB_SRC']
WORK = os.environ.get('HB_WORK', '.')
NAMES = {
    'DEVANAGARI': 'Deva', 'BENGALI': 'Beng', 'GURMUKHI': 'Guru', 'GUJARATI': 'Gujr',
    'ORIYA': 'Orya', 'TAMIL': 'Taml', 'TELUGU': 'Telu', 'KANNADA': 'Knda',
    'MALAYALAM': 'Mlym', 'SINHALA': 'Sinh', 'BRAHMI': 'Brah', 'KHOJKI': 'Khoj',
    'KHUDAWADI': 'Sind', 'TIRHUTA': 'Tirh', 'MODI': 'Modi', 'TAKRI': 'Takr',
}
src = open(SRC + '/hb-ot-shaper-vowel-constraints.cc').read()
blocks = re.split(r'\n    case HB_SCRIPT_(\w+):\n', src)
rules = []
for n in range(1, len(blocks), 2):
    name = blocks[n]
    body = blocks[n + 1]
    tag = NAMES[name]
    tagv = (ord(tag[0]) << 24) | (ord(tag[1]) << 16) | (ord(tag[2]) << 8) | ord(tag[3])
    first = None
    pending = []
    lines = body.split('\n')
    k = 0
    while k < len(lines):
        line = lines[k]
        if re.match(r'^\t  case ', line):
            first = [int(x, 16) for x in re.findall(r'case 0x([0-9A-F]+)u:', line)]; pending = []; k += 1; continue
        if re.match(r'^\t      case ', line):
            pending += [int(x, 16) for x in re.findall(r'case 0x([0-9A-F]+)u:', line)]
            k += 1; continue
        if 'matched = true' in line and pending:
            for f in first:
                for s in pending: rules.append((tagv, f, s, 0))
            pending = []; k += 1; continue
        m = re.search(r'matched = 0x([0-9A-F]+)u == buffer->cur \(1\)\.codepoint;', line)
        if m:
            for f in first: rules.append((tagv, f, int(m.group(1), 16), 0))
            k += 1; continue
        m = re.search(r'if \(0x([0-9A-F]+)u == buffer->cur \(1\)\.codepoint &&', line)
        if m:
            m2 = re.search(r'0x([0-9A-F]+)u == buffer->cur \(2\)\.codepoint\)', lines[k + 2])
            for f in first: rules.append((tagv, f, int(m.group(1), 16), int(m2.group(1), 16)))
            k += 3; continue
        k += 1
vals = []
for r in rules: vals += list(r)
lines = []
for i in range(0, len(vals), 8):
    lines.append('    ' + ', '.join('0x%X' % v for v in vals[i:i + 8]) + ',')
out = '''// Vowel sequences that look like another vowel (the data of HarfBuzz
// 8.3's hb-ot-shaper-vowel-constraints.cc, from the Universal Shaping
// Engine's script development specification): per rule, the script tag,
// the first and second characters and a third one (0: none). A dotted
// circle is inserted before the last character of a matching sequence.
pub var RULES = [%d]u32{
%s
}
''' % (len(vals), '\n'.join(lines))
open('std/src/shaping/voweltable.mlx', 'w').write(out)
print(len(rules))
