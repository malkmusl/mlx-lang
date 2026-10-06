# Mlx Files for Android

This is the native Android slice of the Mlx file manager. It browses the
app's private files directory, which needs no storage permission, and uses
the same raw `AInputQueue` lifecycle as `examples/vulkan-android`.

The app does not have a separate mobile skin. It imports the same responsive
Files UI used by `projects/desktop/files`: the palette, toolbar, list rows,
sidebar/status chrome, icons and breakpoints live in
`projects/filemanager-shared`. The Android adapter supplies the safe-area
insets, framebuffer stride/format and a density scale. Portrait phones use
the touch-sized phone layout; wider windows gain the list header and then the
sidebar without forking the UI.

Vulkan is the default renderer. The app opens Android's system
`libvulkan.so`, records the shared Canvas UI with the desktop paint compute
shader, and presents it through a `VK_KHR_android_surface` swapchain. If the
loader, device, surface or a frame fails, it keeps the same UI and switches
to a locked RGBA_8888 `ANativeWindow` buffer instead.

Build a signed APK from the repository root:

```sh
zig-out/bin/mlx1 projects/android/files/main.mlx \
  -o mlx-files-android.apk \
  --target=aarch64-android \
  --android-package=org.mlx.files \
  --android-label="Mlx Files"
```

Controls:

- tap the grid/list buttons to switch the responsive content view;
- tap a row to select a file or open that directory;
- tap a grid cell to select a file or open that directory;
- tap the toolbar's Back button to go to the parent directory;
- tap Search in any layout to focus it and open Android's configured system
  keyboard; tap outside it to dismiss the keyboard;
- tap the sidebar button in any layout; phones and compact windows open a
  drawer, while wide windows switch between the full sidebar and icon rail;
- swipe up/down to move the selection;
- swipe left to open the selected directory;
- swipe right to go to the parent directory;
- long-press to refresh.

The browser is deliberately pinned to `ANativeActivity.internalDataPath`.
Shared device storage and file opening require an Android document-picker
(Storage Access Framework) integration and are not part of this slice.
