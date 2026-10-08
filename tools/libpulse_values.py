#!/usr/bin/env python3
"""libpulse's functions on values, ours against PulseAudio's.

Calls the same functions with the same arguments in both libraries
(ctypes) and compares what they return and what they write: sample specs,
channel maps, volumes (dB, linear, balance, fade, remapping), property lists,
formats, UTF-8, times, error texts. Prints each difference; exits 1 when
there is one.

    tools/libpulse_values.py OURS.so THEIRS.so
"""
import ctypes
import sys

ours = ctypes.CDLL(sys.argv[1], mode=ctypes.RTLD_LOCAL)
theirs = ctypes.CDLL(sys.argv[2], mode=ctypes.RTLD_LOCAL)
failures = []
checked = 0

P = ctypes.c_void_p
S = ctypes.c_char_p
U32 = ctypes.c_uint32
I32 = ctypes.c_int
U64 = ctypes.c_uint64
I64 = ctypes.c_int64
SZ = ctypes.c_size_t
F64 = ctypes.c_double
F32 = ctypes.c_float


def fn(lib, name, restype, *argtypes):
    f = getattr(lib, name)
    f.restype = restype
    f.argtypes = list(argtypes)
    return f


def same(label, a, b):
    global checked
    checked += 1
    if a != b:
        failures.append(f"{label}: ours {a!r}, theirs {b!r}")


def both(name, restype, *argtypes):
    return fn(ours, name, restype, *argtypes), fn(theirs, name, restype, *argtypes)


def text(f, *args, size=1024):
    buffer = ctypes.create_string_buffer(size)
    f(buffer, size, *args)
    return buffer.value


class Spec(ctypes.Structure):
    _fields_ = [("format", I32), ("rate", U32), ("channels", ctypes.c_uint8)]


class Map(ctypes.Structure):
    _fields_ = [("channels", ctypes.c_uint8), ("map", I32 * 32)]


class Volume(ctypes.Structure):
    _fields_ = [("channels", ctypes.c_uint8), ("values", U32 * 32)]


# A map of `count` channels from both libraries (init_extend's, so that
# PulseAudio's assertions on valid maps hold).
def map_of(lib_pair, count, definition):
    maps = []
    for f in init_extend:
        m = Map()
        f(ctypes.byref(m), count, definition)
        maps.append(m)
    return maps


def raw(structure):
    if isinstance(structure, Volume):
        return (structure.channels, list(structure.values[:structure.channels]))
    return bytes(memoryview(structure).cast("B"))


# Sample specs.
snprint = both("pa_sample_spec_snprint", P, P, SZ, P)
for fmt, rate, ch in [(3, 44100, 2), (5, 48000, 1), (9, 22050, 6), (0, 8000, 1), (13, 44100, 2), (3, 0, 2), (12, 384000, 32)]:
    s = Spec(fmt, rate, ch)
    same(f"pa_sample_spec_snprint {fmt} {rate} {ch}", text(snprint[0], ctypes.byref(s)), text(snprint[1], ctypes.byref(s)))
    for name, kind in [("pa_frame_size", SZ), ("pa_sample_size", SZ), ("pa_bytes_per_second", SZ), ("pa_sample_spec_valid", I32)]:
        a, b = both(name, kind, P)
        if fmt < 13 and rate > 0:
            same(f"{name} {fmt} {rate} {ch}", a(ctypes.byref(s)), b(ctypes.byref(s)))
    if fmt < 13 and rate > 0:
        a, b = both("pa_bytes_to_usec", U64, U64, P)
        same(f"pa_bytes_to_usec {fmt}", a(123456, ctypes.byref(s)), b(123456, ctypes.byref(s)))
        a, b = both("pa_usec_to_bytes", SZ, U64, P)
        same(f"pa_usec_to_bytes {fmt}", a(250000, ctypes.byref(s)), b(250000, ctypes.byref(s)))
bytes_snprint = both("pa_bytes_snprint", P, P, SZ, U32)
for value in [0, 1, 1023, 1024, 1536, 1048576, 5000000, 1073741824, 4294967295]:
    same(f"pa_bytes_snprint {value}", text(bytes_snprint[0], value), text(bytes_snprint[1], value))
