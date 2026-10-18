#!/usr/bin/env python3
"""Writes compiler/capi/harfbuzz/exports.mlx: for each function the API
modules (compiler/capi/harfbuzz/api) declare, an exported function calling
the std.shaping function of the same name (hb_ dropped, camel case:
hb_buffer_add_utf8 is bufferAddUtf8), found in std/src/shaping. Functions
of the C layer itself (ft.mlx, icu.mlx) and the adapters below are left to
their modules. With --check, says what is declared but defined nowhere and
exits 1 if anything is.

    python3 tools/harfbuzz_exports.py [--check]
"""
import glob, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
API = os.path.join(ROOT, 'compiler/capi/harfbuzz/api')
STD = os.path.join(ROOT, 'std/src/shaping')
CAPI = os.path.join(ROOT, 'compiler/capi/harfbuzz')

# C functions whose std.shaping counterpart takes other types.
ADAPTERS = {
    'hb_tag_from_string': ('common', 'str: usize, len: i32', 'u32', 'std_common.tagFromString(str, @intCast(i64, len))'),
    'hb_language_from_string': ('common', 'str: usize, len: i32', 'usize', 'std_common.languageFromString(str, @intCast(i64, len))'),
    'hb_language_get_default': ('common', '', 'usize', 'std_common.languageGetDefault()'),
    'hb_script_from_iso15924_tag': ('common', 'tag: u32', 'u32', 'std_common.scriptFromIso15924Tag(tag)'),
    'hb_feature_from_string': ('common', 'str: usize, len: i32, feature: usize', 'i32', 'if std_common.featureFromString(str, @intCast(i64, len), feature) { 1 } else { 0 }'),
    'hb_variation_from_string': ('common', 'str: usize, len: i32, variation: usize', 'i32', 'if std_common.variationFromString(str, @intCast(i64, len), variation) { 1 } else { 0 }'),
    'hb_map_create': ('set', '', 'usize', 'std_set.mapCreate()'),
    'hb_map_get_empty': ('set', '', 'usize', 'std_set.mapGetEmpty()'),
    'hb_map_destroy': ('set', 'p: usize', 'void', 'std_set.mapDestroy(p)'),
    'hb_map_clear': ('set', 'p: usize', 'void', 'std_set.mapClear(p)'),
    'hb_map_set': ('set', 'p: usize, k: u32, v: u32', 'void', 'std_set.mapSet(p, k, v)'),
    'hb_map_get': ('set', 'p: usize, k: u32', 'u32', 'std_set.mapGet(p, k)'),
    'hb_map_del': ('set', 'p: usize, k: u32', 'void', 'std_set.mapDel(p, k)'),
    'hb_map_update': ('set', 'p: usize, other: usize', 'void', 'std_set.mapUpdate(p, other)'),
    'hb_map_has': ('set', 'p: usize, k: u32', 'i32', 'std_set.mapContains(p, k)'),
    'hb_map_next': ('set', 'p: usize, idx: usize, key: usize, value: usize', 'i32', 'std_set.mapIterate(p, idx, key, value)'),
}

def camel(s):
    parts = [p for p in s.split('_') if p]
    return parts[0] + ''.join(p[:1].upper() + p[1:] for p in parts[1:])

declared = []
for f in sorted(glob.glob(API + '/*.mlx')):
    for name in re.findall(r'^pub extern\("c"\) fn (hb_\w+)\(', open(f).read(), re.M):
        if name not in declared:
            declared.append(name)

defined = {}
for f in sorted(glob.glob(STD + '/*.mlx')):
    mod = os.path.basename(f)[:-4]
    for name, params, ret in re.findall(r'^pub fn (\w+)\((.*?)\) -> (\w+) \{', open(f).read(), re.M):
        defined.setdefault(name, (mod, params, ret))

# Exported by the C layer's own modules.
own = set()
for f in glob.glob(CAPI + '/*.mlx'):
    if os.path.basename(f) == 'exports.mlx':
        continue
    own.update(re.findall(r'^export fn (hb_\w+)\(', open(f).read(), re.M))

entries = []
missing = []
for name in declared:
    if name in own:
        continue
    if name in ADAPTERS:
        mod, params, ret, call = ADAPTERS[name]
        entries.append((mod, name, params, ret, call))
        continue
    target = camel(name[3:])
    if target not in defined:
        missing.append(name)
        continue
    mod, params, ret = defined[target]
    args = ', '.join(p.split(':')[0].strip() for p in params.split(',') if p.strip())
    entries.append((mod, name, params, ret, 'std_%s.%s(%s)' % (mod, target, args)))

if '--check' in sys.argv:
    for name in missing:
        print('declared, not defined:', name)
    sys.exit(1 if missing else 0)

mods = sorted(set(e[0] for e in entries))
out = ['// The C API: one exported function per HarfBuzz function, each a thin',
       '// call into std.shaping (after setup.ensure() has handed the library',
       "// C's memory functions and locale). Written by tools/harfbuzz_exports.py",
       '// from the API modules; not to be edited by hand.',
       'const setup = @import("./setup.mlx")']
for mod in mods:
    out.append('const std_%s = @import("std.shaping.%s")' % (mod, mod))
for mod in mods:
    out.append('')
    out.append('// std.shaping.%s' % mod)
    for m_, name, params, ret, call in entries:
        if m_ != mod:
            continue
        body = call if ret == 'void' else 'return ' + call
        out.append('export fn %s(%s) -> %s {\n    setup.ensure()\n    %s\n}' % (name, params, ret, body))
open(CAPI + '/exports.mlx', 'w').write('\n'.join(out) + '\n')
print('%d exported, %d declared but not defined yet' % (len(entries), len(missing)))
