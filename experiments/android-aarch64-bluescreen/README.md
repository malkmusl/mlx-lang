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

- **Tenth report: a real blue screen, but "Mlx Blue Screen reagiert
  nicht" (an ANR, "app not responding") on the back gesture, after
  backgrounding, and even just after sitting idle for about a minute.**
  Not a crash — no new tombstone appeared for it at all (confirmed by
  cross-checking timestamps across two separate bug-report pulls), which
  is itself the key clue: an unresponsive main thread, not a fault. All
  three triggers share the same underlying event: Android running a live
  window-resize/transition animation (the back gesture's dismiss preview,
  the recents/home transition, and — for the idle case — most likely the
  screen-timeout transition), each of which can fire
  `onNativeWindowResized` on every animation frame. This handler was
  still registered for that callback (registered back when the actual
  bug was the callback-registration one, to make sure *some* redraw path
  would eventually fire) and re-ran its full synchronous
  `ANativeWindow_lock`/`_setBuffersGeometry`/`_unlockAndPost` cycle --
  real Binder IPC round-trips -- on *every* firing, with no rate
  limiting. Enough of those firing back-to-back during one animation was
  enough blocking work on the main thread to trip the ANR watchdog.
  Fixed by no longer registering `onNativeWindowResized` at all, in both
  `native_activity.zig` and `native_activity.mlx` -- since the fill is a
  single solid color, it doesn't need repainting on every resize anyway;
  the already-posted buffer is simply scaled by the compositor during the
  animation and still looks correct, and `onNativeWindowRedrawNeeded`
  (fired sparingly, not per-frame, specifically when a fresh frame is
  actually needed) remains registered alongside `onNativeWindowCreated`.
  Verified by decoding the rebuilt `.text` bytes: `ANativeActivity_onCreate`
  is now 6 words and stores the handler's address at only two offsets (56
  and 72), with no store to 64.
- **Eleventh report: the exact same ANR persisted** even with
  `onNativeWindowResized` gone. `onNativeWindowRedrawNeeded` was the
  remaining live suspect: the framework also uses it specifically to
  *synchronize app-transition animations* -- it can call in and wait on
  exactly the same triggers (back gesture, home, screen timeout) already
  implicated above, so the same "handler is slow/blocking, animation
  waits on it" mechanism applies just as well. It had only ever been
  registered on a guess, made *before* the real callback-registration bug
  was found, that the very first paint might get silently discarded
  without it; now that the actual bug is fixed and confirmed working on
  a real device, that guess no longer has anything to justify it -- this
  experiment's content is one static, unchanging solid-color frame, and
  once it's successfully posted the compositor keeps showing that same
  buffer indefinitely (scaled as needed) with zero further involvement
  from the app, so there is nothing to ever redraw on demand. Fixed by
  registering *only* `onNativeWindowCreated` in both `native_activity.zig`
  and `native_activity.mlx` -- the minimal, most conservative
  configuration, and arguably what this experiment should have used from
  the start once the real bug was understood. Verified by decoding the
  rebuilt `.text` bytes: `ANativeActivity_onCreate` is now 5 words with a
  single store, to offset 56 only.
- **Twelfth report: the exact same ANR persisted, even with *no* window
  callback registered at all.** This was decisive: since the paint
  handler could then only ever fire once (at window creation) and had
  already succeeded (a real blue screen had rendered), the ANR could no
  longer have anything to do with painting. That pointed at something
  structural instead: `android.app.NativeActivity`'s own `onCreate`
  unconditionally calls `getWindow().takeInputQueue(this)`, handing this
  app an input event queue that Android expects native code to actively
  *drain* -- exactly what `android_native_app_glue.c`'s own internal
  plumbing does for every app built on it, which this bare-NativeActivity
  experiment had never done. An input event sitting in the queue forever
  with nothing ever consuming it is exactly what trips Android's "Input
  dispatching timed out" watchdog -- and the back gesture is fundamentally
  a touch/drag gesture, home a swipe or button press, and even the idle
  case plausibly involves *some* system-generated input-adjacent event
  during the screen-timeout transition.

  Fixed by registering `onInputQueueCreated` (offset 88) and implementing
  a lightweight drain: `ALooper_forThread()` (finds the main thread's
  already-running looper -- the same thread `onInputQueueCreated` itself
  runs on, so no new thread needed) plus `AInputQueue_attachLooper()` to
  register a small callback that loops `AInputQueue_getEvent`/
  `AInputQueue_finishEvent` until the queue is drained, mirroring what
  `android_native_app_glue.c` does minus its extra thread. This needed
  four new NDK imports (`ALooper_forThread`, `AInputQueue_attachLooper`,
  `AInputQueue_getEvent`, `AInputQueue_finishEvent` -- all, like the
  `ANativeWindow_*` imports, part of the same stable `libandroid.so`, so
  no new `DT_NEEDED` entry) and two new AArch64 instruction encoders this
  port had never needed before: `B` (unconditional branch) and `TBZ`/
  `TBNZ` (test bit and branch), both added to `aarch64.zig`/`aarch64.mlx`
  and independently verified against `llvm-mc -triple=aarch64
  -filetype=obj` + `llvm-objdump -d` (both available in this sandbox)
  before use, the same rigor as every other encoder in this file --
  every hand-derived encoding matched exactly. `elf_so.zig`/`elf_so.mlx`
  were extended accordingly (9 dynsym entries, 7 relocations, a 7-slot
  GOT). The full generated `.text` (320 bytes, 80 words) was then decoded
  and checked instruction-by-instruction against the intended sequence --
  every branch target, GOT offset, and patched address landed exactly
  where designed, with no discrepancies. Full external-tool suite
  (`aapt`, `readelf`, `unzip -t`, `jarsigner -verify`) re-run clean.
- **Thirteenth report: confirmed working, including the back gesture** --
  the blue screen renders and the app survives every lifecycle transition
  that used to ANR. The one remaining request: the fullscreen theme was
  hiding the system status bar entirely; real-device feedback was that the
  normal status bar should stay visible instead. Added a toggle rather
  than just flipping the default: `axml.zig`/`axml.mlx`'s `buildManifest`
  now takes a `fullscreen` parameter selecting between
  `Theme.Black.NoTitleBar.Fullscreen` (0x0103000a, hides the status bar)
  and `Theme.Black.NoTitleBar` (0x01030009, keeps it) -- the latter's
  resource ID ground-truthed against the real `aapt` the same way as every
  other theme ID in this file, not memorized. `main.zig`/`main.mlx` expose
  it as a single switch (`FULLSCREEN_ENABLED` / `fullscreenEnabled()`),
  currently set to `false` (status bar visible) per that feedback. No
  native-code changes needed: `ANativeWindow_lock`'s reported buffer
  dimensions already reflect whatever area the system actually gives the
  window, status bar included or not, so the existing fill logic adapts
  automatically. Verified via `aapt dump xmltree` showing
  `android:theme(0x01010000)=@0x01030009` and the rest of the external-tool
  suite once more.
