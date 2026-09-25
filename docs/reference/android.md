# AArch64 and Android targets

`mlx1` generates AArch64 code in addition to x86_64. Two targets use it:

| Flag | Output |
| --- | --- |
| `--target=x86_64-linux` (default) | static ELF executable |
| `--target=aarch64-linux` | static ELF executable for Linux on AArch64 |
| `--target=aarch64-android` | `libmain.so`, or a signed APK when the output ends in `.apk` |

## Building an Android app

```sh
zig build mlx1
zig-out/bin/mlx1 examples/android/gestures.mlx -o gestures.apk \
    --target=aarch64-android \
    --android-package=dev.mlxlang.gestures --android-label="Mlx Gestures"
adb install -r gestures.apk
```

Run `mlx1` from the repository root so the standard library resolves.

| Option | Default | Meaning |
| --- | --- | --- |
| `--android-package=NAME` | `dev.mlxlang.app` | application id |
| `--android-label=TEXT` | `mlx app` | launcher label |
| `--android-version-code=N` | `1` | `versionCode` |
| `--android-version-name=TEXT` | `1.0` | `versionName` |
| `--android-min-sdk=N` | `21` | `minSdkVersion` (21 or higher: the first release with arm64-v8a) |
| `--android-target-sdk=N` | `37` | `targetSdkVersion`; also selects the signature schemes |
| `--android-fullscreen` | off | hide the status bar |
| `--android-key=PATH` | `$HOME/.mlx/android-debug.key` | signing key |

Without `--android-key`, the compiler signs with a per-user debug key and
creates it (RSA-2048, a few seconds) on first use, like Android Studio's
`debug.keystore`. Android only installs an update over an app signed with
the same key, so keep the key; a different key needs `adb uninstall` first.
The key file holds the modulus length (u32, little endian), then the
modulus and the private exponent (big endian); the public exponent is 65537.
`experiments/android-aarch64-bluescreen/debug_signing_key.bin` is a
checked-in, non-secret key in this format for reproducible debug builds.

The APK contains a binary `AndroidManifest.xml` (one
`android.app.NativeActivity` with `android.app.lib_name=main`, a MAIN/LAUNCHER
intent filter, `exported=true`, `extractNativeLibs=true`), an empty
`classes.dex`, `lib/arm64-v8a/libmain.so`, the v1 (JAR) signature files and
an APK Signing Block: v2 from target SDK 24, v3 from 28. Entries are stored
uncompressed; the library is aligned to 16 KiB and every other entry to 4
bytes (`zipalign -c -p 4` passes). `libmain.so` has 16 KiB-aligned segments,
as devices with 16 KiB pages require, and links against the stable NDK
system libraries `libandroid.so`, `liblog.so`, `libm.so`, `libc.so` and
`libdl.so`.

## Foreign and exported functions

```mlx
extern("c") fn ANativeWindow_release(window: usize) -> void
export fn ANativeActivity_onCreate(activity: usize, saved_state: usize, saved_state_size: usize) -> void { ... }
```

An `extern` function whose ABI string is not `"syscall"` (or that has none)
is a foreign C function: it has no body, keeps its source name, and calls go
through a GOT slot the dynamic linker fills. Libraries are not named in
source; the linker searches the needed libraries above. `export fn` makes a
function visible to the dynamic linker under its source name. Both use the
C ABI (AAPCS64 on aarch64): integer arguments in x0-x7, floating point in
v0-v7, further arguments on the stack. Integers narrower than 64 bits are
sign- or zero-extended on entry to every function and after any call that
may reach C code, because AAPCS64 leaves their upper bits unspecified. On
`x86_64-linux` a call to a foreign function is a compile error.

## `std.android`

`@import("std.android")` gives the NDK bindings (`ANativeWindow_*`,
`AInputQueue_*`, `AMotionEvent_*`, `ALooper_*`, `__android_log_write`,
`timerfd_*`, `malloc`) and a small NativeActivity runtime. An app is a
`draw` and an `update` function over one `usize` of state:

