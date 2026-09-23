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
- **Third report, after both fixes above: still black.** No adb/computer
  access was available, so diagnosis moved to phone-only evidence instead:
  a raw view-hierarchy dump of the running activity's decor view (obtained
  on-device, no adb). It showed the tree rooted at the standard
  `DecorView`, but containing a full `ActionBarOverlayLayout` →
  `ActionBarContainer` → `Toolbar` (with a title `TextView` and
  `ActionMenuView`), themed `android:style/Theme.DeviceDefault.Light.
  DarkActionBar` — i.e. the activity was running under the **device's
  default themed window** (action bar and all), not a plain fullscreen
  native window, because the manifest never set an explicit
  `android:theme`. This is atypical for a native-activity/game-style app
  (virtually every real one sets an explicit fullscreen/no-title theme)
  and is a real, independent bug regardless of whether it's the sole
  explanation for the black screen. Fixed by adding
  `android:theme="@android:style/Theme.Black.NoTitleBar.Fullscreen"` to
  the `<activity>` element in both `axml.zig` and `axml.mlx`. The
  resource ID for `android:theme` (`0x01010000`) and the resolved
  reference value for that theme (`0x0103000a`) were ground-truthed the
  same way `configChanges`'s `0x4a0` was originally: compiled a minimal
  reference manifest with that exact theme attribute through the real
  `aapt` against `/usr/share/android-framework-res/framework-res.apk`
  (present in this sandbox) and read the resolved IDs back from
  `aapt dump xmltree`, rather than trusting memory for either number.
  Verified the rebuilt manifest against both `aapt dump xmltree` and
  `aapt2 dump xmltree` showing `android:theme(0x01010000)=@0x0103000a`
  correctly, with the rest of the tree unchanged.
