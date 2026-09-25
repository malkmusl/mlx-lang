# Prebuilt APK (for testing)

`mlx-vulkan.apk` is `examples/vulkan-android/main.mlx` built by the
self-hosted compiler (from the commit that last changed this file) with

```sh
mlx4 examples/vulkan-android/main.mlx -o mlx-vulkan.apk --target=aarch64-android \
    --android-package=dev.mlxlang.vulkan --android-label="Mlx Vulkan"
```

and signed with a throwaway debug key, so remove any other build first:

```sh
adb uninstall dev.mlxlang.vulkan
adb install mlx-vulkan.apk
```

643237 bytes, SHA-256
`ce7cd33272960b1ce744460fadad628be5b52f6a0bc7d0e8a227ee856b90ff59`.
It logs each startup step to `adb logcat -s mlx` and shows a label drawn
with std.truetype from the system font (see ../README.md).
