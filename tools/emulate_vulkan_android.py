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
  - without libvulkan.so the app logs it and keeps running.

The mock "GPU" runs the pattern shader's formula (the shader itself is
checked against that formula on a real driver by
tests/258_vulkan_swapchain_runtime.mlx and tests/257_vulkan_sharing_runtime.mlx).

Usage: python3 tools/emulate_vulkan_android.py [-v] [app.apk | libmain.so]
With no file, builds examples/vulkan-android/main.mlx with zig-out/bin/mlx1.
"""
import os
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
from unicorn.arm64_const import UC_ARM64_REG_SP  # noqa: E402

LIBVULKAN = 0x5800_0000_0001
FORMAT_R8G8B8A8_UNORM = 37
LAYOUT_UNDEFINED, LAYOUT_TRANSFER_DST, LAYOUT_PRESENT_SRC = 0, 7, 1000001002
ERROR_OUT_OF_DATE = (-1000001004) & 0xFFFFFFFF
COMPOSITE_ALPHA_INHERIT = 8
ACTION_DOWN, ACTION_UP, ACTION_MOVE = 0, 1, 2
MS = 1_000_000


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
        self.out_of_date_once = False
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
        for index in range(count):
            self.put_u64(pipelines + 8 * index, self.handle())
        return 0

    def nothing(self, process, *_):
        return 0

    vk_DestroyShaderModule = vk_DestroyFence = vk_DestroyCommandPool = vk_DestroyDescriptorPool = nothing
    vk_DestroySemaphore = vk_DeviceWaitIdle = vk_WaitForFences = vk_ResetFences = nothing
    vk_ResetDescriptorPool = vk_BeginCommandBuffer = vk_EndCommandBuffer = vk_UnmapMemory = nothing
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
        self.lib.write(capabilities, struct.pack("<IIIIIIIIIIIII", 2, 3, width, height, 1, 1, 4096, 4096, 1,
                                                 1, 1, COMPOSITE_ALPHA_INHERIT, 2 | 16))
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
        alpha = self.u32(info + 84)
        assert surface in self.surfaces, "swapchain on a destroyed surface"
        assert image_format == FORMAT_R8G8B8A8_UNORM, image_format
        assert (width, height) == self.extent, ((width, height), self.extent)
        assert usage & 2, "swapchain images are not transfer destinations"
        assert alpha == COMPOSITE_ALPHA_INHERIT, alpha
        handle = self.handle()
        images = []
        for _ in range(3):
            image = self.handle()
            self.images[image] = {"layout": LAYOUT_UNDEFINED, "pixels": None, "size": (width, height)}
            images.append(image)
        self.swapchains[handle] = {"images": images, "next": 0, "extent": (width, height)}
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
        chain = self.swapchains[swapchain]
        image = self.images[chain["images"][index]]
        assert image["layout"] == LAYOUT_PRESENT_SRC, "presented image is not in PRESENT_SRC"
        assert image["pixels"] is not None, "presented image was never written"
        self.frames.append((chain["extent"], image["pixels"]))
        image["layout"] = LAYOUT_UNDEFINED
        if self.out_of_date_once:
            self.out_of_date_once = False
            return ERROR_OUT_OF_DATE
        return 0


class App(Framework):
    """The framework model with a periodic timerfd and the mock driver."""

    def __init__(self, library_path, available=True, verbose=False):
        super().__init__(library_path, verbose=verbose)
        self.timer_interval = 0
        self.vulkan = MockVulkan(self, available)

    def _set_timer(self, process, fd, flags, new, old, *_):
        interval = struct.unpack("<qq", process.read(new, 16))
        value = struct.unpack("<qq", process.read(new + 16, 16))
        self.timer_interval = interval[0] * 1_000_000_000 + interval[1]
        first = value[0] * 1_000_000_000 + value[1]
        self.timer_deadline = None if first == 0 else self.now + first
        self.timer_expired = False
        return 0

    def advance(self, until):
        while self.timer_deadline is not None and self.timer_deadline <= until:
            self.now = self.timer_deadline
            self.timer_deadline = self.now + self.timer_interval if self.timer_interval else None
            self.timer_expired = True
            callback, data = self.fd_callbacks[TIMER_FD]
            keep = self.lib.call_address(callback, TIMER_FD, 1, data, name="timer")
            assert keep & 0xFFFFFFFF == 1, "timer callback unregistered itself"
        self.now = max(self.now, until)

    def messages(self):
        return [text for tag, text in self.logs if tag == "mlx"]


def check_frame(frame, time, pointer=None, pressed=False):
    (width, height), pixels = frame
    flags = 4 | (1 if pointer else 0) | (2 if pressed else 0)
    px, py = pointer or (0, 0)
    for y in range(height):
        for x in range(width):
            expected = pattern(x, y, width, time, px, py, flags)
            if pixels[y * width + x] != expected:
                raise AssertionError(f"frame at time {time}: pixel ({x}, {y}) is {pixels[y * width + x]:#010x}, expected {expected:#010x}")


def scenario_render(library, spirv_val):
    app = App(library)
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
    logged = len(app.messages())
    assert len(vulkan.frames) == 1, "no frame on window creation"
    check_frame(vulkan.frames[0], 0)
    # The timer presents a frame every 16.7 ms; time advances by 2 a frame.
    app.advance(51 * MS)
    assert len(vulkan.frames) == 4, len(vulkan.frames)
    assert len(app.messages()) == logged, "steps are still logged after the first frame"
    for index, frame in enumerate(vulkan.frames):
        check_frame(frame, index * 2)
    # Touch: the ring follows the finger, white while down, amber after.
    # (Timer ticks land at multiples of 16.67 ms.)
    app.touch(ACTION_DOWN, 50, 30, 52 * MS)
    app.advance(70 * MS)
    check_frame(vulkan.frames[-1], (len(vulkan.frames) - 1) * 2, (50, 30), pressed=True)
    app.touch(ACTION_MOVE, 20.7, 40.2, 71 * MS)
    app.touch(ACTION_UP, 20.7, 40.2, 72 * MS)
    app.advance(90 * MS)
    check_frame(vulkan.frames[-1], (len(vulkan.frames) - 1) * 2, (20, 40), pressed=False)
    # An out-of-date swapchain is recreated on the same surface; that frame
    # does not count, so the next one repeats its time.
    swapchains_before = set(vulkan.swapchains)
    vulkan.out_of_date_once = True
    app.advance(110 * MS)
    assert set(vulkan.swapchains) != swapchains_before and len(vulkan.swapchains) == 1, "swapchain not recreated"
    presented = len(vulkan.frames)
    app.advance(130 * MS)
    assert len(vulkan.frames) == presented + 1, "no frame after recreating the swapchain"
    check_frame(vulkan.frames[-1], (len(vulkan.frames) - 2) * 2, (20, 40))
    # Rotation: the window is resized and the swapchain follows.
    vulkan.extent = (64, 96)
    app.callback("onNativeWindowResized", WINDOW)
    assert vulkan.frames[-1][0] == (64, 96), vulkan.frames[-1][0]
    check_frame(vulkan.frames[-1], (len(vulkan.frames) - 2) * 2, (20, 40))
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


def scenario_no_vulkan(library):
    app = App(library, available=False)
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
        print(f"ok   render: {frames} frames checked pixel by pixel (touch, out-of-date, resize, background)"
              + ("" if spirv_val else " [spirv-val not installed]"))
        scenario_no_vulkan(path)
        print("ok   no libvulkan.so: logged, input and window callbacks still work")
    return 0


if __name__ == "__main__":
    sys.exit(main())
