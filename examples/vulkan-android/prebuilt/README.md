# Prebuilt APK (for testing)

`mlx-vulkan.apk` is `examples/vulkan-android/main.mlx` built by the
self-hosted compiler at commit 4ccab91 with

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
`f5c727aae9425127ac903ec4ce2d69a7e6d443bd3bc009e1aa35749ea4850324`.
It logs each startup step to `adb logcat -s mlx` (see ../README.md).
