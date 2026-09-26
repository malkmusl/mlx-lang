#!/usr/bin/env python3
"""Runs the Mlx compositor freestanding (--backend drm) against an emulated
kernel and an emulated systemd-logind, with real Wayland clients.

There is no GPU, monitor or input device here, so this plays the parts the
compositor talks to, at the lowest level it uses:

- Device nodes are real (mknod: DRM 226:0, evdev 13:64...) so the compositor
  stats them for their numbers as usual.
- logind runs on a private dbus-daemon (Gio; DBUS_SYSTEM_BUS_ADDRESS points
  the compositor there): GetSession, TakeControl, TakeDevice (handing out
  descriptors), ReleaseDevice, PauseDeviceComplete, ReleaseControl, the
  seat's SwitchTo, and the PauseDevice/ResumeDevice signals of a VT switch.
- The descriptors it hands out are Unix sockets speaking device.mlx's frame
  protocol, so every ioctl arrives here with its request number and
  argument bytes. The DRM card checks them against <drm/drm_mode.h>'s
  layouts and follows the pointers inside them (arrays the kernel fills or
  reads) in the compositor's memory through /proc/PID/mem, as the kernel
  would. Dumb buffers are memfds, so the frames the compositor shows can be
  read back. Input devices answer the EVIOCG* queries and send
  struct input_event records.

The scenario: start (modeset on the connected connector at its preferred
mode), the Mlx terminal maps and gets typed into from the keyboard, the
mouse and the touchpad move the cursor, a mouse plugged in later works, a
VT switch (Ctrl+Alt+F2) pauses and resumes everything, and Alt+Shift+Q
quits with the CRTC restored and every device given back.

weston-terminal, when installed, then types through the compositor's xkb
keymap and modifiers.

With --renderer vulkan the compositor composes with Vulkan (VK_DRIVER_FILES
should name lavapipe's manifest): it must export both dumb buffers as
dma-bufs (PRIME_HANDLE_TO_FD, answered with the buffer's memfd) and render
into them, so the frames read back are the GPU's. With
MLX_VULKAN_NO_HOST_IMPORT=1 in the environment it must instead render into
a buffer of its own and copy each frame in (the path drivers that cannot
import the buffers take).

Usage: fake_drm_session.py COMPOSITOR TERMINAL [--screenshot PNG] [--renderer cpu|vulkan]
"""
import os
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import zlib

import gi
gi.require_version("Gio", "2.0")
from gi.repository import Gio, GLib  # noqa: E402

# <drm/drm.h>, <drm/drm_mode.h>, <linux/input.h> on 64-bit Linux.
SET_MASTER, DROP_MASTER = 25630, 25631
GET_CAP = 3222299660
GETRESOURCES, GETCONNECTOR, GETENCODER = 3225445536, 3226494119, 3222561958
GETCRTC, SETCRTC = 3228066977, 3228066978
CREATE_DUMB, MAP_DUMB, DESTROY_DUMB = 3223348402, 3222299827, 3221513396
ADDFB, RMFB, PAGE_FLIP = 3223086254, 3221513391, 3222824112
PRIME_HANDLE_TO_FD = 3222037549
SIZES = {GET_CAP: 16, GETRESOURCES: 64, GETCONNECTOR: 80, GETENCODER: 20, GETCRTC: 104, SETCRTC: 104,
         CREATE_DUMB: 32, MAP_DUMB: 16, DESTROY_DUMB: 4, ADDFB: 28, RMFB: 4, PAGE_FLIP: 24,
         PRIME_HANDLE_TO_FD: 12}
DRM_CLOEXEC, DRM_RDWR = 0x80000000, 2
EACCES, ENOTTY, EINVAL = 13, 25, 22
DRM_MAJOR, INPUT_MAJOR = 226, 13

EV_SYN, EV_KEY, EV_REL, EV_ABS = 0, 1, 2, 3
REL_X, REL_Y, REL_WHEEL = 0, 1, 8
ABS_X, ABS_Y = 0, 1
BTN_LEFT, BTN_RIGHT, BTN_MIDDLE = 272, 273, 274
BTN_TOOL_FINGER, BTN_TOUCH, BTN_TOOL_DOUBLETAP = 325, 330, 333
KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_LEFTALT, KEY_F2, KEY_Q, KEY_ENTER = 42, 29, 56, 60, 16, 28

# The monitor: connector 42 (eDP, connected) with a preferred 1000x700 mode
# (not the first one) and no current encoder; 41 is a disconnected HDMI.
WIDTH, HEIGHT = 1000, 700
CRTCS = [31, 32]
CONNECTORS = [41, 42]
ENCODERS = [51, 52]
CONSOLE_FB = 99


def mode(width, height, refresh, preferred):
    """struct drm_mode_modeinfo (68 bytes)."""
    name = f"{width}x{height}".encode().ljust(32, b"\0")
    return struct.pack("<IHHHHHHHHHHIII", width * height * refresh // 1000, width, width + 16, width + 32,
                       width + 48, 0, height, height + 3, height + 6, height + 9, 0, refresh, 0,
                       8 if preferred else 0) + name