- **Fourteenth report: two more requests after confirming the app fully
  works** -- the status bar should adapt to the device's actual system
  color instead of a hardcoded black bar, as its own toggle independent of
  fullscreen; and rotation is broken, leaving half the screen black
  because the buffer isn't updated.
  - *Status bar color.* Ground-truthed two more theme IDs the same way as
    the two already in use: compiled a reference manifest for each through
    the real `aapt` against `/usr/share/android-framework-res/framework-res.apk`.
    `Theme.DeviceDefault.NoActionBar` = `0x01030129` (status bar visible,
    system-adaptive) and `Theme.DeviceDefault.NoActionBar.Fullscreen` =
    `0x0103012a` (hidden, system-adaptive) -- neither memorized.
    `buildManifest` now takes a second, independent bool
    (`system_status_bar_color` / `systemStatusBarColor`) selecting between
    the hardcoded-black and system-adaptive theme within whichever of the
    fullscreen/status-bar-visible pair `fullscreen` already picked, so all
    four themes are reachable via the two toggles' four combinations.
    Exposed the same way as the existing toggle
    (`SYSTEM_STATUS_BAR_COLOR_ENABLED` / `systemStatusBarColorEnabled()`),
    defaulted to `true` per the feedback. Verified via `aapt dump xmltree`
    showing `android:theme(0x01010000)=@0x01030129`.
  - *Rotation.* Root-caused to this experiment's own manifest: it declares
    `configChanges = orientation|keyboardHidden|screenSize`, which tells
    Android to resize the existing window in place on rotation rather than
    destroying and recreating the whole activity -- and nothing was
    listening for that resize to repaint. `onNativeWindowResized` had been
    deliberately left unregistered since an earlier round (see the tenth
    report above), but that removal was only ever a broad diagnostic guess
    made while chasing the ANR, never actually confirmed as the cause --
    the ANR persisted after removing it and was eventually traced to the
    unrelated unconsumed `AInputQueue`, fixed independently two rounds
    later. With no remaining evidence against it, re-registered
    `onNativeWindowResized` to point at the exact same handler already
    used for `onNativeWindowCreated`: both share the identical
    `(activity, window) -> void` signature, and the handler already
    re-reads the buffer's current width/height/stride from
    `ANativeWindow_lock`'s own out-param on every call rather than caching
    them, so no other change was needed. Verified with `llvm-objdump
    -d --triple=aarch64` on the rebuilt `.so`: `ANativeActivity_onCreate`
    now stores the same handler address into `activity->callbacks` at both
    offset `0x38` (56, `onNativeWindowCreated`) and offset `0x40` (64,
    `onNativeWindowResized`). `onNativeWindowRedrawNeeded` stays
    unregistered -- no evidence it's needed for anything this experiment
    does. Because this reintroduces a callback whose earlier removal was
    never conclusively tied to the ANR, this round's real-device request
    should re-test not just rotation but also the previously-fragile
    interactions (back gesture, idle timeout, home) to confirm the ANR
    stays fixed now that it's back. Full external-tool suite (`aapt`,
    `jarsigner -verify`, `unzip -t`) re-run clean either way.
