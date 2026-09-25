#!/usr/bin/env python3
"""Runs an mlx Android app (libmain.so, or the one inside an APK) against a
model of the Android framework under Unicorn, and checks the gesture example
examples/android/gestures.mlx end to end.

The model plays the parts of the platform the std.android runtime talks to:
ANativeActivity and its callback table, one window (a small RGBA_8888
buffer; every unlockAndPost records the posted frame), the main looper
(input-queue and fd callbacks), the input queue (scripted motion events),
a timerfd on a simulated clock, liblog and malloc. Each scenario starts a
fresh activity, feeds touch events at given times, lets the long-press timer
fire when the simulated clock passes its deadline, and checks the color of
every posted frame and the logcat lines.

Usage: python3 tools/emulate_android_app.py [-v] [app.apk | libmain.so]
With no file, builds examples/android/gestures.mlx with zig-out/bin/mlx1.
"""
import os
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from aarch64_linux_emulator import Float32, SharedLibrary  # noqa: E402

HEAP = 0x6000_0000_0000
HEAP_SIZE = 16 << 20
WINDOW = 0x5700_0000_0001
QUEUE = 0x5700_0000_0002
LOOPER = 0x5700_0000_0003
TIMER_FD = 57
WIDTH, HEIGHT = 48, 32
MS = 1_000_000

ACTION_DOWN, ACTION_UP, ACTION_MOVE, ACTION_CANCEL, ACTION_POINTER_DOWN = 0, 1, 2, 3, 5
CALLBACKS = {"onSaveInstanceState": 16, "onDestroy": 40, "onNativeWindowCreated": 56,
             "onNativeWindowResized": 64, "onNativeWindowRedrawNeeded": 72,
             "onNativeWindowDestroyed": 80, "onInputQueueCreated": 88, "onInputQueueDestroyed": 96,
             "onContentRectChanged": 104}


def rgb(red, green, blue):
    return 0xFF000000 | blue << 16 | green << 8 | red


BLUE, GREEN, YELLOW = rgb(0, 0, 255), rgb(0, 200, 0), rgb(255, 220, 0)
RED, MAGENTA, CYAN, ORANGE = rgb(220, 0, 0), rgb(255, 0, 255), rgb(0, 255, 255), rgb(255, 128, 0)
NAMES = {BLUE: "blue", GREEN: "green", YELLOW: "yellow", RED: "red", MAGENTA: "magenta",
         CYAN: "cyan", ORANGE: "orange"}


ACTIVITY_OBJECT = 0x7A00_0001
# WindowInsets.Type: statusBars(), navigationBars(), systemBars() (with the
# caption bar) and displayCutout().
STATUS_BARS, NAVIGATION_BARS, SYSTEM_BARS, DISPLAY_CUTOUT = 1, 2, 7, 128
FLAG_FULLSCREEN = 0x400
# View.SYSTEM_UI_FLAG_*: the layout flags std.android always sets, and the
# two that hide bars.
SYSTEM_UI_LAYOUT = 0x100 | 0x200 | 0x400 | 0x1000
SYSTEM_UI_FULLSCREEN, SYSTEM_UI_HIDE_NAVIGATION = 4, 2
SIDES = ("top", "right", "bottom", "left")
# Resource ids of the status bar dimens.
PORTRAIT_DIMEN, STATUS_BAR_DIMEN = 0x1050_0001, 0x1050_0002


