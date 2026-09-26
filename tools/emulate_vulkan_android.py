#!/usr/bin/env python3
"""Runs examples/vulkan-android (its libmain.so, or the one inside an APK)
under Unicorn against the Android framework model of
tools/emulate_android_app.py plus a mock Vulkan driver behind a fake
libvulkan.so, and checks the app end to end:

  - the app opens libvulkan.so with dlopen/dlsym and creates an instance with
    VK_KHR_surface and VK_KHR_android_surface and a device with
    VK_KHR_swapchain;
  - the compute shader it submits passes spirv-val (when installed);
  - a swapchain on the window's surface: the driver offers only R8G8B8A8 and
    "inherit" composition, as Android drivers commonly do;
  - every presented frame holds the pattern for its time value, with red and
    blue swapped for R8G8B8A8, after dispatch -> copy -> present with the
    image in PRESENT_SRC layout and the acquire semaphore waited on;
  - the 60 Hz timer keeps presenting; touch moves the ring (white while
    down, amber after); an out-of-date swapchain and a window resize
    recreate it; destroying the window destroys the swapchain and surface;
  - landscape on a portrait display (currentTransform ROTATE_90): the
    swapchain is IDENTITY at the landscape size, and the suboptimal
    presents Android then returns rebuild it only when the size changes;
  - the label: the app loads a system font (/system/fonts/..., served
    from tests/support/fonts/DejaVuSans-subset.ttf), lays it out with
    std.truetype and draws it with the `text` shader, whose arithmetic the
    mock runs on the atlas and glyph runs the aarch64 code produced;
  - without libvulkan.so the app logs it and keeps running.

The mock "GPU" runs the pattern shader's formula (the shader itself is
checked against that formula on a real driver by
tests/258_vulkan_swapchain_runtime.mlx and tests/257_vulkan_sharing_runtime.mlx).

Usage: python3 tools/emulate_vulkan_android.py [-v] [app.apk | libmain.so]
With no file, builds examples/vulkan-android/main.mlx with zig-out/bin/mlx1.
"""
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from aarch64_linux_emulator import SharedLibrary  # noqa: E402
from emulate_android_app import Framework, TIMER_FD, WINDOW  # noqa: E402
from unicorn.arm64_const import UC_ARM64_REG_S0, UC_ARM64_REG_SP  # noqa: E402

LIBVULKAN = 0x5800_0000_0001
LIBANDROID = 0x5800_0000_0002
CHOREOGRAPHER = 0x5C00_0000_0001
# The display refreshes every 16.67 ms (60 Hz), or every 8.33 ms at 120 Hz
# when the app asks for it and the user's setting allows it.
VSYNC = 16_666_667
VSYNC_120 = 8_333_333
FORMAT_R8G8B8A8_UNORM = 37
LAYOUT_UNDEFINED, LAYOUT_TRANSFER_DST, LAYOUT_PRESENT_SRC = 0, 7, 1000001002
ERROR_OUT_OF_DATE = (-1000001004) & 0xFFFFFFFF
SUBOPTIMAL = 1000001003
TRANSFORM_IDENTITY, TRANSFORM_ROTATE_90 = 1, 2
COMPOSITE_ALPHA_INHERIT = 8
ACTION_DOWN, ACTION_UP, ACTION_MOVE = 0, 1, 2
MS = 1_000_000
FONT = os.path.join(ROOT, "tests/support/fonts/DejaVuSans-subset.ttf")
# The app's background (examples/vulkan-android BACKGROUND_COLOR,
# 0xAARRGGBB before the app swaps red and blue for R8G8B8A8): all that may
# be drawn under the system bars.
BACKGROUND_COLOR = 0xFF101418
# Rows below the top inset the label and the frame counter under it (with
# their padding and shadows) may use.
LABEL_ROWS = 50


def pattern(x, y, width, time, pointer_x, pointer_y, flags):
    """The pattern shader (examples/vulkan-shared/shaders.mlx) for one pixel."""
    red = (x + time) & 255
    green = (y + (time >> 1)) & 255
    blue = ((x ^ y) + (time << 1)) & 255
    color = 0xFF000000 | red << 16 | green << 8 | blue
    distance = (x - pointer_x) ** 2 + (y - pointer_y) ** 2
    if flags & 1 and 900 <= distance < 1600:
        color = 0xFFFFFFFF if flags & 2 else 0xFFFFD040
    if flags & 4:
        color = (color & 0xFF00FF00) | (color & 255) << 16 | (color >> 16) & 255
    return color


