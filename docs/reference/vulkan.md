# Vulkan, SPIR-V and JSON

This expands [`VULKAN.md`](../VULKAN.md): how `std.vulkan` is materialized
from the Vulkan registry, what the generated API looks like, how
`std.vulkan.loader` reaches drivers, the SPIR-V modules, the driver-side
interface, and how all of it is tested.

| Module | Source | Role |
| --- | --- | --- |
| `std.json` | `std/src/json.mlx` | allocation-free JSON pull tokenizer, string/number decoding, string escaping |
| `std.vulkan` | `std/src/vulkan.mlx` (generated) | the Vulkan API |
| `std.vulkan.registry` | `std/src/vulkan/registry.mlx` | reads `vk.xml` |
| `std.vulkan.materialize` | `std/src/vulkan/materialize.mlx` | writes `std.vulkan` |
| `std.vulkan.loader` | `std/src/vulkan/loader.mlx` | finds and opens drivers |
| `std.vulkan.icd` | `std/src/vulkan/icd.mlx` | the driver side of the loader interface |
| `std.spirv.core` | `std/src/spirv/core.mlx` (generated) | SPIR-V opcodes, operand kinds, enumerants, signatures |
| `std.spirv.materialize` | `std/src/spirv/materialize.mlx` | writes `std.spirv.core` |
| `std.spirv.builder` | `std/src/spirv/builder.mlx` | writes SPIR-V modules |
| `std.spirv.module` | `std/src/spirv/module.mlx` | reads and validates SPIR-V modules |

Each is imported by its own name and is not part of `std.mlx`.

## Canonical sources and materialization

`std/registry/vulkan/vk.xml` is Vulkan-Headers 1.3.275 and
`std/registry/spirv/spirv.core.grammar.json` is SPIRV-Headers
vulkan-sdk-1.4.309.0 (SPIR-V 1.6 revision 4); each directory's `SOURCES`
records the upstream URL and SHA-256, and its `materialize.sh` refuses a
modified file. `materialize.sh` builds the directory's `materialize.mlx`
tool and runs it; `--check` verifies the committed module instead of writing
it. Both generators are Mlx programs using `std.xml` and `std.json` — no
Python or C generator is involved.

The Vulkan selection (in `std/registry/vulkan/materialize.mlx`) is
`VK_VERSION_1_0` to `VK_VERSION_1_3` plus `VK_KHR_surface`,
`VK_KHR_swapchain`, `VK_KHR_display`, `VK_KHR_wayland_surface`,
`VK_KHR_android_surface`, `VK_EXT_debug_utils`,
`VK_KHR_portability_enumeration`, `VK_KHR_external_memory_fd`,
`VK_KHR_external_semaphore_fd`, `VK_KHR_external_fence_fd`,
`VK_EXT_external_memory_dma_buf`, `VK_EXT_image_drm_format_modifier`,
`VK_EXT_queue_family_foreign`, `VK_EXT_external_memory_host` and
`VK_EXT_headless_surface`. `std.vulkan.registry` records every
definition for the `vulkan` API (skipping `vulkansc` variants), the
requirements of the selected features and extensions (evaluating `depends`
expressions and computing extension enumerant values from
`1000000000 + (extension - 1) * 1000 + offset`), and resolves the closure of
types the required commands and structures use.

## The generated API