MODES = [mode(1280, 800, 60, False), mode(WIDTH, HEIGHT, 60, True)]
CONSOLE_MODE = mode(800, 600, 60, False)


def fail(message):
    print(f"FAIL {message}", file=sys.stderr)
    raise SystemExit(1)


class Recorder:
    """What the compositor did, with a condition to wait for things."""

    def __init__(self):
        self.lock = threading.Condition()
        self.calls = []

    def add(self, *call):
        with self.lock:
            self.calls.append(call)
            self.lock.notify_all()

    def wait(self, predicate, timeout, what):
        deadline = time.time() + timeout
        with self.lock:
            while True:
                found = [c for c in self.calls if predicate(c)]
                if found:
                    return found
                left = deadline - time.time()
                if left <= 0:
                    fail(f"timed out waiting for {what}; calls: {self.calls[-12:]}")
                self.lock.wait(left)

    def count(self, predicate):
        with self.lock:
            return len([c for c in self.calls if predicate(c)])


class DeviceServer(threading.Thread):
    """One emulated device on a socket pair: ioctl frames in, replies and
    data frames out (device.mlx's protocol)."""

    def __init__(self, harness):
        super().__init__(daemon=True)
        self.harness = harness
        self.ours, self.theirs = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
        self.send_lock = threading.Lock()
        self.closed = False

    def take(self):
        """The descriptor for the compositor (a copy; ours stays open)."""
        return os.dup(self.theirs.fileno())

    def run(self):
        buffer = b""
        while not self.closed:
            try:
                chunk = self.ours.recv(65536)
            except OSError:
                return
            if not chunk:
                return
            buffer += chunk
            while len(buffer) >= 8:
                kind, length = struct.unpack_from("<II", buffer)
                if len(buffer) < 8 + length:
                    break
                payload = buffer[8:8 + length]
                buffer = buffer[8 + length:]
                if kind != 1:
                    fail(f"unknown frame kind {kind}")
                request = struct.unpack_from("<Q", payload)[0]
                result, argument, descriptor = self.ioctl(request, bytearray(payload[8:]))
                frame = struct.pack("<IIq", 1, 8 + len(argument), result) + bytes(argument)
                with self.send_lock:
                    if descriptor is None:
                        self.ours.sendall(frame)
                    else:
                        socket.send_fds(self.ours, [frame], [descriptor])
                        os.close(descriptor)

    def send_data(self, data):
        if self.closed:
            return
        with self.send_lock:
            try:
                self.ours.sendall(struct.pack("<II", 2, len(data)) + data)
            except OSError:
                pass

    def revoke(self):
        """Like EVIOCREVOKE: the compositor's descriptor stops working."""
        self.closed = True
        try:
            self.ours.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        self.ours.close()
        self.theirs.close()


class Memory:
    """The compositor's memory, as the kernel sees it for ioctl pointers."""

    def __init__(self, pid):
        self.file = os.open(f"/proc/{pid}/mem", os.O_RDWR)

    def read(self, address, size):
        return os.pread(self.file, size, address)

    def write(self, address, data):
        os.pwrite(self.file, data, address)