class FakeJni:
    """The activity's JNIEnv, as far as std.android's reserved space and
    system bar switches use it: the NativeActivity object and its
    Resources (the status bar dimens: `portrait_status_bar` for
    status_bar_height_portrait, `status_bar` for status_bar_height, None
    when missing), its Window with its LayoutParams flags (FLAG_FULLSCREEN
    when `fullscreen`) and WindowInsetsController, the decor View
    (setSystemUiVisibility) and its WindowInsets: `insets` (top, right,
    bottom, left) are the system bars, the top one the status bar and the
    others the navigation bar, as long as they are shown; None means no
    WindowInsets yet (not laid out). `cutout` is the display cutout's safe
    insets. API 30+ goes through WindowInsets.Type, android.graphics.Insets
    and the controller, older levels through getSystemWindowInset*,
    getDisplayCutout (API 28+) and system UI flags. Unknown classes, methods
    and fields throw (a pending exception, as in Java); any call made while
    an exception is pending, and unbalanced local frames, fail the run."""

    FUNCTIONS = {6: "FindClass", 17: "ExceptionClear", 19: "PushLocalFrame", 20: "PopLocalFrame",
                 31: "GetObjectClass", 33: "GetMethodID", 36: "CallObjectMethodA", 51: "CallIntMethodA",
                 63: "CallVoidMethodA", 94: "GetFieldID", 100: "GetIntField", 113: "GetStaticMethodID",
                 131: "CallStaticIntMethodA", 167: "NewStringUTF", 228: "ExceptionCheck"}
    CLASSES = {"activity": "android/app/NativeActivity", "window": "android/view/Window",
               "attributes": "android/view/WindowManager$LayoutParams",
               "controller": "android/view/WindowInsetsController", "resources": "android/content/res/Resources",
               "string": "java/lang/String", "cutout": "android/view/DisplayCutout",
               "decor": "android/view/View", "insets": "android/view/WindowInsets", "values": "android/graphics/Insets"}
    METHODS = {("android/app/NativeActivity", "getWindow", "()Landroid/view/Window;"),
               ("android/app/NativeActivity", "getResources", "()Landroid/content/res/Resources;"),
               ("android/content/res/Resources", "getIdentifier",
                "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I"),
               ("android/content/res/Resources", "getDimensionPixelSize", "(I)I"),
               ("android/view/Window", "getDecorView", "()Landroid/view/View;"),
               ("android/view/Window", "getAttributes", "()Landroid/view/WindowManager$LayoutParams;"),
               ("android/view/View", "getRootWindowInsets", "()Landroid/view/WindowInsets;"),
               ("android/view/WindowInsets", "getSystemWindowInsetTop", "()I"),
               ("android/view/WindowInsets", "getSystemWindowInsetRight", "()I"),
               ("android/view/WindowInsets", "getSystemWindowInsetBottom", "()I"),
               ("android/view/WindowInsets", "getSystemWindowInsetLeft", "()I")}
    # Only from these API levels on.
    METHODS_SINCE = {28: {("android/view/WindowInsets", "getDisplayCutout", "()Landroid/view/DisplayCutout;"),
                          ("android/view/DisplayCutout", "getSafeInsetTop", "()I"),
                          ("android/view/DisplayCutout", "getSafeInsetRight", "()I"),
                          ("android/view/DisplayCutout", "getSafeInsetBottom", "()I"),
                          ("android/view/DisplayCutout", "getSafeInsetLeft", "()I")},
                     30: {("android/view/WindowInsets", "getInsets", "(I)Landroid/graphics/Insets;"),
                          ("android/view/Window", "getInsetsController", "()Landroid/view/WindowInsetsController;"),
                          ("android/view/WindowInsetsController", "setSystemBarsBehavior", "(I)V"),
                          ("android/view/WindowInsetsController", "hide", "(I)V"),
                          ("android/view/WindowInsetsController", "show", "(I)V")}}
    VOID_METHODS = {("android/view/View", "setSystemUiVisibility", "(I)V")}

    def __init__(self, framework, stub, sdk=34, insets=(0, 0, 0, 0), fullscreen=False, cutout=(0, 0, 0, 0),
                 portrait_status_bar=None, status_bar=None):
        self.framework = framework
        self.sdk = sdk
        self.insets = insets
        self.cutout = cutout
        self.fullscreen = fullscreen
        self.portrait_status_bar = portrait_status_bar
        self.status_bar = status_bar
        self.status_hidden = False
        self.navigation_hidden = False
        self.behavior = None
        # (method, argument) of every system bar change, in order.
        self.bar_calls = []
        self.pending = False
        self.frames = 0
        self.queries = 0
        self.objects = {ACTIVITY_OBJECT: "activity"}
        self.data = {}
        self.methods = set(self.METHODS) | self.VOID_METHODS
        for level, methods in self.METHODS_SINCE.items():
            if sdk >= level:
                self.methods |= methods
        table = framework.malloc(8 * 240)
        for index in range(240):
            name = self.FUNCTIONS.get(index)
            handler = getattr(self, name) if name else self.unexpected(index)
            framework.put_u64(table + 8 * index, stub(f"jni:{name or index}", handler))
        self.env = framework.malloc(8)
        framework.put_u64(self.env, table)

    def visible(self):
        """The system bars' insets as far as they are shown."""
        top, right, bottom, left = self.insets
        if self.status_hidden:
            top = 0
        if self.navigation_hidden:
            right = bottom = left = 0
        return (top, right, bottom, left)

    def reserved(self):
        """What std.android.reservedInsets should answer: the shown bars,
        with the status bar's band as tall as in portrait."""
        if self.fullscreen or self.insets is None:
            return (0, 0, 0, 0)
        top, right, bottom, left = self.visible()
        if top > 0:
            resource = self.portrait_status_bar if self.portrait_status_bar is not None else (self.status_bar or 0)
            cutout = max(self.cutout) if self.sdk >= 28 else 0
            top = max(top, resource, cutout)
        return (top, right, bottom, left)

    def unexpected(self, index):
        def fail(process, *_):
            raise AssertionError(f"unexpected JNI function {index}")
        return fail

    def new(self, kind, data=None):
        handle = 0x7A00_0000 + 0x10 * (len(self.objects) + 1)
        self.objects[handle] = kind
        if data is not None:
            self.data[handle] = data
        return handle

    def calm(self, name):
        assert not self.pending, f"JNI {name} called with an exception pending"

    def throw(self):
        self.pending = True
        return 0

    def name_of(self, method):
        return self.objects[method].split(":", 1)[1]

    def FindClass(self, process, env, name, *_):
        self.calm("FindClass")
        if process.cstring(name) == "android/view/WindowInsets$Type" and self.sdk >= 30:
            return self.new("class:android/view/WindowInsets$Type")
        return self.throw()

    def NewStringUTF(self, process, env, text, *_):
        self.calm("NewStringUTF")
        return self.new("string", process.cstring(text))

    def ExceptionCheck(self, process, env, *_):
        return 1 if self.pending else 0

    def ExceptionClear(self, process, env, *_):
        self.pending = False

    def PushLocalFrame(self, process, env, capacity, *_):
        self.calm("PushLocalFrame")
        self.frames += 1
        return 0

    def PopLocalFrame(self, process, env, result, *_):
        assert self.frames > 0, "PopLocalFrame without PushLocalFrame"
        self.frames -= 1
        return 0

    def GetObjectClass(self, process, env, obj, *_):
        self.calm("GetObjectClass")
        return self.new("class:" + self.CLASSES[self.objects[obj]])

    def member(self, process, cls, name, signature, known):
        class_name = self.objects[cls].split(":", 1)[1]
        key = (class_name, process.cstring(name), process.cstring(signature))
        if key not in known:
            return self.throw()
        return self.new("member:" + key[1])

    def GetMethodID(self, process, env, cls, name, signature, *_):
        self.calm("GetMethodID")
        return self.member(process, cls, name, signature, self.methods)

    def GetStaticMethodID(self, process, env, cls, name, signature, *_):
        self.calm("GetStaticMethodID")
        return self.member(process, cls, name, signature,
                           {("android/view/WindowInsets$Type", method, "()I")
                            for method in ("statusBars", "navigationBars", "systemBars", "displayCutout")})

    def GetFieldID(self, process, env, cls, name, signature, *_):
        self.calm("GetFieldID")
        return self.member(process, cls, name, signature, {("android/graphics/Insets", side, "I") for side in SIDES}
                           | {("android/view/WindowManager$LayoutParams", "flags", "I")})

    def int_argument(self, process, arguments, index=0):
        return struct.unpack("<i", process.read(arguments + 8 * index, 4))[0]

    def CallObjectMethodA(self, process, env, obj, method, arguments, *_):
        self.calm("CallObjectMethodA")
        name = self.name_of(method)
        simple = {"getWindow": "window", "getDecorView": "decor", "getAttributes": "attributes",
                  "getInsetsController": "controller", "getResources": "resources"}
        if name in simple:
            return self.new(simple[name])
        if name == "getRootWindowInsets":
            self.queries += 1
            return 0 if self.insets is None else self.new("insets")
        if name == "getDisplayCutout":
            return self.new("cutout") if any(self.cutout) else 0
        assert name == "getInsets", name
        mask = self.int_argument(process, arguments)
        assert mask in (SYSTEM_BARS, DISPLAY_CUTOUT), f"getInsets({mask}): neither the system bars nor the cutout"
        return self.new("values", self.visible() if mask == SYSTEM_BARS else self.cutout)

    def CallIntMethodA(self, process, env, obj, method, arguments, *_):
        self.calm("CallIntMethodA")
        name = self.name_of(method)
        if name.startswith("getSystemWindowInset"):
            return self.visible()[SIDES.index(name[len("getSystemWindowInset"):].lower())]
        if name.startswith("getSafeInset"):
            return self.cutout[SIDES.index(name[len("getSafeInset"):].lower())]
        if name == "getIdentifier":
            text = [self.data[struct.unpack("<Q", process.read(arguments + 8 * index, 8))[0]] for index in range(3)]
            assert text[1:] == ["dimen", "android"], text
            if text[0] == "status_bar_height_portrait" and self.portrait_status_bar is not None:
                return PORTRAIT_DIMEN
            if text[0] == "status_bar_height" and self.status_bar is not None:
                return STATUS_BAR_DIMEN
            return 0
        assert name == "getDimensionPixelSize", name
        return {PORTRAIT_DIMEN: self.portrait_status_bar, STATUS_BAR_DIMEN: self.status_bar}[self.int_argument(process, arguments)]

    def CallVoidMethodA(self, process, env, obj, method, arguments, *_):
        self.calm("CallVoidMethodA")
        name = self.name_of(method)
        value = self.int_argument(process, arguments)
        self.bar_calls.append((name, value))
        if name == "setSystemBarsBehavior":
            self.behavior = value
        elif name == "hide":
            self.status_hidden |= bool(value & STATUS_BARS)
            self.navigation_hidden |= bool(value & NAVIGATION_BARS)
        elif name == "show":
            self.status_hidden &= not value & STATUS_BARS
            self.navigation_hidden &= not value & NAVIGATION_BARS
        else:
            assert name == "setSystemUiVisibility" and self.sdk < 30, name
            assert value & SYSTEM_UI_LAYOUT == SYSTEM_UI_LAYOUT, f"system UI flags {value:#x} without the layout flags"
            self.status_hidden = bool(value & SYSTEM_UI_FULLSCREEN)
            self.navigation_hidden = bool(value & SYSTEM_UI_HIDE_NAVIGATION)

    def CallStaticIntMethodA(self, process, env, cls, method, arguments, *_):
        self.calm("CallStaticIntMethodA")
        return {"statusBars": STATUS_BARS, "navigationBars": NAVIGATION_BARS, "systemBars": SYSTEM_BARS,
                "displayCutout": DISPLAY_CUTOUT}[self.name_of(method)]

    def GetIntField(self, process, env, obj, field, *_):
        self.calm("GetIntField")
        if self.objects[obj] == "attributes":
            # FLAG_LAYOUT_IN_SCREEN and FLAG_LAYOUT_INSET_DECOR are always set.
            return (FLAG_FULLSCREEN if self.fullscreen else 0) | 0x100 | 0x10000
        assert self.objects[obj] == "values"
        return self.data[obj][SIDES.index(self.name_of(field))]


