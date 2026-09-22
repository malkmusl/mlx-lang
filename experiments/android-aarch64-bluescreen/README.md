# Experiment: `--target android-aarch64` → a signed, installable APK

**Status: Phase 0 — proof of concept, unverified on real hardware.**

## Goal

The ask this experiment starts working toward: get mlx to the point where
you can hand it an `android_aarch64` (or another ARM) target and get back a
**valid, signed APK** that runs on-device — starting from the simplest
possible payload, a solid-color ("blue screen") window.

mlx's own README is explicit about its approach: *"Mlx emits native machine
code directly and does not require LLVM, GCC, a C compiler, libc, an
external assembler, or an external linker for its reference path."* That
rule is the whole point of this experiment, not a constraint to work
around. Everything here — the AArch64 machine code, the ELF64 shared
object, the binary `AndroidManifest.xml`, the DEX header, the ZIP
container, and the RSA/X.509/PKCS#7 JAR signature — is produced by
hand-written encoders with **no NDK, no `aapt`/`aapt2`, no `clang`, no
`jarsigner`/`apksigner`, no external crypto library**. The only build-time
dependency is Zig, exactly as it already is for `mlx0` (the bootstrap
compiler in `compiler/bootstrap/`, which hand-writes x86_64 ELF64
executables the same way — see `compiler/bootstrap/object/elf64.zig`).
This experiment is that same approach, aimed at `aarch64`/Android instead
of `x86_64`/Linux.

**This is Phase 0, not a finished mlx feature.** None of this is wired
into the mlx language or the self-hosted/bootstrap compiler yet — it's a
standalone Zig tool (`tool/`) that proves the *target pipeline* (codegen →
object format → package format → signature) is buildable with mlx's
no-external-toolchain philosophy, before investing in the much larger work
of a real AArch64 backend inside the compiler itself. See "Where this goes
next" below.

## What it builds

`tool/main.zig` produces `bluescreen.apk`: an `android.app.NativeActivity`
(the stock Android framework activity class — no JVM/Dalvik app code
needed) backed by a native library that, on `onNativeWindowCreated`, locks
the window buffer, fills it with opaque blue, and posts it.

```
bluescreen.apk
├── AndroidManifest.xml           (binary AXML)
├── classes.dex                   (structurally valid, empty)
├── lib/arm64-v8a/libmain.so      (ELF64 DYN, aarch64, hand-assembled)
└── META-INF/
    ├── MANIFEST.MF               (JAR manifest, SHA-256 per-entry digests)
    ├── CERT.SF                   (signature file)
    └── CERT.RSA                  (PKCS#7 SignedData: RSA-2048 + self-signed X.509)
```

## Build & run

```sh
cd tool
zig build-exe -O ReleaseFast main.zig --name mlx-android-bluescreen
./mlx-android-bluescreen bluescreen.apk dev.mlxlang.experiments.bluescreen
```

Key generation (RSA-2048, from scratch — see `bignum.zig`) is the slow
part, typically several seconds to around 20s depending on how many
candidate primes Miller-Rabin has to reject; everything else is
sub-second. To install on a connected device/emulator once you've built
it: `adb install bluescreen.apk`.

Run the unit tests for every module (25 tests, no device/network needed):

```sh
zig test all_tests.zig
```

