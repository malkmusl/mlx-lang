#!/usr/bin/env python3
"""Builds an APK with mlx1 --target=aarch64-android and checks it with the
Android SDK tools: apksigner (v1/v2/v3), jarsigner, zipalign (4-byte entries,
page-aligned libraries), and aapt2 (manifest contents). Also checks that the
APK is reproducible (two builds with the same key are byte-identical), and,
without the SDK, what lets Android run the library straight from the APK
(extractNativeLibs=false, the default): lib/arm64-v8a/libmain.so stored
uncompressed with its data 16 KB-aligned in the file, ELF load segments
16 KB-aligned, and the manifest's extractNativeLibs (true with
--android-extract-native-libs).

Uses experiments/android-aarch64-bluescreen/debug_signing_key.bin so no key
is generated. Tools that are not installed are reported and skipped.
Usage: python3 tools/check_android_apk.py [path/to/mlx1]
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KEY = "experiments/android-aarch64-bluescreen/debug_signing_key.bin"
SOURCE = "tests/aarch64/shared_library.mlx"


def run(command):
    result = subprocess.run(command, capture_output=True, text=True)
    output = "\n".join(line for line in (result.stdout + result.stderr).splitlines()
                       if not line.startswith("Picked up JAVA_TOOL_OPTIONS"))
    return result.returncode, output


def build(mlx1, output, extra=()):
    subprocess.run([mlx1, SOURCE, "-o", output, "--target=aarch64-android", "--android-key=" + KEY,
                    "--android-package=dev.mlxlang.check", "--android-label=Mlx Check",
                    "--android-version-code=7", "--android-version-name=7.0", "--quiet", *extra],
                   cwd=ROOT, check=True)


EXTRACT_NATIVE_LIBS = 0x010104EA
PAGE = 16384


def manifest_boolean(apk, resource_id):
    """The boolean attribute `resource_id` in the APK's binary manifest
    (None when absent)."""
    data = zipfile.ZipFile(apk).read("AndroidManifest.xml")
    ids = []
    offset = 8
    while offset < len(data):
        kind, header, size = struct.unpack_from("<HHI", data, offset)
        if kind == 0x0180:  # resource map: string index -> resource id
            ids = list(struct.unpack_from(f"<{(size - header) // 4}I", data, offset + header))
        elif kind == 0x0102:  # start element
            start, attribute_size, count = struct.unpack_from("<HHH", data, offset + header + 8)
            for index in range(count):
                at = offset + header + start + index * attribute_size
                _ns, name, _raw, _size, _res0, value_type, value = struct.unpack_from("<IIIHBBI", data, at)
                if name < len(ids) and ids[name] == resource_id:
                    assert value_type == 0x12, f"attribute {resource_id:#x} is not a boolean"
                    return value != 0
        offset += size
    return None


def library_checks(apk):
    """Problems that would stop Android from loading the library in place."""
    problems = []
    archive = zipfile.ZipFile(apk)
    info = archive.getinfo("lib/arm64-v8a/libmain.so")
    if info.compress_type != zipfile.ZIP_STORED:
        problems.append("libmain.so is compressed")
    raw = open(apk, "rb").read()
    name_length, extra_length = struct.unpack_from("<HH", raw, info.header_offset + 26)
    data_start = info.header_offset + 30 + name_length + extra_length
    if data_start % PAGE:
        problems.append(f"libmain.so data at {data_start}, not 16 KB-aligned")
    elf = archive.read("lib/arm64-v8a/libmain.so")
    phoff, = struct.unpack_from("<Q", elf, 32)
    phentsize, phnum = struct.unpack_from("<HH", elf, 54)
    for index in range(phnum):
        kind, _flags, offset, vaddr = struct.unpack_from("<IIQQ", elf, phoff + index * phentsize)
        align, = struct.unpack_from("<Q", elf, phoff + index * phentsize + 48)
        if kind == 1 and (align < PAGE or offset % PAGE != vaddr % PAGE):
            problems.append(f"load segment at {offset:#x} is not 16 KB-aligned")
    return problems


def main():
    mlx1 = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "zig-out/bin/mlx1")
    failures = []
    with tempfile.TemporaryDirectory() as tmp:
        apk = os.path.join(tmp, "check.apk")
        again = os.path.join(tmp, "again.apk")
        build(mlx1, apk)
        build(mlx1, again)
        if open(apk, "rb").read() != open(again, "rb").read():
            failures.append("two builds with the same inputs differ")

        checks = [
            ("apksigner", ["apksigner", "verify", "-v", apk],
             ["Verified using v1 scheme (JAR signing): true",
              "Verified using v2 scheme (APK Signature Scheme v2): true",
              "Verified using v3 scheme (APK Signature Scheme v3): true"]),
            ("jarsigner", ["jarsigner", "-verify", apk], ["jar verified."]),
            ("zipalign", ["zipalign", "-c", "-p", "4", apk], []),
            ("aapt2", ["aapt2", "dump", "xmltree", "--file", "AndroidManifest.xml", apk],
             ['package="dev.mlxlang.check"', "versionCode(0x0101021b)=7", "minSdkVersion(0x0101020c)=21",
              "targetSdkVersion(0x01010270)=37", '"android.app.NativeActivity"', "exported(0x01010010)=true",
              '"android.app.lib_name"', '"main"', "extractNativeLibs(0x010104ea)=false"]),
        ]
        for tool, command, expected in checks:
            if shutil.which(tool) is None:
                print(f"skip {tool}: not installed")
                continue
            status, output = run(command)
            missing = [text for text in expected if text not in output]
            if status != 0 or missing:
                failures.append(f"{tool}: exit {status}, missing {missing}\n{output[-1500:]}")
            else:
                print(f"ok   {tool}")

        # The library runs from the APK unless extraction is asked for.
        problems = library_checks(apk)
        if manifest_boolean(apk, EXTRACT_NATIVE_LIBS) is not False:
            problems.append(f"extractNativeLibs is {manifest_boolean(apk, EXTRACT_NATIVE_LIBS)}, expected false")
        extracting = os.path.join(tmp, "extract.apk")
        build(mlx1, extracting, ["--android-extract-native-libs"])
        if manifest_boolean(extracting, EXTRACT_NATIVE_LIBS) is not True:
            problems.append("--android-extract-native-libs: extractNativeLibs is not true")
        if problems:
            failures.extend(problems)
        else:
            print("ok   libmain.so stored, 16 KB-aligned in the APK and in its segments; extractNativeLibs false (true with --android-extract-native-libs)")

        # The minimum/target SDK choose the signature schemes: below 24 only v1.
        old = os.path.join(tmp, "old.apk")
        build(mlx1, old, ["--android-target-sdk=23"])
        if shutil.which("apksigner"):
            status, output = run(["apksigner", "verify", "-v", "--min-sdk-version", "21", old])
            if status != 0 or "v2 scheme (APK Signature Scheme v2): false" not in output:
                failures.append(f"target 23 APK: expected v1 only\n{output[-800:]}")
            else:
                print("ok   apksigner (target 23: v1 only)")

    for failure in failures:
        print("FAIL", failure)
    print("android APK checks:", "failed" if failures else "passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