names = both("pa_sample_format_to_string", S, I32)
endian = both("pa_sample_format_is_le", I32, I32), both("pa_sample_format_is_be", I32, I32)
for fmt in range(-1, 14):
    same(f"pa_sample_format_to_string {fmt}", names[0](fmt), names[1](fmt))
    if 0 <= fmt < 13:
        same(f"pa_sample_format_is_le {fmt}", endian[0][0](fmt), endian[0][1](fmt))
        same(f"pa_sample_format_is_be {fmt}", endian[1][0](fmt), endian[1][1](fmt))
parse = both("pa_parse_sample_format", I32, S)
for name in [b"s16le", b"S16NE", b"16", b"float", b"float32be", b"ulaw", b"mulaw", b"alaw", b"s32", b"s24", b"s24-32le", b"s24-32be", b"u8", b"8", b"x"]:
    same(f"pa_parse_sample_format {name}", parse[0](name), parse[1](name))
for name, kind, arg in [("pa_sample_rate_valid", I32, U32), ("pa_channels_valid", I32, ctypes.c_uint8)]:
    a, b = both(name, kind, arg)
    for value in [0, 1, 8000, 32, 33, 384000, 387840, 387841]:
        if arg is ctypes.c_uint8 and value > 255:
            continue
        same(f"{name} {value}", a(value), b(value))

# Channel maps.
init_auto = both("pa_channel_map_init_auto", P, P, U32, I32)
init_extend = both("pa_channel_map_init_extend", P, P, U32, I32)
map_snprint = both("pa_channel_map_snprint", P, P, SZ, P)
to_name = both("pa_channel_map_to_name", S, P)
to_pretty = both("pa_channel_map_to_pretty_name", S, P)
mask = both("pa_channel_map_mask", U64, P)
flags = [both(n, I32, P) for n in ("pa_channel_map_can_balance", "pa_channel_map_can_fade", "pa_channel_map_can_lfe_balance", "pa_channel_map_valid")]
for count in range(1, 10):
    for definition in range(5):
        for label, pair in [("init_auto", init_auto), ("init_extend", init_extend)]:
            a, b = Map(), Map()
            ra = pair[0](ctypes.byref(a), count, definition)
            rb = pair[1](ctypes.byref(b), count, definition)
            same(f"pa_channel_map_{label} {count} {definition} succeeds", ra is not None, rb is not None)
            if ra is None or rb is None:
                continue
            same(f"pa_channel_map_{label} {count} {definition}", text(map_snprint[0], ctypes.byref(a)), text(map_snprint[1], ctypes.byref(b)))
            same(f"pa_channel_map_to_name {label} {count} {definition}", to_name[0](ctypes.byref(a)), to_name[1](ctypes.byref(b)))
            same(f"pa_channel_map_to_pretty_name {label} {count} {definition}", to_pretty[0](ctypes.byref(a)), to_pretty[1](ctypes.byref(b)))
            same(f"pa_channel_map_mask {label} {count} {definition}", mask[0](ctypes.byref(a)), mask[1](ctypes.byref(b)))
            for f in flags:
                same(f"{f[0].__name__} {label} {count} {definition}", f[0](ctypes.byref(a)), f[1](ctypes.byref(b)))
map_parse = both("pa_channel_map_parse", P, P, S)
for spelling in [b"stereo", b"mono", b"surround-21", b"surround-40", b"surround-41", b"surround-50", b"surround-51", b"surround-71",
                 b"front-left,front-right", b"left,right", b"front-center,lfe", b"aux0,aux1,aux31", b"top-center", b"rear-left,rear-right,side-left",
                 b"bogus", b"front-left,,front-right", b"", b"front-left,front-right,front-left"]:
    a, b = Map(), Map()
    ra = map_parse[0](ctypes.byref(a), spelling)
    rb = map_parse[1](ctypes.byref(b), spelling)
    same(f"pa_channel_map_parse {spelling} succeeds", ra is not None, rb is not None)
    if ra is not None and rb is not None:
        same(f"pa_channel_map_parse {spelling}", text(map_snprint[0], ctypes.byref(a)), text(map_snprint[1], ctypes.byref(b)))
position = both("pa_channel_position_to_string", S, I32)
pretty = both("pa_channel_position_to_pretty_string", S, I32)
from_string = both("pa_channel_position_from_string", I32, S)
for p in range(-1, 52):
    same(f"pa_channel_position_to_string {p}", position[0](p), position[1](p))
    same(f"pa_channel_position_to_pretty_string {p}", pretty[0](p), pretty[1](p))
    name = position[1](p)
    if name:
        same(f"pa_channel_position_from_string {name}", from_string[0](name), from_string[1](name))
