# vulkan-wayland-client

A Wayland window rendered by Vulkan: an animated pattern from a compute
shader, with a ring that follows the pointer (white while a button is
held). It uses only `std.wayland`, `std.vulkan.loader` and `std.spirv`: no
libwayland, no C Vulkan loader, no GLSL compiler.

```sh
mlx4 examples/vulkan-wayland-client/main.mlx -o vulkan-wayland-client
./vulkan-wayland-client --verbose
```

Frames go to the compositor the cheapest way both sides support
(`--mode`, default `auto`):

| Mode | How a frame reaches the compositor |
| --- | --- |
| `dmabuf` | The GPU renders into buffers exported as dma-bufs and handed over with `zwp_linux_dmabuf_v1` (XRGB8888, linear modifier). Zero-copy on GPUs. |
| `shm-direct` | The GPU renders straight into the `wl_shm` pool, imported with `VK_EXT_external_memory_host`. Zero-copy with CPU drivers such as lavapipe. |
| `shm-copy` | The GPU renders into its own memory and the frame is copied into the pool. |

Options: `--mode auto|dmabuf|shm-direct|shm-copy`, `--size WxH` (default
480x320), `--frames N` (exit after N frames), `--verbose`, and, for tests,
`--test-memfd-dmabuf`: where no dma-buf exporter exists (lavapipe), offer
the shared memory through linux-dmabuf to exercise a compositor's import
path.

`VK_DRIVER_FILES` picks a driver (for example lavapipe's
`/usr/share/vulkan/icd.d/lvp_icd.json`). It runs on any compositor with
`wl_shm` and xdg-shell, including the Mlx compositor:

```sh
mlx4 examples/wayland-compositor/main.mlx -o mlx-compositor
./mlx-compositor --renderer vulkan --run ./vulkan-wayland-client
```

The shaders and device setup are shared with the other Vulkan examples in
`examples/vulkan-shared`. `tools/check_vulkan_wayland.sh` checks this
client inside the compositor pixel by pixel. See `docs/reference/vulkan.md`.