```mlx
const android = @import("std.android")

fn draw(canvas: *android.Canvas, state: usize) -> void {
    android.fill(canvas, if state == 0 { android.rgb(0, 0, 255) } else { android.rgb(0, 200, 0) })
}

fn update(state: usize, gesture: android.Gesture) -> usize { return state + 1 }

export fn ANativeActivity_onCreate(activity: usize, saved_state: usize, saved_state_size: usize) -> void {
    android.start(activity, saved_state, saved_state_size, draw, update)
}
```

`android.start` registers the activity callbacks and repaints the window
with `draw` whenever Android asks (window created, resized, redraw needed).
It drains the input queue on the main looper, finishing every event as
unhandled so system gestures still work. It recognizes one-finger gestures:

- tap: released within 24 px of the start and before 500 ms;
- long press: held still for 500 ms, recognized by a timerfd while the
  finger is still down;
- swipe: left, right, up or down by the dominant direction once the touch
  leaves the 24 px slop.

Each gesture goes to `update` and triggers a repaint. The state survives the
activity restart a rotation causes (`onSaveInstanceState`), and recognized
gestures are logged under the logcat tag `mlx`. Per-event memory lives in
one malloc'd block reached through `ANativeActivity.instance`, so handling
input allocates nothing. `Canvas`, `fill`, `fillRect`, `fillArea` and `rgb`
draw into the locked RGBA_8888 window buffer; each paint also sets the
canvas's `reserved` insets and `free` rectangle (below).

`examples/android/gestures.mlx` colors the screen below the status bar
and above the navigation bar by the last gesture: blue at start, green for
a tap, yellow for a long press, red/magenta for a swipe right/left,
cyan/orange for a swipe down/up.

### Reserved space

The layout rule for every Android window: an app that is not fullscreen
keeps the status and navigation bars, and its window reaches under them
(edge to edge, enforced from Android 15). That space is reserved: only the
app's background is drawn there, and every UI component starts below it
(and inside the other edges), in the `free` area. The top band keeps its
portrait height in landscape: while the status bar is shown it is as tall
as the status bar in portrait, the larger of the system's
`status_bar_height_portrait` (else `status_bar_height`) and the display
cutout's depth. The cutout itself is not reserved: in landscape the camera
sits at a side edge and the content goes there. A fullscreen app
(`--android-fullscreen`, whose theme sets `FLAG_FULLSCREEN`) has nothing
reserved and lays out over the whole window, as if the rule did not exist.

`reservedInsets(activity, &insets)` applies it: it returns
`Reserved.system_bars` with the reserved insets, `Reserved.fullscreen` with
zero insets (the bars are not even asked for), or `Reserved.unknown` with
zero insets until the window has been laid out. With `std.ui`,
`ui.screen(width, height, insets)` then gives `free` for the components
(see [ui.md](ui.md)). The `Canvas` runtime does this on every paint
(`canvas.*.reserved`, `canvas.*.free`), and `examples/vulkan-android` on
window creation, resize, each layout pass (`onContentRectChanged`) and
every 8 frames, logging `ui: reserved for the system bars: top … right …
bottom … left …` or `ui: fullscreen, nothing reserved`.

`setSystemBarsHidden(activity, status, navigation)` switches the bars at
runtime: both hidden for fullscreen, the navigation bar alone for more
room at the bottom; a swipe from the edge shows hidden bars for a moment.
API 30+ uses the window's `WindowInsetsController` (`hide`/`show` with
`BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE`), older levels the decor view's
`setSystemUiVisibility` (immersive sticky, with the layout-stable,
hide-navigation and fullscreen layout flags). Hidden bars reserve nothing,
and with the status bar hidden the top band goes too. With edge-to-edge
windows no layout callback reports the change, so ask `reservedInsets`
again a few frames later (the Vulkan example looks every 8 frames).

An app may let its content show behind a shown navigation (gesture) bar
instead of the background, as `examples/vulkan-android`'s "Gesture bar:
transparent" switch does: it leaves out the background fill in the
navigation bar's bands, while the reserved space, and so every component,
stays the same.

