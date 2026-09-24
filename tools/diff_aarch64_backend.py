#!/usr/bin/env python3
"""Differential test of mlx1's AArch64 backend against its x86_64 backend.

For every program given (default: tests/*.mlx), compiles it twice with mlx1,
once natively (x86_64) and once with --target=aarch64-linux, runs the x86_64
build on the host and the aarch64 build under tools/aarch64_linux_emulator.py,
and compares exit status, stdout and stderr. Programs that do not compile for
x86_64 (the negative tests) are skipped.

Usage: python3 tools/diff_aarch64_backend.py [--mlx1 PATH] [-j N] [-v] [files...]
Run from anywhere; programs are compiled and run from the repository root.
"""
import argparse
import concurrent.futures
import glob
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import aarch64_linux_emulator  # noqa: E402

TIMEOUT = 60


def compile_program(mlx1, source, output, target):
    command = [mlx1, source, "-o", output, "--quiet"]
    if target:
        command.append("--target=" + target)
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=TIMEOUT)
    return result.returncode == 0 and os.path.exists(output), result.stdout + result.stderr


def run_native(program):
    try:
        result = subprocess.run([program], cwd=ROOT, capture_output=True, stdin=subprocess.DEVNULL, timeout=TIMEOUT)
    except subprocess.TimeoutExpired:
        return 124, b"", b""
    status = result.returncode if result.returncode >= 0 else 128 - result.returncode
    return status, result.stdout, result.stderr


def run_emulated(program):
    previous = os.getcwd()
    process = aarch64_linux_emulator.Process(program, [program], timeout_seconds=TIMEOUT)
    try:
        os.chdir(ROOT)
        status = process.run()
    finally:
        os.chdir(previous)
    return status, bytes(process.stdout), bytes(process.stderr)


def check(mlx1, source, workdir):
    name = os.path.splitext(os.path.basename(source))[0]
    native = os.path.join(workdir, name + ".x86_64")
    emulated = os.path.join(workdir, name + ".aarch64")
    ok, _ = compile_program(mlx1, source, native, None)
    if not ok:
        return "skip", source, "does not compile for x86_64"
    ok, log = compile_program(mlx1, source, emulated, "aarch64-linux")
    if not ok:
        return "fail", source, "aarch64 compile failed:\n" + log[-2000:]
    expected = run_native(native)
    actual = run_emulated(emulated)
    if expected == actual:
        return "pass", source, f"exit={expected[0]}"
    lines = [f"x86_64:  exit={expected[0]} stdout={expected[1][:300]!r} stderr={expected[2][:300]!r}",
             f"aarch64: exit={actual[0]} stdout={actual[1][:300]!r} stderr={actual[2][:300]!r}"]
    return "fail", source, "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mlx1", default=os.path.join(ROOT, "zig-out/bin/mlx1"))
    parser.add_argument("-j", type=int, default=1, help="parallel programs (emulation runs in-process, so 1 is safest)")
    parser.add_argument("-v", action="store_true", help="list passing programs too")
    parser.add_argument("files", nargs="*")
    options = parser.parse_args()
    files = options.files or sorted(glob.glob(os.path.join(ROOT, "tests", "*.mlx")))
    files = [os.path.relpath(os.path.abspath(f), ROOT) for f in files]
    counts = {"pass": 0, "fail": 0, "skip": 0}
    with tempfile.TemporaryDirectory() as workdir:
        with concurrent.futures.ThreadPoolExecutor(max_workers=options.j) as pool:
            for status, source, detail in pool.map(lambda f: check(options.mlx1, f, workdir), files):
                counts[status] += 1
                if status == "fail" or (options.v and status == "pass"):
                    print(f"{status.upper()} {source}: {detail}")
    print(f"{counts['pass']} passed, {counts['fail']} failed, {counts['skip']} skipped (no x86_64 build)")
    return 1 if counts["fail"] else 0


if __name__ == "__main__":
    sys.exit(main())