- **Fourth report: fullscreen now, but still black on both a black-
  background theme and a light-background theme.** This is the single most
  informative result so far. `Theme.Black.NoTitleBar.Fullscreen`'s own
  default window background is black, so a black result under it was
  ambiguous -- it could mean the native fill was failing, or it could mean
  the fill was irrelevant because the theme's own black background was
  always what was showing. Swapping to `Theme.Light.NoTitleBar.Fullscreen`
  (0x0103000e, ground-truthed the same way) and seeing the *same* solid
  black ruled that out: a `SurfaceView`/`NativeContentView` shows solid
  black by default *before any buffer has ever been posted to it*,
  independent of the window's own theme/background, which is compositied
  underneath it. So this result specifically means: no buffer is reaching
  the screen -- either the window-created callback is never firing, or
  `ANativeWindow_lock`/`_setBuffersGeometry`/`_unlockAndPost` are failing
  every time.

  With no adb/logcat access at all, the fix turned into a diagnostic:
  removed the `ANativeWindow_setBuffersGeometry(window, 0, 0, RGBA_8888)`
  call entirely (ruling it out as a variable -- `ANativeWindow_lock` now
  uses the window's current/default size and format), and added explicit
  checks on `ANativeWindow_lock`'s and `ANativeWindow_unlockAndPost`'s
  return values that deliberately execute `udf #0` (a permanently
  undefined instruction, guaranteed `SIGILL`) at two distinct, known
  addresses on failure -- added as a new `udf` encoder in both
  `aarch64.zig`/`aarch64.mlx`. If this build is still black with no crash,
  that means the handler is never being invoked at all (a different bug
  than previously suspected). If it crashes, the crash's faulting PC
  (from a tombstone, or the same kind of on-device dump used for the
  theme diagnosis) pinpoints exactly which call failed, since the two
  `udf`s sit at distinct, known offsets in `.text`. If it turns blue,
  `setBuffersGeometry(0, 0, ...)` itself was the actual problem all along.
  Verified by decoding the rebuilt `.text` bytes directly to confirm both
  `cbnz` checks branch to their correct, distinct trap addresses.
- **Fifth report: still black, no crash.** This ruled out `ANativeWindow_lock`/
  `_unlockAndPost` returning failure (either trap would have fired) and
  left exactly two explanations: the callback is never invoked, or it runs
  and every call reports success but nothing reaches the screen.
  Disambiguated with an unconditional `udf #0` as the *handler's* own
  first instruction (sixth round): **no crash** — conclusive proof the
  window-created/resized/redraw-needed callback is never invoked by the
  framework at all, pointing upstream at how it gets *registered*, not at
  anything inside the handler.
- **Sixth/seventh rounds, narrowing further:** moved the trap to
  `ANativeActivity_onCreate`'s own first instruction — **crashed
  immediately on launch**, proving `onCreate` itself does run. Every
  `ANativeActivityCallbacks`/`ANativeActivity` struct offset this code
  uses was then cross-checked against the real AOSP source
  (`android.googlesource.com`, `frameworks/native/include/android/
  native_activity.h`) instead of memory, and matched exactly
  (`callbacks@0`, `onNativeWindowCreated@56`, `onNativeWindowResized@64`,
  `onNativeWindowRedrawNeeded@72`) — ruling out a struct-offset mistake.
  One assumption remained: does `x0` (the `ANativeActivity*` argument)
  genuinely point where expected? Read `activity->sdkVersion` (an
  `int32_t` at offset 48) and deliberately dereferenced it as a pointer —
  a real SDK version is never a valid address, so this reliably `SIGSEGV`s,
  and Android's crash report's fault address directly reveals the exact
  value with no adb needed. The resulting tombstone (extracted from a
  Developer-options bug report's `FS/data/tombstones/` folder, the same
  phone-only technique used for the earlier view-hierarchy dump) showed
  **`fault addr 0x0000000000000025` (37 decimal)** — a plausible SDK level
  for a bleeding-edge Canary build — confirming `x0` and the struct layout
  were right all along. The same tombstone's backtrace independently
  confirmed the crash landed at `ANativeActivity_onCreate+4`, called from
  `android::loadNativeCode_native` → `android.app.NativeActivity.onCreate`,
  and its kernel string (`6.6.127-android15-8-...-4k`) confirmed a 4 KiB
  page size, ruling out a 16 KiB-page-alignment theory too.
- **Root cause, found by reading the actual framework source
  (`frameworks/base/core/jni/android_app_NativeActivity.cpp`) rather than
  guessing further:** `onSurfaceCreated_native` there checks
  `code->callbacks.onNativeWindowCreated != NULL`, reading through
  `activity->callbacks` as a pointer the *framework itself* already
  allocates and pre-populates (as part of its own internal `NativeCode`
  object) *before* ever calling our `onCreate`. The correct contract is to
  write **into** that already-allocated `ANativeActivityCallbacks` struct
  through the pointer the framework hands you
  (`activity->callbacks->onNativeWindowCreated = ...`) — not to allocate a
  separate struct of your own and replace the pointer
  (`activity->callbacks = &ourOwnStruct`), which is what this code had
  been doing since the very first version. Replacing the pointer leaves
  the framework's own internal storage (the one its guard actually checks)
  untouched and permanently all-`NULL`, so the callback silently never
  fires — no crash, no error, no visible effect — exactly matching every
  round of on-device testing above. Fixed in both `native_activity.zig`
  and `native_activity.mlx`: `ANativeActivity_onCreate` now reads
  `activity->callbacks` with a plain `LDR` instead of computing the
  address of an owned buffer and overwriting the pointer with `STR`; the
  now-unnecessary 128-byte `g_callbacks` allocation and its ELF `.data`
  section were removed from `elf_so.zig`/`elf_so.mlx` accordingly. Also
  restored the `ANativeWindow_setBuffersGeometry` call the diagnostic
  rounds had removed, now that fetching the real `NativeActivity.java`
  source (also read during this investigation) confirmed the window's
  *actual* default surface format is `PixelFormat.RGB_565` (16-bit), not
  `RGBA_8888` — needed regardless of the callback-registration bug, since
  the 32-bit fill loop would otherwise misinterpret a 16-bit buffer.
  Verified by decoding the rebuilt `.text` bytes instruction-by-instruction
  against the intended sequence, and the full external-tool suite (`aapt`,
  `readelf`, `unzip -t`, `jarsigner -verify`) once more.
- **Confirmed on-device: a real blue screen.** The fix above worked.
  Immediately surfaced a second, unrelated issue: performing the Android
  back gesture crashed the app. The only crash points left anywhere in the
  code at that point were the `ANativeWindow_lock`/`_unlockAndPost`
  failure traps kept from the diagnostic rounds above (as a "safety net")
  — and a lock failure is exactly what a window mid-teardown, which is
  what the back gesture's dismiss animation causes, legitimately produces.
  A well-behaved app is supposed to silently skip that frame, not crash.
  Downgraded both traps back to a plain skip-to-epilogue (their original,
  pre-diagnostic behavior) in both `native_activity.zig` and
  `native_activity.mlx`, now that they'd served their purpose of
  confirming the real callback-registration bug was fixed. Also reverted
  the manifest's theme from the diagnostic `Theme.Light.NoTitleBar.
  Fullscreen` back to the originally-intended `Theme.Black.NoTitleBar.
  Fullscreen` in both `axml.zig`/`axml.mlx`, now that the blue fill is
  confirmed to work independent of the theme's own background color.

This closes out the black-screen investigation: nine real-device rounds,
three genuine bugs found and fixed (missing `onNativeWindowResized`/
`onNativeWindowRedrawNeeded` registration, missing `LR` preservation
across the handler's nested calls, and — the actual root cause —
replacing `activity->callbacks` instead of writing through it), one
correctness fix found along the way (the window's real default format is
`RGB_565`, not `RGBA_8888`), and a lot of what turned out to be
correctly-transcribed ABI assumptions (struct offsets, calling
conventions, ELF/dynamic-linking details) that real-device evidence
independently confirmed rather than contradicted.

## What's genuinely unverified

The real-device rounds above confirm the pipeline end to end: JAR
signature acceptance, binary-manifest parsing, empty-DEX validity, ELF64
`.so` loading and dynamic-linking, the `ANativeActivity`/
`ANativeActivityCallbacks` struct layout (cross-checked against real AOSP
source, not just memory), and the actual `ANativeWindow_lock`/
`_setBuffersGeometry`/`_unlockAndPost` fill-and-present path all the way
to visible pixels on a real screen. What's left:

- **APK Signature Scheme v1 (JAR signing) only.** No v2/v3 signing block.
  This is why `minSdkVersion`/`targetSdkVersion` are kept at 21/29 in
  `axml.zig` — v1-only APKs are accepted at those levels. Devices/policies
  that require v2+ (Android enforces this starting around API 30 for
  `targetSdkVersion`) will reject this APK as-is; adding a v2 signing
  block is a reasonably contained follow-up once v1 is confirmed to
  actually install and run.
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