class Framework:
    """The Android side of one app process. jni: {"sdk", "insets",
    "fullscreen"} gives the activity a JNIEnv (FakeJni); without it the
    activity has none, as far as the app can tell."""

    def __init__(self, library_path, verbose=False, jni=None):
        self.verbose = verbose
        self.heap_next = HEAP
        self.now = 0
        self.timer_deadline = None
        self.timer_expired = False
        self.fd_callbacks = {}
        self.input_callback = None
        self.pending = []
        self.events = {}
        self.finished = []
        self.frames = []
        self.logs = []
        self.lib = SharedLibrary(library_path, imports=self._imports())
        self.lib.uc.mem_map(HEAP, HEAP_SIZE)
        self.framebuffer = self.malloc(WIDTH * HEIGHT * 4)
        self.jni = FakeJni(self, self.stub, **jni) if jni is not None else None

    def stub(self, name, function):
        """An address that calls `function(process, *arguments)`."""
        address = SharedLibrary.STUB_BASE + len(self.lib.stubs) * 16
        self.lib.uc.mem_write(address, struct.pack("<I", 0xD65F03C0))  # ret
        self.lib.stubs[address] = name
        self.lib.imports[name] = function
        return address

    # ── memory ──
    def malloc(self, size):
        address = self.heap_next
        self.heap_next += (size + 15) & ~15
        self.lib.uc.mem_write(address, bytes(size))
        return address

    def u64(self, address):
        return struct.unpack("<Q", self.lib.read(address, 8))[0]

    def put_u64(self, address, value):
        self.lib.write(address, struct.pack("<Q", value & 0xFFFFFFFFFFFFFFFF))

    # ── NDK functions ──
    def _imports(self):
        f = self
        return {
            "malloc": lambda p, size, *_: f.malloc(size),
            "free": lambda p, *_: 0,
            "ANativeActivity_finish": lambda p, *_: 0,
            "ANativeWindow_setBuffersGeometry": lambda p, *_: 0,
            "ANativeWindow_lock": f._lock,
            "ANativeWindow_unlockAndPost": f._post,
            "ALooper_forThread": lambda p, *_: LOOPER,
            "ALooper_addFd": f._add_fd,
            "ALooper_removeFd": lambda p, looper, fd, *_: 1 if f.fd_callbacks.pop(fd & 0xFFFFFFFF, None) else 0,
            "AInputQueue_attachLooper": f._attach,
            "AInputQueue_detachLooper": f._detach,
            "AInputQueue_getEvent": f._get_event,
            "AInputQueue_preDispatchEvent": lambda p, *_: 0,
            "AInputQueue_finishEvent": lambda p, queue, event, handled, *_: f.finished.append(event),
            "AInputEvent_getType": lambda p, *_: 2,
            "AMotionEvent_getAction": lambda p, event, *_: f.events[event][0],
            "AMotionEvent_getX": lambda p, event, *_: Float32(f.events[event][1]),
            "AMotionEvent_getY": lambda p, event, *_: Float32(f.events[event][2]),
            "AMotionEvent_getEventTime": lambda p, event, *_: f.events[event][3],
            "__android_log_write": f._log,
            "timerfd_create": lambda p, *_: TIMER_FD,
            "timerfd_settime": f._set_timer,
            "read": f._read,
            "close": lambda p, *_: 0,
        }

    def _lock(self, process, window, buffer, dirty, *_):
        assert window == self.window, "lock of a window that is not current"
        self.lib.write(buffer, struct.pack("<iiiiQ", WIDTH, HEIGHT, WIDTH, 1, self.framebuffer))
        return 0

    def _post(self, process, window, *_):
        pixels = struct.unpack(f"<{WIDTH * HEIGHT}I", self.lib.read(self.framebuffer, WIDTH * HEIGHT * 4))
        colors = set(pixels)
        self.frames.append(pixels[0] if len(colors) == 1 else ("mixed", colors))
        return 0

    def _add_fd(self, process, looper, fd, ident, events, callback, data, *_):
        self.fd_callbacks[fd & 0xFFFFFFFF] = (callback, data)
        return 1

    def _attach(self, process, queue, looper, ident, callback, data, *_):
        self.input_callback = (callback, data)

    def _detach(self, process, queue, *_):
        self.input_callback = None

    def _get_event(self, process, queue, out, *_):
        if not self.pending:
            return -1
        event = self.pending.pop(0)
        self.put_u64(out, event)
        return 0

    def _log(self, process, priority, tag, text, *_):
        self.logs.append((process.cstring(tag), process.cstring(text)))
        return 1

    def _set_timer(self, process, fd, flags, new, old, *_):
        seconds, nanoseconds = struct.unpack("<qq", process.read(new + 16, 16))
        value = seconds * 1_000_000_000 + nanoseconds
        self.timer_deadline = None if value == 0 else self.now + value
        self.timer_expired = False
        return 0

    def _read(self, process, fd, buffer, length, *_):
        if (fd & 0xFFFFFFFF) == TIMER_FD and self.timer_expired:
            self.timer_expired = False
            self.put_u64(buffer, 1)
            return 8
        return -1  # EAGAIN (non-blocking)

    # ── framework actions ──
    def create(self, saved=0, saved_size=0):
        self.activity = self.malloc(80)
        self.callbacks = self.malloc(128)
        self.put_u64(self.activity, self.callbacks)
        self.prepare_activity()
        self.lib.call("ANativeActivity_onCreate", self.activity, saved, saved_size)

    def prepare_activity(self):
        """The ANativeActivity's env, clazz and sdkVersion, with a JNIEnv."""
        if self.jni is not None:
            self.put_u64(self.activity + 16, self.jni.env)
            self.put_u64(self.activity + 24, ACTIVITY_OBJECT)
            self.lib.write(self.activity + 48, struct.pack("<i", self.jni.sdk))

    def callback(self, name, *args):
        address = self.u64(self.callbacks + CALLBACKS[name])
        assert address, f"{name} not registered"
        return self.lib.call_address(address, self.activity, *args, name=name)

    def show_window(self):
        self.window = WINDOW
        self.callback("onNativeWindowCreated", WINDOW)

    def attach_input(self):
        self.callback("onInputQueueCreated", QUEUE)
        assert self.input_callback, "input queue not attached to the looper"

    def advance(self, until):
        """Moves the clock, running the timer callback if its deadline passes."""
        while self.timer_deadline is not None and self.timer_deadline <= until:
            self.now = self.timer_deadline
            self.timer_deadline = None
            self.timer_expired = True
            callback, data = self.fd_callbacks[TIMER_FD]
            keep = self.lib.call_address(callback, TIMER_FD, 1, data, name="timer")
            assert keep & 0xFFFFFFFF == 1, "timer callback unregistered itself"
        self.now = max(self.now, until)

    def touch(self, action, x, y, at):
        self.advance(at)
        event = 0x4E00_0000_0000 + len(self.events)
        self.events[event] = (action, float(x), float(y), at)
        self.pending.append(event)
        callback, data = self.input_callback
        keep = self.lib.call_address(callback, 0, 1, data, name="input")
        assert keep & 0xFFFFFFFF == 1, "input callback unregistered itself"
        assert not self.pending, "input callback left events in the queue"


