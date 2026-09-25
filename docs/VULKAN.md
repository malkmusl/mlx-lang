# Native Vulkan in Mlx

Mlx speaks Vulkan without a C loader, C headers or a C toolchain. Like
Wayland, the API is a standard-library extension materialized from the
canonical Khronos machine-readable sources while the standard library is
bootstrapped:

```text
 vk.xml (Vulkan-Headers)        spirv.core.grammar.json (SPIRV-Headers)
          |                                  |
          v                                  v
 std.vulkan.registry  (std.xml)     std.spirv.materialize  (std.json)
          |                                  |
          v                                  v
     std.vulkan  ------------------->  std.spirv.core
          |                             /           \
          v                            v             v
 std.vulkan.loader  (std.json)   std.spirv.builder  std.spirv.module
          |                       (write shaders)   (read and validate)
    +-----+------------------+
    |                        |
 ICD manifests + dlopen   system libvulkan
 (Linux: any installed    (Android: libvulkan.so;
  driver, e.g. lavapipe)   Linux: libvulkan.so.1)
```

Applications import `std.vulkan` for the API (types, enums, constants and
commands), `std.vulkan.loader` to reach a driver, and `std.spirv.*` to
produce shaders:

```mlx
const std = @import("std")
const vk = @import("std.vulkan")
const loader = @import("std.vulkan.loader")

pub fn main(arguments: [][*]const u8) -> u8 {
    var driver: loader.Driver = undefined
    if loader.openFirstDriver(&driver, arguments, std.page_allocator.init()) != loader.Status.ok { return 1 }
    var info = vk.InstanceCreateInfo.init()
    var instance: vk.Instance = 0
    if vk.createInstance(driver.global, &info, null, &instance) != vk.Result.success { return 2 }
    var commands: vk.InstanceCommands = undefined
    vk.loadInstanceCommands(&commands, instance, driver.getInstanceProcAddr)
    ...
}
```

On Linux the loader reads the installed drivers' ICD manifests, `dlopen`s a
driver and negotiates the loader/driver interface with it directly: every
Mesa driver in the test environment loads, and lavapipe runs transfer work
and an Mlx-built SPIR-V compute shader. On Android the system `libvulkan.so`
is the loader applications must use; `openSystemLoader` takes that path,
and `examples/vulkan-android` builds into an APK with
`--target=aarch64-android`. Calling C at all needed foreign functions in the compiler
(`extern("c")`, `export fn`, dynamically linked executables); see
[the ABI reference](reference/abi.md#c-functions-externc-and-export-fn).

## Examples

| Example | |
| --- | --- |
| [`examples/vulkan-info`](../examples/vulkan-info/README.md) | every installed driver, its devices and extensions |
| [`examples/vulkan-wayland-client`](../examples/vulkan-wayland-client/README.md) | a Wayland window rendered by a compute shader, handed to the compositor as dma-bufs or rendered straight into shared memory |
| [`examples/wayland-compositor`](../examples/wayland-compositor/README.md) `--renderer vulkan` | a compositor that composes its clients' buffers on the GPU, reading `wl_shm` pools and dma-bufs in place |
| [`examples/vulkan-android`](../examples/vulkan-android/README.md) | an Android NativeActivity presenting through a `VK_KHR_android_surface` swapchain, with touch input |

All of them share `examples/vulkan-shared`: the shaders (built with
`std.spirv.builder`), device and buffer setup with dma-buf and host-memory
sharing, swapchain presentation, and text: the client's label, the
compositor's window titles and the Android label are laid out by
[`std.truetype`](reference/truetype.md) and drawn by a compute shader.

![The Vulkan client inside the compositor's Vulkan renderer](../examples/wayland-compositor/screenshots/vulkan-client.png)

The reference — naming rules, the loader, SPIR-V, the driver interface,
the examples, tests and known limits — is
[`reference/vulkan.md`](reference/vulkan.md).

## Toward a driver

A Vulkan driver is what brixOS needs, and these are its building blocks:

- **The API itself** (`std.vulkan`): every structure a driver receives, with
  C layout verified against `vulkan.h` for all 318 generated aggregates.
- **The loader contract** (`std.vulkan.icd`): interface negotiation, the
  loader magic that starts every dispatchable object, and the ICD manifest
  that announces a driver. `std.vulkan.loader` exercises the same contract
  from the other side against Mesa's drivers.
- **Shaders** (`std.spirv`): the SPIR-V grammar as data, a decoder and
  structural validator that accepts glslang output and rejects malformed
  modules, and a builder whose output passes Khronos `spirv-val`.

What comes next, in order:

1. Shared-object output for x86_64 (the aarch64-android target already
   writes `libmain.so`), so an Mlx driver can be loaded by any Vulkan loader
   and tested with the Khronos loader, the CTS and `vulkaninfo`.
2. A software driver in Mlx: instance, physical device, memory, buffers,
   command buffers and queues on the CPU, with compute shaders executed from
   `std.spirv.module`'s decoded instructions (an interpreter first, then
   translation to x86_64 through the compiler's backend).
3. On brixOS, a DRM-shaped kernel interface (buffer objects, command
   submission, fences) with virtio-gpu first — its Venus protocol forwards
   Vulkan to the host, so the guest driver stays small — then native GPU
   backends.
4. Presentation: `VK_KHR_display` for direct scan-out. For Wayland the
   path already works without `VK_KHR_wayland_surface` (which would need a
   libwayland `wl_display`): frames travel as dma-bufs over linux-dmabuf,
   as the examples show; a driver only has to export them.