| Registry | Mlx | Example |
| --- | --- | --- |
| struct / union `VkX` | `vk.X`, C layout | `vk.InstanceCreateInfo` |
| dispatchable handle | `usize` | `vk.Instance`, `vk.CommandBuffer` |
| non-dispatchable handle | `u64` | `vk.Buffer`, `vk.Pipeline`; `vk.NULL_HANDLE` |
| enum `VkX` | `enum(i32, nonexhaustive)`, camelCase members | `vk.Result.errorOutOfDateKhr`, `vk.Format.r8g8b8a8Unorm`, `vk.ImageType.type2d` |
| bitmask `VkXFlags` / `VkXFlagBits` | `Flags` (`u32`) or `Flags64` | `vk.BufferUsageFlags` |
| flag bit `VK_X_BIT` | constant without `VK_` | `vk.BUFFER_USAGE_STORAGE_BUFFER_BIT` |
| API / extension constant | constant without `VK_` | `vk.WHOLE_SIZE`, `vk.KHR_SWAPCHAIN_EXTENSION_NAME` |
| `PFN_vkX` callback type | `usize` (a function address) | `vk.PFN_vkDebugUtilsMessengerCallbackEXT` |
| command `vkX` | `vk.PFN_vkX` (fn type), a dispatch-table slot, wrapper `vk.x` | `vk.createInstance(driver.global, ...)` |

Enumerant names drop the enum's prefix (`VK_STRUCTURE_TYPE_`, or `VK_` when
it does not match) and a repeated vendor tag, are camelCased, borrow the
prefix's last word when they would start with a digit (`type2d`), and get a
trailing `_` when they are keywords (`vk.Format.undefined_`); fields that are
keywords get one too (`vk.DescriptorPoolSize.type_`).

Every structure and union has `init()`, which zeroes it and sets `sType`
when the registry fixes it. Pointer members are optional (`?*const T`, or
`?[*]const T` when the registry gives a length) so a zeroed structure is
valid; so are array parameters of commands, since Vulkan allows null when
the count is zero:

```mlx
var info = vk.InstanceCreateInfo.init()
if info.sType != vk.StructureType.instanceCreateInfo || info.pNext != null || info.enabledExtensionCount != 0 { return 1 }
var application = vk.ApplicationInfo.init()
application.apiVersion = vk.API_VERSION_1_3
info.pApplicationInfo = &application
```

(`tests/251_vulkan_api_runtime.mlx`) Extension structures chain through
`pNext`, which takes any pointer (it converts to `*anyopaque`):
`info.pNext = &external`.

Commands are grouped by the object they dispatch on: `vk.GlobalCommands`
(`vkCreateInstance`, `vkEnumerateInstance*`, `vkGetInstanceProcAddr`),
`vk.InstanceCommands` (instance and physical-device commands, and
`vkGetDeviceProcAddr`) and `vk.DeviceCommands` (device, queue and command
buffer commands). Each table is a struct of addresses (0 when the driver
does not provide a command), filled by `vk.loadGlobalCommands`,
`vk.loadInstanceCommands` and `vk.loadDeviceCommands`, which fall back to
an extension's name for promoted commands (`vkGetPhysicalDeviceProperties2KHR`).
Each wrapper takes its table first:

```mlx
var instance_commands: vk.InstanceCommands = undefined
vk.loadInstanceCommands(&instance_commands, instance, driver.getInstanceProcAddr)
...
var d: vk.DeviceCommands = undefined
vk.loadDeviceCommands(&d, device, instance_commands.vkGetDeviceProcAddr)
vk.cmdDispatch(&d, commands, COUNT / doubler.LOCAL_SIZE, 1, 1)
```

(`tests/254_spirv_compute_runtime.mlx`)

Device commands come straight from the driver's `vkGetDeviceProcAddr`, so
there is no loader trampoline between an application and its driver.

`tools/check_vulkan_layout.py` compiles a C program and an Mlx program that
print the size, alignment and member offsets of every generated structure
and union: all 322 aggregates and 2028 members match `<vulkan/vulkan.h>`
(the Android surface structure is skipped for lack of NDK headers).

## The loader

`std.vulkan.loader` opens a Vulkan implementation as a `loader.Driver`:
the library handle, its `getInstanceProcAddr`, the negotiated interface
version, its API version and its `global` command table.