Built and tested against Zig **0.14.0** (stable). It's a standalone tool,
not wired into the repository root `build.zig` — the bootstrap compiler's
own Zig files (`compiler/bootstrap/`) currently need a much newer/nightly
Zig (they use `std.process.Init`-style APIs that don't exist in 0.14), so
rather than fight that mismatch this experiment stays self-contained until
either the bootstrap compiler's pinned Zig version is documented/settled or
this code moves into the self-hosted compiler instead (see below).

## How each layer was verified

No component here was taken on faith. Each hand-written encoder was
checked against a real, independent, authoritative implementation —
**used only as a development-time oracle to catch bugs, never as a
dependency of the tool itself** (nothing in `tool/` shells out to any of
these):

| Layer | File | Checked against |
|---|---|---|
| AArch64 instruction encoding | `aarch64.zig` | `llvm-mc -triple=aarch64 -show-encoding` — every encoder function has a unit test asserting the exact hex `llvm-mc` produced for the same instruction |
| Generated machine code | `native_activity.zig` | Disassembled with `llvm-mc -disassemble` and read back instruction-by-instruction (branch targets, GOT offsets, struct field offsets all confirmed to land where intended) |
| ELF64 shared object | `elf_so.zig` | `readelf -h/-l/-d/-r/-x --dyn-syms`: correct `ET_DYN`/`EM_AARCH64` header, program headers, `.dynamic`, relocations, SysV hash table bytes |
| SysV `.hash` algorithm | `elf_so.zig` (`elfHash`) | An independent from-scratch Python re-implementation of the System V ABI hash function |
| Binary `AndroidManifest.xml` | `axml.zig` | **Real Google `aapt`/`aapt2`** (from Ubuntu's `google-android-build-tools-30.0.3-installer` package, installed purely as a verification oracle — see note below) via `aapt dump xmltree` and `aapt dump badging`, both fully succeeding and correctly reporting the package name, SDK levels, and `android.app.NativeActivity` as the launchable activity. The `android:*` attribute resource IDs (e.g. `versionCode = 0x0101021b`) were *read out of* a real `aapt`-compiled reference manifest rather than trusted from memory, specifically because getting one wrong would silently produce a manifest the real platform parser misreads. |
| `classes.dex` | `dex.zig` | AOSP's `dexdump` (from the same build-tools package) |
| ZIP/APK container | `zip.zig` | `unzip -t`/`unzip -l`, and the full APK additionally passed `file`, which correctly identified it as an "Android package (APK), with AndroidManifest.xml" |
| RSA/bignum arithmetic | `bignum.zig` | Known textbook modexp test vectors, and Miller-Rabin cross-checked against small known primes/composites |
| ASN.1 DER / OIDs | `asn1.zig` | Hand-verified OID byte sequences for `rsaEncryption`, `sha256WithRSAEncryption`, `id-sha256`, `commonName` |
| X.509 cert / PKCS#7 | `jar_sign.zig` | `openssl x509 -text` and `openssl pkcs7 -print_certs -text` both fully parse the generated certificate and signed-data structure |
| **The whole signed APK** | — | **`jarsigner -verify -verbose -certs` reports `jar verified.`** on the final output, with every entry marked `sm` (signature verified, listed in manifest) |

The `google-android-build-tools`/`aapt`/`dexdump`/`jarsigner`/`openssl`/
`llvm-mc` tools above were installed into this development sandbox
specifically to *validate* the hand-written encoders during this session —
they are not referenced anywhere in `tool/`'s source and nothing here
requires them to be present to build or run.

## What's genuinely unverified

This environment has no Android emulator or device, so the one thing that
has **not** been confirmed is the thing the experiment is ultimately for:
whether the app actually shows a blue screen when it runs. Specifically:

- **The `ANativeActivity`/`ANativeWindow` ABI** (`native_activity.zig`'s
  doc comment) was transcribed from memory, not compiled against real NDK
  headers (none are available in this sandbox). Struct field offsets and
  function signatures are believed correct — the NDK guarantees ABI
  stability here — but this is the single highest-risk assumption in the
  whole pipeline and should be the first thing checked against real
  `android/native_activity.h`/`android/native_window.h` before trusting
  this further.
- **APK Signature Scheme v1 (JAR signing) only.** No v2/v3 signing block.
  This is why `minSdkVersion`/`targetSdkVersion` are kept at 21/29 in
  `axml.zig` — v1-only APKs are accepted at those levels. Devices/policies
  that require v2+ (Android enforces this starting around API 30 for
  `targetSdkVersion`) will reject this APK as-is; adding a v2 signing
  block is a reasonably contained follow-up once v1 is confirmed to
  actually install and run.
- **No native-library page alignment.** `libmain.so` is stored (not
  compressed) but not 4/16 KiB-aligned within the ZIP, so it relies on
  `extractNativeLibs`-style extraction rather than direct `mmap`. Fine
  functionally, worth tightening later.
- **RSA-2048 implementation is from-scratch and unaudited.** It produces
  signatures `jarsigner`/`openssl` accept, which is a strong structural and
  interoperability signal, but this code has not had any cryptographic
  review and should not be treated as a hardened signing implementation —
  treat every key it generates as a throwaway debug key.
- The `configChanges` flag value (`0x4a0`) was taken directly from what a
  real `aapt` resolved for
  `orientation|keyboardHidden|screenSize` rather than re-derived bit by
  bit, to avoid a second source of memorization risk.

## Where this goes next

Roughly in order of what unblocks what:

1. **Validate on a real target** — an Android emulator (`qemu-system-aarch64`
   or the Android emulator itself) or device, via `adb install`. This is
   the actual "does it work" gate everything above is standing in for.
2. If the ABI assumptions need fixing, fix `native_activity.zig` and
   re-verify with the same `llvm-mc` disassembly technique.
3. **Bring this into the language**, not just a side tool: an `aarch64`
   backend under `compiler/selfhost/backend/` (mirroring
   `compiler/selfhost/backend/x86_64/`), an `android`/ELF-shared-object
   object-format mode alongside `compiler/selfhost/object/elf64.mlx`, and
   eventually a `std.android` module so this is expressible in ordinary
   mlx source rather than hand-built by a bespoke Zig tool. The Zig tool
   here is scaffolding to de-risk that work, not a replacement for it.
4. APK Signature Scheme v2 (or v2+v1 for broader compatibility).
5. A real target triple story in the build system (`mlx build --target
   aarch64-android`), matching how `zig build`'s `standardTargetOptions`
   already works for the *bootstrap* compiler's own binary today (that's
   the host `mlx0`'s target, not the target mlx *emits code for* — this
   experiment is about the latter).
