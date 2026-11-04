#!/usr/bin/env python3
"""Materializes std.vulkan.video (std/src/vulkan/video.mlx) from the Vulkan
video codec registry (video.xml): the StdVideo* types the H.264 and H.265
decode extensions take (vk_video/vulkan_video_codec_h264std*.h and
h265std*.h), as Mlx structures with C's layout: enumerations as u32,
bit-field flags packed into u32 words from bit 0 up, arrays and pointers
as they are. Run from the repository root; `--check` verifies the
committed module is current."""
import re
import sys
import xml.etree.ElementTree as ET

SOURCE = 'std/registry/vulkan/video.xml'
OUTPUT = 'std/src/vulkan/video.mlx'
SCALARS = {'uint8_t': 'u8', 'int8_t': 'i8', 'uint16_t': 'u16', 'int16_t': 'i16',
           'uint32_t': 'u32', 'int32_t': 'i32', 'uint64_t': 'u64', 'int64_t': 'i64'}
WANTED = re.compile(r'StdVideo(Decode)?H26[45]')


def wanted(name):
    return bool(WANTED.match(name))


def main():
    check = '--check' in sys.argv
    root = ET.parse(SOURCE).getroot()
    constants = []
    for enum in root.iter('enum'):
        name = enum.get('name')
        if name and enum.get('value') is not None and name.startswith(('STD_VIDEO_H264', 'STD_VIDEO_H265', 'STD_VIDEO_DECODE_H264', 'STD_VIDEO_DECODE_H265')) and enum.get('type'):
            constants.append((name, enum.get('value'), SCALARS.get(enum.get('type'), 'u32')))
    values = {n: int(v, 0) for n, v, _ in constants}
    enums = {}
    for block in root.iter('enums'):
        if wanted(block.get('name') or ''):
            enums[block.get('name')] = [(e.get('name'), e.get('value')) for e in block.findall('enum') if e.get('value') is not None]
    types = {}
    order = []
    for ty in root.iter('type'):
        name = ty.get('name')
        if not name or not wanted(name):
            continue
        if ty.get('category') == 'struct':
            members = []
            for m in ty.findall('member'):
                text = ''.join(t for t in m.itertext() if t is not None)
                comment = m.find('comment')
                if comment is not None and comment.text:
                    text = text.replace(comment.text, '')
                members.append((m.find('type').text, m.find('name').text, text))
            types[name] = members
            order.append(name)
    out = []
    out.append('// std.vulkan.video: the Vulkan video codec registry\'s types for H.264 and')
    out.append('// H.265 decoding (vk_video/vulkan_video_codec_h264std.h, _h264std_decode.h,')
    out.append('// _h265std.h, _h265std_decode.h), as the VK_KHR_video_decode_h264 and')
    out.append('// _h265 structures of std.vulkan point to them: enumerations as u32')
    out.append('// constants, flag bit-fields packed into a u32 from bit 0 (the C ABI),')
    out.append('// arrays and pointers as in C. Materialized from')
    out.append('// std/registry/vulkan/video.xml by video_materialize.py; do not edit.')
    out.append('')
    out.append('// Constants')
    for name, value, kind in constants:
        out.append('pub const %s: %s = %s' % (name, kind, value))
    out.append('')
    for name, entries in enums.items():
        out.append('// %s' % name)
        out.append('pub const %s = u32' % name)
        for n, v in entries:
            if v.startswith('0x7FFFFFFF') or v.startswith('0x7fffffff'):
                v = '2147483647'
            if v.startswith('-'):
                out.append('pub const %s: u32 = 4294967296 - %s' % (n, v[1:]))
            else:
                out.append('pub const %s: u32 = %s' % (n, v))
        out.append('')

    def mlx_type(ctype, name, text):
        # Arrays: name[A][B] in the text.
        dims = re.findall(r'\[([A-Za-z0-9_]+)\]', text.split(name, 1)[1] if name in text else '')
        base = SCALARS.get(ctype)
        if base is None:
            if ctype in enums:
                base = 'u32'
            elif ctype in types:
                base = ctype
            else:
                raise SystemExit('unknown type ' + ctype)
        pointer = '*' in text.split(name, 1)[0]
        const = 'const' in text.split(name, 1)[0]
        result = base
        for d in reversed(dims):
            result = '[%s]%s' % (values.get(d, d), result)
        if pointer:
            result = ('?*const ' if const else '?*') + result
        return result

    for name in order:
        members = types[name]
        out.append('pub const %s = struct {' % name)
        bit = 0
        word = 0
        flags = []
        for ctype, mname, text in members:
            m = re.search(r':\s*(\d+)', text)
            if m:
                width = int(m.group(1))
                if bit + width > 32:
                    out.append('    bits%d: u32' % word)
                    word += 1
                    bit = 0
                flags.append((mname, word, bit, width))
                bit += width
                continue
            if bit > 0:
                out.append('    bits%d: u32' % word)
                word += 1
                bit = 0
            out.append('    %s: %s' % (mname, mlx_type(ctype, mname, text)))
        if bit > 0:
            out.append('    bits%d: u32' % word)
        out.append('}')
        for mname, w, b, width in flags:
            out.append('// %s.%s: bits%d bit %d, %d wide' % (name, mname, w, b, width))
            out.append('pub const %s_%s_SHIFT: u32 = %d' % (name, mname, b))
            if width == 1:
                out.append('pub const %s_%s: u32 = %d' % (name, mname, 1 << b))
        out.append('')
    text = '\n'.join(out) + '\n'
    if check:
        if open(OUTPUT).read() != text:
            print('%s is not current: run std/registry/vulkan/video_materialize.py' % OUTPUT)
            sys.exit(1)
        return
    open(OUTPUT, 'w').write(text)
    print('materialized std.vulkan.video into ' + OUTPUT)


if __name__ == '__main__':
    main()
