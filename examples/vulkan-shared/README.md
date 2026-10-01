# vulkan-shared

The small Vulkan renderer the Vulkan examples share. It is not a program.

- `shaders.mlx`: the compute shaders, built with `std.spirv.builder`:
  `pattern` (the animated pattern with a pointer ring) and `blit`
  (composites premultiplied ARGB or opaque XRGB into a target, clipped; a
  source stride of 0 fills rectangles), `text` (draws a `std.truetype`
  glyph run from an atlas), `blurPass` and `blurMask` (the compositor's
  blur) and `paint` (the desktop apps' canvas: rounded rectangles,
  outlines, scaled images and glyphs from a list of commands, one pass
  over the image, the same pixels as `examples/desktop-shared/canvas.mlx`
  draws on the CPU), `warp` (wobbly windows) and `shrink` (window
  previews). `paint` lives in `paint_shader.mlx`, `warp` and `shrink` in
  `warp_shader.mlx`; every shader starts from `std.spirv.compute`
  (invocation id, push constants, storage buffers and the small
  instructions they share).
- `gpu.mlx`: a device with one compute queue and optional buffer sharing
  (dma-buf import and export, imported host memory); buffers, kernels,
  dispatches.
- `swapchain.mlx`: presentation to a window surface (Android, headless).
- `text.mlx`: text from `std.truetype` drawn by the `text` shader (atlas
  and glyph runs in GPU memory).
- `check_shaders.mlx`: validates the shaders with `std.spirv.module` and
  writes them out (`check_shaders DIR`) for `spirv-val`.

Used by `examples/vulkan-wayland-client`, `examples/wayland-compositor`
(`--renderer vulkan`), `examples/vulkan-android` and the desktop apps
(`examples/desktop-shared/gpu_canvas.mlx`); tested by
`tests/257_vulkan_sharing_runtime.mlx` and
`tests/258_vulkan_swapchain_runtime.mlx`. See `docs/reference/vulkan.md`.
