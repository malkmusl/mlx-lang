# Prebuilt APK (for testing)

`mlx-vulkan.apk` is `examples/vulkan-android/main.mlx` built by the
self-hosted compiler (from the commit that last changed this file) with

```sh
mlx4 examples/vulkan-android/main.mlx -o mlx-vulkan.apk --target=aarch64-android \
    --android-package=dev.mlxlang.vulkan --android-label="Mlx Vulkan" \
    --android-fullscreen
```

and signed with a throwaway debug key, so remove any other build first:

```sh
adb uninstall dev.mlxlang.vulkan
adb install mlx-vulkan.apk
```

495781 bytes, SHA-256
`49bb659b79c62799c33bd01b7a62a0cd05dfabb81c920e1634a979daf9ad6be7`.
It logs each startup step to `adb logcat -s mlx` and shows a label drawn
with std.truetype from the system font (see ../README.md).
