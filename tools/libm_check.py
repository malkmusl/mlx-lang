#!/usr/bin/env python3
"""Compares libm.so.6 built from compiler/libc/libm.mlx with the system's
libm through ctypes: the same calls answer the same bits, or within a few
units in the last place where the algorithms differ (the hyperbolic and
inverse functions, powers with fractional exponents).

    python3 tools/libm_check.py OURS.so [SYSTEM.so]
"""
import ctypes
import math
import struct
import sys

ours = ctypes.CDLL(sys.argv[1])
theirs = ctypes.CDLL(sys.argv[2] if len(sys.argv) > 2 else "libm.so.6")

D = ctypes.c_double
F = ctypes.c_float
I = ctypes.c_int
L = ctypes.c_long

INPUTS = [0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 0.25, 2.0, 3.0, 10.0, 100.0, 1e-5, 7.25, -7.25,
          0.1, 0.3, 0.7, 0.9, 1.5, 2.5, -2.5, 3.5, 1e10, 123456.789, -98765.4321, 1e-300,
          1e300, 1e-320, 0.001, 4.0, 8.0, 9.0, 16.0, 1024.0, 65536.0, 1e15, 2.0 ** 52,
          2.0 ** 53, 0.7853981633974483, 1.5707963267948966, 3.141592653589793,
          6.283185307179586, 1e6, 22.5, 700.0, -700.0, 0.9999, 1.0001, 20.0, 0.999999,
          float("inf"), float("-inf"), float("nan")]


def ulps(a, b):
    if math.isnan(a) and math.isnan(b):
        return 0
    if math.isnan(a) or math.isnan(b):
        return float("inf")
    if a == b:
        if math.copysign(1.0, a) != math.copysign(1.0, b):
            return 1
        return 0
    if math.isinf(a) or math.isinf(b):
        return float("inf")
    ia = struct.unpack("<q", struct.pack("<d", a))[0]
    ib = struct.unpack("<q", struct.pack("<d", b))[0]
    return abs(ia - ib)


def call(lib, name, restype, argtypes, args):
    function = getattr(lib, name)
    function.restype = restype
    function.argtypes = argtypes
    return function(*args)


failures = 0
checked = 0


def compare(name, argtypes, args, tolerance, restype=D):
    global failures, checked
    a = call(ours, name, restype, argtypes, args)
    b = call(theirs, name, restype, argtypes, args)
    checked += 1
    if restype is D or restype is F:
        distance = ulps(float(a), float(b))
        if distance > tolerance:
            failures += 1
            if failures <= 40:
                print(f"FAIL {name}{tuple(args)}: ours {a!r}, system {b!r} ({distance} ulps)")
    elif a != b:
        failures += 1
        if failures <= 40:
            print(f"FAIL {name}{tuple(args)}: ours {a!r}, system {b!r}")


# Exact ones.
for name in ["fabs", "floor", "ceil", "trunc", "round", "rint", "nearbyint", "sqrt", "logb"]:
    for x in INPUTS:
        compare(name, [D], [x], 0)
for name in ["fmod", "remainder", "copysign", "fmin", "fmax", "fdim", "nextafter"]:
    for x in INPUTS:
        for y in [1.0, -1.0, 0.5, 3.0, 2.5, 7.0, 0.1, 1e-310, 1e300, 0.0, float("inf"), float("nan"), -3.0]:
            compare(name, [D, D], [x, y], 0)
for name in ["ldexp", "scalbn"]:
    for x in INPUTS:
        for n in [0, 1, -1, 3, 52, 53, -52, -1022, -1074, 1023, 1024, -1100, 2000, -2000]:
            compare(name, [D, I], [x, n], 0)
for x in INPUTS:
    e1, e2 = I(), I()
    a = call(ours, "frexp", D, [D, ctypes.POINTER(I)], [x, ctypes.byref(e1)])
    b = call(theirs, "frexp", D, [D, ctypes.POINTER(I)], [x, ctypes.byref(e2)])
    checked += 1
    if ulps(a, b) != 0 or e1.value != e2.value:
        failures += 1
        print(f"FAIL frexp({x!r}): ours {a!r} e{e1.value}, system {b!r} e{e2.value}")
    w1, w2 = D(), D()
    a = call(ours, "modf", D, [D, ctypes.POINTER(D)], [x, ctypes.byref(w1)])
    b = call(theirs, "modf", D, [D, ctypes.POINTER(D)], [x, ctypes.byref(w2)])
    checked += 1
    if ulps(a, b) != 0 or ulps(w1.value, w2.value) != 0:
        failures += 1
        print(f"FAIL modf({x!r}): ours {a!r} {w1.value!r}, system {b!r} {w2.value!r}")
for name in ["lround", "lrint"]:
    for x in [0.0, 0.5, 1.5, 2.5, -0.5, -1.5, -2.5, 3.49, 3.51, 1e15, -7.25]:
        compare(name, [D], [x], 0, restype=L)
for name in ["__fpclassify", "ilogb"]:
    for x in INPUTS:
        compare(name, [D], [x], 0, restype=I)
for x in INPUTS:
    # glibc answers with the sign bit where it sits; any nonzero value is the sign.
    a = call(ours, "__signbit", I, [D], [x]) != 0
    b = call(theirs, "__signbit", I, [D], [x]) != 0
    checked += 1
    if a != b:
        failures += 1
        print(f"FAIL __signbit({x!r}): ours {a}, system {b}")