**`findDrivers(&manifests, arguments, allocator)`** reads ICD manifests
(JSON) from the Khronos loader's Linux search path: `VK_DRIVER_FILES` (or
the older `VK_ICD_FILENAMES`) alone when set — manifest files or
directories, colon-separated — else `VK_ADD_DRIVER_FILES`, then `icd.d`
under `$XDG_CONFIG_HOME` (`~/.config`), each `$XDG_CONFIG_DIRS` entry
(`/etc/xdg`), `/etc/vulkan`, `$XDG_DATA_HOME` (`~/.local/share`) and each
`$XDG_DATA_DIRS` entry (`/usr/local/share:/usr/share`), manifests in name
order within a directory. Manifests for another architecture
(`library_arch` other than 64) or without `ICD.library_path` are skipped; a
relative library path is resolved against the manifest's directory.

**`openDriver(&driver, manifest, allocator)`** `dlopen`s the library, calls
`vk_icdNegotiateLoaderICDInterfaceVersion` offering interface 5 and
requiring at least 3 (so the driver owns `VkSurfaceKHR` objects), takes
`vk_icdGetInstanceProcAddr` (and `vk_icdGetPhysicalDeviceProcAddr` from
interface 4) and loads the global commands. `openFirstDriver` tries each
manifest in order.

**`openSystemLoader(&driver, name, allocator)`** opens a library that is
itself a loader and takes its `vkGetInstanceProcAddr`:
`loader.SYSTEM_LOADER_ANDROID` (`libvulkan.so`, the only way in on Android)
or `loader.SYSTEM_LOADER_LINUX` (`libvulkan.so.1`, when layers are wanted).

A `Driver` is one implementation: unlike the Khronos loader, the ICD path
does not merge several drivers' physical devices into one instance and
inserts no layers. `examples/vulkan-info` lists every installed driver and
its devices; with lavapipe selected it prints

```text
driver libvulkan_lvp.so (/usr/share/vulkan/icd.d/lvp_icd.json)
    interface version 5, Vulkan 1.4.318
    device: llvmpipe (LLVM 20.1.2, 256 bits) (CPU, Vulkan 1.4.318)
      queue family 0: 1 queue(s), graphics compute transfer
      memory heap 0: 16095 MiB, device local
```

and all eight Mesa drivers installed in the test environment load and
negotiate interface 5 (only lavapipe finds a device there).
`tests/253_vulkan_loader_runtime.mlx` opens lavapipe, creates a device,
records `vkCmdFillBuffer` and `vkCmdCopyBuffer` into host-visible buffers,
submits, waits on a fence and checks the mapped result.

## The driver interface

`std.vulkan.icd` is the other side of the same contract, for a driver
written in Mlx: `icd.negotiate(&version, supported)` answers the loader's
negotiation, `icd.initDispatchable` writes `icd.LOADER_MAGIC`
(`0x01CDC0DE`) at the start of a dispatchable object, `icd.hasLoaderMagic`
checks one, and `icd.writeManifest` produces the JSON manifest a driver
installs (using `json.escapeString`). The entry-point names are
`icd.NEGOTIATE_ENTRY`, `icd.GET_INSTANCE_PROC_ADDR_ENTRY` and
`icd.GET_PHYSICAL_DEVICE_PROC_ADDR_ENTRY`.
`tests/256_vulkan_icd_runtime.mlx` writes a manifest for lavapipe with
`writeManifest`, reads it back with `loader.readManifestFile`, opens the
driver through it, and checks that the instance and physical device lavapipe
returns start with the loader magic.

## SPIR-V

**`std.spirv.core`** names every opcode as an `Op` member without the `Op`
prefix (`spv.Op.typeVoid`, `spv.Op.return_`), every value enumeration as an
enum (`spv.StorageClass.storageBuffer`, `spv.ExecutionModel.glCompute`,
`spv.Dim.dim2D`) and every bit enumeration as a mask type with constants
(`spv.FUNCTION_CONTROL_INLINE`). It also carries the grammar as data:
`operandSignature(opcode)` describes an instruction's operands (one byte per
operand kind, `!` before an optional one, `#` before a repeated one),
`enumerantParameters(kind, value)` the operands that follow an enumerant
(`Decoration.builtIn` takes a `BuiltIn`, `ExecutionMode.localSize` three
literals), `compositeSignature` the parts of pair operands, and
`opcodeName`/`kindName` the grammar's names.