- **Fifteenth report: the fourteenth round's two fixes weren't quite
  right** -- the status bar still didn't show the system color with the
  blue background underneath, and rotation still left a black bar on one
  edge.
  - *Status bar color, take two.* `Theme.DeviceDefault.NoActionBar`
    (0x01030129, the previous round's fix) is still a real, correctly-
    resolved theme, but it turns out to be the wrong mechanism: it's an
    *opaque* status bar, just painted with whatever unthemed color
    DeviceDefault happens to default to -- not visibly different from the
    old hardcoded-black theme, and it never lets the app's own blue fill
    show through. Fetched the real AOSP source
    (`themes_material.xml`, which `Theme.DeviceDefault.NoActionBar.
    TranslucentDecor` inherits from) rather than guessing what
    "TranslucentDecor" actually does: it sets `windowTranslucentStatus`
    =true, `windowTranslucentNavigation`=true, `windowContentOverlay`
    =@null -- exactly the mechanism for a see-through status bar with the
    app's own content extending underneath and the system's icons/clock
    drawn on top, which is what "adapt to the system color" + "blue
    background" actually meant. Ground-truthed the real resource ID the
    same way as every other theme in this file: `aapt` against
    `/usr/share/android-framework-res/framework-res.apk` resolved
    `Theme.DeviceDefault.NoActionBar.TranslucentDecor` to `0x010301e3`.
    `themeSystemWithStatusBar()`/`THEME_SYSTEM_WITH_STATUS_BAR` now point
    at this instead -- no change needed to the toggle plumbing itself,
    since it was already just a resource-ID constant. Verified via
    `aapt dump xmltree` showing `android:theme(0x01010000)=@0x010301e3`.
  - *Rotation, take two.* The fourteenth round's fix (re-registering
    `onNativeWindowResized`) treated the symptom, not the root cause. The
    actual root cause is this experiment's own `configChanges` value: it
    declares `orientation|screenSize` (0x4a0), which opts the app INTO
    handling rotation itself by resizing the existing window in place
    (dispatched via `onNativeWindowResized`) instead of letting Android use
    its normal, vastly more exercised path of destroying and recreating the
    whole activity on rotation. That in-place path turns out to be
    genuinely fragile in practice, not just under this hand-rolled
    implementation: a fetched real NDK issue
    ([android/ndk#1139](https://github.com/android/ndk/issues/1139))
    confirms that even Google's own reference
    `android_native_app_glue.c` -- the library literally every C/C++ NDK
    game and app is built on -- never wires up
    `onNativeWindowResized`/`onNativeWindowRedrawNeeded` at all, despite
    defining the command constants for them. This experiment has no state
    worth preserving across an activity restart (it's a single static
    frame), so there's no reason to keep opting into the fragile path:
    dropped `orientation|screenSize` from `configChanges`, leaving only
    `keyboardHidden` (ground-truthed via `aapt` to resolve to `0x20`, down
    from `0x4a0`). A rotation now goes through a fresh
    `ANativeActivity_onCreate` -> `onNativeWindowCreated` with the new
    orientation's correct dimensions from the start -- the same path
    already proven correct, real-device-confirmed, across every other
    lifecycle transition this experiment has been tested against.
    `onNativeWindowResized`/`onNativeWindowRedrawNeeded` stay registered
    regardless (now both, not just the former -- registering both costs two
    cheap instructions and the shared handler is already proven idempotent
    and safe to call repeatedly), as a harmless fallback for any resize
    Android dispatches to a still-live window without a restart. Verified
    with `llvm-objdump -d --triple=aarch64` on the rebuilt `.so`:
    `ANativeActivity_onCreate` now stores the identical handler address
    into `activity->callbacks` at offsets `0x38`/`0x40`/`0x48` (56/64/72 --
    `onNativeWindowCreated`/`onNativeWindowResized`/
    `onNativeWindowRedrawNeeded`), and `aapt dump xmltree` confirms
    `android:configChanges` now reads `0x20`. Full external-tool suite
    re-run clean.
- **Sixteenth report: the status bar still wasn't right, and a request to
  test touch input** -- the fifteenth round's `TranslucentDecor` theme
  still didn't look like a normal status bar with the blue underneath, the
  landscape black bar persisted (now visibly next to the front camera,
  confirming it as the display cutout, not the earlier resize bug), and a
  request to verify touch works by cycling through colors on tap.
  - *Display cutout, the real fix.* `windowTranslucentStatus`/
    `windowTranslucentNavigation` (what `TranslucentDecor` actually sets)
    only make the *system bars* see-through -- they say nothing about
    Android's separate, independent default for the *display cutout*
    (the front camera). Fetched Android's own developer documentation
    rather than guessing further: "the platform default allows a window
    under the cutout in portrait, but avoids it in landscape, leaving a
    black bar over the cutout area" -- exactly the symptom. The fix is
    `WindowManager.LayoutParams.layoutInDisplayCutoutMode =
    LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES` (`1`, ground-truthed against
    the real Android docs), settable only via a custom theme (a
    resources.arsc/style writer this experiment doesn't have) or directly
    through JNI -- so `fixDisplayCutoutMode` makes this project's
    first-ever JNI calls: `activity.getWindow().getAttributes()`, set the
    field, `.setAttributes(...)`. Every JNI function-table byte offset used
    (`FindClass`=48, `GetMethodID`=264, `CallObjectMethod`=272,
    `CallVoidMethod`=488, `GetFieldID`=752, `SetIntField`=872) was cross-
    checked against both Oracle's published JNI Functions reference and a
    fetched real AOSP `jni.h`, not trusted from memory, given a wrong
    vtable offset here is a straight crash with no adb/logcat to diagnose
    it from. Given that real risk -- a materially bigger, riskier class of
    change than anything shipped in this experiment before -- this round's
    implementation was only started after explicitly confirming with the
    user that the risk was worth taking rather than deferring it.
  - *Touch, to actually test something new rather than just re-verify
    rotation.* `drainInputEvents` now calls the two more NDK imports this
    needed (`AInputEvent_getType`, `AMotionEvent_getAction`, both stable
    `libandroid.so` exports like every other import here) on each event
    instead of immediately finishing it unread; on a finger touching down
    it advances through a four-color palette (blue/red/green/yellow) and
    repaints immediately by calling the shared paint handler directly.
    Backing this needed this experiment's first mutable global state since
    the `g_callbacks` allocation an earlier round removed (that removal
    was for an unrelated reason -- writing into the framework's own struct
    instead of replacing its pointer, see the tenth report -- not a
    reason to avoid app-owned mutable state in general): a small 24-byte
    `.data` block (`g_currentColor`, `g_colorIndex`, `g_window`,
    `g_activity`) the paint handler and `fixDisplayCutoutMode` both also
    read from.
  - *Verification, given the elevated risk.* No adb/logcat access exists
    to fall back on, so this round leaned harder than usual on static
    verification before shipping: disassembled the entire rebuilt `.text`
    with `llvm-objdump -d --triple=aarch64` and checked every one of the
    ~330 words -- every JNI vtable offset, every stack save/reload, every
    branch target, and every embedded C-string constant (packed as raw
    data words in the same R+X `.text` section the disassembler
    obligingly -- if confusingly -- tries and fails to decode as
    instructions) -- against the intended design by hand. All of it
    matched exactly, including string bytes that decode character-for-
    character to `"android/app/NativeActivity\0"`,
    `"()Landroid/view/WindowManager$LayoutParams;\0"`, and so on. `readelf
    -d`/`--dyn-syms`/`-r` confirmed the two new dynsym entries and
    relocations resolve correctly, and the full external-tool suite
    (`aapt`, `jarsigner -verify`, `unzip -t`) re-run clean. Real-device
    behavior is still the only way to know whether the cutout fix and
    touch actually work as intended.
- **Seventeenth report: the sixteenth round's real-device warning came
  true** -- instant crash on launch. This time a bug report (tombstone)
  was available, and it named the exact fault precisely: `SIGILL,
  ILL_ILLOPC` (illegal instruction) at a PC landing inside
  `ANativeActivity_onCreate`, with the memory dump around the fault
  address reading `"android/app/Nati"` -- literally the bytes of the
  first JNI call's class-name string, being fetched and executed as an
  AArch64 instruction. `emitCString` was placing each string's raw bytes
  directly in the middle of straight-line code -- right after one real
  instruction and right before the next -- with nothing to jump over it.
  Ordinary sequential execution has no way to know those bytes aren't
  code; it just fell out of the last real instruction straight into the
  string data. This was a real, confirmed bug, not the JNI vtable
  offsets or method signatures independently re-verified while
  investigating (both checked out; the AOSP struct-field offsets used
  for `activity->env`/`activity->clazz` were also freshly re-verified
  against real NDK source rather than re-trusted from memory, and also
  checked out). Fixed by hoisting every one of `fixDisplayCutoutMode`'s
  11 C-string constants into one block immediately after the function's
  entry point, guarded by a single unconditional branch that jumps over
  all of them before any instruction can fall through into the data --
  every string's address is still an ordinary ADRP+ADD against an
  address already known by the time it's used, only the *order* of
  emission changed. While already in there, also closed a second, latent
  gap the crash investigation surfaced: the original JNI call chain never
  checked any handle for `NULL`, and per the JNI spec a `NULL` from
  `FindClass`/`GetMethodID`/`GetFieldID` always means an exception is now
  pending on that `JNIEnv` -- calling almost any other JNI function while
  one is pending is undefined behavior, and ART's real response to that
  is a fatal abort of the whole process. Every handle the function
  depends on is now null-checked immediately after it's produced, bailing
  out to a shared epilogue that unconditionally clears any pending
  exception before returning, so a failure there now degrades to "cutout
  fix skipped" rather than a second way to take the whole app down.
  Rebuilt and re-verified with the same rigor as the round that shipped
  the bug: disassembled the corrected `.text`, confirmed the new
  unconditional branch (`B`, `0x226c` -> `0x2388`) correctly jumps clear
  over the entire string-data block, confirmed all 9 null-check branches
  land on the same shared exception-clearing epilogue, and re-checked
  every stack offset and JNI table offset the restructuring touched --
  all unchanged and correct. Full external-tool suite re-run clean.

This closes out the black-screen-and-ANR investigation and covers four
follow-up feature/fix rounds: seventeen real-device rounds, nine genuine
bugs found and fixed (missing `onNativeWindowResized`/
`onNativeWindowRedrawNeeded` registration; missing `LR` preservation
across the handler's nested calls; the actual root cause, replacing
`activity->callbacks` instead of writing through it; registering
`onNativeWindowResized` and then `onNativeWindowRedrawNeeded`, each of
which caused an ANR once the real rendering path was finally reachable;
never draining the input queue `NativeActivity.java` always hands this
app, which caused the ANR to persist even with no window callback left
registered; never repainting on resize, which left newly-exposed buffer
area black after rotation; once that repaint was added back, the deeper
issue it was only papering over -- opting into a resize-in-place rotation
path that's fragile enough that even Google's own reference NDK glue
library doesn't support it, fixed by routing rotation through Android's
standard destroy-and-recreate path instead; and, most recently, a string
constant embedded straight in the middle of executable code with nothing
to jump over it, which real-device evidence caught as an immediate
`SIGILL` crash on launch), one correctness fix found along the way (the
window's real default format is `RGB_565`, not `RGBA_8888`), three feature
additions after the app was confirmed working
(fullscreen vs. status-bar-visible; hardcoded-black vs. system-adaptive
status bar color -- which itself took two follow-up rounds, first to the
wrong translucency-only mechanism and then to the actually-independent
display-cutout fix, this project's first JNI calls; and touch-to-cycle-
colors, backed by this experiment's first legitimate mutable app state),
and a lot of what turned out to be correctly-transcribed ABI assumptions
(struct offsets, calling conventions, ELF/dynamic-linking, and now JNI
function-table details) that real-device evidence independently confirmed
rather than contradicted.
- **Eighteenth report: two more requests after the crash was fixed and the
  app confirmed fully working again** -- the touch-cycled color resets to
  blue on every rotation instead of surviving it, and combining rotation
  with touch input doesn't work right.
  - *Color resets on rotation.* Root cause: `onCreate` unconditionally
    wrote blue/index-0 into `g_currentColor`/`g_colorIndex` on every call
    -- including the destroy-and-recreate a rotation now triggers (see the
    fifteenth report), which discards whatever color a touch had already
    picked. Android's own standard mechanism for exactly this --
    preserving state across a configuration-change-driven recreate -- is
    `onSaveInstanceState`/the `savedState`/`savedStateSize` params
    `onCreate` already receives and, until now, ignored. Implemented both
    halves: `onSaveInstanceState` (newly registered at callback offset 16)
    hands the framework a small heap-allocated buffer holding
    `g_colorIndex` before the old instance is destroyed; `onCreate` now
    checks `savedStateSize` and, when the framework hands the same buffer
    back on the new instance, restores `g_colorIndex` from it (defaulting
    to blue/index 0 only when there's nothing to restore, e.g. a fresh
    process launch). Per the real, documented NDK contract ("the returned
    data will be freed by the caller using `free()`, so this data should
    be allocated using `malloc()`"), the saved buffer must be heap-
    allocated -- this project's first `libc.so` import (`malloc`), needing
    its own second `DT_NEEDED` entry alongside the existing
    `libandroid.so` one; `.dynsym`/`.rela.dyn`/`.hash`/`.got` all grew
    accordingly, verified via `readelf -d`/`--dyn-syms`/`-r` the same way
    as every other import in this file. A new, deliberately duplicated
    (not shared with `drainInputEvents`' own copy, to avoid touching
    already real-device-verified code) `colorForIndex` leaf function maps
    a restored index back to its color.
  - *Rotation + touch.* Root cause: `onInputQueueDestroyed` was never
    implemented. Per the real NDK contract, it must call
    `AInputQueue_detachLooper` on the queue before it's destroyed --
    `android_native_app_glue.c`'s own internal plumbing does exactly this.
    Without it, rotating (which destroys the old activity instance and its
    input queue, per the fifteenth report's destroy-and-recreate path) left
    the old queue's looper attachment dangling right as the new instance's
    `onInputQueueCreated` set up a fresh one -- consistent with input only
    misbehaving specifically around a rotation. Fixed by implementing and
    registering `onInputQueueDestroyed` (callback offset 96) to detach the
    old queue.
  - Verified with the same rigor the last round's crash made non-
    negotiable: disassembled the entire rebuilt `.text`, confirmed the
    `savedStateSize`-check branch and its fallback path both land
    correctly, confirmed every `.data` global offset and JNI/GOT slot the
    two new callbacks touch, and confirmed `colorForIndex` has no
    fall-through-into-data risk (no data of any kind embedded in it, only
    instructions, learned the hard way last round). `readelf`/`aapt`/
    `jarsigner -verify`/`unzip -t` all clean.

This closes out the black-screen-and-ANR investigation and covers five
follow-up feature/fix rounds: eighteen real-device rounds, eleven genuine
bugs found and fixed (missing `onNativeWindowResized`/
`onNativeWindowRedrawNeeded` registration; missing `LR` preservation
across the handler's nested calls; the actual root cause, replacing
`activity->callbacks` instead of writing through it; registering
`onNativeWindowResized` and then `onNativeWindowRedrawNeeded`, each of
which caused an ANR once the real rendering path was finally reachable;
never draining the input queue `NativeActivity.java` always hands this
app, which caused the ANR to persist even with no window callback left
registered; never repainting on resize, which left newly-exposed buffer
area black after rotation; once that repaint was added back, the deeper
issue it was only papering over -- opting into a resize-in-place rotation
path that's fragile enough that even Google's own reference NDK glue
library doesn't support it, fixed by routing rotation through Android's
standard destroy-and-recreate path instead; a string constant embedded
straight in the middle of executable code with nothing to jump over it,
which real-device evidence caught as an immediate `SIGILL` crash on
launch; never implementing `onSaveInstanceState`, which silently
discarded touch-picked state across every rotation; and never implementing
`onInputQueueDestroyed`, which left a stale looper attachment dangling
right as rotation's destroy-and-recreate set up a fresh input queue),
one correctness fix found along the way (the window's real default format
is `RGB_565`, not `RGBA_8888`), three feature additions after the app was
confirmed working (fullscreen vs. status-bar-visible; hardcoded-black vs.
system-adaptive status bar color -- which itself took two follow-up
rounds, first to the wrong translucency-only mechanism and then to the
actually-independent display-cutout fix, this project's first JNI calls;
and touch-to-cycle-colors, backed by this experiment's first legitimate
mutable app state, later extended with its own state-preservation and
`libc.so`/`malloc` needs), and a lot of what turned out to be
correctly-transcribed ABI assumptions (struct offsets, calling
conventions, ELF/dynamic-linking, and now JNI function-table details)
that real-device evidence independently confirmed rather than
contradicted.
- **Nineteenth report: APK Signature Scheme v2 and v3 signing**, added
  alongside the existing v1 (JAR) signature rather than replacing it (`apk_sign_v2v3.mlx`/
  `apk_sign_v2v3.zig`, wired into both `main.mlx` and `main.zig` right
  after `zip.build()`). This is a build-time/packaging change with no
  runtime behavior for the app itself to test on-device -- verification
  here is against the real `apksigner verify --print-certs -v` tool
  (`apksigner` version 0.9, from `build-tools;30.0.3`), the same
  "external, independently-implemented oracle" standard every other layer
  in this project has been held to, not a fresh physical-device install.
  Reused `hash.sha256`, `jar_sign.signSha256Pkcs1v15` (same RSA keypair,
  same PKCS1v1.5-SHA256 primitive, algorithm ID `0x0103`), and `asn1.mlx`'s
  DER encoders as-is; duplicated `jar_sign.buildSelfSignedCert`'s inline
  SubjectPublicKeyInfo construction locally (its helpers aren't `pub`,
  same tradeoff `jar_sign.mlx` itself documents). Three real bugs were
  found in this round, each one only by a genuine `apksigner verify`
  failure -- in every case the wrong version's bytes still "lined up"
  under an incomplete mental model of the format, so nothing short of
  the real verifier caught it:
  - **Missing per-entry length prefix on digest/signature pairs.** Each
    entry in a `digests`/`signatures` sequence isn't `algId(4) ++
    lp(payload)` as a first read of the spec's own comments suggests --
    real apksig's `encodeAsSequenceOfLengthPrefixedPairsOfIntAndLengthPrefixedBytes`
    wraps each pair in its own *additional* outer length prefix, making
    every entry `lp(algId(4) ++ lp(payload))`, three levels deep. Missing
    this produced a v3 block that crashed `apksigner` outright
    (`IllegalArgumentException: Negative length` while parsing signer #1)
    before even getting to a content check.
  - **Chunking the three signed sections as one flat stream instead of
    independently.** The v2/v3 content digest algorithm chunks *each* of
    the three sections (pre-Central-Directory bytes, Central Directory,
    EOCD) into <=1MB pieces on its own, always starting a fresh chunk
    boundary at a section's first byte -- confirmed against apksig's real
    `computeOneMbChunkContentDigests`, which loops a `DataSource[]` array
    and restarts hashing at each element rather than treating the whole
    thing as one concatenated byte stream. Every previous structural
    check (nested length prefixes, per-signer framing, the two now-fixed
    bugs above) had already passed by the time this one showed up as a
    `CHUNKED_SHA256 digest mismatch` -- offsets and framing can be
    perfectly self-consistent while the digest algorithm itself is still
    wrong.
  - **Digesting the file's real (post-insertion) EOCD instead of the
    pre-insertion one.** The natural assumption -- that the content digest
    should cover the *actual bytes on disk*, including the EOCD's real,
    already-adjusted "start of Central Directory" field -- is wrong.
    apksig's own verifier comment says the EOCD "must be treated as
    though its Central Directory offset points to the start of [the] APK
    Signing Block" when computing the digest, and its source confirms
    this literally: it rewrites the digested EOCD's offset field to
    `beforeApkSigningBlock.size()` (the block's *start* position, i.e.
    the *original*, pre-insertion `cdStart`), discarding the file's real
    value for this one purpose. The file written to disk still needs the
    real, adjusted `cdStart` -- normal ZIP/Android readers must be able
    to physically find the Central Directory -- so the fix keeps two
    separate EOCD buffers: one (unmodified) for the digest, one
    (`cdStart`-patched) for the actual output bytes. A pleasant
    side-effect of finding this: since the digest input never depends on
    the Signing Block's own size, the two-pass "measure with a
    placeholder digest, then rebuild for real" approach this module
    started with (mirroring `elf_so.mlx`'s `.text`-sizing pattern) turned
    out to be unnecessary -- the real digest, signatures, and block size
    can all be computed in one straight pass once the EOCD-for-digesting
    is right.
  - Final state verified end to end with `apksigner verify
    --print-certs -v`: `Verifies` / `v1 scheme: true` / `v2 scheme: true`
    / `v3 scheme: true`, plus `unzip -t` (no errors) and `jarsigner
    -verify` (`jar verified.`, same pre-existing self-signed/1024-bit-key
    warnings as every prior round -- nothing new). v3's `minSdkVersion`
    field is set to 28 (the first API level that verifies v3 at all;
    `RSA_PKCS1_V1_5_WITH_SHA256`'s own supported-from version is 24, and
    setting the signer's declared range below either floor makes
    `apksigner` report "No supported signatures" for that scheme even
    though nothing about the bytes is malformed) -- independent of, and
    not required to match, the manifest's own `minSdkVersion="21"`. No
    proof-of-rotation attribute: a single, fresh (non-rotated) signing
    key needs none.
- **Twentieth report: real-device install showed "This app was built for
  an older version of Android and may not work properly"** even with v2/v3
  signing now in place. Root cause: unrelated to signing entirely -- this
  is Android's own compatibility warning, surfaced purely off the gap
  between the manifest's `targetSdkVersion` (29, set back when this
  experiment started and never revisited) and the real device's actual
  platform version. No signing scheme changes that; the previous round's
  work was necessary for install-time integrity checking but was never
  going to touch this warning. Fixed by bumping `targetSdkVersion` to 35
  (Android 15, the latest well-established stable level as of this
  writing) in both `axml.mlx`/`axml.zig` -- nothing this app does is
  gated on newer platform behavior, so there's no downside to targeting
  it, and `minSdkVersion` stays at 21 for broad compatibility.
  - Also requested: automatically picking the right signing variant from
    the SDK target instead of always hardcoding v1+v2+v3. Added
    `apk_sign_v2v3.schemesForTargetSdk(targetSdk)`, a small policy
    function mirroring when each scheme actually became meaningful on
    the real platform (v1 works on every API level, so it's never
    dropped; v2 verification began at API 24; v3 at API 28) -- monotonic
    in `targetSdk`, so raising the target only ever adds schemes, never
    removes one. `buildPairsBytes`/`signV2V3` now take explicit
    `includeV2`/`includeV3` flags and, at 21/24-27/28+, produce v1-only /
    v1+v2 / v1+v2+v3 respectively; `signV2V3` returns `zipBytes`
    unchanged (no Signing Block at all) when both are disabled. Both
    `minSdkVersion` and `targetSdkVersion` are now threaded in from
    `main.mlx`/`main.zig` as the single source of truth (previously
    hardcoded separately inside `axml.mlx`'s `buildManifest`), so the
    manifest's declared SDK range and the auto-picked signing schemes can
    never drift out of sync with each other. At this experiment's own
    `targetSdkVersion=35`, the auto-pick still yields v1+v2+v3 -- no
    behavior change from the nineteenth report's already-verified build,
    just the hardcoded "always all three" replaced with a real,
    general-purpose policy.
  - Re-verified with the same `apksigner verify --print-certs -v` /
    `unzip -t` pair as the nineteenth report (still all green), plus
    `aapt dump badging` confirming `targetSdkVersion:'35'` in the actual
    built APK. The `targetSdkVersion` fix itself still needs a fresh
    on-device install to confirm the warning is actually gone -- this
    round's verification is tool-based, same caveat as the nineteenth
    report's.
- **Twenty-first report: real-device install of the twentieth report's
  build was flatly refused** ("You can't install this app on your
  device"), not just warned about. Root cause: a genuine, previously
  latent bug that every prior round's real-device testing happened not to
  trigger. `main.mlx`/`main.zig` called `jar_sign.generateKeyPair` fresh
  on *every single build* -- meaning every one of the twenty-one APKs
  shipped across this experiment's real-device rounds was signed with a
  **different, randomly-generated** self-signed certificate. Android
  requires an app "update" (same package name) to be signed with the
  *same* certificate as whatever's currently installed; once the device
  already had one of these randomly-keyed builds installed, the next
  differently-keyed build could no longer install over it at all -- not a
  warning, an outright block, and unrelated to the targetSdkVersion fix
  from the twentieth report (which had already shipped and was still
  correct; this is a second, independent bug the block symptom happened
  to surface at the same time).
  - Fixed by persisting the RSA keypair (`n`, `d` -- `e` is always the
    fixed 65537 public exponent, not stored) to a small file
    (`debug_signing_key.bin`, checked into the repo, at a path both ports
    agree on) instead of generating a new one every build: `n_bytes` as a
    4-byte little-endian header, then `n` and `d` each as exactly
    `n_bytes` big-endian bytes -- not a real keystore format (no JKS/PKCS12
    parser exists here), just enough to make the signing identity stable.
    `main.mlx`/`main.zig` now try loading this file first and only fall
    back to `generateKeyPair` (saving the result for next time) if it's
    missing or short/corrupt. Mirrors the *purpose* of Android Studio's
    own `debug.keystore` -- a fixed, throwaway, non-secret signing
    identity checked in so builds stay installable as updates over each
    other -- not its file format. Since the self-signed certificate's
    other inputs (CN, serial, validity dates) were already static, the
    certificate bytes are now fully deterministic across rebuilds, not
    just "some equivalent key": confirmed by building twice in a row and
    diffing `apksigner verify --print-certs`'s reported certificate
    SHA-256 digest -- byte-identical, and a second build now takes
    seconds instead of the ~1-2 minute keygen.
  - Real consequence for whoever is testing this on-device: **this fix
    does not retroactively repair an already-installed, differently-keyed
    build.** The device needs one manual uninstall of whatever's
    currently installed; every build from this point forward (using the
    now-checked-in key) will then install cleanly as an update over the
    previous one, indefinitely, with no further uninstalls needed unless
    `debug_signing_key.bin` itself is deleted or regenerated.
  - Verified the same way as the previous two rounds (`apksigner verify`
    still v1/v2/v3 all `true`, `unzip -t` clean) plus the
    build-twice-diff-the-certificate check described above. Still
    pending: a real device confirming both the install block is gone
    *and* the targetSdkVersion warning from the twentieth report doesn't
    reappear now that install can actually proceed.
- **Twenty-second report: the fix above didn't help -- same "You can't
  install this app on your device" toast, confirmed after a clean
  uninstall first** (ruling out a leftover signature mismatch). A
  genuinely different, third bug: 16&nbsp;KB memory page size. Real,
  modern Android hardware (confirmed on the same real Pixel 10 Pro,
  running a current/Canary build) is moving to a 16&nbsp;KB page size
  instead of the historical 4&nbsp;KB, and devices on the new page size
  can refuse to install an APK outright if its native libraries' ELF
  `PT_LOAD` segments aren't aligned to at least 16&nbsp;KB --
  independent of, and unrelated to, both the targetSdkVersion warning
  and the signing-key block from the previous two reports. Verified via
  Google's own documented check
  (developer.android.com/guide/practices/page-sizes): `llvm-objdump -p
  libmain.so | grep LOAD` showed `align 2**12` (4096) on all three
  `PT_LOAD` segments -- confirmed `UNALIGNED` by exactly the criterion
  that page-sizes doc describes (`>= 2**14` required). This had been
  true since this experiment's very first ELF writer and simply never
  mattered until testing happened to land on 16&nbsp;KB-page hardware.
  - Fixed in `elf_so.mlx`/`elf_so.zig` by moving `TEXT_VADDR` from
    `0x2000` to `0x4000` and `RW_VADDR` from `0x3000` to `0x8000` (true
    16&nbsp;KB-multiple file offsets, not just relabeling the existing
    4&nbsp;KB-spaced layout's `p_align` field to `0x4000` while leaving
    the actual segment starts unmoved -- that would satisfy a check that
    only inspects the `align` column without the segments actually being
    safely `mmap`-able at a real 16&nbsp;KB boundary, which is not
    something this project settles for). Since every segment already has
    `vaddr == offset`, the ELF-spec congruency requirement
    (`p_vaddr &equiv; p_offset (mod p_align)`) holds either way, so this
    was a pure constant change with the file's actual bytes growing
    (`libmain.so`: 13,304 -&gt; 33,784 bytes) to hold the real padding
    now needed between segments -- verified this doesn't touch the
    machine-code generator at all: `aarch64.mlx`'s `adrp` encoder already
    does proper page-unit arithmetic (`/ 4096`, the AArch64 ISA's own
    fixed page-unit for the instruction itself, entirely independent of
    the OS/ELF page-size concept being changed here) for arbitrary
    target/PC deltas, not a hardcoded small-offset assumption, so it
    needed no changes to correctly address the new, larger vaddrs.
  - Also added the matching ZIP-level fix in `zip.mlx`/`zip.zig`: real
    `zipalign`/apksigner additionally page-align *where the uncompressed
    library's bytes sit inside the APK* (so the OS can `mmap` it directly
    out of the APK without extracting it first), via padding written as
    the local file header's "extra field" -- this project's ZIP writer
    previously always wrote extra length `0` for every entry. Now any
    entry whose name ends in `.so` gets exactly enough padding for its
    data to start at a 16,384-byte-aligned offset, matching real
    zipalign's own heuristic of only padding native libraries, not every
    entry. Verified with `zipalign -c -v -p 4`: `lib/arm64-v8a/libmain.so
    (OK)`, and directly confirmed the entry's data offset is `%16384 ==
    0` via a Python zip-parsing script cross-check.
  - Re-verified `apksigner verify` (still v1/v2/v3 all `true`) and
    `unzip -t` (still clean) after both changes. Still pending: the
    actual real-device install this was all aimed at unblocking -- three
    independent, real bugs have now been found and fixed across three
    rounds triggered by the same one install attempt, which is a
    reasonable prompt to expect *this* one might not be the last if the
    device still refuses it.
- **Twenty-third report: still the same toast, confirmed via two
  screenshots this time** -- the install *confirmation* dialog (package
  name, icon, "Mlx Blue Screen" label) rendered correctly, then tapping
  "Installieren" failed near-instantly with the same generic "You can't
  install this app on your device." That the confirm dialog renders fine
  but the real install step fails fast pointed at a deeper, early
  manifest-validation check the confirm dialog's lighter parse doesn't
  perform -- and re-examining exactly what changed between the last
  build that's known to have installed successfully (the nineteenth/
  twenty-first report's builds, both at the original
  `targetSdkVersion=29`) and every failing build since (all at the
  twentieth report's `targetSdkVersion=35`) pointed straight at the
  bump itself, not at either of the last two rounds' fixes (both still
  correct and still needed, just not sufficient alone). Root cause: since
  Android 12 (API 31), any activity/service/receiver that has an
  `<intent-filter>` **must** explicitly declare `android:exported` --
  omitting it is only a lint warning below `targetSdkVersion=31`, but a
  hard manifest-validation failure at install time at 31 and above. This
  project's `<activity>` has always had a MAIN/LAUNCHER `<intent-filter>`
  and never declared `android:exported`, because it never needed to
  until `targetSdkVersion` crossed 31 -- invisible for this experiment's
  entire life at `targetSdkVersion=29`, silently exposed by the
  twentieth report's otherwise-correct bump to 35.
  - Fixed by adding `android:exported="true"` to the `<activity>`
    element in `axml.mlx`/`axml.zig` (true, since it's the app's
    LAUNCHER activity and must be externally invokable by the home
    screen). Resource ID (`0x01010010`) ground-truthed the same way
    every other attribute in this file was: compiled a minimal reference
    manifest with an explicit `android:exported` through the real `aapt`
    against `/usr/share/android-framework-res/framework-res.apk` and
    read back the resolved ID from `aapt dump xmltree`, which also
    confirmed the boolean-`true` encoding (`0xffffffff`, TYPE_INT_BOOLEAN
    `0x12`) matches what this project's `writeBoolAttr`/`.boolean` helper
    already produces for `hasCode` -- no new encoding logic needed, just
    the missing attribute itself.
  - Verified with `aapt dump xmltree`, confirming byte-for-byte the same
    `android:exported(0x01010010)=(type 0x12)0xffffffff` the reference
    manifest produced, plus the usual `apksigner verify` (still v1/v2/v3
    all `true`, same persisted certificate) and `unzip -t` (still clean).
    This is the fourth independent real bug found across four rounds
    from the same one install attempt -- still pending the actual
    real-device confirmation all of them were aimed at.
- **Twenty-fourth report: still the same "You can't install this app on
  your device" toast with `android:exported` added.** The user separately
  researched this exact symptom and brought back a converging suspicion:
  `android:extractNativeLibs`, the attribute controlling whether the
  platform extracts `libmain.so` to a normal file at install time or
  mmaps it directly out of the (uncompressed) APK. This manifest has
  never declared it at all. The real platform default when it's absent
  has always been documented as `true` (extract) -- so this was reasoned
  through rather than confirmed as a behavior change, but declaring it
  *explicitly* removes that ambiguity for zero downside, and is cheap
  enough to be worth doing regardless of whether it turns out to be the
  actual remaining blocker.
  - Added `android:extractNativeLibs="true"` to the `<application>`
    element in `axml.mlx`/`axml.zig`. Resource ID (`0x010104ea`)
    ground-truthed the same way as every other attribute in this file:
    a minimal reference manifest compiled through the real `aapt`,
    resolved ID read back from `aapt dump xmltree`.
  - Verified with `aapt dump xmltree` (byte-identical to the reference),
    `apksigner verify` (still v1/v2/v3 all `true`, same persisted
    certificate -- no reinstall needed), and `unzip -t` (still clean).
    No adb/logcat access is available on the test device, so unlike
    every other layer in this project, this round -- and the previous
    few -- can't be verified against the platform's actual specific
    rejection reason, only against the same generic symptom and a
    plausible, but not certain, mechanism.
- **Twenty-fifth report (diagnostic, not a permanent decision):**
  `targetSdkVersion` temporarily dropped from 35 to 34 in
  `main.mlx`/`main.zig`, to isolate whether the still-unresolved install
  block is actually tied to 35 specifically. 34 still requires
  `android:exported` (mandatory since API 31) and still clears the
  original "built for an older Android version" warning, but sits one
  level below whatever 35 enforces that the last two rounds' fixes
  haven't identified. Whichever way the real-device result goes decides
  the next step -- back to 35 with more investigation, or staying at 34
  -- so this isn't logged as a fix, just a control experiment.
- **Twenty-sixth report: bisected the install block to an exact
  `targetSdkVersion` threshold (30 installs, 31 does not), and shipped
  30 as the new baseline.** The user confirmed 34 (twenty-fifth report)
  still failed the same way as 35, ruling out 35-specific behavior --
  whatever blocks this device's install is gated at some level at or
  below 34, not something unique to 35. That reopened every earlier fix
  in this saga (signing-key stability, 16 KB alignment, `exported`,
  `extractNativeLibs`) as a suspect again, since all of them had only
  ever been tested in combination, never in isolation against a known
  install/fail baseline. Isolating the real variable required a proper
  bisection, done with `git worktree` (to build exact historical
  commits without disturbing the working tree) and scratch copies of
  `mlx/*.mlx` with individual files swapped back to older versions via
  `git show <commit>:<path>`, since this device has no adb/logcat
  access to read the platform's actual rejection reason directly:
  1. **v21 reproduction** (commit `c1e5a1f`, the last known-good build
     before this entire investigation started, targetSdk 29, no
     `exported`/`extractNativeLibs`, 4 KB alignment): **installs.**
     Confirms the regression really is something introduced since then,
     not an environment change on the device side.
  2. **bisect1** (current code, but 4 KB alignment instead of 16 KB,
     targetSdk still current at the time): **fails.** Rules out 16 KB
     alignment as the cause -- reverting it to 4 KB didn't fix anything.
  3. **bisect2** (current code minus `extractNativeLibs`, targetSdk 34):
     **fails**, identically to bisect1. Rules out `extractNativeLibs`.
  4. **bisect3** (v21 baseline + `android:exported` only, targetSdk left
     at 29): **installs.** Proves the `exported` encoder change itself
     has no bug -- adding it to a build that already installs doesn't
     break it. *(Wrong -- see the twenty-eighth report. Below target 31
     the platform never checks for `exported`, so this build installing
     proved nothing about whether the platform could actually read it.
     It couldn't.)*
  5. **bisect4** (full current combination -- 16 KB alignment,
     `exported`, `extractNativeLibs`, stable key -- targetSdk 31):
     **fails.**
  6. **bisect5** (identical to bisect4, only `targetSdkVersion` changed
     from 31 to 30): **installs.**
  Steps 5 and 6 are a true single-variable A/B test: `cmp -l` on the two
  builds' `AndroidManifest.xml` confirmed exactly one byte differs
  between them (the `targetSdkVersion` integer itself), so this isn't an
  AXML-encoder artifact -- the threshold is real and exact. The user
  also checked the device's own Android version (Settings → About phone):
  a Pixel 10 Pro on the "CANARY" pre-release channel, build
  `ZP11.260717.006`, with no numeric SDK level shown -- a genuinely
  bleeding-edge platform build, which doesn't explain *why* 31 specifically
  fails but is at least consistent with this device enforcing something
  newer or stricter than documented API-31 behavior.
  - **Decision:** ship `targetSdkVersion = 30` (Android 11) as the new
    baseline in `main.mlx`/`main.zig`, rather than continue bisecting
    blind into the 31-35 range with no way to read the platform's actual
    rejection reason. 30 still clears the original "built for an older
    version of Android" warning (the twentieth report's fix), keeps
    every other fix from this investigation (stable signing key, 16 KB
    alignment, `exported`, `extractNativeLibs` -- none of which were the
    actual culprit, and all of which are harmless or beneficial
    regardless of target), and is the highest value confirmed to
    actually install.
  - **What's still unknown:** the exact platform requirement gated at
    `targetSdkVersion >= 31` on this specific device. Every documented
    API-31+ manifest requirement found and checked (`exported`,
    `extractNativeLibs`, 16 KB alignment) was individually implemented
    correctly and individually ruled out by this bisection -- the actual
    mechanism remains unidentified for lack of adb/logcat access to see
    the platform's real rejection reason. *(Since identified -- see the
    twenty-eighth report. `exported` was never correctly implemented; it
    was present but unreadable due to attribute ordering.)*
  - Verified with the full tool suite before shipping: `apksigner
    verify --print-certs -v` (`Verifies`, v1/v2/v3 all `true`, same
    persisted certificate SHA-256 as every prior build -- no reinstall
    needed), `aapt dump badging` (`targetSdkVersion:'30'`), `zipalign -c
    -v -p 4` (`lib/arm64-v8a/libmain.so (OK)`), `llvm-objdump -p` on the
    extracted `.so` (all `LOAD` segments `align 2**14`), and `unzip -t`
    (clean). This exact combined build (30 + 16 KB alignment +
    `exported` + `extractNativeLibs` + stable key, all present
    simultaneously) is tool-verified but still pending real-device
    confirmation -- bisect5 confirmed targetSdk 30 alone installs, but
    not yet this specific full combination together.
  - **Real-device confirmed:** the user installed this exact combined
    build (`bluescreen_stable_target30.apk`) on the Pixel 10 Pro test
    device and confirmed it installs. This is now the project's stable,
    shippable baseline.
- **Twenty-seventh report: made the min/target SDK bounds configurable
  instead of hardcoded, ahead of bringing this into the main compiler as
  an `aarch64`/Android backend.** Before generalizing this experiment,
  two gaps needed closing: testing a different SDK combination shouldn't
  require editing source, and there was no single place documenting what
  "min" and "target" actually mean here or why there's no "max."
  - `tool/main.zig`: `MIN_SDK_VERSION`/`TARGET_SDK_VERSION` renamed to
    `DEFAULT_MIN_SDK_VERSION`/`DEFAULT_TARGET_SDK_VERSION` and made
    overridable with new `--min-sdk=N`/`--target-sdk=N` CLI flags,
    validated (`target >= min`, reject anything else with a clear error)
    before any work starts. The build banner now prints the resolved
    `min-sdk=N target-sdk=N`, with an inline warning if target is pushed
    above 30 (the known-installing ceiling).
  - `mlx/main.mlx`: unchanged in mechanism -- mlx still has no argv
    equivalent (this file's header comment), so `minSdkVersion()`/
    `targetSdkVersion()` stay hardcoded functions requiring a source
    edit + rebuild to change, now cross-documented against the Zig
    port's flags. Added `validateSdkRange()`, called first thing in
    `main()`, as the same defensive `target >= min` check the Zig CLI
    now does at parse time -- there's no shared validation point here
    since these are two independent functions, so it's a runtime guard
    instead of a parse-time one.
  - **On "max SDK":** deliberately did *not* add a
    `android:maxSdkVersion` attribute to `<uses-sdk>`. It's a real
    manifest attribute, but Android's own documentation deprecated it at
    API level 4 and the framework has ignored it at install time on
    every version since -- adding it would look like a "supports a
    range" fix while doing nothing. The actual lever for "install on
    everything" is the single `targetSdkVersion` ceiling: Android
    normally installs on any device with API >= `minSdkVersion`
    regardless of target (that's the whole reason this project's
    `minSdkVersion` has stayed at 21 throughout), so raising
    `targetSdkVersion` past 30 -- once the block below is actually
    root-caused -- is what widens real-world support, not a separate
    max field.
  - Verified by rebuilding both ports: `zig ast-check tool/main.zig` (OK,
    `zig run`/`zig test` remain blocked by the project's pre-existing,
    unrelated Zig-version stdlib API mismatch -- the established
    fallback bar for this port throughout the project) and running the
    rebuilt mlx1-compiled `main.mlx` end to end, which produced a
    byte-identical-sized 53,961-byte APK to the twenty-sixth report's
    shipped build (same `min-sdk=21 target-sdk=30` banner, same signing
    key reused, same v2/v3 signing) -- confirming the new validation and
    banner output didn't disturb anything downstream.
  - **Still open, and the actual blocker on bringing this into the main
    compiler:** the `targetSdkVersion >= 31` install failure itself is
    still unfixed, just easier to keep probing now. Continuing to guess
    at more manifest attributes without a real error message has a poor
    hit rate -- three plausible, well-researched candidates
    (`exported`, `extractNativeLibs`, 16 KB alignment) were each
    individually ruled out over five rounds. The most promising
    un-tried path needs no adb or PC at all: modern Android ships a
    `pm` command-line tool as part of the OS itself, reachable from any
    on-device terminal app (e.g. Termux, installed like any other APK --
    from F-Droid, since the Play Store build is deprecated). Running
    `pm install -r /sdcard/Download/bluescreen.apk` locally, in a shell
    on the test device itself, prints the actual
    `PackageManager`/`PackageInstaller` rejection string directly to the
    terminal (e.g. `INSTALL_FAILED_INVALID_APK: ...`) -- the same
    information `adb install` would show, without needing adb or a
    computer. This is the fastest real path to root-causing (not just
    routing around) the 31+ block, and hasn't been tried yet.
    *(Tried; didn't work. Non-rooted Termux runs as an ordinary app UID,
    which can't call the package service --
    `cmd: Failure calling service package: Failed transaction`. `pm`
    needs the `shell` UID that `adb` provides. The real answer came from
    `adb install` -- see the twenty-eighth report.)*
- **Twenty-eighth report: root cause of the `targetSdkVersion >= 31`
  install block, found with `adb install`, and fixed.** With adb
  available, `adb install -r` on a target-31 build printed the actual
  rejection instead of the generic toast:
  ```
  Failure [INSTALL_PARSE_FAILED_MANIFEST_MALFORMED: Failed parse during
  installPackageLI: /data/app/vmdl1882762383.tmp/base.apk (at Binary XML
  file line #16): android.app.NativeActivity: Targeting S+ (version 31
  and above) requires that an explicit value for android:exported be
  defined when intent filters are present]
  ```
  So the platform wasn't seeing `android:exported` at all, even though
  it was in the file and `aapt dump xmltree` showed it. The cause:
  **attribute order.** The framework reads an element's attributes with
  `AttributeResolution`'s `RetrieveAttributes`, which walks the requested
  attribute IDs (sorted) and the element's XML attributes (in file order)
  in a single forward merge -- it assumes the XML attributes are already
  sorted by resource ID, which is what `aapt2` always emits. This
  project's `<activity>` wrote them as `theme` (0x01010000), `label`
  (0x01010001), `name` (0x01010003), `configChanges` (0x0101001f),
  `exported` (0x01010010) -- `exported` appended last when it was added
  in the twenty-third report. Looking for 0x10, the walk passes 0x00,
  0x01, 0x03, reaches 0x1f > 0x10, concludes `exported` is absent, and
  moves on. `aapt dump xmltree` iterates every attribute regardless of
  order, so it never showed a problem. "Line #16" is the `</activity>`
  end tag, where the parser checks exported-vs-intent-filters.
  - This also explains every earlier result: below target 31 a missing
    `exported` is legal, so every target-29/30 build installed whether
    or not the platform could read it -- which is why bisect3 (twenty-
    sixth report) "proved" nothing, and why the 30-vs-31 boundary was
    exact. `extractNativeLibs` and 16 KB alignment really weren't
    involved; `exported` was the right fix all along, just never
    actually delivered to the platform.
  - **Fix:** `exported` moved between `name` and `configChanges` in both
    `mlx/axml.mlx` and `tool/axml.zig`, with the ordering invariant
    documented on `writeStartElementHeader`/`writeStartElement` so a
    future attribute doesn't repeat this.
  - **Verified:** a small AXML checker (parses the string pool, resource
    map, and each start element, and flags any element whose resource-
    ID'd attributes aren't ascending, with non-ID'd ones last) reported
    exactly one violation -- the `<activity>`'s `exported` -- on the
    failing target-31 build, and zero on fixed builds at targets 30, 31,
    35 and 36. All four fixed builds also pass `apksigner verify`
    (v1/v2/v3, same persisted certificate), `aapt dump badging` (correct
    `targetSdkVersion`), and `unzip -t`. `zipalign -c -p 4` flags the
    non-`.so` entries as unaligned, but identically on the previously
    device-installed target-30 build -- only `libmain.so` needs page
    alignment (it has it), and the platform's only other alignment rule
    (target 30+) is for `resources.arsc`, which this APK doesn't have.
  - Real-device confirmation of the fixed builds at 31/35/36 is pending;
    the default `targetSdkVersion` stays at 30 until that comes back.
- **Twenty-ninth report: 31, 35 and 36 real-device confirmed; default
  raised to 36; Android 17 (API 37) build ready.** All three fixed builds
  installed via `adb install -r` and ran on the Pixel 10 Pro. The default
  in `main.mlx`/`main.zig` is now 36 (Android 16), the highest confirmed
  value; the repo's default build is byte-identical to the device-
  confirmed target-36 test APK.
  - **Android 17 is API level 37, not 38.** Checked against
    developer.android.com: the Android 17 behavior-changes page gates on
    "Android 17 (API level 37)", its API diffs run "API 36 → API 37", and
    its QPR betas are 37.1/37.2. Nothing on Google's pages mentions an
    API 38 yet.
  - Two of the target-37 behavior changes are relevant to a native app,
    and neither should apply here: `System.load()`'d native files must
    now be read-only (this app loads nothing dynamically, and
    `NativeActivity` loads `libmain.so` from the installer-extracted,
    system-owned lib directory), and apps can no longer opt out of
    ignoring orientation/resizability restrictions on large screens
    (this manifest declares none).
  - A target-37 build passes every local check (attribute-order checker,
    `apksigner verify` with the same certificate, `aapt dump badging`,
    `unzip -t`) and differs from the confirmed target-36 manifest by
    exactly one byte. **Real-device confirmed** (installs and runs); the
    default is now 37, and the repo's default build is byte-identical to
    that confirmed APK.
  - The v3 signer's `maxSdk` is `0x7fffffff` and `schemesForTargetSdk`
    returns v1+v2+v3 for any target >= 28, so neither needs to change as
    targets go up.
- **Thirtieth report: touch input now tells a tap, a long press and a
  swipe apart.** Until now any finger-down just cycled through four
  colors. The native code now classifies each single-finger touch and
  fills the screen with a color for what it recognized:

  | Touch | Rule | Color |
  |---|---|---|
  | Tap | released within 24 px of where it went down, before 500 ms | green |
  | Long press | held within 24 px for 500 ms — recognized *while still held* | yellow |
  | Swipe right / left | left the 24 px slop, mostly horizontal | red / magenta |
  | Swipe down / up | left the 24 px slop, mostly vertical | cyan / orange |

  A second finger or an `ACTION_CANCEL` abandons the touch (nothing is
  recognized). The last recognized kind still survives rotation through
  `onSaveInstanceState`, the same way the color index did.
  - **How it works:** `drainInputEvents` masks each motion event's action
    to its low byte (so multi-touch actions are recognized), reads pointer
    0's position (`AMotionEvent_getX/getY`, converted with `fcvtzs`) and
    `AMotionEvent_getEventTime`. `ACTION_DOWN` records position and time
    and arms a one-shot 500 ms `timerfd`; `ACTION_MOVE` past the slop marks
    the touch as moved and disarms it; `ACTION_UP` classifies. The timer
    lives on the same main-thread `ALooper` as the input queue
    (`ALooper_addFd`), so its callback runs on the main thread with no
    locking, and recognizes a long press the moment it fires. If creating
    the timer ever fails, long presses are still recognized, on release.
    `onNativeWindowDestroyed` is now registered to clear `g_window`, so a
    timer firing after the window is gone doesn't paint into it, and
    `onInputQueueDestroyed` drops any touch in progress.
  - **New pieces:** 7 imports (`AMotionEvent_getX/getY/getEventTime`,
    `ALooper_addFd` from libandroid; `timerfd_create`, `timerfd_settime`,
    `read` from libc -- all available at this APK's minSdkVersion 21), a
    56-byte `.data` block for touch state, a color table indexed by kind,
    and 9 new A64 encodings (`fcvtzs`, `sub`/`cmp` register and immediate
    forms, `b.cond`, `cneg`, `uxtb`, `add` with shift). Every encoding was
    ground-truthed against `llvm-mc` and is checked by
    `verify_aarch64.mlx` (47/47) and `aarch64.zig`'s tests (12/12). The
    code buffer grew from 512 to 1024 words (the text is now 612).
  - **Verified off-device, by running the real machine code:**
    `emulate_gestures.py` loads the built `libmain.so` into the Unicorn
    AArch64 emulator, points every GOT slot at a stub emulating the
    Android/libc function behind it, and drives the library the way the
    framework does -- `ANativeActivity_onCreate`, then whatever callbacks it
    registered. 17 scenarios pass: tap, long press via the timer and on
    release, the 499 ms and 24/25 px boundaries, all four swipe
    directions, a slow swipe, cancel, a second finger, non-touch events,
    a timer firing after the window is destroyed, a failed
    `timerfd_create`, and rotation (save/restore). It also checks that
    every event is finished exactly once, that stack and callee-saved
    registers are preserved across every callback, and it fills the
    upper half of every `int` return with junk to catch code treating it
    as 64-bit. Three deliberately planted bugs (no action mask, swapped
    left/right, timer ignoring movement) were each caught by exactly the
    expected scenarios. Run it with `pip install unicorn; python3
    emulate_gestures.py bluescreen.apk`.
  - Both ports updated in lockstep (the Zig port was translated from the
    emulator-verified mlx code and passes `zig ast-check`). The APK passes
    `apksigner verify` (v1/v2/v3, same certificate), the attribute-order
    check, `zipalign -p 4` for `libmain.so`, 16 KB `PT_LOAD` alignment and
    `unzip -t`. Real-device confirmation is pending.
- **Thirty-first report: the two ports produce byte-identical APKs.**
  Until now the Zig port was only checked with `zig ast-check`, because
  the installed Zig (0.16) no longer has the `std.ArrayList`/`std.fs`/
  `std.io` APIs it was written against. `ast-check` doesn't type-check,
  and it had let a real type error through: `zip.zig` passed the 16 KB
  alignment's `u32` padding length where a `u16` is required (fixed with
  a checked cast; the value is always below 16384). To compare the ports
  for real, a scratch copy of `tool/` with `std.ArrayList` renamed to
  0.16's `std.array_list.Managed` (and `jar_sign.zig`'s three
  `writer().print` calls swapped for `std.fmt.allocPrint`) was driven
  through `main.zig`'s exact pipeline with the persisted signing key. Its
  `libmain.so`, `AndroidManifest.xml`, `classes.dex` and the complete
  signed APK (54,073 bytes, v1/v2/v3) are byte-identical to the mlx
  port's. What still differs is only in `main`: the Zig CLI takes the
  output path, package name and `--min-sdk`/`--target-sdk`, while mlx has
  no argv yet and uses fixed values; and without a saved key the Zig port
  would generate RSA-2048 where mlx generates RSA-1024 (both use the
  committed 1024-bit key when it's present). `main.zig` itself still
  doesn't build on Zig 0.16 for the API reasons above.

## What's genuinely unverified

The real-device rounds above confirm the pipeline end to end: JAR
signature acceptance, binary-manifest parsing, empty-DEX validity, ELF64
`.so` loading and dynamic-linking, the `ANativeActivity`/
`ANativeActivityCallbacks` struct layout (cross-checked against real AOSP
source, not just memory), and the actual `ANativeWindow_lock`/
`_setBuffersGeometry`/`_unlockAndPost` fill-and-present path all the way
to visible pixels on a real screen. What's left:

- ~~APK Signature Scheme v2/v3 signing, the `targetSdkVersion=30` build,
  and the "built for an older Android version" fix.~~ **Real-device
  confirmed** -- see the twenty-sixth report's closing note.
- ~~`targetSdkVersion >= 31` install block.~~ **Fixed and real-device
  confirmed** at 31, 35, 36 and 37 (twenty-eighth and twenty-ninth
  reports); default is now 37 (Android 17).
- **Tap / long press / swipe recognition (thirtieth report) is verified in
  emulation, not yet on a device.** The emulator runs the real machine
  code, but the Android and libc functions it calls are stubs written
  from their documented behavior, and the 24 px slop is a fixed pixel
  value tuned for the test device's density rather than read from the
  display.
- **16 KB native-library page alignment and `extractNativeLibs` were
  never the cause of the 31+ block**, but are real, correct settings for
  modern devices and stay in the build regardless of target.
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

1. ~~Validate on a real target.~~ Done — a real Pixel 10 Pro confirmed
   install and correct visible rendering; see the twenty-sixth and
   twenty-seventh reports for the full path to a stable, real-device-
   confirmed baseline at `targetSdkVersion=30`.
2. ~~Root-cause and fix the `targetSdkVersion >= 31` install block.~~
   Done — it was attribute ordering in the AXML encoder (twenty-eighth
   report); 31/35/36/37 are device-confirmed. When this
   moves into the compiler, the AXML writer should enforce ascending
   resource-ID attribute order itself (sort, or reject) rather than
   relying on each call site, since that's the invariant that cost the
   most time here.
3. If the ABI assumptions need fixing, fix `native_activity.zig` and
   re-verify with the same `llvm-mc` disassembly technique.
4. **Bring this into the language**, not just a side tool: an `aarch64`
   backend under `compiler/selfhost/backend/` (mirroring
   `compiler/selfhost/backend/x86_64/`), an `android`/ELF-shared-object
   object-format mode alongside `compiler/selfhost/object/elf64.mlx`, and
   eventually a `std.android` module so this is expressible in ordinary
   mlx source rather than hand-built by a bespoke Zig tool. The Zig tool
   here is scaffolding to de-risk that work, not a replacement for it.
   Blocked on item 2 by design, not just sequencing.
5. ~~APK Signature Scheme v2 (or v2+v1 for broader compatibility).~~ Done
   — see the nineteenth report above (v2 *and* v3, alongside v1).
6. A real target triple story in the build system (`mlx build --target
   aarch64-android`), matching how `zig build`'s `standardTargetOptions`
   already works for the *bootstrap* compiler's own binary today (that's
   the host `mlx0`'s target, not the target mlx *emits code for* — this
   experiment is about the latter). Should expose `--min-sdk`/
   `--target-sdk` (or a `std.android` config struct, once item 4 lands)
   the same way `tool/main.zig`'s CLI flags do now.
