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

348325 bytes, SHA-256
`2f799fdd78f84bdb23ac71362d12ce94760865a47961819d86d5a3099df04ba3`.
Android runs its library straight from the APK (`extractNativeLibs=false`),
so the installed app takes about the APK's size.
It logs each startup step to `adb logcat -s mlx` and shows a label drawn
with std.truetype from the system font (see ../README.md).
