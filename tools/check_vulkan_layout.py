#!/usr/bin/env python3
"""Checks that every struct and union in std/src/vulkan.mlx has the same
size, alignment and member offsets as the C definition in <vulkan/vulkan.h>.

    tools/check_vulkan_layout.py [compiler]

Needs gcc and the Vulkan headers (libvulkan-dev); the Wayland platform
structs also need wayland-client.h. Android structs are skipped (no NDK
headers). The compiler defaults to mlx-out/bin/compiler/mlx4. Set
LAYOUT_WORK=<dir> to keep the generated C and Mlx programs there.
"""
import os
import re
import subprocess
import sys
import tempfile

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(root)
compiler = sys.argv[1] if len(sys.argv) > 1 else "mlx-out/bin/compiler/mlx4"

source = open("std/src/vulkan.mlx").read()
aggregates = []
for match in re.finditer(r"^pub const (\w+) = (struct|union) \{\n(.*?)\n\n    pub fn init", source, re.M | re.S):
    name, kind, body = match.group(1), match.group(2), match.group(3)
    fields = [line.split(":")[0].strip() for line in body.split("\n") if line.strip()]
    aggregates.append((name, fields))

skipped = [name for name, _ in aggregates if "Android" in name]
aggregates = [(name, fields) for name, fields in aggregates if name not in skipped]

def c_field(field):
    return field[:-1] if field.endswith("_") else field

c_lines = [
    "#define VK_USE_PLATFORM_WAYLAND_KHR 1",
    "#include <stddef.h>",
    "#include <stdio.h>",
    "#include <vulkan/vulkan.h>",
    "int main(void) {",
]
mlx_lines = [
    'const std = @import("std")',
    'const vk = @import("std.vulkan")',
    "fn say(text: []const u8) -> void { const ignored = std.io.writeAll(std.io.stdout(), text) }",
    "fn number(value: usize) -> void { const ignored = std.io.writeUnsigned(std.io.stdout(), value) }",
    "fn line(name: []const u8, a: usize, b: usize) -> void {",
    "    say(name)",
    '    say(" ")',
    "    number(a)",
    '    say(" ")',
    "    number(b)",
    '    say("\\n")',
    "}",
]
# One Mlx function per aggregate keeps function bodies small.
calls = []
for index, (name, fields) in enumerate(aggregates):
    c_lines.append(f'    printf("{name} %zu %zu\\n", sizeof(Vk{name}), _Alignof(Vk{name}));')
    mlx_lines.append(f"fn check{index}() -> void {{")
    mlx_lines.append(f'    line("{name}", @sizeOf(vk.{name}), @alignOf(vk.{name}))')
    for field in fields:
        c_lines.append(f'    printf("{name}.{c_field(field)} %zu 0\\n", offsetof(Vk{name}, {c_field(field)}));')
        mlx_lines.append(f'    line("{name}.{c_field(field)}", @offsetOf(vk.{name}, "{field}"), 0)')
    mlx_lines.append("}")
    calls.append(f"    check{index}()")
c_lines += ["    return 0;", "}"]
mlx_lines.append("pub fn main() -> u8 {")
mlx_lines += calls
mlx_lines += ["    return 0", "}"]

with tempfile.TemporaryDirectory() as work:
    work = os.environ.get("LAYOUT_WORK", work)
    open(f"{work}/layout.c", "w").write("\n".join(c_lines) + "\n")
    open(f"{work}/layout.mlx", "w").write("\n".join(mlx_lines) + "\n")
    subprocess.run(["gcc", "-o", f"{work}/c_layout", f"{work}/layout.c"], check=True)
    subprocess.run([compiler, "--quiet", f"{work}/layout.mlx", "-o", f"{work}/mlx_layout"], check=True)
    expected = subprocess.run([f"{work}/c_layout"], check=True, capture_output=True, text=True).stdout.splitlines()
    actual = subprocess.run([f"{work}/mlx_layout"], check=True, capture_output=True, text=True).stdout.splitlines()

mismatches = [(e, a) for e, a in zip(expected, actual) if e != a]
if len(expected) != len(actual):
    print(f"check_vulkan_layout: line counts differ (C {len(expected)}, Mlx {len(actual)})")
    sys.exit(1)
for e, a in mismatches[:40]:
    print(f"mismatch: C '{e}' vs Mlx '{a}'")
members = sum(len(fields) for _, fields in aggregates)
print(f"check_vulkan_layout: {len(aggregates)} aggregates, {members} members, {len(mismatches)} mismatches"
      f" (skipped without headers: {', '.join(skipped) or 'none'})")
sys.exit(1 if mismatches else 0)
