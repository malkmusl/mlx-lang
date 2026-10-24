# mlx-vulkan (libmlxvulkan.so)

Vulkan's C API for the C and C++ code compiled here (ANGLE's Vulkan
backend, WebKit's WebGL): `vulkan/vulkan.h`, `vulkan/vulkan_core.h` and
`vulkan/vk_platform.h`, made by mlx-cheaders of `api/`, and
`libmlxvulkan.so`, which a program links in place of Khronos's
libvulkan.so.1. There is no Khronos loader: the driver is found and
opened by std.vulkan.loader.

```sh
./mlx build vulkan                       # the headers (mlx-out/include/mlx-vulkan) and the library
compiler/libc/vulkan/generate.sh         # api/vulkan_core.mlx and commands.mlx again, from the registry
tools/check_mlx_vulkan.sh                # the headers in C and C++, a buffer filled on the GPU
```

- **The headers**: `api/vulkan_core.mlx` is generated from
  std/registry/vulkan/vk.xml (Vulkan-Headers 1.4.362) by `generate.mlx`
  with std.vulkan.registry: Vulkan 1.4 and every extension of no window
  system, not provisional, not of video coding, as Vulkan-Headers'
  vulkan_core.h has them (the handles, the enumerations with their
  `_MAX_ENUM`, 64-bit flag bits as constants, the structures in
  dependency order, a run of bit-fields as one `uint32_t` named after its
  fields, every command's `PFN_` type and prototype); the macros
  (`VK_DEFINE_HANDLE`, `VK_NULL_HANDLE`, `VK_MAKE_API_VERSION`...) are its
  `c_macros`. The window systems' headers (vulkan_wayland.h...) are not
  made.
- **The driver**: the first, in the Khronos loader's search order
  (`VK_DRIVER_FILES`, `VK_ICD_FILENAMES`, `VK_ADD_DRIVER_FILES`, the
  icd.d directories), that loads and has a physical device; it is opened
  with `RTLD_LAZY`, as that loader opens drivers (lavapipe's
  dependencies name glibc functions mlxlibc binds lazily). No layers.
- **The dispatch**: objects are the driver's, unwrapped; the first word of
  each dispatchable one (left to the loader by the loader/driver
  interface) points to its instance's or its device's table (`dispatch.mlx`),
  filled from the driver's `vk_icdGetInstanceProcAddr` and
  `vkGetDeviceProcAddr` (an alias's target when the driver lacks the
  name). `commands.mlx` (generated) exports every command under its C name,
  calling through the table; `vulkan.mlx` the commands that make, find or
  end dispatchable objects (instances, physical devices, devices, queues,
  command buffers) and those only a loader answers (versions, layers,
  instance extensions). `vkGetInstanceProcAddr` and `vkGetDeviceProcAddr`
  give the driver's own functions but for those.
- **Tracing**: with `MLX_VULKAN_TRACE` set, each command called through
  the tables is written on the standard error with its first argument
  (`vulkan: vkCmdDraw 0x...`): what a crash in the driver came after.
  Then `vkGetDeviceProcAddr` gives this library's functions too, so that
  the commands a program calls through pointers it asked for are said
  as well.
