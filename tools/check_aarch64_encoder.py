#!/usr/bin/env python3
"""Checks compiler/selfhost/backend/aarch64/encoder.mlx against llvm-mc.

Builds encoder_check.mlx with mlx1 (run from the repository root), runs it,
and assembles every printed instruction with llvm-mc. Exits non-zero on any
mismatch. Usage: python3 tools/check_aarch64_encoder.py [path/to/mlx1]
"""
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = "compiler/selfhost/backend/aarch64/encoder_check.mlx"


def main():
    mlx1 = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "zig-out/bin/mlx1")
    with tempfile.TemporaryDirectory() as tmp:
        exe = os.path.join(tmp, "encoder_check")
        subprocess.run([mlx1, SOURCE, "-o", exe, "--quiet"], cwd=ROOT, check=True)
        lines = subprocess.run([exe], check=True, capture_output=True, text=True).stdout.splitlines()
    cases = [line.split(" ", 1) for line in lines]
    asm = "\n".join(text for _, text in cases) + "\n"
    out = subprocess.run(["llvm-mc", "--triple=aarch64", "--show-encoding"], input=asm,
                         check=True, capture_output=True, text=True).stdout
    encodings = []
    for line in out.splitlines():
        match = re.search(r"encoding: \[([^\]]*)\]", line)
        if match:
            parts = [int(b, 16) for b in match.group(1).split(",")]
            encodings.append(parts[0] | parts[1] << 8 | parts[2] << 16 | parts[3] << 24)
    if len(encodings) != len(cases):
        print(f"llvm-mc produced {len(encodings)} encodings for {len(cases)} cases")
        return 1
    failures = 0
    for (word, text), expected in zip(cases, encodings):
        if int(word, 16) != expected:
            failures += 1
            print(f"MISMATCH {text}: mlx {int(word, 16):08x}, llvm-mc {expected:08x}")
    print(f"{len(cases) - failures}/{len(cases)} aarch64 encodings match llvm-mc")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