def gesture_lines(framework):
    return [text for tag, text in framework.logs if tag == "mlx"]


def scenario_events(name):
    """(events, wait_after_ms, expected_final_color, expected_log)"""
    tap = [(ACTION_DOWN, 100, 100, 0), (ACTION_UP, 102, 101, 90 * MS)]
    table = {
        "tap": (tap, 0, GREEN, ["gesture: tap"]),
        # The reported bug: a tap must stay green, not turn yellow when the
        # long-press deadline passes later.
        "tap, then wait 2 s": (tap, 2000, GREEN, ["gesture: tap"]),
        "tap within slop": ([(ACTION_DOWN, 100, 100, 0), (ACTION_MOVE, 110, 95, 30 * MS),
                             (ACTION_UP, 115, 90, 80 * MS)], 1000, GREEN, ["gesture: tap"]),
        "long press": ([(ACTION_DOWN, 100, 100, 0), (ACTION_UP, 100, 100, 800 * MS)], 0, YELLOW,
                       ["gesture: long press"]),
        "slow tap (400 ms)": ([(ACTION_DOWN, 100, 100, 0), (ACTION_UP, 100, 100, 400 * MS)], 1000, GREEN,
                              ["gesture: tap"]),
        "swipe right": ([(ACTION_DOWN, 100, 100, 0), (ACTION_MOVE, 160, 105, 40 * MS),
                         (ACTION_UP, 300, 110, 120 * MS)], 1000, RED, ["gesture: swipe right"]),
        "swipe left": ([(ACTION_DOWN, 300, 100, 0), (ACTION_MOVE, 250, 100, 40 * MS),
                        (ACTION_UP, 100, 90, 120 * MS)], 1000, MAGENTA, ["gesture: swipe left"]),
        "swipe down": ([(ACTION_DOWN, 100, 100, 0), (ACTION_MOVE, 100, 160, 40 * MS),
                        (ACTION_UP, 110, 400, 120 * MS)], 1000, CYAN, ["gesture: swipe down"]),
        "swipe up": ([(ACTION_DOWN, 100, 400, 0), (ACTION_MOVE, 100, 340, 40 * MS),
                      (ACTION_UP, 90, 100, 120 * MS)], 1000, ORANGE, ["gesture: swipe up"]),
        "quick flick (no move event)": ([(ACTION_DOWN, 100, 100, 0), (ACTION_UP, 400, 100, 60 * MS)], 1000, RED,
                                        ["gesture: swipe right"]),
        "hold then drag": ([(ACTION_DOWN, 100, 100, 0), (ACTION_MOVE, 300, 100, 700 * MS),
                            (ACTION_UP, 300, 100, 800 * MS)], 0, YELLOW, ["gesture: long press"]),
        "cancel": ([(ACTION_DOWN, 100, 100, 0), (ACTION_CANCEL, 100, 100, 50 * MS)], 1000, BLUE, []),
        "second finger": ([(ACTION_DOWN, 100, 100, 0), (ACTION_POINTER_DOWN, 100, 100, 50 * MS),
                           (ACTION_UP, 100, 100, 100 * MS)], 1000, BLUE, []),
        "two taps": (tap + [(ACTION_DOWN, 50, 50, 500 * MS), (ACTION_UP, 50, 50, 560 * MS)], 1000, GREEN,
                     ["gesture: tap", "gesture: tap"]),
    }
    return table[name]


