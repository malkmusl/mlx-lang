import re
# Makes std/src/shaping/tagtable.mlx of HarfBuzz 8.3's hb-ot-tag-table.hh:
# the OpenType language tags of BCP 47 languages (two and three letters) and
# the languages of the complex cases (zh-*, sr-Latn...), as data and code.
import os
# (tools/harfbuzz/generate_tables.sh runs it: SRC is HarfBuzz 8.3's src
# directory, WORK where it put the preprocessed tables.)
SRC = os.environ['HB_SRC']
WORK = os.environ.get('HB_WORK', '.')
src = open(SRC + '/hb-ot-tag-table.hh').read()

def tagval(s):
    chars = re.findall(r"'(.)'", s)
    if len(chars) != 4:
        raise Exception(s)
    v = 0
    for ch in chars:
        v = (v << 8) | ord(ch)
    return v

def table(name):
    a = src.index('static const LangTag %s[] = {' % name)
    b = src.index('};', a)
    out = []
    for line in src[a:b].split('\n'):
        m = re.match(r"\s*\{(HB_TAG\([^)]*\)),\s*(HB_TAG\([^)]*\)|HB_TAG_NONE)\s*\}", line)
        if not m:
            continue
        lang = tagval(m.group(1))
        tag = 0 if m.group(2) == 'HB_TAG_NONE' else tagval(m.group(2))
        out.append((lang, tag))
    return out

l2 = table('ot_languages2')
l3 = table('ot_languages3')

a = src.index('hb_ot_tags_from_complex_language (const char')
b = src.index('\n}\n', a)
body = src[a:b]
rules = []
first = None
blocks = re.split(r'return true;', body)
for blk in blocks:
    # The first character of the switch case this block is under.
    cases = re.findall(r"case '(.)':", blk)
    if cases:
        first = cases[-1]
    conds = blk[blk.rfind('if ('):] if 'if (' in blk else ''
    tags = ['%08x' % tagval(t) for t in re.findall(r"HB_TAG\('.','.','.','.'\)", conds)]
    if not tags:
        continue
    m = re.search(r'if \(subtag_matches \(p, limit, "([^"]*)", \d+\)\)', conds)
    if m:
        rules.append(('p', '-', m.group(1), '', tags)); continue
    m = re.search(r'if \(lang_matches \(&lang_str\[1\], limit, "([^"]*)", \d+\)\)', conds)
    if m:
        rules.append(('l', first, m.group(1), '', tags)); continue
    m = re.search(r'if \(0 == strncmp \(&lang_str\[1\], "([^"]*)", \d+\)\s*&& subtag_matches \(lang_str, limit, "([^"]*)", \d+\)\)', conds, re.S)
    if m:
        rules.append(('n', first, m.group(1), m.group(2), tags)); continue
    m = re.search(r'if \(0 == strcmp \(&lang_str\[1\], "([^"]*)"\)\)', conds)
    if m:
        rules.append(('s', first, m.group(1), '', tags)); continue
    raise Exception('unparsed: ' + conds[:300])

a = src.index('hb_ot_ambiguous_tag_to_language (hb_tag_t tag)')
b = src.index('\n}\n', a)
amb = re.findall(r"case (HB_TAG\([^)]*\)):.*?\n\s*return hb_language_from_string \(\"([^\"]*)\", -1\);", src[a:b])

def arr(name, pairs):
    vals = []
    for x, y in pairs:
        vals += ['0x%08X' % x, '0x%08X' % y]
    lines = []
    for i in range(0, len(vals), 8):
        lines.append('    ' + ', '.join(vals[i:i + 8]) + ',')
    return 'pub var %s = [%d]u32{\n%s\n}\n' % (name, len(vals), '\n'.join(lines))

out = []
out.append('''// The OpenType language system tags of BCP 47 languages (the data of
// HarfBuzz 8.3's hb-ot-tag-table.hh, from the OpenType language system
// registry and the IANA language subtag registry): pairs of a language's
// first subtag (as a tag, two or three letters and spaces) and its
// OpenType tag (0: the language has none, though the tag of the same
// letters is another language's), sorted by language.
''')
out.append(arr('LANGUAGES2', l2))
out.append(arr('LANGUAGES3', l3))
out.append('''// Languages of more than one subtag. One rule a line, tab-separated: the
// kind (p: the subtag at the first hyphen matches; l: the language from
// its second letter matches, up to a hyphen; n: it starts so and a subtag
// matches; s: it is so from its second letter), its first letter, the
// strings, and the tags (hexadecimal, comma-separated), tried in order.
''')
text = ''.join('%s\\t%s\\t%s\\t%s\\t%s\\n' % (k, c, s1, s2, ','.join(t)) for k, c, s1, s2, t in rules)
out.append('pub const COMPLEX: []const u8 = "%s"\n' % text)
out.append('''// The languages of OpenType tags that more than one language maps to:
// the tag (hexadecimal) and the language, a line each.
''')
text = ''.join('%08x\\t%s\\n' % (tagval(t), l) for t, l in amb)
out.append('pub const AMBIGUOUS: []const u8 = "%s"\n' % text)
open('std/src/shaping/tagtable.mlx', 'w').write('\n'.join(out))
print(len(l2), len(l3), len(rules), len(amb))