class FakeCard(DeviceServer):
    def __init__(self, harness):
        super().__init__(harness)
        self.master = True
        self.buffers = {}          # handle -> [memfd, size, pitch]
        self.framebuffers = {}     # fb -> handle
        self.next_handle = 1
        self.next_fb = 101
        self.flips = 0
        self.shown = None          # framebuffer on screen

    def ioctl(self, request, argument):
        record = self.harness.record
        if request in (SET_MASTER, DROP_MASTER):
            if argument:
                fail("SET/DROP_MASTER take no argument")
            record("drm", "set_master" if request == SET_MASTER else "drop_master")
            return 0, argument, None
        expected = SIZES.get(request)
        if expected is None:
            record("drm", "unknown", request)
            return -ENOTTY, argument, None
        if len(argument) != expected or (request >> 16) & 0x3FFF != expected:
            fail(f"ioctl {request:#x}: {len(argument)} argument bytes, the kernel expects {expected}")
        memory = self.harness.memory
        if request == GET_CAP:
            capability = struct.unpack_from("<Q", argument)[0]
            struct.pack_into("<Q", argument, 8, 1 if capability == 1 else 0)
            return 0, argument, None
        if request == GETRESOURCES:
            pointers = struct.unpack_from("<QQQQ", argument)
            counts = struct.unpack_from("<IIII", argument, 32)
            lists = [[], CRTCS, CONNECTORS, ENCODERS]
            for index, values in enumerate(lists):
                if index > 0 and pointers[index] and counts[index] >= len(values):
                    memory.write(pointers[index], struct.pack(f"<{len(values)}I", *values))
            struct.pack_into("<IIII", argument, 32, 0, len(CRTCS), len(CONNECTORS), len(ENCODERS))
            struct.pack_into("<IIII", argument, 48, 320, 8192, 200, 8192)
            record("drm", "getresources")
            return 0, argument, None
        if request == GETCONNECTOR:
            encoders_ptr, modes_ptr, props_ptr, values_ptr = struct.unpack_from("<QQQQ", argument)
            count_modes, count_props, count_encoders = struct.unpack_from("<III", argument, 32)
            connector = struct.unpack_from("<I", argument, 48)[0]
            if connector not in CONNECTORS:
                return -EINVAL, argument, None
            connected = connector == 42
            modes = MODES if connected else []
            encoders = [52] if connected else [51]
            if modes_ptr and count_modes >= len(modes) and modes:
                memory.write(modes_ptr, b"".join(modes))
            if encoders_ptr and count_encoders >= len(encoders):
                memory.write(encoders_ptr, struct.pack(f"<{len(encoders)}I", *encoders))
            struct.pack_into("<IIIIIIIIII", argument, 32, len(modes), 0, len(encoders), 0, connector,
                             14 if connected else 11, 1, 1 if connected else 2, 300, 200)
            record("drm", "getconnector", connector)
            return 0, argument, None
        if request == GETENCODER:
            encoder = struct.unpack_from("<I", argument)[0]
            if encoder not in ENCODERS:
                return -EINVAL, argument, None
            # Encoder 52 drives the second CRTC only; no CRTC is set now.
            struct.pack_into("<IIIII", argument, 0, encoder, 2, 0, 0b10 if encoder == 52 else 0b01, 0)
            return 0, argument, None
        if request == GETCRTC:
            crtc = struct.unpack_from("<I", argument, 12)[0]
            if crtc not in CRTCS:
                return -EINVAL, argument, None
            struct.pack_into("<IIII", argument, 16, CONSOLE_FB, 0, 0, 256)
            struct.pack_into("<I", argument, 32, 1)
            argument[36:104] = CONSOLE_MODE
            record("drm", "getcrtc", crtc)
            return 0, argument, None
        if request == SETCRTC:
            pointer, count, crtc, fb, x, y, gamma, valid = struct.unpack_from("<QIIIIIII", argument)
            if not self.master:
                return -EACCES, argument, None
            connectors = list(struct.unpack(f"<{count}I", memory.read(pointer, 4 * count))) if count else []
            width, height = struct.unpack_from("<H", argument, 36 + 4)[0], struct.unpack_from("<H", argument, 36 + 14)[0]
            if fb != 0 and fb not in self.framebuffers and fb != CONSOLE_FB:
                fail(f"SETCRTC with unknown framebuffer {fb}")
            self.shown = fb
            record("drm", "setcrtc", crtc, fb, connectors, valid, width, height)
            return 0, argument, None
        if request == CREATE_DUMB:
            height, width, bpp, flags = struct.unpack_from("<IIII", argument)
            if bpp != 32 or width == 0 or height == 0:
                fail(f"CREATE_DUMB {width}x{height} at {bpp} bpp")
            pitch = (width * 4 + 255) // 256 * 256
            size = pitch * height
            memfd = os.memfd_create("dumb")
            os.ftruncate(memfd, size)
            handle = self.next_handle
            self.next_handle += 1
            self.buffers[handle] = [memfd, size, pitch]
            struct.pack_into("<IIQ", argument, 16, handle, pitch, size)
            record("drm", "create_dumb", handle, width, height)
            return 0, argument, None
        if request == ADDFB:
            _, width, height, pitch, bpp, depth, handle = struct.unpack_from("<IIIIIII", argument)
            if handle not in self.buffers or pitch != self.buffers[handle][2] or bpp != 32 or depth != 24:
                fail(f"ADDFB {width}x{height} pitch {pitch} bpp {bpp} depth {depth} handle {handle}")
            fb = self.next_fb
            self.next_fb += 1
            self.framebuffers[fb] = handle
            struct.pack_into("<I", argument, 0, fb)
            record("drm", "addfb", fb, handle)
            return 0, argument, None
        if request == MAP_DUMB:
            handle = struct.unpack_from("<I", argument)[0]
            if handle not in self.buffers:
                return -EINVAL, argument, None
            struct.pack_into("<Q", argument, 8, 0)
            return 0, argument, os.dup(self.buffers[handle][0])
        if request == PRIME_HANDLE_TO_FD:
            # The dumb buffer as a dma-buf: its memfd (lavapipe imports a
            # memfd as one). The descriptor number is the receiver's.
            handle, flags = struct.unpack_from("<II", argument)
            if handle not in self.buffers:
                return -EINVAL, argument, None
            if flags & ~(DRM_CLOEXEC | DRM_RDWR):
                fail(f"PRIME_HANDLE_TO_FD with flags {flags:#x}")
            struct.pack_into("<i", argument, 8, 0)
            record("drm", "prime_handle_to_fd", handle)
            return 0, argument, os.dup(self.buffers[handle][0])
        if request == PAGE_FLIP:
            crtc, fb, flags, _, user_data = struct.unpack_from("<IIIIQ", argument)
            if not self.master:
                return -EACCES, argument, None
            if fb not in self.framebuffers or not flags & 1:
                fail(f"PAGE_FLIP to {fb} with flags {flags}")
            self.flips += 1
            record("drm", "page_flip", crtc, fb)

            def complete():
                time.sleep(0.016)
                self.shown = fb
                now = time.clock_gettime(time.CLOCK_MONOTONIC)
                self.send_data(struct.pack("<IIQIIII", 2, 32, user_data, int(now), int(now * 1e6) % 1000000,
                                           self.flips, crtc))
            threading.Thread(target=complete, daemon=True).start()
            return 0, argument, None
        if request == RMFB:
            fb = struct.unpack_from("<I", argument)[0]
            record("drm", "rmfb", fb)
            return (0 if self.framebuffers.pop(fb, None) else -EINVAL), argument, None
        if request == DESTROY_DUMB:
            handle = struct.unpack_from("<I", argument)[0]
            record("drm", "destroy_dumb", handle)
            return (0 if self.buffers.pop(handle, None) else -EINVAL), argument, None
        return -ENOTTY, argument, None

    def pixel(self, x, y):
        handle = self.framebuffers[self.shown]
        memfd, size, pitch = self.buffers[handle]
        return struct.unpack("<I", os.pread(memfd, 4, y * pitch + x * 4))[0] & 0xFFFFFF