BACKGROUND = rgb(16, 20, 24)


def check_reserved(library, verbose, fullscreen):
    top, bottom = 4, 3
    framework = Framework(library, verbose, jni={"sdk": 34, "insets": (top, 0, bottom, 0), "fullscreen": fullscreen})
    framework.create()
    framework.show_window()
    framework.attach_input()
    problems = []
    for color, label in ((BLUE, "at start"), (GREEN, "after a tap")):
        if label == "after a tap":
            for action, x, y, at in scenario_events("tap")[0]:
                framework.touch(action, x, y, at)
        pixels = struct.unpack(f"<{WIDTH * HEIGHT}I", framework.lib.read(framework.framebuffer, WIDTH * HEIGHT * 4))
        for y in range(HEIGHT):
            reserved = not fullscreen and (y < top or y >= HEIGHT - bottom)
            expected = BACKGROUND if reserved else color
            row = set(pixels[y * WIDTH:(y + 1) * WIDTH])
            if row != {expected}:
                problems.append(f"{label}: row {y} is {sorted(row)}, expected {expected:#010x}")
    if fullscreen and framework.jni.queries:
        problems.append("the insets were asked for although the app is fullscreen")
    if framework.jni.frames:
        problems.append("JNI local frames left pushed")
    name = "fullscreen: nothing reserved" if fullscreen else "status and navigation bars: only the background"
    if verbose or problems:
        print(f"{'ok  ' if not problems else 'FAIL'} {name}" + "".join(f"\n     {p}" for p in problems[:4]))
    return not problems