**`std.spirv.builder`** collects instructions into the sections of the
logical layout in any order and writes the module with its header:

```mlx
builder.capability(b, spv.Capability.shader)
builder.memoryModel(b, spv.AddressingModel.logical, spv.MemoryModel.glsl450)
const uint = builder.typeInt(b, 32, false)
...
const doubled = builder.binary(b, spv.Op.iMul, uint, value, two)
const sum = builder.binary(b, spv.Op.iAdd, uint, doubled, index)
builder.store(b, element, sum)
```

(`tests/support/spirv/doubler.mlx`, a compute shader computing
`values[i] = values[i] * 2 + i`)

**`std.spirv.module`** walks a module (`readHeader`, `nextInstruction`) and
validates its structure: header (magic, version, bound, schema), word counts,
known opcodes, operands decoded exactly by signature (enumerant parameters
and composites included), result ids inside the bound and defined once,
every referenced id defined somewhere, the logical layout order, function
delimiting, exactly one memory model and at least one entry point. It does
not check typing or execution-model rules — that is `spirv-val`'s job — but
it guarantees a driver can decode the module safely.

`tests/255_spirv_module_runtime.mlx` builds the shader, walks and validates
it, writes it out (`tests/run_vulkan.sh` then runs Khronos `spirv-val
--target-env vulkan1.1` on it, which accepts it) and checks that corrupted
copies fail with the right error: swapped magic, a future version, a zero
bound, a truncated instruction, an unterminated function, the memory model
before the capabilities, an extra operand, an unknown opcode, a zero word
count and an id at the bound. `run_vulkan.sh` also compiles the sample
shaders in `tests/support/spirv/` (`features.comp`, `textures.frag`,
`camera.vert`) with glslang for Vulkan 1.0 and 1.3 and validates them.
`tests/254_spirv_compute_runtime.mlx` runs the builder's shader on lavapipe
through the loader: 256 invocations, all results checked.

## JSON

`std.json` is a pull tokenizer in the style of `std.xml`: `next` reports
object and array starts and ends, member keys, strings, numbers and the
three literals, checking the grammar (nesting, commas, colons, one top-level
value, 64 levels); `skipValue` passes over a subtree; `decodeString`
expands escapes including UTF-16 surrogate pairs; `parseInteger` and
`parseUnsigned` convert numbers with overflow checks; `escapeString` writes
a JSON string. (`tests/248_json_runtime.mlx`)

## Examples

The examples share one small renderer in `examples/vulkan-shared`:

- `shaders.mlx` builds two compute shaders with `std.spirv.builder`:
  `pattern` (an animated pattern with a ring around the pointer, written as
  `0xAARRGGBB` or, for R8G8B8A8 targets, with red and blue swapped) and
  `blit` (composites a premultiplied ARGB or opaque XRGB source into a
  target at an offset, clipped; a source stride of 0 fills a rectangle).
  `check_shaders.mlx` validates both and writes them out for `spirv-val`.
- `gpu.mlx` opens a device with one compute queue and, on request, the
  sharing extensions the driver supports: dma-buf import and export
  (`VK_EXT_external_memory_dma_buf`) and imported host memory
  (`VK_EXT_external_memory_host`). It creates, imports and destroys storage
  buffers, compiles kernels and records dispatches.
- `swapchain.mlx` presents to a window surface: each frame the pattern is
  rendered into a buffer, copied into the acquired image
  (`vkCmdCopyBufferToImage`) and presented (FIFO).

