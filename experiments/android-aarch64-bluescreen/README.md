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

## The self-hosted mlx port (`mlx/`)

`tool/` above is a *Zig* implementation, which only exists because mlx's
own build (`mlx0`, the bootstrap compiler) is itself written in Zig. But
Zig is meant to be the bootstrap for `mlx0` only — everything else is
supposed to run on mlx's own self-hosted compiler (`mlx1`, built from
`compiler/selfhost/`). `mlx/` is this same experiment ported to genuine mlx
source, compiled by `./zig-out/bin/mlx1` and run as a native x86_64
executable with **no Zig involved at runtime at all** (`mlx0`/Zig only
built the `mlx1` compiler itself, same as always).

```sh
zig build mlx1                                   # build the self-hosted compiler once
./zig-out/bin/mlx1 experiments/android-aarch64-bluescreen/mlx/main.mlx -o /tmp/build-apk
cd /tmp && /tmp/build-apk                        # writes bluescreen.apk in the cwd
```

Every layer was re-verified against the same tools as the Zig version
(`aapt`/`aapt2`, `readelf`, `dexdump`, `unzip -t`, `openssl x509`/`asn1parse`/
`verify -check_ss_sig`, and `jarsigner -verify` reporting `jar verified.` on
the final signed APK) — porting line-by-line isn't enough of a guarantee on
its own given how young the self-hosted compiler is, so each module got the
same external-oracle treatment the Zig version did, not just a recompile.

Two differences from `tool/`, both load-bearing:

- **RSA-1024 (512-bit primes), not RSA-2048.** `mlx/bignum.mlx` is a
  from-scratch arbitrary-precision integer library (mlx's stdlib has no
  bignum, and `uN`/`iN` wider than 64 bits doesn't actually work yet — see
  below), using binary long division rather than Knuth's algorithm for
  simplicity. Combined with a genuinely severe compiler bug found while
  building it (below), 2048-bit keys aren't practical here yet; 1024-bit
  completes reliably in well under a minute. Every operation was verified
  against Python's arbitrary-precision `int` before being ported.
- **Every module works around real self-hosted-compiler bugs**, most of
  them newly found by this port, all written up in
  [`docs/internals/selfhost-compiler.md`](../../docs/internals/selfhost-compiler.md):
  a fixed array of slices silently corrupts on write (`axml.mlx`'s
  `StringPool`, `zip.mlx`'s `EntryList` both use parallel `usize` arrays
  instead); a module-level `const` of non-integer/boolean type miscompiles
  into a bogus link error instead of a diagnostic (every such constant here
  is a zero-argument function instead); taking the address of a struct
  *field* (`&x.field`) doesn't resolve at all, at any nesting depth
  (`bignum.mlx`'s signed-value helper and `jar_sign.mlx`'s RSA keypair both
  thread plain values instead of bundling into a struct); and, most
  severely, a local struct/array variable declared with `undefined` is
  `alloca`'d from a single fixed 256 MiB arena that is **never reclaimed**,
  so any loop that declares one eventually exhausts it and crashes — this
  is the real reason RSA-2048 isn't practical yet, independent of how fast
  the arithmetic itself is. One bug (a struct/array field written with
  `undefined` inside an aggregate literal dereferencing a bogus address)
  was small and clear enough to actually fix, with a regression test; the
  rest are documented as known gaps and worked around here, matching this
  repository's existing practice for the handful of gaps found before this
  port (see the same doc's `uN`/`iN`-above-64-bits entry, itself the reason
  `bignum.mlx` uses u64 limbs instead of a wider native integer type).

This satisfies item 3 of "Where this goes next" below in spirit — the
pipeline is now expressible in ordinary mlx source, not just a bespoke Zig
tool — but it's still hand-written mlx, not a compiler-generated `aarch64`
backend; that larger step (an actual `backend/aarch64/` alongside
`backend/x86_64/`, wired into `mlx build --target aarch64-android`) remains
future work, arguably better-informed now that this port has exercised the
self-hosted compiler this hard and found what it doesn't handle yet.

## Real-device test results

Tested by installing the mlx-built `bluescreen.apk` on a real device
(Google Pixel 10 Pro, Android Canary build 2608):