def bits(*numbers, size):
    data = bytearray(size)
    for number in numbers:
        data[number // 8] |= 1 << (number % 8)
    return data


class FakeInput(DeviceServer):
    def __init__(self, harness, kind, name):
        super().__init__(harness)
        self.kind = kind
        self.name = name

    def ioctl(self, request, argument):
        number = request & 0xFF
        size = (request >> 16) & 0x3FFF
        if (request >> 8) & 0xFF != ord("E") or request >> 30 != 2 or size != len(argument):
            fail(f"evdev ioctl {request:#x} with {len(argument)} bytes")
        if number == 0x06:
            name = self.name.encode()[:size - 1]
            argument[:len(name)] = name
            return len(name) + 1, argument, None
        if number == 0x20:
            kinds = {"keyboard": (EV_SYN, EV_KEY), "mouse": (EV_SYN, EV_KEY, EV_REL),
                     "touchpad": (EV_SYN, EV_KEY, EV_ABS)}[self.kind]
            argument[:] = bits(*kinds, size=size)
            return size, argument, None
        if number == 0x21:
            if self.kind == "keyboard":
                argument[:] = bits(*range(1, 128), size=size)
            elif self.kind == "mouse":
                argument[:] = bits(BTN_LEFT, BTN_RIGHT, BTN_MIDDLE, size=size)
            else:
                argument[:] = bits(BTN_LEFT, BTN_TOOL_FINGER, BTN_TOUCH, BTN_TOOL_DOUBLETAP, size=size)
            return size, argument, None
        if number == 0x22:
            argument[:] = bits(REL_X, REL_Y, REL_WHEEL, size=size) if self.kind == "mouse" else bytearray(size)
            return size, argument, None
        if number == 0x09:
            # INPUT_PROP_POINTER | INPUT_PROP_BUTTONPAD for the touchpad.
            argument[:] = bits(0, 2, size=size) if self.kind == "touchpad" else bytearray(size)
            return size, argument, None
        if 0x40 <= number < 0x80:
            # struct input_absinfo: a 100 mm wide pad, 30 units per mm.
            argument[:] = struct.pack("<iiiiii", 0, 0, 3000, 0, 0, 30)
            return 0, argument, None
        return -ENOTTY, argument, None

    def events(self, *records):
        now = time.time()
        data = b"".join(struct.pack("<qqHHi", int(now), int(now * 1e6) % 1000000, kind, code, value)
                        for kind, code, value in records)
        self.send_data(data)

    def key(self, code, pressed):
        self.events((EV_KEY, code, 1 if pressed else 0), (EV_SYN, 0, 0))

    def tap_key(self, code):
        self.key(code, True)
        time.sleep(0.004)
        self.key(code, False)
        time.sleep(0.004)


# US layout: characters to evdev codes (with Shift).
LETTERS = "qwertyuiop"
KEYS = {}
for row, first in (("qwertyuiop", 16), ("asdfghjkl", 30), ("zxcvbnm", 44)):
    for index, letter in enumerate(row):
        KEYS[letter] = (first + index, False)
for index, digit in enumerate("1234567890"):
    KEYS[digit] = (2 + index, False)
KEYS.update({" ": (57, False), "-": (12, False), "_": (12, True), ".": (52, False), ">": (52, True),
             "/": (53, False), "\n": (KEY_ENTER, False)})


def type_text(keyboard, text):
    for character in text:
        code, shift = KEYS[character]
        if shift:
            keyboard.key(KEY_LEFTSHIFT, True)
        keyboard.tap_key(code)
        if shift:
            keyboard.key(KEY_LEFTSHIFT, False)


INTROSPECTION = """
<node>
 <interface name="org.freedesktop.login1.Manager">
  <method name="GetSession"><arg type="s" direction="in"/><arg type="o" direction="out"/></method>
  <method name="GetSessionByPID"><arg type="u" direction="in"/><arg type="o" direction="out"/></method>
 </interface>
 <interface name="org.freedesktop.login1.Session">
  <method name="TakeControl"><arg type="b" direction="in"/></method>
  <method name="ReleaseControl"/>
  <method name="TakeDevice"><arg type="u" direction="in"/><arg type="u" direction="in"/>
   <arg type="h" direction="out"/><arg type="b" direction="out"/></method>
  <method name="ReleaseDevice"><arg type="u" direction="in"/><arg type="u" direction="in"/></method>
  <method name="PauseDeviceComplete"><arg type="u" direction="in"/><arg type="u" direction="in"/></method>
  <signal name="PauseDevice"><arg type="u"/><arg type="u"/><arg type="s"/></signal>
  <signal name="ResumeDevice"><arg type="u"/><arg type="u"/><arg type="h"/></signal>
 </interface>
 <interface name="org.freedesktop.login1.Seat">
  <method name="SwitchTo"><arg type="u" direction="in"/></method>
 </interface>
</node>
"""
SESSION_PATH = "/org/freedesktop/login1/session/_31"
SEAT_PATH = "/org/freedesktop/login1/seat/seat0"


class Harness:
    def __init__(self, compositor, terminal, renderer="cpu"):
        self.compositor_path = compositor
        self.terminal_path = terminal
        self.renderer = renderer
        self.recorder = Recorder()
        self.work = tempfile.mkdtemp(prefix="mlxdrm")
        self.memory = None
        self.devices = {}        # (major, minor) -> device server
        self.controller = None
        self.log_path = os.path.join(self.work, "compositor.log")

    def record(self, *call):
        self.recorder.add(*call)

    # -- nodes ---------------------------------------------------------------
    def make_nodes(self):
        os.makedirs(os.path.join(self.work, "dri"))
        os.makedirs(os.path.join(self.work, "input"))
        os.mknod(os.path.join(self.work, "dri", "card0"), 0o660 | 0o020000, os.makedev(DRM_MAJOR, 0))
        for number, (kind, name) in enumerate((("keyboard", "Fake Keyboard"), ("mouse", "Fake Mouse"),
                                               ("touchpad", "Fake Touchpad"))):
            self.add_input(number, kind, name, create_node=True)
        self.devices[(DRM_MAJOR, 0)] = FakeCard(self)
        self.devices[(DRM_MAJOR, 0)].start()

    def add_input(self, number, kind, name, create_node):
        device = FakeInput(self, kind, name)
        device.start()
        self.devices[(INPUT_MAJOR, 64 + number)] = device
        if create_node:
            os.mknod(os.path.join(self.work, "input", f"event{number}"), 0o660 | 0o020000,
                     os.makedev(INPUT_MAJOR, 64 + number))
        return device

    # -- the bus and logind ------------------------------------------------------
    def start_bus(self):
        path = os.path.join(self.work, "bus.sock")
        config = os.path.join(self.work, "bus.conf")
        with open(config, "w") as out:
            out.write(f"""<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig><type>system</type><listen>unix:path={path}</listen><auth>EXTERNAL</auth>
<policy context="default"><allow send_destination="*"/><allow receive_sender="*"/><allow own="*"/>
<allow user="*"/></policy></busconfig>""")
        self.bus = subprocess.Popen(["dbus-daemon", "--config-file=" + config, "--nofork"],
                                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(100):
            if os.path.exists(path):
                break
            time.sleep(0.05)
        self.address = "unix:path=" + path
        self.connection = Gio.DBusConnection.new_for_address_sync(
            self.address, Gio.DBusConnectionFlags.AUTHENTICATION_CLIENT |
            Gio.DBusConnectionFlags.MESSAGE_BUS_CONNECTION, None, None)
        info = Gio.DBusNodeInfo.new_for_xml(INTROSPECTION)
        for interface, path_ in ((info.interfaces[0], "/org/freedesktop/login1"),
                                 (info.interfaces[1], SESSION_PATH), (info.interfaces[2], SEAT_PATH)):
            self.connection.register_object(path_, interface, self.on_call, None, None)
        self.connection.call_sync("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                                  "RequestName", GLib.Variant("(su)", ("org.freedesktop.login1", 4)),
                                  None, Gio.DBusCallFlags.NONE, -1, None)
        self.loop = GLib.MainLoop()
        threading.Thread(target=self.loop.run, daemon=True).start()

    def on_call(self, connection, sender, path, interface, method, parameters, invocation):
        arguments = parameters.unpack()
        self.record("logind", method, *arguments)
        if method in ("GetSession", "GetSessionByPID"):
            invocation.return_value(GLib.Variant("(o)", (SESSION_PATH,)))
        elif method == "TakeControl":
            if self.controller is not None:
                invocation.return_dbus_error("org.freedesktop.login1.DeviceIsTaken", "taken")
                return
            self.controller = sender
            invocation.return_value(None)
        elif method == "TakeDevice":
            device = self.devices.get(tuple(arguments))
            if device is None or sender != self.controller:
                invocation.return_dbus_error("org.freedesktop.login1.NoSuchDevice", "no such device")
                return
            invocation.return_value_with_unix_fd_list(GLib.Variant("(hb)", (0, False)),
                                                      Gio.UnixFDList.new_from_array([device.take()]))
        elif method == "SwitchTo":
            invocation.return_value(None)
            threading.Thread(target=self.switch_away, daemon=True).start()
        else:
            invocation.return_value(None)

    def signal(self, name, major, minor, extra, descriptor=None):
        message = Gio.DBusMessage.new_signal(SESSION_PATH, "org.freedesktop.login1.Session", name)
        if descriptor is None:
            message.set_body(GLib.Variant("(uus)", (major, minor, extra)))
        else:
            message.set_body(GLib.Variant("(uuh)", (major, minor, 0)))
            message.set_unix_fd_list(Gio.UnixFDList.new_from_array([descriptor]))
        self.connection.send_message(message, Gio.DBusSendMessageFlags.NONE)

    # -- a VT switch ---------------------------------------------------------------
    def switch_away(self):
        """logind switching to VT 2 and, a moment later, back."""
        card = self.devices[(DRM_MAJOR, 0)]
        inputs = [key for key in self.devices if key[0] == INPUT_MAJOR]
        card.master = False
        self.signal("PauseDevice", DRM_MAJOR, 0, "pause")
        for key in inputs:
            self.devices[key].revoke()
            self.signal("PauseDevice", key[0], key[1], "force")
        self.record("vt", "away")
        time.sleep(0.6)
        card.master = True
        for key in inputs:
            old = self.devices[key]
            self.devices[key] = FakeInput(self, old.kind, old.name)
            self.devices[key].start()
        self.signal("ResumeDevice", DRM_MAJOR, 0, None, card.take())
        for key in inputs:
            self.signal("ResumeDevice", key[0], key[1], None, self.devices[key].take())
        self.record("vt", "back")

    # -- the compositor ------------------------------------------------------------
    def start_compositor(self):
        environment = {
            "PATH": os.environ.get("PATH", "/usr/bin:/bin"), "HOME": self.work,
            "XDG_RUNTIME_DIR": self.work, "XDG_SESSION_ID": "31", "XKB_DEFAULT_LAYOUT": "us",
            "DBUS_SYSTEM_BUS_ADDRESS": self.address,
            "MLX_DRM_DIR": os.path.join(self.work, "dri"), "MLX_INPUT_DIR": os.path.join(self.work, "input"),
        }
        # The Vulkan driver (lavapipe) the caller chose.
        for name in ("VK_DRIVER_FILES", "VK_ICD_FILENAMES", "VK_ADD_DRIVER_FILES", "MLX_VULKAN_NO_HOST_IMPORT"):
            if name in os.environ:
                environment[name] = os.environ[name]
        self.log = open(self.log_path, "w")
        self.process = subprocess.Popen([self.compositor_path, "--verbose", "--socket", "wayland-drm",
                                         "--renderer", self.renderer,
                                         "--terminal", self.terminal_path, "--run", self.terminal_path],
                                        cwd=self.work, env=environment, stdout=self.log, stderr=subprocess.STDOUT)
        # The kernel side reads its memory from the start.
        for _ in range(100):
            try:
                self.memory = Memory(self.process.pid)
                break
            except OSError:
                time.sleep(0.01)

    def log_text(self):
        with open(self.log_path) as source:
            return source.read()

    def wait_log(self, text, timeout, what):
        deadline = time.time() + timeout
        while text not in self.log_text():
            if time.time() > deadline or self.process.poll() is not None:
                fail(f"no {what} in the compositor's log:\n{self.log_text()}")
            time.sleep(0.05)

    def wait_file(self, name, content, timeout):
        path = os.path.join(self.work, name)
        deadline = time.time() + timeout
        while True:
            if os.path.exists(path):
                with open(path) as source:
                    if source.read().strip() == content:
                        return
            if time.time() > deadline:
                fail(f"{name} does not say {content!r}; log:\n{self.log_text()}")
            time.sleep(0.05)

    def cursor_at(self, x, y, timeout=3):
        """The arrow's tip (black outline) at (x, y), white fill inside."""
        card = self.devices[(DRM_MAJOR, 0)]
        deadline = time.time() + timeout
        while True:
            if card.shown in card.framebuffers and card.pixel(x, y) == 0 and card.pixel(x + 1, y + 2) == 0xFFFFFF:
                return
            if time.time() > deadline:
                shown = [hex(card.pixel(x + dx, y + dy)) for dx, dy in ((0, 0), (1, 2))] if card.shown in card.framebuffers else None
                fail(f"no cursor at ({x}, {y}): {shown}")
            time.sleep(0.05)


def background(row):
    shade = row * 48 // HEIGHT
    return ((24 + shade // 2) << 16) | ((34 + shade // 2) << 8) | (52 + shade)


def write_png(card, path):
    """The frame on screen, as a PNG."""
    handle = card.framebuffers[card.shown]
    memfd, size, pitch = card.buffers[handle]
    data = os.pread(memfd, size, 0)
    rows = []
    for y in range(HEIGHT):
        row = bytearray(b"\0")
        for x in range(WIDTH):
            b, g, r = data[y * pitch + x * 4:y * pitch + x * 4 + 3]
            row += bytes((r, g, b))
        rows.append(bytes(row))

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)
    with open(path, "wb") as out:
        out.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 2, 0, 0, 0)) +
                  chunk(b"IDAT", zlib.compress(b"".join(rows), 6)) + chunk(b"IEND", b""))


