import struct, sys
# Copies a font, renaming script tags in the ScriptLists of GSUB and GPOS
# (each rename must keep the list sorted): patchtags.py in out old=new...
src, dst = sys.argv[1], sys.argv[2]
renames = [a.split('=') for a in sys.argv[3:]]
data = bytearray(open(src, 'rb').read())
numTables = struct.unpack('>H', data[4:6])[0]
for i in range(numTables):
    rec = 12 + i * 16
    tag = bytes(data[rec:rec + 4])
    off = struct.unpack('>I', data[rec + 8:rec + 12])[0]
    if tag not in (b'GSUB', b'GPOS'):
        continue
    sl = off + struct.unpack('>H', data[off + 4:off + 6])[0]
    count = struct.unpack('>H', data[sl:sl + 2])[0]
    for k in range(count):
        at = sl + 2 + k * 6
        for old, new in renames:
            if bytes(data[at:at + 4]) == old.encode():
                data[at:at + 4] = new.encode()
    tags = [bytes(data[sl + 2 + k * 6:sl + 6 + k * 6]) for k in range(count)]
    assert tags == sorted(tags), tags
open(dst, 'wb').write(data)