`isFullscreen(activity)` reads `getWindow().getAttributes().flags`.
`windowInsets(activity, &insets)` fills a `std.ui` `Insets` with where the
shown system bars cover the window, whether or not the app is fullscreen,
in window pixels, without the portrait-height rule and without cutouts.
The NDK has no C call for any of this, so it goes through the activity's
`JNIEnv`: `getWindow().getDecorView().getRootWindowInsets()`, then
`getInsets(WindowInsets.Type.systemBars())` (and `displayCutout()`) from
API 30 and `getSystemWindowInset*()` (and `getDisplayCutout()` from API 28)
on API 23 to 29, and `getResources().getIdentifier(...)` for the status
bar dimens, inside a local reference frame, with any Java exception
cleared. It returns false, with zero insets, below API 23, before the
window's first layout pass (no insets yet) or when a call fails. Call these
functions on the main thread.

## Runtime and Linux compatibility

Generated code needs no C runtime. Helpers emitted once per program provide
the aggregate arena (256 MiB, mapped on first use), `@byteMask64`, and a
syscall translator. The standard library issues Linux x86_64 syscall
numbers; on aarch64 the translator maps them and adapts arguments and
results: the legacy path calls (`open`, `stat`, `mkdir`, ... become `*at`
calls), the `O_*` bits that differ, and the `struct stat` and
`struct epoll_event` layouts. Unknown numbers return `-ENOSYS`.

## Verification tools

| Tool | Checks |
| --- | --- |
| `tools/check_aarch64_encoder.py` | every instruction encoding against `llvm-mc` |
| `tools/diff_aarch64_backend.py` | each `tests/*.mlx` built for both targets: x86_64 run natively, aarch64 run under `tools/aarch64_linux_emulator.py` (Unicorn); exit status and output must match |
| `tools/check_aarch64_shared_library.py` | exports, imports, stack arguments and narrow-integer extension of a `.so` |
| `tools/check_android_packaging.py` | CRC-32, Adler-32, SHA-1, SHA-256 and bignum results of the packaging code, built by mlx0 and by mlx1, against Python |
| `tools/check_android_apk.py` | `apksigner`, `jarsigner`, `zipalign`, `aapt2` on a built APK; reproducible output |
| `tools/emulate_android_app.py` | the gesture example against a model of the Android framework: taps, long presses, swipes, cancel, rotation, and the reserved space (a fake `JNIEnv`, see below): only the background under the status and navigation bars, nothing reserved when fullscreen |
| `tools/emulate_vulkan_android.py` | `examples/vulkan-android` against the same framework model plus a mock Vulkan driver behind `libvulkan.so`: instance and device extensions, the submitted shader (`spirv-val`), swapchain creation on an R8G8B8A8, "inherit"-alpha surface, every presented frame pixel by pixel, the reserved space (a fake `JNIEnv` answers `getRootWindowInsets` on API 34 through `WindowInsets.Type` and `Insets`, and on API 29 through `getSystemWindowInset*` and `getDisplayCutout`, plus the status bar dimens; before the first layout pass it has none) with only the background under it and the landscape top band at its portrait height, the fullscreen, navigation bar and gesture bar switches tapped at runtime (the content behind a transparent gesture bar) (`WindowInsetsController` on API 34, system UI flags on API 29) with the reserved space following, the std.truetype label at the top center of the rest and the buttons at its bottom center (the system font served from the test font, the `text` shader run on the fills, atlas and runs the app built), touch, out-of-date and resized swapchains, background and return, and devices without a system font or without Vulkan |

The emulator cannot run Android itself, so the last step is a device:
`adb install -r gestures.apk`, then `adb logcat -s mlx` shows each
recognized gesture.

In `tools/diff_aarch64_backend.py`, tests that load C libraries or a Vulkan
driver (`tests/247`, `253`, `254`, `257`, `258`, `263`) cannot run on the static
aarch64-linux target, and the Wayland transport tests (`tests/243` to `245`)
need `memfd_create`, `sendmsg` and `wait4`, which the emulator does not
implement.
