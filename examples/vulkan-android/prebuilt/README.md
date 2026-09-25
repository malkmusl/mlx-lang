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

594085 bytes, SHA-256
`8d0367328498187678007d6ffacea552df95e3599f26d7013e914e0b154efdab`.
It logs each startup step to `adb logcat -s mlx` and shows a label drawn
with std.truetype from the system font (see ../README.md).