class MockVulkan:
    """Just enough of a Vulkan driver for the app, recording what it does."""

    def __init__(self, framework, available=True):
        self.fw = framework
        self.lib = framework.lib
        self.available = available
        self.next_handle = 0x1000
        self.extent = (96, 64)
        # The display rotation Android reports (ROTATE_90 in landscape on a
        # portrait phone); currentExtent is always in the window's current
        # orientation.
        self.transform = TRANSFORM_IDENTITY
        self.swapchains_created = 0
        self.suboptimal = 0
        self.calls = []
        self.shaders = []
        self.buffers = {}        # handle -> [size, memory]
        self.memory = {}         # handle -> (address, size)
        self.sets = {}           # descriptor set -> {binding: buffer}
        self.recorded = {}       # command buffer -> [commands]
        self.images = {}         # image -> {"layout", "pixels", "size"}
        self.swapchains = {}     # handle -> {"images", "next", "extent", "format"}
        self.surfaces = set()
        self.acquire_semaphore = None
        self.frames = []         # (extent, pixels)
        self.pipelines = {}      # handle -> "pattern" or "text"
        self.text_draws = 0
        self.out_of_date_once = False
        # The fence: signaled (the mock GPU finishes a submission at once)
        # and whether the app has waited for it since. Frames must be waited
        # for before the command buffer is used again or the next image is
        # acquired, and should be presented before that wait.
        self.fence_signaled = False
        self.fence_unwaited = False
        self.presented_unwaited = 0
        # The semaphore each swapchain image was presented with.
        self.present_semaphores = {}
        self.commands = {name[3:]: getattr(self, name) for name in dir(self) if name.startswith("vk_")}
        self.addresses = {}
        framework.lib.imports.update({"dlopen": self.dlopen, "dlsym": self.dlsym, "dlclose": lambda p, *_: 0})

    # ── plumbing ──
    def handle(self):
        self.next_handle += 0x10
        return self.next_handle

    def stub(self, name, function):
        if name in self.addresses:
            return self.addresses[name]
        address = SharedLibrary.STUB_BASE + len(self.lib.stubs) * 16
        self.lib.uc.mem_write(address, struct.pack("<I", 0xD65F03C0))  # ret
        self.lib.stubs[address] = name

        def call(process, *args):
            self.calls.append(name)
            return function(process, *args)
        self.lib.imports[name] = call
        self.addresses[name] = address
        return address

    def u32(self, address):
        return struct.unpack("<I", self.lib.read(address, 4))[0]

    def u64(self, address):
        return struct.unpack("<Q", self.lib.read(address, 8))[0]

    def put_u32(self, address, value):
        self.lib.write(address, struct.pack("<I", value & 0xFFFFFFFF))

    def put_u64(self, address, value):
        self.lib.write(address, struct.pack("<Q", value & 0xFFFFFFFFFFFFFFFF))

    def strings(self, array, count):
        return [self.lib.cstring(self.u64(array + 8 * index)) for index in range(count)]

    def stack_argument(self, index):
        return self.u64(self.lib.uc.reg_read(UC_ARM64_REG_SP) + 8 * index)

    # ── libdl ──
    def dlopen(self, process, name, flags, *_):
        return LIBVULKAN if self.available and process.cstring(name) == "libvulkan.so" else 0

    def dlsym(self, process, library, name, *_):
        symbol = process.cstring(name)
        if library == LIBVULKAN and symbol == "vkGetInstanceProcAddr":
            return self.stub(symbol, self.vk_GetInstanceProcAddr)
        return 0

    def vk_GetInstanceProcAddr(self, process, instance, name, *_):
        command = process.cstring(name)
        function = self.commands.get(command[2:])
        return self.stub(command, function) if function else 0

    vk_GetDeviceProcAddr = vk_GetInstanceProcAddr

    # ── instance and device ──
    def vk_EnumerateInstanceVersion(self, process, version, *_):
        self.put_u32(version, (1 << 22) | (1 << 12))
        return 0

    def vk_CreateInstance(self, process, info, allocator, instance, *_):
        self.instance_extensions = self.strings(self.u64(info + 56), self.u32(info + 48))
        self.put_u64(instance, self.handle())
        return 0

    def vk_EnumeratePhysicalDevices(self, process, instance, count, devices, *_):
        if devices:
            self.put_u64(devices, 0x7777)
        self.put_u32(count, 1)
        return 0

    def vk_GetPhysicalDeviceQueueFamilyProperties(self, process, device, count, properties, *_):
        if properties:
            self.lib.write(properties, struct.pack("<IIIIII", 7, 1, 64, 1, 1, 1))
        self.put_u32(count, 1)

    def vk_GetPhysicalDeviceMemoryProperties(self, process, device, properties, *_):
        self.put_u32(properties, 1)
        self.lib.write(properties + 4, struct.pack("<II", 7, 0))  # device local, host visible, coherent
        self.put_u32(properties + 260, 1)
        self.put_u64(properties + 264, 1 << 30)

    def vk_GetPhysicalDeviceProperties(self, process, device, properties, *_):
        self.lib.write(properties, struct.pack("<IIIII", (1 << 22) | (1 << 12), 1, 0x13B5, 1, 1))
        self.lib.write(properties + 20, b"Mock Android GPU\0")

    def vk_CreateDevice(self, process, physical, info, allocator, device, *_):
        self.device_extensions = self.strings(self.u64(info + 56), self.u32(info + 48))
        self.put_u64(device, self.handle())
        return 0

    def vk_GetDeviceQueue(self, process, device, family, index, queue, *_):
        self.put_u64(queue, 0x9999)

    def create_handle(self, process, device, info, allocator, result, *_):
        self.put_u64(result, self.handle())
        return 0

    vk_CreateCommandPool = vk_CreateFence = vk_CreateDescriptorPool = create_handle
    vk_CreateDescriptorSetLayout = vk_CreateSemaphore = create_handle

    def vk_CreatePipelineLayout(self, process, device, info, allocator, layout, *_):
        ranges = self.u64(info + 40)
        self.push_size = self.u32(ranges + 8) if self.u32(info + 32) else 0
        self.put_u64(layout, self.handle())
        return 0

    def vk_AllocateCommandBuffers(self, process, device, info, buffers, *_):
        self.put_u64(buffers, self.handle())
        return 0

    def vk_CreateShaderModule(self, process, device, info, allocator, module, *_):
        size = self.u64(info + 24)
        self.shaders.append(self.lib.read(self.u64(info + 32), size))
        self.put_u64(module, self.handle())
        return 0

    def vk_CreateComputePipelines(self, process, device, cache, count, infos, allocator, pipelines, *_):
        # The app creates the pattern pipeline first, the text one second.
        for index in range(count):
            handle = self.handle()
            self.pipelines[handle] = "pattern" if not self.pipelines else "text"
            self.put_u64(pipelines + 8 * index, handle)
        return 0

    def nothing(self, process, *_):
        return 0

    vk_DestroyShaderModule = vk_DestroyFence = vk_DestroyCommandPool = vk_DestroyDescriptorPool = nothing
    vk_DestroySemaphore = vk_DeviceWaitIdle = nothing
    vk_EndCommandBuffer = vk_UnmapMemory = nothing

    def vk_WaitForFences(self, process, device, count, fences, *_):
        assert self.fence_signaled, "waiting for a fence nothing will signal"
        self.fence_unwaited = False
        return 0

    def vk_ResetFences(self, process, device, count, fences, *_):
        assert not self.fence_unwaited, "fence reset before it was waited for"
        self.fence_signaled = False
        return 0

    def reuse(self, what):
        assert not self.fence_unwaited, f"{what} while the last frame may still run on the GPU"

    def vk_ResetDescriptorPool(self, process, *_):
        self.reuse("descriptor pool reset")
        return 0

    def vk_BeginCommandBuffer(self, process, *_):
        self.reuse("command buffer recorded")
        return 0
    vk_FreeMemory = vk_DestroyBuffer = vk_DestroyPipeline = vk_DestroyPipelineLayout = nothing
    vk_DestroyDescriptorSetLayout = nothing

    def vk_DestroyDevice(self, process, *_):
        self.device_destroyed = True

    def vk_DestroyInstance(self, process, *_):
        self.instance_destroyed = True

    # ── memory ──
    def vk_CreateBuffer(self, process, device, info, allocator, buffer, *_):
        handle = self.handle()
        self.buffers[handle] = [self.u64(info + 24), None]
        self.put_u64(buffer, handle)
        return 0

    def vk_GetBufferMemoryRequirements(self, process, device, buffer, requirements, *_):
        self.lib.write(requirements, struct.pack("<QQI", self.buffers[buffer][0], 64, 1))

    def vk_AllocateMemory(self, process, device, info, allocator, memory, *_):
        size = self.u64(info + 16)
        handle = self.handle()
        self.memory[handle] = (self.fw.malloc(size), size)
        self.put_u64(memory, handle)
        return 0

    def vk_BindBufferMemory(self, process, device, buffer, memory, offset, *_):
        self.buffers[buffer][1] = self.memory[memory][0] + offset
        return 0

    def vk_MapMemory(self, process, device, memory, offset, size, flags, data, *_):
        self.put_u64(data, self.memory[memory][0] + offset)
        return 0

    # ── descriptors and commands ──
    def vk_AllocateDescriptorSets(self, process, device, info, sets, *_):
        for index in range(self.u32(info + 24)):
            handle = self.handle()
            self.sets[handle] = {}
            self.put_u64(sets + 8 * index, handle)
        return 0

    def vk_UpdateDescriptorSets(self, process, device, count, writes, *_):
        for index in range(count):
            write = writes + 64 * index
            info = self.u64(write + 48)
            self.sets[self.u64(write + 16)][self.u32(write + 24)] = self.u64(info)

    def vk_ResetCommandBuffer(self, process, buffer, *_):
        self.reuse("command buffer reset")
        self.recorded[buffer] = []
        return 0

    def record(self, buffer, *command):
        self.recorded.setdefault(buffer, []).append(command)

    def vk_CmdBindPipeline(self, process, buffer, point, pipeline, *_):
        self.record(buffer, "pipeline", pipeline)

    def vk_CmdBindDescriptorSets(self, process, buffer, point, layout, first, count, sets, *_):
        self.record(buffer, "set", self.u64(sets))

    def vk_CmdPushConstants(self, process, buffer, layout, stages, offset, size, values, *_):
        self.record(buffer, "push", struct.unpack(f"<{size // 4}I", self.lib.read(values, size)))

    def vk_CmdDispatch(self, process, buffer, x, y, z, *_):
        self.record(buffer, "dispatch", x & 0xFFFFFFFF, y & 0xFFFFFFFF)

    def vk_CmdPipelineBarrier(self, process, buffer, source, target, dependency, memory_count, memory, buffer_count, buffers):
        image_count = self.stack_argument(0) & 0xFFFFFFFF
        images = self.stack_argument(1)
        for index in range(image_count):
            barrier = images + 72 * index
            old, new = struct.unpack("<II", self.lib.read(barrier + 24, 8))
            self.record(buffer, "layout", self.u64(barrier + 40), old, new)

    def vk_CmdCopyBufferToImage(self, process, buffer, source, image, layout, count, regions, *_):
        width, height = struct.unpack("<II", self.lib.read(regions + 44, 8))
        self.record(buffer, "copy", source, image, layout & 0xFFFFFFFF, width, height, self.u64(regions))

    def vk_QueueSubmit(self, process, queue, count, submits, fence, *_):
        assert not self.fence_signaled, "submitted with a fence still signaled"
        if fence:
            self.fence_signaled = True
            self.fence_unwaited = True
        for index in range(count):
            submit = submits + 72 * index
            waits = [self.u64(self.u64(submit + 24) + 8 * n) for n in range(self.u32(submit + 16))]
            for n in range(self.u32(submit + 40)):
                self.execute(self.u64(self.u64(submit + 48) + 8 * n), waits)
        return 0

    def execute(self, buffer, waits):
        state = {}
        for command in self.recorded.get(buffer, []):
            kind = command[0]
            if kind == "set":
                state["set"] = self.sets[command[1]]
            elif kind == "push":
                state["push"] = command[1]
            elif kind == "pipeline":
                state["pipeline"] = self.pipelines[command[1]]
            elif kind == "dispatch" and state.get("pipeline") == "text":
                self.run_text(state["push"], state["set"], command[1], command[2])
            elif kind == "dispatch":
                width, height, time, pointer_x, pointer_y, flags = state["push"]
                assert command[1] * 8 >= width and command[2] * 8 >= height, "dispatch does not cover the frame"
                target = self.buffers[state["set"][0]][1]
                pixels = [pattern(x, y, width, time, pointer_x, pointer_y, flags)
                          for y in range(height) for x in range(width)]
                self.lib.write(target, struct.pack(f"<{len(pixels)}I", *pixels))
            elif kind == "layout":
                image = self.images[command[1]]
                assert command[2] in (LAYOUT_UNDEFINED, image["layout"]), "barrier from the wrong layout"
                image["layout"] = command[3]
            elif kind == "copy":
                source, image_handle, layout, width, height, offset = command[1:]
                image = self.images[image_handle]
                assert layout == LAYOUT_TRANSFER_DST == image["layout"], "copy into an image not in TRANSFER_DST"
                assert (width, height) == image["size"], "copy does not cover the image"
                assert self.acquire_semaphore in waits, "the copy does not wait for the acquired image"
                image["pixels"] = struct.unpack(f"<{width * height}I", self.lib.read(self.buffers[source][1] + offset, width * height * 4))

    def run_text(self, push, bindings, groups_x, groups_y):
        """The `text` shader: per pixel of the run's box, the largest of the
        base coverage and the atlas coverage of the quads covering it,
        blended over the target (base 255 and no quads: a solid fill)."""
        def signed(value):
            return value - (1 << 32) if value >= 1 << 31 else value
        (width, height, offset, origin_x, origin_y, box_left, box_top, box_width, box_height,
         count, atlas_width, color, run_offset, base_coverage) = push
        origin_x, origin_y, box_left, box_top = map(signed, (origin_x, origin_y, box_left, box_top))
        assert groups_x * 8 >= box_width and groups_y * 8 >= box_height, "text dispatch does not cover the run"
        atlas_handle, runs_handle, target_handle = bindings[0], bindings[1], bindings[2]
        atlas_size, atlas_address = self.buffers[atlas_handle]
        atlas = self.lib.read(atlas_address, atlas_size)
        words = struct.unpack(f"<{count * 4}I", self.lib.read(self.buffers[runs_handle][1] + run_offset * 4, count * 16))
        coverage = {}
        if base_coverage:
            for y in range(box_top, box_top + box_height):
                for x in range(box_left, box_left + box_width):
                    coverage[(x, y)] = base_coverage
        for index in range(count):
            w0, w1, qx, qy = words[4 * index:4 * index + 4]
            ax, ay, qw, qh = w0 & 65535, w0 >> 16, w1 & 65535, w1 >> 16
            qx, qy = signed(qx), signed(qy)
            for ly in range(qh):
                for lx in range(qw):
                    x, y = qx + lx, qy + ly
                    if box_left <= x < box_left + box_width and box_top <= y < box_top + box_height:
                        value = atlas[(ay + ly) * atlas_width + ax + lx]
                        if value > coverage.get((x, y), 0):
                            coverage[(x, y)] = value
        target = self.buffers[target_handle][1]
        for (x, y), value in coverage.items():
            tx, ty = origin_x + x, origin_y + y
            if value == 0 or not (0 <= tx < width and 0 <= ty < height):
                continue
            at = target + (offset + ty * width + tx) * 4
            under = struct.unpack("<I", self.lib.read(at, 4))[0]
            alpha = (value * (color >> 24) + 127) // 255
            result = 0xFF000000
            for shift in (0, 8, 16):
                mixed = (((color >> shift) & 255) * alpha + ((under >> shift) & 255) * (255 - alpha) + 127) // 255
                result |= mixed << shift
            self.lib.write(at, struct.pack("<I", result))
        self.text_draws += 1

    # ── WSI ──
    def vk_CreateAndroidSurfaceKHR(self, process, instance, info, allocator, surface, *_):
        assert self.u64(info + 24) == WINDOW, "surface for the wrong window"
        handle = self.handle()
        self.surfaces.add(handle)
        self.put_u64(surface, handle)
        return 0

    def vk_DestroySurfaceKHR(self, process, instance, surface, *_):
        self.surfaces.discard(surface)

    def vk_GetPhysicalDeviceSurfaceSupportKHR(self, process, device, family, surface, supported, *_):
        self.put_u32(supported, 1)
        return 0

    def vk_GetPhysicalDeviceSurfaceCapabilitiesKHR(self, process, device, surface, capabilities, *_):
        width, height = self.extent
        self.lib.write(capabilities, struct.pack("<IIIIIIIIIIIII", 3, 64, width, height, 1, 1, 4096, 4096, 1,
                                                 1 | 2 | 4 | 8, self.transform, COMPOSITE_ALPHA_INHERIT, 2 | 16))
        return 0

    def vk_GetPhysicalDeviceSurfaceFormatsKHR(self, process, device, surface, count, formats, *_):
        if formats:
            self.lib.write(formats, struct.pack("<II", FORMAT_R8G8B8A8_UNORM, 0))
        self.put_u32(count, 1)
        return 0

    def vk_CreateSwapchainKHR(self, process, device, info, allocator, swapchain, *_):
        surface = self.u64(info + 24)
        image_format, _space, width, height = struct.unpack("<IIII", self.lib.read(info + 36, 16))
        usage = self.u32(info + 56)
        transform = self.u32(info + 80)
        alpha = self.u32(info + 84)
        assert surface in self.surfaces, "swapchain on a destroyed surface"
        assert image_format == FORMAT_R8G8B8A8_UNORM, image_format
        assert (width, height) == self.extent, ((width, height), self.extent)
        assert usage & 2, "swapchain images are not transfer destinations"
        assert alpha == COMPOSITE_ALPHA_INHERIT, alpha
        # As few images as allowed: less queued, less input lag.
        requested = self.u32(info + 32)
        assert requested == 3, f"{requested} images, not minImageCount (3)"
        # Frames are drawn upright at the current orientation's size, so the
        # compositor has to rotate them: IDENTITY, whatever the display says.
        assert transform == TRANSFORM_IDENTITY, f"preTransform {transform} for upright frames"
        self.swapchains_created += 1
        handle = self.handle()
        images = []
        for _ in range(requested):
            image = self.handle()
            self.images[image] = {"layout": LAYOUT_UNDEFINED, "pixels": None, "size": (width, height)}
            images.append(image)
        self.swapchains[handle] = {"images": images, "next": 0, "extent": (width, height), "transform": transform}
        self.put_u64(swapchain, handle)
        return 0

    def vk_DestroySwapchainKHR(self, process, device, swapchain, *_):
        self.swapchains.pop(swapchain, None)

    def vk_GetSwapchainImagesKHR(self, process, device, swapchain, count, images, *_):
        listed = self.swapchains[swapchain]["images"]
        if images:
            for index, image in enumerate(listed[:self.u32(count)]):
                self.put_u64(images + 8 * index, image)
        self.put_u32(count, len(listed))
        return 0

    def vk_AcquireNextImageKHR(self, process, device, swapchain, timeout, semaphore, fence, index, *_):
        # The acquire semaphore is free again only once the frame that
        # waited on it is done.
        self.reuse("next image acquired")
        chain = self.swapchains[swapchain]
        assert semaphore, "acquire without a semaphore"
        self.acquire_semaphore = semaphore
        self.put_u32(index, chain["next"])
        chain["next"] = (chain["next"] + 1) % len(chain["images"])
        return 0

    def vk_QueuePresentKHR(self, process, queue, info, *_):
        assert self.u32(info + 16) == 1, "present waits for no semaphore"
        swapchain = self.u64(self.u64(info + 40))
        index = self.u32(self.u64(info + 48))
        # One semaphore per image, always the same for an image.
        semaphore = self.u64(self.u64(info + 24))
        previous = self.present_semaphores.setdefault((swapchain, index), semaphore)
        assert previous == semaphore and list(self.present_semaphores.values()).count(semaphore) == 1, "images share a present semaphore"
        if self.fence_unwaited:
            self.presented_unwaited += 1
        chain = self.swapchains[swapchain]
        image = self.images[chain["images"][index]]
        assert image["layout"] == LAYOUT_PRESENT_SRC, "presented image is not in PRESENT_SRC"
        assert image["pixels"] is not None, "presented image was never written"
        self.frames.append((chain["extent"], image["pixels"]))
        image["layout"] = LAYOUT_UNDEFINED
        if self.out_of_date_once:
            self.out_of_date_once = False
            return ERROR_OUT_OF_DATE
        # Like Android: a preTransform other than the display's works, but
        # every present says it is suboptimal.
        if chain["transform"] != self.transform:
            self.suboptimal += 1
            return SUBOPTIMAL
        return 0