for name in [b"left", b"right", b"center", b"subwoofer", b"mono", b"aux64", b"nonsense"]:
    same(f"pa_channel_position_from_string {name}", from_string[0](name), from_string[1](name))
superset = both("pa_channel_map_superset", I32, P, P)
has = both("pa_channel_map_has_position", I32, P, I32)
pairs = [(2, 1), (6, 2), (1, 1), (8, 6), (2, 6), (4, 3)]
for x, y in pairs:
    a = map_of(init_auto, x, 0)
    b = map_of(init_auto, y, 0)
    same(f"pa_channel_map_superset {x} {y}", superset[0](ctypes.byref(a[0]), ctypes.byref(b[0])), superset[1](ctypes.byref(a[1]), ctypes.byref(b[1])))
    for p in [1, 2, 3, 7, 10]:
        same(f"pa_channel_map_has_position {x} {p}", has[0](ctypes.byref(a[0]), p), has[1](ctypes.byref(a[1]), p))

# Volumes.
from_db = both("pa_sw_volume_from_dB", U32, F64)
to_db = both("pa_sw_volume_to_dB", F64, U32)
from_linear = both("pa_sw_volume_from_linear", U32, F64)
to_linear = both("pa_sw_volume_to_linear", F64, U32)
for db in [-200.0, -120.0, -60.0, -40.5, -20.0, -6.0, -0.5, 0.0, 0.25, 6.0, 11.0]:
    same(f"pa_sw_volume_from_dB {db}", from_db[0](db), from_db[1](db))
for linear in [0.0, 1e-9, 0.001, 0.125, 0.5, 0.9999, 1.0, 1.5, 4.0]:
    same(f"pa_sw_volume_from_linear {linear}", from_linear[0](linear), from_linear[1](linear))
for value in [0, 1, 100, 6553, 32768, 52000, 65535, 65536, 65537, 98304, 131072, 2147483647]:
    a, b = to_db[0](value), to_db[1](value)
    same(f"pa_sw_volume_to_dB {value}", round(a, 9) if a > -1e300 else a, round(b, 9) if b > -1e300 else b)
    same(f"pa_sw_volume_to_linear {value}", round(to_linear[0](value), 12), round(to_linear[1](value), 12))
multiply = both("pa_sw_volume_multiply", U32, U32, U32)
divide = both("pa_sw_volume_divide", U32, U32, U32)
for x, y in [(65536, 65536), (32768, 32768), (65536, 0), (0, 65536), (98304, 70000), (1000, 3), (2147483647, 2147483647), (4294967295, 1)]:
    same(f"pa_sw_volume_multiply {x} {y}", multiply[0](x, y), multiply[1](x, y))
    same(f"pa_sw_volume_divide {x} {y}", divide[0](x, y), divide[1](x, y))
volume_snprint = both("pa_volume_snprint", P, P, SZ, U32)
db_snprint = both("pa_sw_volume_snprint_dB", P, P, SZ, U32)
verbose = both("pa_volume_snprint_verbose", P, P, SZ, U32, I32)
for value in [0, 1, 655, 6553, 32768, 65536, 65537, 98304, 4294967295, 2147483647]:
    same(f"pa_volume_snprint {value}", text(volume_snprint[0], value), text(volume_snprint[1], value))
    same(f"pa_sw_volume_snprint_dB {value}", text(db_snprint[0], value), text(db_snprint[1], value))
    for decibels in (0, 1):
        same(f"pa_volume_snprint_verbose {value} {decibels}", text(verbose[0], value, decibels), text(verbose[1], value, decibels))


def volume(values):
    v = Volume()
    v.channels = len(values)
    for i, x in enumerate(values):
        v.values[i] = x
    return v


