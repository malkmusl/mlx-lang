#!/usr/bin/env python3
"""Checks compiler/selfhost/object/android against Python reference code.

Builds compiler/selfhost/object/android/check.mlx with mlx0 and with mlx1,
runs both, and compares every printed value with Python's own hashlib/zlib
(and, as the check program grows, base64/bignum/DER references).
Usage: python3 tools/check_android_packaging.py
"""
import hashlib
import os
import subprocess
import sys
import tempfile
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = "compiler/selfhost/object/android/check.mlx"


def pattern(length):
    return bytes((i * 7 + 3) & 255 for i in range(length))


def expected_lines():
    lines = []
    for length in (0, 1, 55, 56, 63, 64, 65, 1000, 70000):
        data = pattern(length)
        lines.append(f"crc32 {zlib.crc32(data):x}")
        lines.append(f"adler32 {zlib.adler32(data):x}")
        lines.append(f"sha1 {hashlib.sha1(data).hexdigest()}")
        lines.append(f"sha256 {hashlib.sha256(data).hexdigest()}")
        lines.append(f"sha256 {hashlib.sha256(data).hexdigest()}")
    lines += bignum_lines()
    return lines


def big_from(length, seed):
    return int.from_bytes(bytes((i * 13 + seed * 31 + 7) & 255 for i in range(length)), "big")


def minimal_hex(value):
    return value.to_bytes(max(1, (value.bit_length() + 7) // 8), "big").hex()


def bignum_lines():
    a, b, m, e = big_from(128, 1), big_from(96, 2), big_from(128, 3), big_from(3, 4)
    return [
        f"a {minimal_hex(a)}",
        f"mul {minimal_hex(a * b)}",
        f"add {minimal_hex(a + b)}",
        f"sub {minimal_hex(a - b)}",
        f"div {minimal_hex(a // b)}",
        f"mod {minimal_hex(a % b)}",
        f"powsmall {minimal_hex(pow(a, e, m))}",
        f"powbig {minimal_hex(pow(a, b, m))}",
        f"poweven {minimal_hex(pow(a, e, m + 1))}",
        f"inverse {minimal_hex(pow(65537, -1, m))}",
        f"padded {e.to_bytes(8, 'big').hex()}",
        f"bits {a.bit_length():x}",
        "prime 1",
        "prime 0",
    ]


def build_and_run(compiler, tmp):
    exe = os.path.join(tmp, os.path.basename(compiler) + "-check")
    if os.path.basename(compiler) == "mlx0":
        command = [compiler, SOURCE, "-o" + exe]
    else:
        command = [compiler, SOURCE, "-o", exe, "--quiet"]
    build = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
    if build.returncode != 0:
        print(build.stdout[-3000:], build.stderr[-3000:])
        raise SystemExit(f"{compiler}: build failed")
    return subprocess.run([exe], capture_output=True, text=True, check=True).stdout.splitlines()


def main():
    expected = expected_lines()
    failures = 0
    with tempfile.TemporaryDirectory() as tmp:
        for compiler in ("zig-out/bin/mlx0", "zig-out/bin/mlx1"):
            actual = build_and_run(os.path.join(ROOT, compiler), tmp)
            bad = [(e, a) for e, a in zip(expected, actual) if e != a]
            if len(actual) != len(expected):
                bad.append((f"{len(expected)} lines", f"{len(actual)} lines"))
            for e, a in bad[:10]:
                print(f"{compiler}: expected {e!r}, got {a!r}")
            failures += len(bad)
            print(f"{compiler}: {len(expected) - len(bad)}/{len(expected)} values match")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