def main():
    arguments = sys.argv[1:]
    screenshot = None
    renderer = "cpu"
    while len(arguments) > 2:
        if len(arguments) >= 4 and arguments[-2] == "--screenshot":
            screenshot = os.path.abspath(arguments[-1])
        elif len(arguments) >= 4 and arguments[-2] == "--renderer" and arguments[-1] in ("cpu", "vulkan"):
            renderer = arguments[-1]
        else:
            print(__doc__)
            return 2
        arguments = arguments[:-2]
    if len(arguments) != 2:
        print(__doc__)
        return 2
    harness = Harness(os.path.abspath(arguments[0]), os.path.abspath(arguments[1]), renderer)
    recorder = harness.recorder
    harness.make_nodes()
    harness.start_bus()
    harness.start_compositor()
    try:
        # Start: the session, the card, the monitor's preferred mode.
        recorder.wait(lambda c: c[:2] == ("logind", "TakeControl"), 10, "TakeControl")
        setcrtc = recorder.wait(lambda c: c[:2] == ("drm", "setcrtc"), 10, "the first modeset")[0]
        if setcrtc[2] != 32 or setcrtc[4] != [42] or setcrtc[5] != 1 or setcrtc[6:] != (WIDTH, HEIGHT):
            fail(f"modeset {setcrtc}: expected CRTC 32, connector [42], {WIDTH}x{HEIGHT}")
        for key in ((DRM_MAJOR, 0), (INPUT_MAJOR, 64), (INPUT_MAJOR, 65), (INPUT_MAJOR, 66)):
            recorder.wait(lambda c, key=key: c[:2] == ("logind", "TakeDevice") and tuple(c[2:]) == key, 5,
                          f"TakeDevice{key}")
        harness.wait_log("drm: ", 5, "output line")
        if "input: " not in harness.log_text() or "keyboard: xkb keymap from libxkbcommon" not in harness.log_text():
            fail(f"inputs or keymap missing:\n{harness.log_text()}")
        card = harness.devices[(DRM_MAJOR, 0)]
        if card.pixel(0, HEIGHT - 1) != background(HEIGHT - 1):
            fail(f"background {card.pixel(0, HEIGHT - 1):#x}, expected {background(HEIGHT - 1):#x}")
        print(f"ok   logind session and devices; modeset CRTC 32 -> connector 42 at its preferred {WIDTH}x{HEIGHT}")
        if renderer == "vulkan":
            harness.wait_log("renderer: vulkan", 5, "the Vulkan renderer")
            exported = sorted(c[2] for c in recorder.calls if c[:2] == ("drm", "prime_handle_to_fd"))
            copying = "copying each frame" in harness.log_text()
            if os.environ.get("MLX_VULKAN_NO_HOST_IMPORT"):
                # The copy path, as on a driver that cannot import the
                # buffers: rendered elsewhere and copied in, row by row.
                if exported or not copying:
                    fail(f"expected frames copied into the dumb buffers (exported {exported}, copying {copying})")
                print("ok   Vulkan renders into its own buffer and copies frames into the dumb buffers")
            else:
                # The GPU renders into the dumb buffers themselves: both
                # were exported as dma-bufs before the first frame.
                if exported != [1, 2] or copying:
                    fail(f"expected both dumb buffers exported as dma-bufs (exported {exported}, copying {copying})")
                print("ok   Vulkan renders into the dumb buffers (both exported as dma-bufs)")

        # The terminal maps; the keyboard types into it.
        harness.wait_log("map: Mlx Terminal", 10, "the terminal")
        time.sleep(0.3)
        keyboard = harness.devices[(INPUT_MAJOR, 64)]
        type_text(keyboard, "echo drm-typed > m1\n")
        harness.wait_file("m1", "drm-typed", 10)
        if recorder.count(lambda c: c[:2] == ("drm", "page_flip")) == 0:
            fail("no page flips")
        print(f"ok   the keyboard typed into the Mlx terminal; frames page-flipped ({card.flips} flips)")

        # The pointer starts at the middle; the mouse moves it (small steps:
        # 1.5x acceleration), the touchpad 4 px per mm.
        mouse = harness.devices[(INPUT_MAJOR, 65)]
        harness.cursor_at(WIDTH // 2, HEIGHT // 2)
        for _ in range(10):
            mouse.events((EV_REL, REL_X, 4), (EV_REL, REL_Y, 2), (EV_SYN, 0, 0))
            time.sleep(0.005)
        x, y = WIDTH // 2 + 60, HEIGHT // 2 + 30
        harness.cursor_at(x, y)
        pad = harness.devices[(INPUT_MAJOR, 66)]
        pad.events((EV_KEY, BTN_TOOL_FINGER, 1), (EV_KEY, BTN_TOUCH, 1), (EV_ABS, ABS_X, 1000),
                   (EV_ABS, ABS_Y, 1000), (EV_SYN, 0, 0))
        for step in range(1, 11):
            time.sleep(0.03)
            pad.events((EV_ABS, ABS_X, 1000 + 30 * step), (EV_SYN, 0, 0))
        pad.events((EV_KEY, BTN_TOUCH, 0), (EV_KEY, BTN_TOOL_FINGER, 0), (EV_SYN, 0, 0))
        x += 40
        harness.cursor_at(x, y)
        print("ok   mouse (with acceleration) and touchpad (10 mm = 40 px) move the cursor")

        # A mouse plugged in now.
        second = harness.add_input(3, "mouse", "Plugged Mouse", create_node=True)
        recorder.wait(lambda c: c[:2] == ("logind", "TakeDevice") and tuple(c[2:]) == (INPUT_MAJOR, 67), 5,
                      "the new mouse")
        time.sleep(0.2)
        second.events((EV_REL, REL_X, -2), (EV_REL, REL_Y, -1), (EV_SYN, 0, 0))
        harness.cursor_at(x - 2, y - 1)
        print("ok   a mouse plugged in later (inotify) works")

        # Ctrl+Alt+F2: logind switches VTs; the card and inputs pause, then resume.
        flips_before = card.flips
        keyboard.key(KEY_LEFTCTRL, True)
        keyboard.key(KEY_LEFTALT, True)
        keyboard.tap_key(KEY_F2)
        keyboard.key(KEY_LEFTALT, False)
        keyboard.key(KEY_LEFTCTRL, False)
        recorder.wait(lambda c: c[:3] == ("logind", "SwitchTo", 2), 5, "SwitchTo(2)")
        recorder.wait(lambda c: c[:2] == ("logind", "PauseDeviceComplete") and tuple(c[2:]) == (DRM_MAJOR, 0), 5,
                      "PauseDeviceComplete for the card")
        recorder.wait(lambda c: c == ("vt", "back"), 5, "the switch back")
        recorder.wait(lambda c: recorder.count(lambda d: d[:2] == ("drm", "setcrtc")) > 1, 5,
                      "the modeset after resuming")
        modesets = recorder.count(lambda c: c[:2] == ("drm", "setcrtc"))
        time.sleep(0.3)
        keyboard = harness.devices[(INPUT_MAJOR, 64)]
        type_text(keyboard, "echo back > m2\n")
        harness.wait_file("m2", "back", 10)
        print(f"ok   Ctrl+Alt+F2 switched VTs through logind; paused, resumed (modeset again), typing works after ({modesets} modesets, {card.flips - flips_before} flips since)")

        # weston-terminal translates keys with the keymap and modifiers the
        # compositor made (libxkbcommon): Shift for '>' and '_' must work.
        if shutil.which("weston-terminal"):
            client_environment = dict(os.environ, WAYLAND_DISPLAY="wayland-drm", XDG_RUNTIME_DIR=harness.work)
            client_environment.pop("DISPLAY", None)
            weston = subprocess.Popen(["weston-terminal"], cwd=harness.work, env=client_environment,
                                      stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            harness.wait_log("map: Wayland Terminal", 10, "weston-terminal")
            time.sleep(0.8)
            type_text(keyboard, "echo xkb_shift > m3\n")
            harness.wait_file("m3", "xkb_shift", 10)
            if screenshot:
                time.sleep(0.5)
                write_png(card, screenshot)
            weston.terminate()
            print("ok   weston-terminal typed through the compositor's xkb keymap and modifiers (Shift: '_', '>')")
        else:
            print("skip weston-terminal is not installed")

        # Alt+Shift+Q: the CRTC gets the console back, everything is released.
        keyboard.key(KEY_LEFTALT, True)
        keyboard.key(KEY_LEFTSHIFT, True)
        keyboard.tap_key(KEY_Q)
        try:
            status = harness.process.wait(10)
        except subprocess.TimeoutExpired:
            fail(f"the compositor did not quit:\n{harness.log_text()}")
        if status != 0:
            fail(f"exit status {status}:\n{harness.log_text()}")
        last = [c for c in recorder.calls if c[:2] == ("drm", "setcrtc")][-1]
        if last[3] != CONSOLE_FB or last[6:] != (800, 600):
            fail(f"CRTC not restored: {last}")
        removed = sorted(c[2] for c in recorder.calls if c[:2] == ("drm", "rmfb"))
        destroyed = sorted(c[2] for c in recorder.calls if c[:2] == ("drm", "destroy_dumb"))
        released = {tuple(c[2:]) for c in recorder.calls if c[:2] == ("logind", "ReleaseDevice")}
        if removed != [101, 102] or destroyed != [1, 2]:
            fail(f"buffers left: removed {removed}, destroyed {destroyed}")
        if (DRM_MAJOR, 0) not in released or (INPUT_MAJOR, 64) not in released or \
                recorder.count(lambda c: c[:2] == ("logind", "ReleaseControl")) != 1:
            fail(f"devices not released: {released}")
        print("ok   Alt+Shift+Q: the console's CRTC restored, buffers freed, devices and control given back")
    finally:
        if harness.process.poll() is None:
            harness.process.kill()
        harness.bus.terminate()
    return 0


if __name__ == "__main__":
    sys.exit(main())