`tests/257_vulkan_sharing_runtime.mlx` runs the sharing paths on lavapipe:
the pattern rendered into a memfd imported as host memory, the blit
reading a memfd imported as a dma-buf (opaque copy, premultiplied blending,
stride-0 fill, target offset), and a dma-buf export when the driver can.
`tests/258_vulkan_swapchain_runtime.mlx` presents through a
`VK_EXT_headless_surface` swapchain, checks the rendered frame and
recreates the swapchain at a new size.

On top of it:

| Example | What it shows |
| --- | --- |
| `examples/vulkan-wayland-client` | A Wayland client (std.wayland) that renders with Vulkan and hands frames over zero-copy: dma-bufs through `zwp_linux_dmabuf_v1`, or rendering straight into its `wl_shm` pool (imported host memory); a copying fallback. |
| `examples/wayland-compositor --renderer vulkan` | The nested compositor composes on the GPU: client `wl_shm` pools and linux-dmabuf buffers are imported where they are, and the output is rendered into the host window's buffer. |
| `examples/vulkan-android` | A NativeActivity presenting through a `VK_KHR_android_surface` swapchain from the system `libvulkan.so`, with touch input moving the ring. Needs the aarch64-android target (see the README there). |

`tools/check_vulkan_wayland.sh` runs the client inside the compositor with
both renderers and both client paths and compares the frames the host
receives pixel by pixel.

## Tests

`tests/run_vulkan.sh [compiler]` runs `tests/248_json_runtime.mlx`,
`tests/251_vulkan_api_runtime.mlx` and `tests/255_spirv_module_runtime.mlx`
everywhere; `tests/253_vulkan_loader_runtime.mlx`,
`tests/254_spirv_compute_runtime.mlx` and
`tests/256_vulkan_icd_runtime.mlx`, `tests/257_vulkan_sharing_runtime.mlx`
and `tests/258_vulkan_swapchain_runtime.mlx` when lavapipe (package
`mesa-vulkan-drivers`) is installed; `tools/check_vulkan_wayland.sh` when
lavapipe and `xkbcli` are; the `spirv-val` and glslang cross-checks
(including the examples' shaders) when those tools are present; the layout
check when gcc and `vulkan/vulkan.h` are; and verifies that
`examples/vulkan-info`, `examples/vulkan-wayland-client` and
`examples/wayland-compositor` build and that both generated modules are
current.

## Limits

- `f32` locals and arguments are not reliable with the bootstrap compiler
  (a float literal stays a 64-bit value); stores into `f32` fields are
  correct, so structures are fine, but commands taking float scalars
  (`vkCmdSetLineWidth`, `vkCmdSetDepthBias`, `vkCmdSetDepthBounds`) should
  not be used yet. Tests pass `1.0` queue priorities as `0x3F800000` bits.
- Methods with a `*const Self` receiver cannot be called on a value yet, so
  commands are free functions taking their dispatch table.
- A struct's methods may only name types declared earlier, and a pointer
  member may only point to a type declared earlier, so aggregates are
  emitted in dependency order and a pointer that would close a cycle is
  `*anyopaque`.
- `VK_KHR_wayland_surface` needs a libwayland `wl_display`; with the native
  std.wayland, Vulkan frames reach Wayland as dma-bufs (linux-dmabuf) or
  through `wl_shm` pools imported as host memory, as
  `examples/vulkan-wayland-client` does.
- lavapipe imports dma-bufs but cannot export them (that needs a kernel
  exporter such as `/dev/udmabuf`), so on it the client's dma-buf path is
  exercised with `--test-memfd-dmabuf`: shared memory offered as a dma-buf,
  which Mesa's import accepts.
- Android: `std.vulkan.loader` uses `dlopen`/`dlsym` from `libdl.so`, which
  the aarch64-android target already links; the aarch64 backend needs the
  same enum extension at C boundaries that x86_64 has
  (`tests/252_negative_enum_runtime.mlx` covers the in-memory part).