class App(Framework):
    """The framework model with a periodic timerfd and the mock driver."""

    def __init__(self, library_path, available=True, verbose=False, font=FONT, jni=None, choreographer=True,
                 peak_rate=60):
        super().__init__(library_path, verbose=verbose, jni=jni)
        self.timer_interval = 0
        self.vulkan = MockVulkan(self, available)
        # libandroid.so's AChoreographer (API 24+): frame callbacks run at
        # the next vsync after they were posted.
        self.choreographer = choreographer
        self.frame_callbacks = []
        # ANativeWindow_setFrameRate: what the app asked for, and the most
        # the user allows (a Pixel's "Smooth Display": 120 on, 60 off).
        self.peak_rate = peak_rate
        self.frame_rate_requests = []
        self.vsync = VSYNC
        vulkan_dlopen, vulkan_dlsym = self.vulkan.dlopen, self.vulkan.dlsym

        def dlopen(process, name, flags, *_):
            if process.cstring(name) == "libandroid.so":
                return LIBANDROID if choreographer else 0
            return vulkan_dlopen(process, name, flags)

        def dlsym(process, library, name, *_):
            if library != LIBANDROID:
                return vulkan_dlsym(process, library, name)
            symbol = process.cstring(name)
            if symbol == "AChoreographer_getInstance":
                return self.vulkan.stub(symbol, lambda p, *_: CHOREOGRAPHER)
            if symbol == "AChoreographer_postFrameCallback":
                return self.vulkan.stub(symbol, self._post_frame_callback)
            if symbol == "ANativeWindow_setFrameRate":
                return self.vulkan.stub(symbol, self._set_frame_rate)
            return 0
        self.lib.imports.update({"dlopen": dlopen, "dlsym": dlsym})

        # clock_gettime (a syscall): the model's time.
        def clock_gettime(clock, spec, *_):
            self.lib.write(spec, struct.pack("<qq", self.now // 1_000_000_000, self.now % 1_000_000_000))
            return 0
        self.lib.sys_113 = clock_gettime
        # /system/fonts/* is the test font (or missing, with font=None).
        lib = self.lib
        original = lib.sys_56

        def openat(dirfd, path, flags, mode, *rest):
            name = lib.cstring(path)
            if name.startswith("/system/fonts/"):
                if font is None:
                    return -2  # ENOENT
                return os.open(font, os.O_RDONLY)
            return original(dirfd, path, flags, mode, *rest)
        lib.sys_56 = openat

    def _set_timer(self, process, fd, flags, new, old, *_):
        interval = struct.unpack("<qq", process.read(new, 16))
        value = struct.unpack("<qq", process.read(new + 16, 16))
        self.timer_interval = interval[0] * 1_000_000_000 + interval[1]
        first = value[0] * 1_000_000_000 + value[1]
        self.timer_deadline = None if first == 0 else self.now + first
        self.timer_expired = False
        return 0

    def _set_frame_rate(self, process, window, compatibility, *_):
        rate = struct.unpack("<f", struct.pack("<I", process.uc.reg_read(UC_ARM64_REG_S0) & 0xFFFFFFFF))[0]
        assert window == WINDOW, "frame rate for another window"
        self.frame_rate_requests.append((rate, compatibility & 0xFF))
        self.vsync = VSYNC_120 if rate >= 120 and self.peak_rate >= 120 else VSYNC
        return 0

    def _post_frame_callback(self, process, instance, callback, data, *_):
        assert instance == CHOREOGRAPHER, "frame callback on an unknown choreographer"
        self.frame_callbacks.append((callback, data))

    def advance(self, until):
        """Runs the timer and the vsync frame callbacks due up to `until`,
        in time order."""
        while True:
            vsync = (self.now // self.vsync + 1) * self.vsync if self.frame_callbacks else None
            due = [t for t in (self.timer_deadline, vsync) if t is not None and t <= until]
            if not due:
                break
            self.now = min(due)
            if self.now == vsync:
                callbacks, self.frame_callbacks = self.frame_callbacks, []
                for callback, data in callbacks:
                    self.lib.call_address(callback, self.now, data, name="frame")
                continue
            self.timer_deadline = self.now + self.timer_interval if self.timer_interval else None
            self.timer_expired = True
            callback, data = self.fd_callbacks[TIMER_FD]
            keep = self.lib.call_address(callback, TIMER_FD, 1, data, name="timer")
            assert keep & 0xFFFFFFFF == 1, "timer callback unregistered itself"
        self.now = max(self.now, until)

    def messages(self):
        return [text for tag, text in self.logs if tag == "mlx"]


def swap_red_blue(color):
    return (color & 0xFF00FF00) | ((color & 255) << 16) | ((color >> 16) & 255)


def blend(under, color):
    """The text shader at full coverage (std.ui.fillRect)."""
    alpha = ((color >> 24) * 255 + 127) // 255
    result = 0xFF000000
    for shift in (0, 8, 16):
        result |= ((((color >> shift) & 255) * alpha + ((under >> shift) & 255) * (255 - alpha) + 127) // 255) << shift
    return result


# Whether each checked label fitted its frame (and so had its centering
# checked).
centered = []


def check_frame(frame, time, pointer=None, pressed=False, label=True, insets=(0, 0, 0, 0), background=None, app=None):
    """Under the reserved insets (top, right, bottom, left) only the
    background, or, outside the `background` insets (a transparent gesture
    bar), the plain pattern; in the free area the pattern, except the
    label's ink in its rows at the top center and the buttons' inside their
    rectangles (as `app` last logged them) at the bottom center."""
    (width, height), pixels = frame
    flags = 4 | (1 if pointer else 0) | (2 if pressed else 0)
    px, py = pointer or (0, 0)
    top, right, bottom, left = insets
    painted_top, painted_right, painted_bottom, painted_left = insets if background is None else background
    background = swap_red_blue(BACKGROUND_COLOR)
    label_end = top + LABEL_ROWS
    rects = [r for r in (buttons(app) if app is not None and label else ()) if r[2] > 0 and r[3] > 0]

    def on_button(x, y):
        return any(bx <= x < bx + bw and by <= y < by + bh for bx, by, bw, bh in rects)
    buttons_start = min((r[1] for r in rects), default=height)
    ink, button_ink = [], []
    for y in range(height):
        for x in range(width):
            value = pixels[y * width + x]
            expected = pattern(x, y, width, time, px, py, flags)
            if not (left <= x < width - right and top <= y < height - bottom):
                if not (painted_left <= x < width - painted_right and painted_top <= y < height - painted_bottom):
                    assert value == background, f"frame at time {time}: pixel ({x}, {y}) under the system bars is {value:#010x}, not the background"
                else:
                    assert value == expected, f"frame at time {time}: pixel ({x}, {y}) behind the transparent gesture bar is {value:#010x}, not the content {expected:#010x}"
                continue
            if value != expected:
                assert label and (y < label_end or on_button(x, y)), f"frame at time {time}: pixel ({x}, {y}) is {value:#010x}, expected {expected:#010x}"
                (button_ink if on_button(x, y) else ink).append((x, y))
    if label:
        # Where the label's rows and the buttons' overlap (a frame too small
        # for both) their ink cannot be told apart.
        apart = label_end <= buttons_start
        if not apart:
            assert len(ink) + len(button_ink) >= 20, f"frame at time {time}: no label or buttons"
            centered.append(False)
            return
        assert len(ink) >= 20, f"frame at time {time}: no label ({len(ink)} pixels)"
        assert len(button_ink) >= 20, f"frame at time {time}: no buttons ({len(button_ink)} pixels)"
        xs = [x for x, _ in ink]
        ys = [y for _, y in ink]
        assert min(ys) > top, f"frame at time {time}: the label touches the top inset (row {min(ys)})"
        assert max(y for _, y in button_ink) < height - bottom - 1, f"frame at time {time}: the buttons touch the bottom inset"
        # Centered in the free area (the label's shadow adds 2 on the right)
        # when it fits. A label wider than the free area is centered too, but
        # clipped at both edges, so its ink no longer shows where the middle is.
        fits = min(xs) > left and max(xs) < width - right - 1
        if fits:
            assert abs((min(xs) + max(xs) - 1) - (left + width - right)) <= 3, f"frame at time {time}: label spans x {min(xs)}..{max(xs)}, not centered in {left}..{width - right}"
            button_left = min(r[0] for r in rects)
            button_right = max(r[0] + r[2] for r in rects)
            assert abs((button_left + button_right) - (left + width - right)) <= 1, f"frame at time {time}: buttons span x {button_left}..{button_right}, not centered in {left}..{width - right}"
        centered.append(fits)


BUTTONS = re.compile(r"ui: buttons: fullscreen (\d+) (\d+) (\d+) (\d+), navigation bar (\d+) (\d+) (\d+) (\d+), "
                     r"gesture bar (\d+) (\d+) (\d+) (\d+)")


def buttons(app):
    """The switch buttons' rectangles (x, y, width, height), as last logged."""
    found = [BUTTONS.fullmatch(text) for text in app.messages()]
    numbers = [int(n) for n in [match for match in found if match][-1].groups()]
    return tuple(numbers[:4]), tuple(numbers[4:8]), tuple(numbers[8:])


def tap(app, button, at):
    """Taps the middle of `button`; returns where."""
    x, y = button[0] + button[2] // 2, button[1] + button[3] // 2
    app.touch(ACTION_DOWN, x, y, at)
    app.touch(ACTION_UP, x, y, at + MS)
    return (x, y)


def scenario_render(library, spirv_val):
    # A phone in portrait: status bar at the top (as tall as the cutout
    # under it), gesture bar at the bottom.
    insets = (8, 0, 6, 0)
    app = App(library, jni={"sdk": 34, "insets": insets, "cutout": (8, 0, 0, 0), "portrait_status_bar": 7})
    vulkan = app.vulkan
    app.create()
    assert "vulkan: device ready" in app.messages(), app.messages()
    # Each setup step is logged, so a device that crashes shows where.
    for step in ("vulkan: onCreate", "vkCreateInstance", "vkCreateDevice", "vkCreateComputePipelines",
                 "vulkan: device Mock Android GPU, Vulkan 1.1.0"):
        assert step in app.messages(), (step, app.messages())
    assert vulkan.instance_extensions == ["VK_KHR_surface", "VK_KHR_android_surface"], vulkan.instance_extensions
    assert vulkan.device_extensions == ["VK_KHR_swapchain"], vulkan.device_extensions
    assert vulkan.push_size == 24, vulkan.push_size
    assert len(vulkan.shaders) == 1
    if spirv_val:
        with tempfile.NamedTemporaryFile(suffix=".spv") as shader:
            shader.write(vulkan.shaders[0])
            shader.flush()
            result = subprocess.run([spirv_val, "--target-env", "vulkan1.1", shader.name], capture_output=True, text=True)
            assert result.returncode == 0, result.stdout + result.stderr
    app.show_window()
    app.attach_input()
    assert "vulkan: swapchain created" in app.messages()
    # The background under the two bars, the label's shadow and the label,
    # and the three buttons (background and text) as far as they are visible:
    # this frame is too small for their column.
    visible = sum(1 for button in buttons(app) if button[2] > 0 and button[3] > 0)
    assert "text: font loaded" in app.messages() and vulkan.text_draws == 4 + 2 * visible, (app.messages(), vulkan.text_draws)
    assert "ui: reserved for the system bars: top 8 right 0 bottom 6 left 0" in app.messages(), app.messages()
    logged = len(app.messages())
    assert len(vulkan.frames) == 1, "no frame on window creation"
    # Frames follow the display's vsync; no timer runs.
    assert "vulkan: frames follow the display (Choreographer)" in app.messages() and app.timer_deadline is None
    check_frame(vulkan.frames[0], 0, insets=insets, app=app)
    # The timer presents a frame every 16.7 ms; time advances by 2 a frame.
    app.advance(51 * MS)
    assert len(vulkan.frames) == 4, len(vulkan.frames)
    assert len(app.messages()) == logged, "steps are still logged after the first frame"
    # Every frame is presented before the app waits for the GPU to finish it.
    assert vulkan.presented_unwaited == len(vulkan.frames), (vulkan.presented_unwaited, len(vulkan.frames))
    for index, frame in enumerate(vulkan.frames):
        check_frame(frame, index * 2, insets=insets, app=app)
    # Touch: the ring follows the finger, white while down, amber after
    # (below the buttons, which fill most of this small frame).
    # (Timer ticks land at multiples of 16.67 ms.)
    app.touch(ACTION_DOWN, 50, 54, 52 * MS)
    app.advance(70 * MS)
    check_frame(vulkan.frames[-1], (len(vulkan.frames) - 1) * 2, (50, 54), pressed=True, insets=insets, app=app)
    app.touch(ACTION_MOVE, 20.7, 55.2, 71 * MS)
    app.touch(ACTION_UP, 20.7, 55.2, 72 * MS)
    app.advance(90 * MS)
    check_frame(vulkan.frames[-1], (len(vulkan.frames) - 1) * 2, (20, 55), pressed=False, insets=insets, app=app)
    # An out-of-date swapchain is recreated on the same surface; that frame
    # does not count, so the next one repeats its time.
    swapchains_before = set(vulkan.swapchains)
    vulkan.out_of_date_once = True
    app.advance(110 * MS)
    assert set(vulkan.swapchains) != swapchains_before and len(vulkan.swapchains) == 1, "swapchain not recreated"
    presented = len(vulkan.frames)
    app.advance(130 * MS)
    assert len(vulkan.frames) == presented + 1, "no frame after recreating the swapchain"
    check_frame(vulkan.frames[-1], (len(vulkan.frames) - 2) * 2, (20, 55), insets=insets, app=app)
    # Rotation: the window is resized and the swapchain follows.
    vulkan.extent = (64, 96)
    app.callback("onNativeWindowResized", WINDOW)
    assert vulkan.frames[-1][0] == (64, 96), vulkan.frames[-1][0]
    check_frame(vulkan.frames[-1], (len(vulkan.frames) - 2) * 2, (20, 55), insets=insets, app=app)
    # The bars change after a layout pass (say, the status bar grows):
    # the next frame follows; a layout pass that changes nothing draws no
    # extra frame.
    app.jni.insets = insets = (12, 0, 4, 0)
    presented = len(vulkan.frames)
    rect = app.malloc(16)
    app.callback("onContentRectChanged", rect)
    assert len(vulkan.frames) == presented + 1, "no frame for the new insets"
    check_frame(vulkan.frames[-1], (len(vulkan.frames) - 2) * 2, (20, 55), insets=insets, app=app)
    app.callback("onContentRectChanged", rect)
    assert len(vulkan.frames) == presented + 1, "a frame for unchanged insets"
    assert app.jni.frames == 0, "JNI local frames left pushed"
    # Going to the background: swapchain, surface and timer go away.
    app.callback("onNativeWindowDestroyed", WINDOW)
    assert not vulkan.swapchains and not vulkan.surfaces, "swapchain or surface left after the window was destroyed"
    presented = len(vulkan.frames)
    app.advance(300 * MS)
    assert len(vulkan.frames) == presented, "frames presented without a window"
    # Back: a new surface and swapchain.
    vulkan.extent = (96, 64)
    app.show_window()
    assert len(vulkan.frames) == presented + 1 and len(vulkan.surfaces) == 1
    app.callback("onDestroy")
    assert getattr(vulkan, "device_destroyed", False) and getattr(vulkan, "instance_destroyed", False)
    assert not app.lib.missing, f"unbound imports: {sorted(app.lib.missing)}"
    return len(vulkan.frames)


def scenario_landscape(library):
    """A portrait phone held in landscape: currentTransform is ROTATE_90,
    currentExtent landscape. Every present is suboptimal; that alone must not
    rebuild the swapchain, a size change still must. (Rotating restarts the
    activity: its manifest does not handle orientation changes.)"""
    # API 29 (getSystemWindowInset*), and no WindowInsets until the first
    # layout pass.
    # Without a Choreographer (as below API 24) a 60 Hz timer paces frames.
    app = App(library, jni={"sdk": 29, "insets": None, "cutout": (0, 20, 0, 0), "status_bar": 16}, choreographer=False)
    vulkan = app.vulkan
    vulkan.transform = TRANSFORM_ROTATE_90
    # Wide enough for the whole label and a row of buttons, tall enough for
    # both.
    vulkan.extent = (640, 120)
    app.create()
    app.show_window()
    app.advance(51 * MS)
    assert "vulkan: frames follow a 60 Hz timer" in app.messages() and app.timer_deadline is not None
    assert len(vulkan.frames) == 4 and vulkan.suboptimal == 4, (len(vulkan.frames), vulkan.suboptimal)
    assert vulkan.swapchains_created == 1, f"{vulkan.swapchains_created} swapchains for one window"
    for index, frame in enumerate(vulkan.frames):
        assert frame[0] == (640, 120), frame[0]
        del centered[:]
        check_frame(frame, index * 2, app=app)
        assert centered == [True], "the label does not fit a 480-pixel frame"
    # Laid out: the status bar on top, a three-button navigation bar on the
    # left, the gesture area at the bottom, the camera cutout on the right.
    # The top band keeps its portrait height: the cutout's depth (20), more
    # than the landscape status bar (10) and status_bar_height (16); the
    # cutout's side is not reserved.
    app.jni.insets = (10, 0, 6, 24)
    app.callback("onContentRectChanged", app.malloc(16))
    insets = app.jni.reserved()
    assert insets == (20, 0, 6, 24), insets
    assert "ui: reserved for the system bars: top 20 right 0 bottom 6 left 24" in app.messages(), app.messages()
    del centered[:]
    check_frame(vulkan.frames[-1], (len(vulkan.frames) - 1) * 2, insets=insets, app=app)
    assert centered == [True], "the label does not fit the free part of a 480-pixel frame"
    # Fullscreen on API 29: the decor view's system UI flags hide both bars
    # (immersive sticky), and nothing is reserved any more.
    app.attach_input()
    fullscreen_button, _, _ = buttons(app)
    pointer = tap(app, fullscreen_button, 60 * MS)
    assert app.jni.bar_calls == [("setSystemUiVisibility", 0x100 | 0x200 | 0x400 | 0x1000 | 4 | 2)], app.jni.bar_calls
    app.advance(100 * MS)
    assert app.jni.reserved() == (0, 0, 0, 0) and "ui: reserved for the system bars: top 0 right 0 bottom 0 left 0" in app.messages()
    check_frame(vulkan.frames[-1], (len(vulkan.frames) - 1) * 2, pointer, insets=(0, 0, 0, 0), app=app)
    # And back: the bars and the reserved space return.
    fullscreen_button, _, _ = buttons(app)
    app.jni.bar_calls.clear()
    pointer = tap(app, fullscreen_button, 110 * MS)
    assert app.jni.bar_calls == [("setSystemUiVisibility", 0x100 | 0x200 | 0x400 | 0x1000)], app.jni.bar_calls
    app.advance(150 * MS)
    assert app.jni.reserved() == insets
    check_frame(vulkan.frames[-1], (len(vulkan.frames) - 1) * 2, pointer, insets=insets, app=app)
    # The window changes size without a resize callback: the suboptimal
    # present notices and the next frame has the new size.
    vulkan.extent = (64, 96)
    app.advance(170 * MS)
    assert vulkan.swapchains_created == 2, vulkan.swapchains_created
    app.advance(190 * MS)
    assert vulkan.frames[-1][0] == (64, 96), vulkan.frames[-1][0]
    check_frame(vulkan.frames[-1], (len(vulkan.frames) - 2) * 2, pointer, insets=insets, app=app)
    assert vulkan.swapchains_created == 2, vulkan.swapchains_created
    app.callback("onNativeWindowDestroyed", WINDOW)
    app.callback("onDestroy")
    return len(vulkan.frames)


def scenario_smooth_display(library):
    """With "Smooth Display" on (the user allows 120 Hz) the app's request
    for 120 frames a second makes the display, and so the frame clock, run
    at 120 Hz: 120 frames a second."""
    app = App(library, jni={"sdk": 34, "insets": (22, 0, 6, 0)}, peak_rate=120)
    vulkan = app.vulkan
    vulkan.extent = (640, 160)
    app.create()
    app.show_window()
    assert app.frame_rate_requests == [(120.0, 0)], app.frame_rate_requests
    assert "vulkan: asked for 120 Hz" in app.messages(), app.messages()
    app.advance(1100 * MS)
    assert "perf: 120 FPS, 0.0 ms per frame" in app.messages(), [m for m in app.messages() if m.startswith("perf")]
    assert len(vulkan.frames) >= 130, len(vulkan.frames)
    del centered[:]
    check_frame(vulkan.frames[-1], (len(vulkan.frames) - 1) * 2, insets=(22, 0, 6, 0), app=app)
    assert centered == [True]
    app.callback("onNativeWindowDestroyed", WINDOW)
    app.callback("onDestroy")
    return len(vulkan.frames)


def scenario_fullscreen(library):
    """Fullscreen: nothing is reserved, so the whole frame is the pattern
    and the label sits at the top center of the whole window, as if there
    were no system bars; their insets are not even asked for."""
    app = App(library, jni={"sdk": 34, "insets": (8, 0, 6, 0), "fullscreen": True})
    vulkan = app.vulkan
    vulkan.extent = (640, 120)
    app.create()
    app.show_window()
    app.advance(40 * MS)
    assert "ui: fullscreen, nothing reserved" in app.messages(), app.messages()
    assert app.jni.queries == 0 and app.jni.frames == 0, (app.jni.queries, app.jni.frames)
    # Shadow, label and the three buttons only: no background bands.
    assert vulkan.text_draws == 8 * len(vulkan.frames), (vulkan.text_draws, len(vulkan.frames))
    for index, frame in enumerate(vulkan.frames):
        del centered[:]
        check_frame(frame, index * 2, app=app)
        assert centered == [True], "the label is not centered in the whole window"
    # A layout pass changes nothing.
    presented = len(vulkan.frames)
    app.callback("onContentRectChanged", app.malloc(16))
    assert len(vulkan.frames) == presented
    app.callback("onNativeWindowDestroyed", WINDOW)
    app.callback("onDestroy")
    return len(vulkan.frames)


def scenario_switches(library):
    """The switch buttons on API 34: a transparent gesture bar shows the
    content behind the navigation bar, hiding the navigation bar frees the
    bottom, fullscreen hides both bars (and the top band with the status
    bar), and turning fullscreen off again keeps the navigation bar hidden
    until its own switch shows it. The window's WindowInsetsController does
    it (bars shown again by a swipe), and the reserved space follows within
    a few frames without a layout callback."""
    app = App(library, jni={"sdk": 34, "insets": (10, 0, 6, 0), "cutout": (0, 0, 0, 22),
                            "portrait_status_bar": 18})
    vulkan = app.vulkan
    vulkan.extent = (640, 160)
    app.create()
    app.show_window()
    app.attach_input()
    app.advance(40 * MS)
    insets = app.jni.reserved()
    assert insets == (22, 0, 6, 0), insets
    del centered[:]
    check_frame(vulkan.frames[-1], (len(vulkan.frames) - 1) * 2, insets=insets, app=app)
    assert centered == [True]
    # The gesture bar's background: transparent shows the content behind
    # the navigation bar (still reserved: the buttons stay above it), and
    # back. No system bar changes.
    for step, (message, painted) in enumerate((("ui: gesture bar transparent", (22, 0, 0, 0)),
                                               ("ui: gesture bar solid", (22, 0, 6, 0)))):
        _, _, gesture_button = buttons(app)
        pointer = tap(app, gesture_button, (50 + 30 * step) * MS)
        assert message in app.messages() and app.jni.bar_calls == [], (app.messages(), app.jni.bar_calls)
        app.advance((70 + 30 * step) * MS)
        del centered[:]
        check_frame(vulkan.frames[-1], (len(vulkan.frames) - 1) * 2, pointer, insets=insets, background=painted, app=app)
        assert centered == [True]
    now = 120
    steps = (("navigation", [("setSystemBarsBehavior", 2), ("hide", 2), ("show", 1)], (22, 0, 0, 0),
              "ui: navigation bar hidden"),
             ("fullscreen", [("setSystemBarsBehavior", 2), ("hide", 3)], (0, 0, 0, 0), "ui: fullscreen on"),
             ("fullscreen", [("setSystemBarsBehavior", 2), ("hide", 2), ("show", 1)], (22, 0, 0, 0),
              "ui: fullscreen off"),
             ("navigation", [("setSystemBarsBehavior", 2), ("show", 3)], (22, 0, 6, 0), "ui: navigation bar shown"))
    for which, calls, expected, message in steps:
        fullscreen_button, navigation_button, _ = buttons(app)
        app.jni.bar_calls.clear()
        pointer = tap(app, fullscreen_button if which == "fullscreen" else navigation_button, now * MS)
        assert app.jni.bar_calls == calls, (which, app.jni.bar_calls)
        assert message in app.messages(), (message, app.messages())
        # Two frames: one to notice, one drawn with the new space.
        app.advance((now + 40) * MS)
        now += 50
        assert app.jni.reserved() == expected, app.jni.reserved()
        del centered[:]
        check_frame(vulkan.frames[-1], (len(vulkan.frames) - 1) * 2, pointer, insets=expected, app=app)
        assert centered == [True], which
    assert app.jni.frames == 0, "JNI local frames left pushed"
    # After a second the frame counter appears under the label and in the
    # log: 60 frames a second, no time spent (the model's clock stands still
    # during a call).
    app.advance(1100 * MS)
    assert "perf: 60 FPS, 0.0 ms per frame" in app.messages(), [m for m in app.messages() if m.startswith("perf")]
    # "Smooth Display" off: the request for 120 Hz is made, the display stays at 60.
    assert app.frame_rate_requests == [(120.0, 0)] and app.vsync == VSYNC, app.frame_rate_requests
    app.advance(1120 * MS)
    del centered[:]
    check_frame(vulkan.frames[-1], (len(vulkan.frames) - 1) * 2, pointer, insets=(22, 0, 6, 0), app=app)
    assert centered == [True]
    (width, _), pixels = vulkan.frames[-1]
    rows = [y for y in range(22, 22 + LABEL_ROWS) if any(pixels[y * width + x] == 0xFFFFFFFF for x in range(width))]
    assert rows and rows[-1] - rows[0] > 20, f"one line of text, not two (rows {rows[:1]}..{rows[-1:]})"
    app.callback("onNativeWindowDestroyed", WINDOW)
    app.callback("onDestroy")
    return len(vulkan.frames)


def scenario_no_font(library):
    # Still the background under the bars, drawn by the text shader
    # without an atlas.
    insets = (8, 0, 6, 0)
    app = App(library, font=None, jni={"sdk": 34, "insets": insets})
    app.create()
    app.show_window()
    app.advance(40 * MS)
    assert "text: no system font (no label)" in app.messages(), app.messages()
    assert len(app.vulkan.frames) == 3 and app.vulkan.text_draws == 6, (len(app.vulkan.frames), app.vulkan.text_draws)
    for index, frame in enumerate(app.vulkan.frames):
        check_frame(frame, index * 2, label=False, insets=insets, app=app)
    app.callback("onNativeWindowDestroyed", WINDOW)
    app.callback("onDestroy")


def scenario_no_vulkan(library):
    app = App(library, available=False, jni={"sdk": 34, "insets": (8, 0, 6, 0)})
    app.create()
    app.show_window()
    app.attach_input()
    app.touch(ACTION_DOWN, 10, 10, 5 * MS)
    app.touch(ACTION_UP, 10, 10, 6 * MS)
    app.advance(100 * MS)
    app.callback("onNativeWindowDestroyed", WINDOW)
    app.callback("onDestroy")
    assert "vulkan: libvulkan.so is not available" in app.messages(), app.messages()
    assert not app.vulkan.frames


def main():
    arguments = sys.argv[1:]
    verbose = "-v" in arguments
    arguments = [a for a in arguments if a != "-v"]
    with tempfile.TemporaryDirectory() as tmp:
        path = arguments[0] if arguments else None
        if path is None:
            path = os.path.join(tmp, "libmain.so")
            subprocess.run([os.path.join(ROOT, "zig-out/bin/mlx1"), "examples/vulkan-android/main.mlx", "-o", path,
                            "--target=aarch64-android", "--quiet"], cwd=ROOT, check=True)
        if path.endswith(".apk"):
            with zipfile.ZipFile(path) as archive:
                library = os.path.join(tmp, "libmain.so")
                with open(library, "wb") as out:
                    out.write(archive.read("lib/arm64-v8a/libmain.so"))
            path = library
        spirv_val = shutil.which("spirv-val")
        frames = scenario_render(path, spirv_val)
        print(f"ok   render: {frames} frames checked pixel by pixel, one per vsync, presented before the GPU wait (system bar insets, touch, out-of-date, resize, new insets, background)"
              + ("" if spirv_val else " [spirv-val not installed]"))
        frames = scenario_landscape(path)
        print(f"ok   landscape (ROTATE_90, API 29 insets, 60 Hz timer): {frames} frames, upright with an IDENTITY swapchain, no rebuild per suboptimal present")
        frames = scenario_switches(path)
        print(f"ok   switches: {frames} frames, navigation bar and fullscreen toggled at runtime, the reserved space following; 60 FPS counter")
        frames = scenario_smooth_display(path)
        print(f"ok   smooth display: {frames} frames in 1.1 s, 120 FPS after asking for 120 Hz (60 with the setting off)")
        frames = scenario_fullscreen(path)
        print(f"ok   fullscreen: {frames} frames, nothing reserved, the label at the top center of the whole window")
        scenario_no_font(path)
        print("ok   no system font: frames without a label")
        scenario_no_vulkan(path)
        print("ok   no libvulkan.so: logged, input and window callbacks still work")
    return 0


if __name__ == "__main__":
    sys.exit(main())
