#!/usr/bin/env python3
# Writes tests/abi.c: a C program printing the size of every structure the
# API modules (projects/freetype/api) declare, the offset of each of its
# fields, the value of every constant and enumerator, and the header
# names. Compiled against FreeType's own headers it prints abi.expected;
# compiled by mlx-cc against the generated headers it must print the same.
#
#   python3 projects/freetype/tests/abi.py > projects/freetype/tests/abi.c
import os
import re
import sys

api = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'api')
structs = []
constants = []
strings = []
enumerators = []
for name in sorted(os.listdir(api)):
    if not name.endswith('.mlx') or name == 'ftoption.mlx':
        continue
    lines = open(os.path.join(api, name)).read().split('\n')
    i = 0
    while i < len(lines):
        line = lines[i]
        m = re.match(r'pub const (\w+) = struct \{(\})?\s*$', line)
        if m and m.group(2):
            i += 1
            continue
        if m:
            fields = []
            depth = 0
            i += 1
            while i < len(lines):
                l = lines[i].strip()
                if l == '}':
                    if depth == 0:
                        break
                    depth -= 1
                elif re.match(r'\w+: (union|struct) \{$', l):
                    fields.append(l.split(':')[0])
                    depth += 1
                elif depth == 0 and re.match(r'\w+: ', l):
                    field = l.split(':')[0]
                    if field.endswith('_') and field[:-1] in ('type', 'error'):
                        field = field[:-1]
                    fields.append(field)
                i += 1
            if fields:
                structs.append((m.group(1), fields))
            i += 1
            continue
        m = re.match(r'pub const (\w+) = enum\(c_int\) \{', line)
        if m:
            i += 1
            while i < len(lines) and lines[i].strip() != '}':
                e = re.match(r'\s*(\w+) = ', lines[i])
                if e:
                    enumerators.append(e.group(1))
                i += 1
            i += 1
            continue
        m = re.match(r'pub const (\w+): \[\]const u8 = ', line)
        if m:
            strings.append(m.group(1))
        else:
            m = re.match(r'pub const (\w+): (\w+) = (.*)$', line)
            # (Not the names of the C library's functions and types, as
            # `ft_smalloc` is `malloc`.)
            if m and not (re.match(r'^[A-Za-z_]\w*$', m.group(3).strip()) and not re.match(r'^(FT|TT|T1)_', m.group(3).strip())):
                constants.append(m.group(1))
        i += 1

out = sys.stdout
out.write('/* Made by abi.py of the API modules: the layout of every structure,\n')
out.write('   every constant and enumerator, as the headers give them. */\n')
out.write('#include <ft2build.h>\n')
for h in ['FT_FREETYPE_H', 'FT_ADVANCES_H', 'FT_BBOX_H', 'FT_BDF_H', 'FT_BITMAP_H', 'FT_CID_H', 'FT_COLOR_H',
          'FT_DRIVER_H', 'FT_FONT_FORMATS_H', 'FT_GASP_H', 'FT_GLYPH_H', 'FT_LCD_FILTER_H', 'FT_MULTIPLE_MASTERS_H',
          'FT_MODULE_H', 'FT_OUTLINE_H', 'FT_PARAMETER_TAGS_H', 'FT_SIZES_H', 'FT_SFNT_NAMES_H', 'FT_SYNTHESIS_H',
          'FT_TRIGONOMETRY_H', 'FT_TYPE1_TABLES_H', 'FT_TRUETYPE_IDS_H', 'FT_TRUETYPE_TABLES_H', 'FT_TRUETYPE_TAGS_H',
          'FT_MODULE_ERRORS_H', 'FT_ERRORS_H', 'FT_SYSTEM_H', 'FT_IMAGE_H', 'FT_TYPES_H']:
    out.write('#include %s\n' % h)
out.write('#include <stddef.h>\n#include <stdio.h>\n\n')
out.write('#define S(T) printf("sizeof %s %zu\\n", #T, sizeof(struct T))\n')
out.write('#define F(T, f) printf("offset %s.%s %zu\\n", #T, #f, offsetof(struct T, f))\n')
out.write('#define C(N) printf("%s %lld\\n", #N, (long long)(N))\n')
out.write('int main(void)\n{\n')
for name, fields in structs:
    out.write('    S(%s);\n' % name)
    for f in fields:
        out.write('    F(%s, %s);\n' % (name, f))
for c in constants + enumerators:
    out.write('    C(%s);\n' % c)
out.write('    return 0;\n}\n')
