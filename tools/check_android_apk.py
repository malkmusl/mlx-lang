#!/usr/bin/env python3
"""Builds an APK with mlx1 --target=aarch64-android and checks it with the
Android SDK tools: apksigner (v1/v2/v3), jarsigner, zipalign (4-byte entries,
page-aligned libraries), and aapt2 (manifest contents). Also checks that the
APK is reproducible (two builds with the same key are byte-identical).

Uses experiments/android-aarch64-bluescreen/debug_signing_key.bin so no key
is generated. Tools that are not installed are reported and skipped.
Usage: python3 tools/check_android_apk.py [path/to/mlx1]
"""
import os
import shutil
import subprocess
import sys
import tempfile

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
              '"android.app.lib_name"', '"main"']),
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