# Within a few ulps.
for name, tolerance in [("sin", 2), ("cos", 2), ("tan", 4), ("exp", 2), ("log", 2), ("log2", 2),
                        ("log10", 2), ("atan", 2), ("asin", 4), ("acos", 4), ("sinh", 4),
                        ("cosh", 4), ("tanh", 4), ("asinh", 4), ("acosh", 4), ("atanh", 4),
                        ("exp2", 4), ("expm1", 4), ("log1p", 4), ("cbrt", 4)]:
    for x in INPUTS:
        compare(name, [D], [x], tolerance)
# Huge arguments to sin, cos and tan: the reduction with the bits of 2/pi.
for name, tolerance in [("sin", 2), ("cos", 2), ("tan", 4)]:
    for x in [1e10, 1e15, 1e22, 1e300, 2.0 ** 52, 2.0 ** 53, 2.0 ** 60 + 0.5, 1647099.0, 1647100.5, 1e7, 3e6,
              -1e10, 1.7976931348623157e308, 1e100, 123456789012345.0, 9e18]:
        compare(name, [D], [x], tolerance)
# erf and erfc (a series in double-double, a continued fraction), gamma
# (Lanczos: within 1e-13 relative, and near lgamma's zeros absolute).
for name, tolerance in [("erf", 8), ("erfc", 8)]:
    for x in [0.0, -0.0, 0.1, 0.5, 1.0, 1.5, 2.0, 2.4, 2.5, 2.6, 3.0, 4.0, 5.0, 6.0, 7.0, 10.0, 20.0, 26.0, 27.0, 28.0,
              -0.3, -2.0, -4.0, -7.0, 1e-5, 1e-10, 1e-300, float("inf"), float("-inf"), float("nan")]:
        compare(name, [D], [x], tolerance)


def compare_loose(name, x, relative, absolute):
    global failures, checked
    a = call(ours, name, D, [D], [x])
    b = call(theirs, name, D, [D], [x])
    checked += 1
    if math.isnan(a) and math.isnan(b):
        return
    if math.isinf(a) or math.isinf(b) or math.isnan(a) or math.isnan(b):
        ok = a == b
    else:
        ok = abs(a - b) <= relative * abs(b) or abs(a - b) <= absolute
    if not ok:
        failures += 1
        if failures <= 40:
            print(f"FAIL {name}({x!r}): ours {a!r}, system {b!r}")


for x in [0.5, 1.0, 2.0, 3.0, 4.0, 5.0, 10.0, 20.0, 22.0, 23.0, 25.0, 26.0, 30.0, 100.0, 170.0, 171.5, 172.0, 0.1,
          1.5, 2.5, 7.75, -0.5, -1.5, -2.5, -7.75, 1e-5, -1e-5, 1e-300, 0.0, -0.0, -1.0, -2.0, float("inf"),
          float("-inf"), float("nan")]:
    compare_loose("tgamma", x, 1e-13, 0.0)
for x in [0.5, 1.0, 2.0, 3.0, 10.0, 20.0, 25.0, 26.0, 100.0, 1000.0, 1e6, 1e10, 1e300, 0.1, 1.5, 2.1, 0.9, 7.75,
          -0.5, -1.5, -2.5, -7.75, 1e-5, -1e-5, 3e-300, 0.0, -1.0, float("inf"), float("-inf"), float("nan")]:
    compare_loose("lgamma", x, 1e-13, 1e-13)
for x in INPUTS:
    for y in [0.0, 1.0, 2.0, 3.0, -1.0, -2.0, 0.5, 10.0, -0.5, 1.5, 0.3, 100.0, float("inf"), float("-inf"), float("nan")]:
        # Whole exponents within an ulp; fractional ones are e^(y ln x) (libm.mlx).
        if math.isinf(y) or math.isnan(y) or x in (0.0, 1.0) or math.isinf(x) or math.isnan(x):
            tolerance = 0
        elif y == math.trunc(y):
            tolerance = 1
        else:
            tolerance = 64
        compare("pow", [D, D], [x, y], tolerance)
        compare("atan2", [D, D], [x, y], 2)
        compare("hypot", [D, D], [x, y], 2)

# The float variants.
for name, tolerance in [("sinf", 1), ("cosf", 1), ("expf", 1), ("logf", 1), ("sqrtf", 0), ("fabsf", 0),
                        ("floorf", 0), ("roundf", 0), ("atanf", 1), ("tanhf", 1)]:
    for x in [0.0, -0.0, 0.5, 1.0, -1.0, 2.0, 7.25, 1e-5, 100.0, 3.0, 0.1, float("inf"), float("nan")]:
        compare(name, [F], [x], tolerance, restype=F)
for x in [0.0, 1.0, 2.0, 7.25, -7.25, 1e5]:
    for y in [1.0, 2.0, 0.5, 3.0, -2.0]:
        compare("powf", [F, F], [x, y], 1, restype=F)
        compare("fmodf", [F, F], [x, y], 0, restype=F)

if failures:
    print(f"libm_check.py: {failures} of {checked} comparisons differ")
    sys.exit(1)
print(f"ok   libm: {checked} calls answer as the system's libm does (within a few ulps where the algorithms differ)")
