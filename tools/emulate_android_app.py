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


class Framework:
    """The Android side of one app process."""

    def __init__(self, library_path, verbose=False):
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
        """Fills more of the ANativeActivity before onCreate (subclasses)."""

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
    total = len(names) + 2
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