def run_scenarios(library, verbose):
    failures = 0
    names = ["tap", "tap, then wait 2 s", "tap within slop", "long press", "slow tap (400 ms)", "swipe right",
             "swipe left", "swipe down", "swipe up", "quick flick (no move event)", "hold then drag", "cancel",
             "second finger", "two taps"]
    for name in names:
        events, wait, expected, expected_log = scenario_events(name)
        framework = Framework(library, verbose)
        framework.create()
        framework.show_window()
        framework.attach_input()
        for action, x, y, at in events:
            framework.touch(action, x, y, at)
        framework.advance(framework.now + wait * MS)
        final = framework.frames[-1]
        logs = gesture_lines(framework)
        ok = final == expected and logs == expected_log and framework.frames[0] == BLUE
        failures += not ok
        colors = " -> ".join(NAMES.get(frame, str(frame)) for frame in framework.frames)
        if verbose or not ok:
            print(f"{'ok  ' if ok else 'FAIL'} {name}: frames {colors}; log {logs}")
        if not ok:
            print(f"     expected final {NAMES[expected]}, log {expected_log}")
    # Rotation: the old instance saves its state, the new one restores it.
    framework = Framework(library, verbose)
    framework.create()
    framework.show_window()
    framework.attach_input()
    for action, x, y, at in scenario_events("swipe up")[0]:
        framework.touch(action, x, y, at)
    size_out = framework.malloc(8)
    saved = framework.callback("onSaveInstanceState", size_out)
    saved_size = framework.u64(size_out)
    framework.callback("onNativeWindowDestroyed", WINDOW)
    framework.callback("onInputQueueDestroyed", QUEUE)
    framework.callback("onDestroy")
    framework.create(saved, saved_size)
    framework.show_window()
    restored = framework.frames[-1]
    ok = saved != 0 and saved_size == 8 and restored == ORANGE and framework.input_callback is None
    failures += not ok
    if verbose or not ok:
        print(f"{'ok  ' if ok else 'FAIL'} rotation keeps the state: restored {NAMES.get(restored, restored)}")
    # Resize and redraw requests repaint with the current state.
    framework.attach_input()
    framework.callback("onNativeWindowRedrawNeeded", WINDOW)
    framework.callback("onNativeWindowResized", WINDOW)
    ok = framework.frames[-2:] == [ORANGE, ORANGE]
    failures += not ok
    if verbose or not ok:
        print(f"{'ok  ' if ok else 'FAIL'} redraw and resize repaint")
    if framework.lib.missing:
        print(f"FAIL unmodeled imports called: {sorted(framework.lib.missing)}")
        failures += 1
    # Not fullscreen: the status bar (top) and navigation bar (bottom) get
    # only the background; the gesture color fills the rest, before and
    # after a tap.
    failures += not check_reserved(library, verbose, fullscreen=False)
    # Fullscreen: nothing reserved, the whole window is the color, and the
    # insets are not even asked for.
    failures += not check_reserved(library, verbose, fullscreen=True)
    total = len(names) + 4
    print(f"{total - failures}/{total} android app scenarios passed")
    return failures


def main():
    arguments = [a for a in sys.argv[1:] if a != "-v"]
    verbose = "-v" in sys.argv[1:]
    with tempfile.TemporaryDirectory() as tmp:
        library = os.path.join(tmp, "libmain.so")
        if not arguments:
            subprocess.run([os.path.join(ROOT, "zig-out/bin/mlx1"), "examples/android/gestures.mlx", "-o", library,
                            "--target=aarch64-android", "--quiet"], cwd=ROOT, check=True)
        elif arguments[0].endswith(".apk"):
            with zipfile.ZipFile(arguments[0]) as apk, open(library, "wb") as out:
                out.write(apk.read("lib/arm64-v8a/libmain.so"))
        else:
            library = arguments[0]
        return 1 if run_scenarios(library, verbose) else 0


if __name__ == "__main__":
    sys.exit(main())