cv_snprint = both("pa_cvolume_snprint", P, P, SZ, P)
cv_db = both("pa_sw_cvolume_snprint_dB", P, P, SZ, P)
cv_verbose = both("pa_cvolume_snprint_verbose", P, P, SZ, P, P, I32)
stats = [both(n, U32, P) for n in ("pa_cvolume_avg", "pa_cvolume_max", "pa_cvolume_min")]
mask_stats = [both(n, U32, P, P, U64) for n in ("pa_cvolume_avg_mask", "pa_cvolume_max_mask", "pa_cvolume_min_mask")]
balance = both("pa_cvolume_get_balance", F32, P, P)
fade = both("pa_cvolume_get_fade", F32, P, P)
lfe = both("pa_cvolume_get_lfe_balance", F32, P, P)
set_balance = both("pa_cvolume_set_balance", P, P, P, F32)
set_fade = both("pa_cvolume_set_fade", P, P, P, F32)
set_lfe = both("pa_cvolume_set_lfe_balance", P, P, P, F32)
scale = both("pa_cvolume_scale", P, P, U32)
inc_clamp = both("pa_cvolume_inc_clamp", P, P, U32, U32)
dec = both("pa_cvolume_dec", P, P, U32)
get_position = both("pa_cvolume_get_position", U32, P, P, I32)
set_position = both("pa_cvolume_set_position", P, P, P, I32, U32)
remap = both("pa_cvolume_remap", P, P, P, P)
merge = both("pa_cvolume_merge", P, P, P, P)
cases = [[65536, 65536], [32768, 65536], [65536, 0], [0, 0], [65536, 32768, 50000, 10000, 65536, 20000], [40000], [65536] * 8]
for values in cases:
    count = len(values)
    maps = map_of(init_auto, count, 0)
    a, b = volume(values), volume(values)
    same(f"pa_cvolume_snprint {values}", text(cv_snprint[0], ctypes.byref(a)), text(cv_snprint[1], ctypes.byref(b)))
    same(f"pa_sw_cvolume_snprint_dB {values}", text(cv_db[0], ctypes.byref(a)), text(cv_db[1], ctypes.byref(b)))
    for decibels in (0, 1):
        same(f"pa_cvolume_snprint_verbose {values} {decibels}", text(cv_verbose[0], ctypes.byref(a), ctypes.byref(maps[0]), decibels, size=2048), text(cv_verbose[1], ctypes.byref(b), ctypes.byref(maps[1]), decibels, size=2048))
    for f in stats:
        same(f"{f[0].__name__} {values}", f[0](ctypes.byref(a)), f[1](ctypes.byref(b)))
    for f in mask_stats:
        for bits in [0xffffffffffffffff, 2, 6, 0x80]:
            same(f"{f[0].__name__} {values} {bits}", f[0](ctypes.byref(a), ctypes.byref(maps[0]), bits), f[1](ctypes.byref(b), ctypes.byref(maps[1]), bits))
    for label, f in [("balance", balance), ("fade", fade), ("lfe balance", lfe)]:
        same(f"pa_cvolume_get_{label} {values}", round(f[0](ctypes.byref(a), ctypes.byref(maps[0])), 5), round(f[1](ctypes.byref(b), ctypes.byref(maps[1])), 5))
    for label, f in [("balance", set_balance), ("fade", set_fade), ("lfe balance", set_lfe)]:
        for amount in [-1.0, -0.5, 0.0, 0.3, 1.0]:
            x, y = volume(values), volume(values)
            rx = f[0](ctypes.byref(x), ctypes.byref(maps[0]), amount)
            ry = f[1](ctypes.byref(y), ctypes.byref(maps[1]), amount)
            same(f"pa_cvolume_set_{label} {values} {amount} succeeds", rx is not None, ry is not None)
            same(f"pa_cvolume_set_{label} {values} {amount}", raw(x), raw(y))
    for most in [0, 32768, 65536, 100000]:
        x, y = volume(values), volume(values)
        scale[0](ctypes.byref(x), most)
        scale[1](ctypes.byref(y), most)
        same(f"pa_cvolume_scale {values} {most}", raw(x), raw(y))
    for step in [1000, 70000]:
        x, y = volume(values), volume(values)
        inc_clamp[0](ctypes.byref(x), step, 98304)
        inc_clamp[1](ctypes.byref(y), step, 98304)
        same(f"pa_cvolume_inc_clamp {values} {step}", raw(x), raw(y))
        x, y = volume(values), volume(values)
        dec[0](ctypes.byref(x), step)
        dec[1](ctypes.byref(y), step)
        same(f"pa_cvolume_dec {values} {step}", raw(x), raw(y))
    for p in [1, 2, 3, 7]:
        same(f"pa_cvolume_get_position {values} {p}", get_position[0](ctypes.byref(a), ctypes.byref(maps[0]), p), get_position[1](ctypes.byref(b), ctypes.byref(maps[1]), p))
        x, y = volume(values), volume(values)
        set_position[0](ctypes.byref(x), ctypes.byref(maps[0]), p, 12345)
        set_position[1](ctypes.byref(y), ctypes.byref(maps[1]), p, 12345)
        same(f"pa_cvolume_set_position {values} {p}", raw(x), raw(y))
    for target in [1, 2, 6]:
        to = map_of(init_auto, target, 0)
        if count > 8:
            continue
        x, y = volume(values), volume(values)
        rx = remap[0](ctypes.byref(x), ctypes.byref(maps[0]), ctypes.byref(to[0]))
        ry = remap[1](ctypes.byref(y), ctypes.byref(maps[1]), ctypes.byref(to[1]))
        same(f"pa_cvolume_remap {values} -> {target}", raw(x), raw(y))
    x, y = Volume(), Volume()
    other = volume([v // 2 + 7 for v in values])
    merge[0](ctypes.byref(x), ctypes.byref(a), ctypes.byref(other))
    merge[1](ctypes.byref(y), ctypes.byref(b), ctypes.byref(other))
    same(f"pa_cvolume_merge {values}", raw(x), raw(y))

# Property lists.
new = both("pa_proplist_new", P)
sets = both("pa_proplist_sets", I32, P, S, S)
setp = both("pa_proplist_setp", I32, P, S)
setf = both("pa_proplist_setf", I32, P, S, S)
pset = both("pa_proplist_set", I32, P, S, P, SZ)
gets = both("pa_proplist_gets", S, P, S)
to_string = both("pa_proplist_to_string", P, P)
to_string_sep = both("pa_proplist_to_string_sep", P, P, S)
from_text = both("pa_proplist_from_string", P, S)
size = both("pa_proplist_size", U32, P)
unset = both("pa_proplist_unset", I32, P, S)
contains = both("pa_proplist_contains", I32, P, S)
free_text = both("pa_xfree", None, P)


def string_of(lib_index, pointer):
    value = ctypes.cast(pointer, S).value
    free_text[lib_index](pointer)
    return value


lists = [new[0](), new[1]()]
for i in range(2):
    same("pa_proplist_sets media.name", sets[0](lists[0], b"media.name", b"Tone \"one\""), sets[1](lists[1], b"media.name", b"Tone \"one\""))
    break
for key, value in [(b"application.name", b"pactl"), (b"x", b"caf\xc3\xa9"), (b"bad", b"\xff\xfe"), (b"", b"empty"), (b"k\xc3\xa9y", b"v")]:
    same(f"pa_proplist_sets {key}", sets[0](lists[0], key, value), sets[1](lists[1], key, value))
for pair in [b"a=b", b"c==d", b"noequals", b"e=", b"=f"]:
    same(f"pa_proplist_setp {pair}", setp[0](lists[0], pair), setp[1](lists[1], pair))
blob = ctypes.create_string_buffer(b"\x00\x01binary\xff", 9)
same("pa_proplist_set binary", pset[0](lists[0], b"blob", blob, 9), pset[1](lists[1], b"blob", blob, 9))
same("pa_proplist_gets binary", gets[0](lists[0], b"blob"), gets[1](lists[1], b"blob"))
same("pa_proplist_gets media.name", gets[0](lists[0], b"media.name"), gets[1](lists[1], b"media.name"))
same("pa_proplist_size", size[0](lists[0]), size[1](lists[1]))
same("pa_proplist_to_string", string_of(0, to_string[0](lists[0])), string_of(1, to_string[1](lists[1])))
same("pa_proplist_to_string_sep", string_of(0, to_string_sep[0](lists[0], b", ")), string_of(1, to_string_sep[1](lists[1], b", ")))
same("pa_proplist_unset", unset[0](lists[0], b"a"), unset[1](lists[1], b"a"))
same("pa_proplist_unset missing", unset[0](lists[0], b"zzz"), unset[1](lists[1], b"zzz"))
same("pa_proplist_contains", contains[0](lists[0], b"c"), contains[1](lists[1], b"c"))
same("pa_proplist_to_string after unset", string_of(0, to_string[0](lists[0])), string_of(1, to_string[1](lists[1])))
for fmt, args in [(b"%s-%d", (S(b"x"), I32(-5))), (b"%u %lu %x %X %o", (U32(7), U64(1 << 40), U32(255), U32(255), U32(8))), (b"[%5d|%-4s|%05u]", (I32(42), S(b"ab"), U32(9))), (b"100%%", ()), (b"%c%c", (I32(79), I32(75))),
                   (b"%f|%.1f|%.3f", (F64(1.5), F64(-2.25), F64(1000.0005))), (b"%g|%g|%g", (F64(44100.0), F64(1.5), F64(0.25))), (b"%d %.2f %s", (I32(3), F64(0.125), S(b"z")))]:
    for lib_index, f in enumerate(setf):
        f.argtypes = [P, S, S] + [type(a) for a in args]
    same(f"pa_proplist_setf {fmt}", setf[0](lists[0], b"f", fmt, *args), setf[1](lists[1], b"f", fmt, *args))
    same(f"pa_proplist_setf {fmt} value", gets[0](lists[0], b"f"), gets[1](lists[1], b"f"))
for source in [b"a=b c=\"d e\" f='g\\'h' i=hex:41420043", b"  x = y  ", b"broken=\"", b"k=v=w", b"", b"=v", b"k=hex:4", b"k=hex:zz"]:
    a, b = from_text[0](source), from_text[1](source)
    same(f"pa_proplist_from_string {source} succeeds", bool(a), bool(b))
    if a and b:
        same(f"pa_proplist_from_string {source}", string_of(0, to_string[0](a)), string_of(1, to_string[1](b)))

# Formats.
format_new = both("pa_format_info_from_string", P, S)
format_snprint = both("pa_format_info_snprint", P, P, SZ, P)
format_valid = both("pa_format_info_valid", I32, P)
format_pcm = both("pa_format_info_is_pcm", I32, P)
compatible = both("pa_format_info_is_compatible", I32, P, P)
prop_type = both("pa_format_info_get_prop_type", I32, P, S)
get_rate = both("pa_format_info_get_rate", I32, P, ctypes.POINTER(U32))
to_spec = both("pa_format_info_to_sample_spec", I32, P, P, P)
from_spec = both("pa_format_info_from_sample_spec", P, P, P)
spellings = [b"pcm", b"pcm, format.rate = \"44100\"", b"pcm, format.rate = \"[ 44100, 48000 ]\"", b"ac3-iec61937, format.rate = \"48000\"",
             b"pcm, format.sample_format = \"\\\"s16le\\\"\"  format.rate = \"44100\"  format.channels = \"2\"", b"nonsense", b"pcm, format.rate = \"{ \\\"min\\\": 8000, \\\"max\\\": 96000 }\""]
infos = []
for spelling in spellings:
    a, b = format_new[0](spelling), format_new[1](spelling)
    same(f"pa_format_info_from_string {spelling} succeeds", bool(a), bool(b))
    if a and b:
        infos.append((a, b))
        same(f"pa_format_info_snprint {spelling}", text(format_snprint[0], a), text(format_snprint[1], b))
        same(f"pa_format_info_valid {spelling}", format_valid[0](a), format_valid[1](b))
        same(f"pa_format_info_is_pcm {spelling}", format_pcm[0](a), format_pcm[1](b))
        for key in [b"format.rate", b"format.sample_format", b"format.channels", b"missing"]:
            same(f"pa_format_info_get_prop_type {spelling} {key}", prop_type[0](a, key), prop_type[1](b, key))
        ra, rb = U32(0), U32(0)
        same(f"pa_format_info_get_rate {spelling}", (get_rate[0](a, ctypes.byref(ra)), ra.value), (get_rate[1](b, ctypes.byref(rb)), rb.value))
        sa, sb, ma, mb = Spec(), Spec(), Map(), Map()
        same(f"pa_format_info_to_sample_spec {spelling}", (to_spec[0](a, ctypes.byref(sa), ctypes.byref(ma)), raw(sa)), (to_spec[1](b, ctypes.byref(sb), ctypes.byref(mb)), raw(sb)))
for i, (a1, b1) in enumerate(infos):
    for j, (a2, b2) in enumerate(infos):
        same(f"pa_format_info_is_compatible {i} {j}", compatible[0](a1, a2), compatible[1](b1, b2))
for fmt, rate, ch in [(3, 44100, 2), (5, 48000, 6)]:
    s = Spec(fmt, rate, ch)
    maps = map_of(init_auto, ch, 0)
    a, b = from_spec[0](ctypes.byref(s), ctypes.byref(maps[0])), from_spec[1](ctypes.byref(s), ctypes.byref(maps[1]))
    same(f"pa_format_info_from_sample_spec {fmt} {rate} {ch}", text(format_snprint[0], a, size=2048), text(format_snprint[1], b, size=2048))

# UTF-8, errors, encodings, times.
utf8 = both("pa_utf8_valid", P, S)
ascii_valid = both("pa_ascii_valid", P, S)
utf8_filter = both("pa_utf8_filter", P, S)
ascii_filter = both("pa_ascii_filter", P, S)
for sample_text in [b"plain", b"caf\xc3\xa9", b"\xff", b"\xc3", b"\xe2\x82\xac", b"\xed\xa0\x80", b"\xf4\x90\x80\x80", b"\xc0\x80", b"a\xefb", b"\xef\xbf\xbe"]:
    same(f"pa_utf8_valid {sample_text}", bool(utf8[0](sample_text)), bool(utf8[1](sample_text)))
    same(f"pa_ascii_valid {sample_text}", bool(ascii_valid[0](sample_text)), bool(ascii_valid[1](sample_text)))
    same(f"pa_utf8_filter {sample_text}", string_of(0, utf8_filter[0](sample_text)), string_of(1, utf8_filter[1](sample_text)))
    same(f"pa_ascii_filter {sample_text}", string_of(0, ascii_filter[0](sample_text)), string_of(1, ascii_filter[1](sample_text)))
strerror = both("pa_strerror", S, I32)
for code in range(-1, 29):
    same(f"pa_strerror {code}", strerror[0](code), strerror[1](code))
encoding = both("pa_encoding_to_string", S, I32)
encoding_from = both("pa_encoding_from_string", I32, S)
for code in range(-1, 10):
    same(f"pa_encoding_to_string {code}", encoding[0](code), encoding[1](code))
    name = encoding[1](code)
    if name:
        same(f"pa_encoding_from_string {name}", encoding_from[0](name), encoding_from[1](name))
direction = both("pa_direction_to_string", S, I32)
direction_valid = both("pa_direction_valid", I32, I32)
for code in range(0, 5):
    same(f"pa_direction_to_string {code}", direction[0](code), direction[1](code))
    same(f"pa_direction_valid {code}", direction_valid[0](code), direction_valid[1](code))
same("pa_get_library_version", fn(ours, "pa_get_library_version", S)(), fn(theirs, "pa_get_library_version", S)())


class Timeval(ctypes.Structure):
    _fields_ = [("sec", ctypes.c_long), ("usec", ctypes.c_long)]


add = both("pa_timeval_add", P, P, U64)
sub = both("pa_timeval_sub", P, P, U64)
diff = both("pa_timeval_diff", U64, P, P)
cmp_ = both("pa_timeval_cmp", I32, P, P)
store = both("pa_timeval_store", P, P, U64)
load = both("pa_timeval_load", U64, P)
for s, u, v in [(10, 500000, 700000), (0, 0, 1), (5, 999999, 1), (3, 100, 4000000), (0, 5, 10)]:
    a, b = Timeval(s, u), Timeval(s, u)
    add[0](ctypes.byref(a), v)
    add[1](ctypes.byref(b), v)
    same(f"pa_timeval_add {s} {u} {v}", (a.sec, a.usec), (b.sec, b.usec))
    a, b = Timeval(s, u), Timeval(s, u)
    sub[0](ctypes.byref(a), v)
    sub[1](ctypes.byref(b), v)
    same(f"pa_timeval_sub {s} {u} {v}", (a.sec, a.usec), (b.sec, b.usec))
    x, y = Timeval(s, u), Timeval(s + 1, (u + 7) % 1000000)
    same(f"pa_timeval_diff {s} {u}", diff[0](ctypes.byref(x), ctypes.byref(y)), diff[1](ctypes.byref(x), ctypes.byref(y)))
    same(f"pa_timeval_cmp {s} {u}", cmp_[0](ctypes.byref(x), ctypes.byref(y)), cmp_[1](ctypes.byref(x), ctypes.byref(y)))
    a, b = Timeval(), Timeval()
    store[0](ctypes.byref(a), v * 1234567)
    store[1](ctypes.byref(b), v * 1234567)
    same(f"pa_timeval_store {v}", (a.sec, a.usec), (b.sec, b.usec))
    same(f"pa_timeval_load {s} {u}", load[0](ctypes.byref(x)), load[1](ctypes.byref(x)))

for failure in failures:
    print("differs:", failure)
print(f"{checked - len(failures)} of {checked} values the same")
sys.exit(1 if failures else 0)
