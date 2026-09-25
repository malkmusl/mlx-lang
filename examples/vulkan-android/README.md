# vulkan-android

Vulkan on Android: a NativeActivity that renders the animated pattern of
`examples/vulkan-shared` with a compute shader and presents it through a
`VK_KHR_android_surface` swapchain. Touch moves the ring; it turns white
while a finger is down. Like other Android apps, it keeps the status and
navigation bars: its window reaches under them, and only its background
color is drawn there. A label (the GPU and the frame count) sits at the
top center of the rest, laid out by
[`std.truetype`](../../docs/reference/truetype.md) from the system font
(`/system/fonts/Roboto-Regular.ttf`, or Noto Sans, Droid Sans) and drawn by
the `text` compute shader.

The Vulkan side is the same code the Linux examples run: the system
`libvulkan.so` is opened with `std.vulkan.loader.openSystemLoader`, the
shader comes from `std.spirv.builder`, and `examples/vulkan-shared/
swapchain.mlx` renders each frame into a buffer, copies it into the
acquired image and presents it (FIFO, paced by a 60 Hz timer on the main
looper). R8G8B8A8 swapchains, common on Android, get red and blue swapped
in the shader.

Rotation: the swapchain has the window's current size and
`preTransform = IDENTITY`, so frames are drawn upright and the compositor
rotates them for the display. Android answers each present of such a
swapchain with `VK_SUBOPTIMAL_KHR`; that only rebuilds the swapchain when
the window's size really changed. Turning the phone restarts the activity
(the manifest does not handle orientation changes), which builds a new
swapchain for the new orientation.

The layout follows `std.android`'s reserved-space rule
([android.md](../../docs/reference/android.md#reserved-space)): unless the
app is fullscreen, the space of the status and navigation bars and display
cutouts is reserved; only the background goes there and everything else
starts below it. `std.android.reservedInsets` asks the Java side through
JNI when the window is created or resized and after each layout pass
(`onContentRectChanged`), and the result is logged (`ui: reserved for the
system bars: top … right … bottom … left …`, or `ui: fullscreen, nothing
reserved`). The frame is a `ui.Screen` with those insets
([`std.ui`](../../docs/reference/ui.md)); the reserved bands are filled with
the background color (`text.fill`), and the label is placed at the top
center of the free area with `ui.Container` and clipped to it
(`text.drawIn`). Built with `--android-fullscreen`, nothing is reserved and
the label sits at the top center of the whole window. The text is sized
from the screen's shorter side, so it is the same size in portrait and
landscape, and the label drops its "Mlx + Vulkan · " prefix when the whole
line does not fit.

## Build

From the repository root, with the aarch64-android target
([`docs/reference/android.md`](../../docs/reference/android.md)):

```sh
zig-out/bin/mlx1 examples/vulkan-android/main.mlx -o vulkan.apk \
    --target=aarch64-android --android-package=dev.mlxlang.vulkan \
    --android-label="Mlx Vulkan"
adb install -r vulkan.apk
adb logcat -s mlx
```

Until the first frame is on screen, every step is logged (tag `mlx`), so a
driver that fails or crashes shows where:

```text
vulkan: onCreate
vulkan: opening libvulkan.so
vkCreateInstance
...
vulkan: device Adreno (TM) 740, Vulkan 1.3.128
vulkan: building the shader
vkCreateShaderModule
...
vulkan: device ready
vulkan: window created
vkCreateAndroidSurfaceKHR
...
vulkan: swapchain created
...
vulkan: first frame presented
```

If the app stops, the last line names the step, and
`adb logcat -b crash -d` holds the native crash report (signal, fault
address and the `libmain.so` offsets of the backtrace). If not even
`vulkan: onCreate` appears, the library did not load: `adb logcat -d` then
shows the linker or `NativeActivity` error.

## What is verified where

- The swapchain path runs on lavapipe through a `VK_EXT_headless_surface`
  (`tests/258_vulkan_swapchain_runtime.mlx`): creation, rendering,
  presentation, recreation at a new size.
- `tools/emulate_vulkan_android.py` runs the app's `libmain.so` (aarch64)
  under Unicorn against a model of the Android framework and a mock Vulkan
  driver: every Vulkan call, the submitted shader (`spirv-val`), each
  presented frame pixel by pixel, touch input, out-of-date and resized
  swapchains, the window going away and coming back, and a device without
  Vulkan.
- On hardware: `adb install -r vulkan.apk`, then `adb logcat -s mlx`.
