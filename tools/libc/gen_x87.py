#!/usr/bin/env python3
"""Writes projects/libc/x87_long.mlx from tools/libc/x87_long.s: the bytes
of the long double functions (assembled with GNU as, position
independent: nothing in them refers outside) and the names with their
offsets and sizes, for mlxlibc's loader to put in a page and bind.

    python3 tools/libc/gen_x87.py > projects/libc/x87_long.mlx
"""
import os
import subprocess
import sys
import tempfile

here = os.path.dirname(os.path.abspath(__file__))
source = os.path.join(here, "x87_long.s")
with tempfile.TemporaryDirectory() as work:
    obj = os.path.join(work, "x87.o")
    binary = os.path.join(work, "x87.bin")
    subprocess.run(["as", "--64", "-o", obj, source], check=True)
    subprocess.run(["objcopy", "-O", "binary", "--only-section=.text", obj, binary], check=True)
    code = open(binary, "rb").read()
    listing = subprocess.run(["nm", "--defined-only", obj], check=True, capture_output=True, text=True).stdout
symbols = {}
for line in listing.splitlines():
    address, kind, name = line.split()
    if kind == "T":
        symbols[name] = int(address, 16)
if not symbols:
    sys.exit("no global symbols")
names = sorted(symbols, key=lambda n: symbols[n])
# A function's size: up to the next symbol (global or not) in the text.
everything = {}
for line in listing.splitlines():
    address, kind, name = line.split()
    if kind in "Tt":
        everything[name] = int(address, 16)
ends = sorted(set(everything.values())) + [len(code)]

out = []
out.append("// The long double functions of <math.h> as x87 machine code, from")
out.append("// tools/libc/x87_long.s by tools/libc/gen_x87.py; do not edit by")
out.append("// hand. The loader copies CODE into a page of its own and binds each")
out.append("// name to its offset (loader.mlx).")
out.append(f"pub const SIZE: usize = {len(code)}")
out.append(f"pub const COUNT: usize = {len(names)}")
out.append(f"pub const CODE: [{len(code)}]u8 = [{len(code)}]u8{{")
for i in range(0, len(code), 24):
    out.append("    " + ", ".join(str(b) for b in code[i:i + 24]) + ",")
out.append("}")
out.append("")
out.append("// The offset and size of the function `name`, (0, 0) for none.")
out.append("pub fn find(name: []const u8) -> (usize, usize) {")
for name in names:
    start = symbols[name]
    end = next(e for e in ends if e > start)
    out.append(f'    if same(name, "{name}") {{ return -> ({start}, {end - start}) }}')
out.append("    return -> (0, 0)")
out.append("}")
out.append("")
out.append("// The functions' names in order, for the symbol entries: the n-th.")
out.append("pub fn nameAt(index: usize) -> []const u8 {")
for i, name in enumerate(names):
    out.append(f'    if index == {i} {{ return "{name}" }}')
out.append('    return ""')
out.append("}")
out.append("")
out.append("fn same(first: []const u8, second: []const u8) -> bool {")
out.append("    if first.length != second.length { return false }")
out.append("    var index: usize = 0")
out.append("    while index < first.length {")
out.append("        if first[index] != second[index] { return false }")
out.append("        index += 1")
out.append("    }")
out.append("    return true")
out.append("}")
print("\n".join(out))
