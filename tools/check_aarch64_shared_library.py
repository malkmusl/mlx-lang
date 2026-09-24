#!/usr/bin/env python3
"""Builds tests/aarch64/shared_library.mlx with --target=aarch64-android and
calls its exported functions under tools/aarch64_linux_emulator.py, with the
C imports bound to Python stubs. Checks the .so's dynamic structure (needed
libraries, exports, 16 KiB segment alignment) along the way.
Usage: python3 tools/check_aarch64_shared_library.py [path/to/mlx1]
"""
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import aarch64_linux_emulator  # noqa: E402


def main():
    mlx1 = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "zig-out/bin/mlx1")
    written = []

    def write(process, fd, buffer, length, *_):
        written.append((fd, process.read(buffer, length)))
        return length

    imports = {
        "getpid": lambda process, *_: 4242,
        "write": write,
        "negative_one": lambda process, *_: 0xFFFFFFFF,  # -1 in w0, upper half zero
        "small_byte": lambda process, *_: 0xABCD00FE,  # 0xFE with garbage above
    }
    checks = [
        ("mlx_add", (3, 4), 13),
        ("mlx_hello", (), 15),
        ("mlx_pid", (), 4242),
        ("mlx_sum_to", (100,), 5050),
        ("mlx_foreign_negative", (), 1),
        ("mlx_param_negative", (0xDEAD0000FFFFFFFF,), 1),
        ("mlx_param_negative", (0xDEAD000000000005,), 0),
        ("mlx_byte_plus_one", (), 255),
        ("mlx_ten", tuple(range(1, 11)), sum(i * i for i in range(1, 11))),
        ("mlx_arena", (), 63 * 63 + 1),
    ]
    with tempfile.TemporaryDirectory() as tmp:
        library = os.path.join(tmp, "libmain.so")
        subprocess.run([mlx1, "tests/aarch64/shared_library.mlx", "-o", library, "--target=aarch64-android", "--quiet"],
                       cwd=ROOT, check=True)
        loaded = aarch64_linux_emulator.SharedLibrary(library, imports=imports)
        failures = 0
        expected_needed = ["libandroid.so", "liblog.so", "libm.so", "libc.so", "libdl.so"]
        if loaded.needed != expected_needed:
            print(f"DT_NEEDED {loaded.needed}, expected {expected_needed}")
            failures += 1
        for name, args, expected in checks:
            actual = loaded.call(name, *args)
            if actual != expected:
                print(f"{name}{args} = {actual}, expected {expected}")
                failures += 1
        if written != [(1, b"hello from mlx\n")]:
            print(f"write() calls {written}")
            failures += 1
        if loaded.missing:
            print(f"unbound imports called: {sorted(loaded.missing)}")
            failures += 1
    print(f"{len(checks) + 2 - failures}/{len(checks) + 2} shared-library checks passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