- **The APK installs and `android.app.NativeActivity` launches without
  crashing.** This is a strong positive signal for everything upstream of
  rendering: the v1 (JAR) signature is accepted, the binary
  `AndroidManifest.xml` parses correctly (`android.app.lib_name`, the
  `hasCode="false"` / native-only activity setup, the MAIN/LAUNCHER intent
  filter), the empty `classes.dex` is valid enough for a code-free app, and
  the hand-written ELF64 `.so` loads and dynamic-links cleanly under
  Bionic (`ANativeActivity_onCreate` is found via the hand-rolled
  `.dynsym`/`.hash` and called; the three `ANativeWindow_*` imports resolve
  through the `R_AARCH64_GLOB_DAT` relocations in `.rela.dyn`).
- **First report: the window stayed black instead of turning blue.** No
  crash, no ANR — just no paint. Root cause: the handler was registered
  only for `onNativeWindowCreated`. A raw `ANativeActivity` (bypassing
  `android_native_app_glue`) can have that very first paint happen before
  the window is actually attached/composited by SurfaceFlinger, get
  silently discarded, and never receive another draw request — which is
  exactly why `android_native_app_glue`'s own sample apps redraw on more
  than a single lifecycle event instead of painting once. Fixed in both
  `tool/native_activity.zig` and `mlx/native_activity.mlx` by registering
  the *same* handler for `onNativeWindowResized` (+64) and
  `onNativeWindowRedrawNeeded` (+72) as well as `onNativeWindowCreated`
  (+56) — all three callbacks share the identical
  `(ANativeActivity*, ANativeWindow*)` signature, and the handler already
  ignores the activity argument, so no new logic was needed, only two more
  `STR` stores into `g_callbacks` in `ANativeActivity_onCreate`'s prologue.
  Verified by decoding the rebuilt `libmain.so`'s `.text` bytes directly
  (not just re-running the external-tool suite) to confirm the three
  stores land at the correct offsets with the correct handler address.
- **Second report, after the fix above: still black.** This pointed at a
  second, more fundamental bug: the window-paint handler calls
  `ANativeWindow_setBuffersGeometry`/`_lock`/`_unlockAndPost` via `BLR`, and
  `BLR` overwrites `LR` (`x30`) with the return address *within the calling
  function* on every call — but the handler never saved its own incoming
  `LR` first. By the time it reached its own `ret`, `LR` held the address
  after the *last* `BLR` (pointing back into the handler's own body, not to
  whoever called it), so the handler never returned control to the Android
  framework at all — most likely hanging or corrupting its own stack right
  after posting the frame, rather than actually failing to draw. This is a
  plain AAPCS64 calling-convention violation (any non-leaf function must
  preserve its incoming `LR` across calls it makes) that both
  `native_activity.zig` and `native_activity.mlx` had from the start; the
  `onNativeWindowCreated`-only registration bug fixed above was real but
  evidently not the whole story. Fixed by saving `LR` to the stack frame
  (`[sp, #48]`, reusing padding that was already there) right after the
  frame is allocated, and restoring it right before the final `ret` — one
  `STR`/`LDR` pair, verified the same way as the previous fix by decoding
  the actual compiled `.text` bytes (`STR x30, [sp, #48]` / ...
  / `LDR x30, [sp, #48]` / `ADD sp, sp, #64` / `RET`).
  Awaiting re-test on-device to confirm the fix; if the screen is still not
  blue after this, the next diagnostic step is `adb logcat` during launch
  to see whether `onNativeWindowCreated`/`onNativeWindowRedrawNeeded` fire
  at all and what `ANativeWindow_lock`/`_setBuffersGeometry` return.

## What's genuinely unverified

Beyond the real-device result above (which confirms the pipeline up through
native library load, and now — pending re-test — the paint path), the
following remain unconfirmed or noteworthy:

- **The `ANativeActivity`/`ANativeWindow` ABI** (`native_activity.zig`'s
  doc comment) was transcribed from memory, not compiled against real NDK
  headers (none are available in this sandbox). Struct field offsets and
  function signatures are believed correct — the NDK guarantees ABI
  stability here, and the real-device test above already confirms
  `ANativeActivity_onCreate`'s signature and the `callbacks` field offset
  are right, since onCreate runs without crashing — but the window-buffer
  fill path (`ANativeWindow_Buffer`, `ANativeWindow_lock`/
  `_setBuffersGeometry`/`_unlockAndPost`) is still only indirectly
  exercised pending confirmation that the blue fill now shows up.
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
