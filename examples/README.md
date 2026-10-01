# Examples

Small programs that show one part of Mlx and its standard library. The
larger programs (the desktop, MLX Observatory, coreutils) are under
[`projects/`](../projects/README.md).

- `hello/`: the smallest program.
- `wayland-client/`, `wayland-server/`: a `std.wayland` client drawing
  into shared memory, and a minimal compositor (`std.wayland.server`).
- [`vulkan-info/`](vulkan-info/README.md): the Vulkan drivers and devices
  (`std.vulkan.loader`).
- [`vulkan-wayland-client/`](vulkan-wayland-client/README.md): a Wayland
  window rendered by a compute shader (`std.gpu`).
- `android/gestures.mlx`: touch gestures on Android (`std.android`).
- [`vulkan-android/`](vulkan-android/README.md): Vulkan on Android through a
  swapchain (`std.gpu.swapchain`).
