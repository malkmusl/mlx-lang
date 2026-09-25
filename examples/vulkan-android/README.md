# vulkan-android

Vulkan on Android: a NativeActivity that renders the animated pattern of
`examples/vulkan-shared` with a compute shader and presents it through a
`VK_KHR_android_surface` swapchain. Touch moves the ring; it turns white
while a finger is down.

The Vulkan side is the same code the Linux examples run: the system
`libvulkan.so` is opened with `std.vulkan.loader.openSystemLoader`, the
shader comes from `std.spirv.builder`, and `examples/vulkan-shared/
swapchain.mlx` renders each frame into a buffer, copies it into the
acquired image and presents it (FIFO, paced by a 60 Hz timer on the main
looper). R8G8B8A8 swapchains, common on Android, get red and blue swapped
in the shader.

## Build

This example needs the aarch64-android target and `std.android` from the
Android compiler branch (`claude/mlx-android-compiler-integration`); once
that branch is merged:

```sh
zig-out/bin/mlx1 examples/vulkan-android/main.mlx -o vulkan.apk \
    --target=aarch64-android --android-package=dev.mlxlang.vulkan \
    --android-label="Mlx Vulkan"
adb install -r vulkan.apk
adb logcat -s mlx
```

The log shows `vulkan: device ready`, `vulkan: swapchain created` and
`vulkan: first frame presented`.

## What is verified where

- The swapchain path runs on lavapipe through a `VK_EXT_headless_surface`
  (`tests/258_vulkan_swapchain_runtime.mlx`): creation, rendering,
  presentation, recreation at a new size.
- The activity code type-checks against the Android branch's `std.android`.
- Running it needs an Android device or emulator with Vulkan.
