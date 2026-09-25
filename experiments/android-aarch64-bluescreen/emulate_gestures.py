#!/usr/bin/env python3
"""Run the generated libmain.so's input handling under an AArch64 emulator.

Loads the real library bytes out of a built APK into Unicorn, points every
GOT slot at a stub that emulates the Android/libc function behind it, and
drives the library the way the framework does: ANativeActivity_onCreate,
then whichever callbacks it registered. Touch sequences are fed through the
input queue and the long-press timer, and the test asserts what each one was
recognized as and what color was painted.

Usage: pip install unicorn; python3 emulate_gestures.py bluescreen.apk [-v]
(-v prints the library's logcat output for each scenario.)
"""
import random
import struct
import sys
import zipfile

from unicorn import Uc, UcError, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
import re

from unicorn.arm64_const import (
    UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X2, UC_ARM64_REG_X3,
    UC_ARM64_REG_X4, UC_ARM64_REG_X5, UC_ARM64_REG_X6, UC_ARM64_REG_X7,
    UC_ARM64_REG_X19, UC_ARM64_REG_X30, UC_ARM64_REG_SP, UC_ARM64_REG_S0,
    UC_ARM64_REG_CPACR_EL1,
)

X = [UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X2, UC_ARM64_REG_X3,
     UC_ARM64_REG_X4, UC_ARM64_REG_X5, UC_ARM64_REG_X6, UC_ARM64_REG_X7]
CALLEE_SAVED = [UC_ARM64_REG_X19 + i for i in range(10)]  # x19..x28

BASE = 0x40000000  # where libmain.so is loaded (any 4 KB-aligned base works)
IMAGE_SIZE = 0x10000
STUBS = 0x50000000
JNI_STUBS = 0x50010000
RET_ADDR = 0x50020000
HEAP = 0x51000000
STACK_TOP = 0x52100000
STACK_SIZE = 0x100000
OBJS = 0x53000000  # fake framework objects

ACTIVITY = OBJS
CALLBACKS = OBJS + 0x100
JNI_ENV = OBJS + 0x200
JNI_TABLE = OBJS + 0x300
WINDOW = OBJS + 0x1000
QUEUE = OBJS + 0x2000
LOOPER = OBJS + 0x3000
EVENTS = OBJS + 0x4000
PIXELS = OBJS + 0x8000
WIDTH, HEIGHT, STRIDE = 4, 2, 4

CB_NAMES = {16: "onSaveInstanceState", 56: "onNativeWindowCreated",
            64: "onNativeWindowResized", 72: "onNativeWindowRedrawNeeded",
            80: "onNativeWindowDestroyed", 88: "onInputQueueCreated",
            96: "onInputQueueDestroyed"}

KIND_NAMES = ["none", "tap", "long press", "swipe right", "swipe left",
              "swipe down", "swipe up"]
COLORS = {0: 0xFFFF0000, 1: 0xFF00FF00, 2: 0xFF00FFFF, 3: 0xFF0000FF,
          4: 0xFFFF00FF, 5: 0xFFFFFF00, 6: 0xFF0080FF}

DOWN, UP, MOVE, CANCEL, POINTER_DOWN = 0, 1, 2, 3, 5
TYPE_KEY, TYPE_MOTION = 1, 2
MS = 1_000_000


def load_so(apk):
    with zipfile.ZipFile(apk) as z:
        return z.read("lib/arm64-v8a/libmain.so")


def parse_elf(so):
    """Return (.data vaddr, GOT relocations {got_vaddr: symbol name})."""
    shoff, = struct.unpack_from("<Q", so, 0x28)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", so, 0x3A)
    secs = []
    for i in range(shnum):
        name, typ, _, addr, off, size, link, _, _, entsize = struct.unpack_from(
            "<IIQQQQIIQQ", so, shoff + i * shentsize)
        secs.append((name, typ, addr, off, size, link, entsize))
    shstr_off = secs[shstrndx][3]

    def secname(s):
        end = so.index(b"\0", shstr_off + s[0])
        return so[shstr_off + s[0]:end].decode()

    by_name = {secname(s): s for s in secs}
    dynsym, dynstr, rela = by_name[".dynsym"], by_name[".dynstr"], by_name[".rela.dyn"]

    def symname(i):
        st_name, = struct.unpack_from("<I", so, dynsym[3] + i * 24)
        start = dynstr[3] + st_name
        return so[start:so.index(b"\0", start)].decode()

    relocs = {}
    for i in range(rela[4] // 24):
        r_off, r_info, _ = struct.unpack_from("<QQq", so, rela[3] + i * 24)
        assert r_info & 0xFFFFFFFF == 1025, "only R_AARCH64_GLOB_DAT expected"
        relocs[r_off] = symname(r_info >> 32)
    return by_name[".data"][2], relocs


class Android:
    """Emulated framework + libc state, and the machine running libmain.so."""

    def __init__(self, so, timerfd_fails=False, seed=1):
        self.rng = random.Random(seed)
        self.timerfd_fails = timerfd_fails
        self.data, relocs = parse_elf(so)
        self.uc = uc = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
        uc.reg_write(UC_ARM64_REG_CPACR_EL1, 3 << 20)  # enable FP/SIMD
        uc.mem_map(BASE, IMAGE_SIZE)
        uc.mem_write(BASE, so[:IMAGE_SIZE])
        for region, size in [(STUBS, 0x30000), (HEAP, 0x10000),
                             (STACK_TOP - STACK_SIZE, STACK_SIZE), (OBJS, 0x10000)]:
            uc.mem_map(region, size)

        # One `ret` per import stub, per JNI function-table entry, and at RET_ADDR.
        ret = struct.pack("<I", 0xD65F03C0)
        self.stub_names = {}
        for i, (got, name) in enumerate(sorted(relocs.items())):
            addr = STUBS + i * 4
            uc.mem_write(addr, ret)
            uc.mem_write(BASE + got, struct.pack("<Q", addr))
            self.stub_names[addr] = name
        for i in range(300):
            uc.mem_write(JNI_STUBS + i * 4, ret)
            uc.mem_write(JNI_TABLE + i * 8, struct.pack("<Q", JNI_STUBS + i * 4))
        uc.mem_write(RET_ADDR, ret)
        uc.mem_write(JNI_ENV, struct.pack("<Q", JNI_TABLE))
        uc.mem_write(ACTIVITY, struct.pack("<QQQQ", CALLBACKS, 0, JNI_ENV, 0x7777))
        uc.hook_add(UC_HOOK_CODE, self._on_stub, begin=STUBS, end=STUBS + 0x20000)

        self.heap_next = HEAP
        self.events = []  # pending (type, action, x, y, time_ns)
        self.finished = []  # (event handle, handled)
        self.paints = []  # first pixel color of each posted frame
        self.calls = []  # (name, args) of every stub call
        self.input_cb = self.input_data = None
        self.timer_cb = self.timer_fd = None
        self.timer_armed_ns = 0
        self.jni_set_int = []
        self.logs = []

    # ── stubs ────────────────────────────────────────────────────────────
    def reg(self, i):
        return self.uc.reg_read(X[i])

    def ret_int(self, v):
        # An `int` return only defines w0: fill x0's upper half with junk to
        # catch any code that wrongly uses it as a 64-bit value.
        junk = self.rng.getrandbits(32) << 32
        self.uc.reg_write(UC_ARM64_REG_X0, junk | (v & 0xFFFFFFFF))

    def ret_ptr(self, v):
        self.uc.reg_write(UC_ARM64_REG_X0, v & 0xFFFFFFFFFFFFFFFF)

    def cstr(self, addr):
        out = b""
        while True:
            c = bytes(self.uc.mem_read(addr + len(out), 1))
            if c == b"\0":
                return out.decode()
            out += c

    def format_log(self, fmt, vals):
        # The subset of printf the library's format strings use.
        it = iter(vals)

        def conv(m):
            v = next(it)
            spec = m.group(1)
            if spec == "lld":
                return str(v - (1 << 64) if v >> 63 else v)
            v &= 0xFFFFFFFF
            if spec == "x":
                return "%x" % v
            return str(v - (1 << 32) if v >> 31 else v)
        return re.sub(r"%(lld|d|x)", conv, fmt)

    def event(self, handle):
        idx = (handle - EVENTS) // 16
        return self.current_events[idx]

    def _on_stub(self, uc, addr, size, _):
        if addr >= JNI_STUBS:
            self._on_jni((addr - JNI_STUBS) // 4)
            return
        name = self.stub_names[addr]
        args = [self.reg(i) for i in range(8)]
        self.calls.append((name, args))
        if name == "ANativeWindow_setBuffersGeometry":
            assert args[0] == WINDOW and args[3] & 0xFFFFFFFF == 1
            self.ret_int(0)
        elif name == "ANativeWindow_lock":
            assert args[0] == WINDOW, "lock on a window that isn't the live one"
            uc.mem_write(args[1], struct.pack("<iiiiQ", WIDTH, HEIGHT, STRIDE, 1, PIXELS))
            uc.mem_write(PIXELS, b"\0" * (STRIDE * HEIGHT * 4))
            self.ret_int(0)
        elif name == "ANativeWindow_unlockAndPost":
            px = struct.unpack("<%dI" % (STRIDE * HEIGHT),
                               uc.mem_read(PIXELS, STRIDE * HEIGHT * 4))
            assert len(set(px)) == 1, "frame not filled with a single color"
            self.paints.append(px[0])
            self.ret_int(0)
        elif name == "ALooper_forThread":
            self.ret_ptr(LOOPER)
        elif name == "AInputQueue_attachLooper":
            assert args[0] == QUEUE and args[1] == LOOPER and args[2] & 0xFFFFFFFF == 1
            self.input_cb, self.input_data = args[3], args[4]
        elif name == "AInputQueue_detachLooper":
            assert args[0] == QUEUE
            self.input_cb = None
        elif name == "AInputQueue_getEvent":
            assert args[0] == QUEUE
            if self.next_event < len(self.current_events):
                handle = EVENTS + self.next_event * 16
                self.next_event += 1
                uc.mem_write(args[1], struct.pack("<Q", handle))
                self.ret_int(0)
            else:
                self.ret_int(-1)
        elif name == "AInputQueue_finishEvent":
            assert args[0] == QUEUE
            self.finished.append((args[1], args[2] & 0xFFFFFFFF))
        elif name == "AInputEvent_getType":
            self.ret_int(self.event(args[0])[0])
        elif name == "AMotionEvent_getAction":
            self.ret_int(self.event(args[0])[1])
        elif name in ("AMotionEvent_getX", "AMotionEvent_getY"):
            assert args[1] == 0, "expected pointer index 0"
            ev = self.event(args[0])
            v = ev[2] if name.endswith("X") else ev[3]
            uc.reg_write(UC_ARM64_REG_S0, struct.unpack("<I", struct.pack("<f", v))[0])
        elif name == "AMotionEvent_getEventTime":
            self.ret_ptr(self.event(args[0])[4])
        elif name == "timerfd_create":
            assert args[0] & 0xFFFFFFFF == 1, "expected CLOCK_MONOTONIC"
            assert args[1] & 0xFFFFFFFF == 0x80800, "expected TFD_NONBLOCK|TFD_CLOEXEC"
            self.ret_int(-1 if self.timerfd_fails else 7)
        elif name == "ALooper_addFd":
            assert args[0] == LOOPER and args[1] & 0xFFFFFFFF == 7
            assert args[2] & 0xFFFFFFFF == 2 and args[3] & 0xFFFFFFFF == 1 and args[5] == 0
            self.timer_cb, self.timer_fd = args[4], 7
            self.ret_int(1)
        elif name == "timerfd_settime":
            assert args[0] & 0xFFFFFFFF == 7 and args[1] & 0xFFFFFFFF == 0 and args[3] == 0
            it = struct.unpack("<qqqq", uc.mem_read(args[2], 32))
            assert it[:3] == (0, 0, 0), "expected a one-shot, sub-second timer"
            self.timer_armed_ns = it[3]
            self.ret_int(0)
        elif name == "read":
            assert args[0] & 0xFFFFFFFF == 7 and args[2] == 8
            uc.mem_write(args[1], struct.pack("<Q", 1))
            self.ret_ptr(8)
        elif name == "__android_log_print":
            assert args[0] & 0xFFFFFFFF == 4 and self.cstr(args[1]) == "mlx"
            self.logs.append(self.format_log(self.cstr(args[2]), args[3:8]))
            self.ret_int(0)
        elif name == "malloc":
            p = self.heap_next
            self.heap_next += (args[0] + 15) & ~15
            self.ret_ptr(p)
        else:
            raise AssertionError("unexpected import " + name)

    def _on_jni(self, index):
        # Every JNI call succeeds with a distinct non-NULL handle.
        if index == 109:  # SetIntField(env, obj, fieldID, value)
            self.jni_set_int.append(self.reg(3) & 0xFFFFFFFF)
        self.ret_ptr(0x6000 + index)

    # ── driving the library ──────────────────────────────────────────────
    def call(self, fn, *args):
        uc = self.uc
        sentinels = [0x1900 + i for i in range(10)]
        for r, v in zip(CALLEE_SAVED, sentinels):
            uc.reg_write(r, v)
        for i, a in enumerate(args):
            uc.reg_write(X[i], a)
        uc.reg_write(UC_ARM64_REG_SP, STACK_TOP - 0x100)
        uc.reg_write(UC_ARM64_REG_X30, RET_ADDR)
        uc.emu_start(fn, RET_ADDR, count=100000)
        assert uc.reg_read(UC_ARM64_REG_SP) == STACK_TOP - 0x100, "SP not restored"
        assert [uc.reg_read(r) for r in CALLEE_SAVED] == sentinels, "callee-saved reg clobbered"
        return uc.reg_read(UC_ARM64_REG_X0)

    def u32(self, off):
        return struct.unpack("<I", self.uc.mem_read(BASE + self.data + off, 4))[0]

    def callback(self, off):
        return struct.unpack("<Q", self.uc.mem_read(CALLBACKS + off, 8))[0]

    def deliver(self, *events):
        """Queue events and run the input-queue callback once, as the looper would."""
        self.current_events = list(events)
        self.next_event = 0
        before = len(self.finished)
        r = self.call(self.input_cb, 3, 1, self.input_data)
        assert r & 0xFFFFFFFF == 1, "input callback must return 1"
        done = self.finished[before:]
        assert len(done) == len(events), "every event must be finished exactly once"
        assert all(h == 0 for _, h in done), "events are reported unhandled"

    def fire_timer(self):
        r = self.call(self.timer_cb, self.timer_fd, 1, 0)
        assert r & 0xFFFFFFFF == 1, "timer callback must return 1"

    @property
    def kind(self):
        return self.u32(4)


def touch(action, x, y, t_ms, typ=TYPE_MOTION):
    return (typ, action, float(x), float(y), int(t_ms * MS))


def start(so, **kw):
    a = Android(so, **kw)
    a.call(BASE + 0x4000, ACTIVITY, 0, 0)  # ANativeActivity_onCreate
    for off in range(0, 128, 8):
        if off in CB_NAMES:
            assert a.callback(off), CB_NAMES[off] + " not registered"
        else:
            assert a.callback(off) == 0, "unexpected callback at %d" % off
    assert a.jni_set_int == [1], "display-cutout mode not set to SHORT_EDGES"
    a.call(a.callback(56), ACTIVITY, WINDOW)  # onNativeWindowCreated
    assert a.paints == [COLORS[0]]
    a.call(a.callback(88), ACTIVITY, QUEUE)  # onInputQueueCreated
    assert a.input_cb and a.input_data == QUEUE
    return a


VERBOSE = "-v" in sys.argv


def check(a, name, expected_kind, expected_paints):
    got = KIND_NAMES[a.kind]
    painted = [next(k for k, c in COLORS.items() if c == p) for p in a.paints]
    ok = a.kind == expected_kind and painted == expected_paints
    print("%s %-44s -> %-11s painted %s" % ("PASS" if ok else "FAIL", name, got,
                                           [KIND_NAMES[k] for k in painted]))
    if VERBOSE:
        for line in a.logs:
            print("       mlx: " + line)
    return ok


def scenario(so, name, steps, expected_kind, expected_paint_kinds, **kw):
    a = start(so, **kw)
    a.paints.clear()
    a.logs.clear()
    for step in steps:
        step(a)
    return check(a, name, expected_kind, expected_paint_kinds)


def run(so):
    D = lambda *ev: (lambda a: a.deliver(*ev))
    T = lambda a: a.fire_timer()
    results = []

    def armed(ns):
        def f(a):
            assert a.timer_armed_ns == ns, "timer %d, expected %d" % (a.timer_armed_ns, ns)
        return f

    results.append(scenario(so, "tap", [
        D(touch(DOWN, 100, 100, 0)), armed(500 * MS),
        D(touch(UP, 110, 95, 120)), armed(0)], 1, [1]))
    results.append(scenario(so, "long press, recognized while held", [
        D(touch(DOWN, 100, 100, 0)), T, D(touch(MOVE, 104, 102, 700)),
        D(touch(UP, 104, 102, 900))], 2, [2]))
    results.append(scenario(so, "long press, on release (timer never ran)", [
        D(touch(DOWN, 100, 100, 0)), D(touch(UP, 101, 101, 650))], 2, [2]))
    results.append(scenario(so, "held 499 ms is still a tap", [
        D(touch(DOWN, 100, 100, 0)), D(touch(UP, 100, 100, 499))], 1, [1]))
    results.append(scenario(so, "swipe right (with moves)", [
        D(touch(DOWN, 100, 100, 0)), D(touch(MOVE, 110, 100, 10)), armed(500 * MS),
        D(touch(MOVE, 300, 110, 40)), armed(0), T,
        D(touch(UP, 400, 120, 80))], 3, [3]))
    results.append(scenario(so, "swipe left (no moves)", [
        D(touch(DOWN, 500, 500, 0)), D(touch(UP, 200, 480, 90))], 4, [4]))
    results.append(scenario(so, "swipe down", [
        D(touch(DOWN, 100, 100, 0), touch(MOVE, 100, 200, 30), touch(UP, 105, 400, 60))],
        5, [5]))
    results.append(scenario(so, "swipe up", [
        D(touch(DOWN, 100, 800, 0)), D(touch(UP, 110, 300, 70))], 6, [6]))
    results.append(scenario(so, "slow swipe (held > 500 ms) is still a swipe", [
        D(touch(DOWN, 100, 100, 0)), D(touch(MOVE, 180, 100, 300)),
        D(touch(UP, 600, 100, 1500))], 3, [3]))
    results.append(scenario(so, "exactly 24 px is inside the slop", [
        D(touch(DOWN, 100, 100, 0)), D(touch(MOVE, 124, 76, 50)),
        D(touch(UP, 124, 76, 100))], 1, [1]))
    results.append(scenario(so, "25 px is a swipe", [
        D(touch(DOWN, 100, 100, 0)), D(touch(UP, 75, 100, 100))], 4, [4]))
    results.append(scenario(so, "cancel abandons the touch", [
        D(touch(DOWN, 100, 100, 0)), D(touch(CANCEL, 100, 100, 50)), armed(0), T,
        D(touch(UP, 100, 100, 900))], 0, []))
    results.append(scenario(so, "second finger abandons the touch", [
        D(touch(DOWN, 100, 100, 0), touch(POINTER_DOWN | (1 << 8), 300, 300, 20)),
        T, D(touch(UP, 100, 100, 900))], 0, []))
    results.append(scenario(so, "non-touch events are ignored", [
        D(touch(DOWN, 100, 100, 0, typ=TYPE_KEY), touch(UP, 100, 100, 50, typ=TYPE_KEY))],
        0, []))
    results.append(scenario(so, "timer after window destroyed: no paint", [
        D(touch(DOWN, 100, 100, 0)), lambda a: a.call(a.callback(80), ACTIVITY, WINDOW), T],
        2, []))

    # Timer creation fails: long press still recognized, on release.
    results.append(scenario(so, "no timerfd: long press on release", [
        lambda a: (a.timer_cb is None) or (_ for _ in ()).throw(AssertionError("addFd ran")),
        D(touch(DOWN, 100, 100, 0)), D(touch(UP, 100, 100, 700))], 2, [2],
        timerfd_fails=True))

    # Rotation: the recognized kind survives onSaveInstanceState -> onCreate.
    a = start(so)
    a.deliver(touch(DOWN, 100, 800, 0), touch(UP, 100, 200, 60))  # swipe up
    out_len = HEAP + 0x8000
    saved = a.call(a.callback(16), ACTIVITY, out_len)
    size = struct.unpack("<Q", a.uc.mem_read(out_len, 8))[0]
    blob = struct.unpack("<I", a.uc.mem_read(saved, 4))[0]
    a.call(a.callback(96), ACTIVITY, QUEUE)  # onInputQueueDestroyed
    assert a.input_cb is None
    a.paints.clear()
    a.logs.clear()
    a.call(BASE + 0x4000, ACTIVITY, saved, size)  # new instance
    a.call(a.callback(56), ACTIVITY, WINDOW)
    results.append(check(a, "rotation keeps the last gesture (saved %d/%d B)" % (blob, size),
                         6, [6]))

    print("%d/%d passed" % (sum(results), len(results)))
    return all(results)


if __name__ == "__main__":
    try:
        ok = run(load_so(sys.argv[1]))
    except (AssertionError, UcError) as e:
        print("ERROR:", e)
        ok = False
    sys.exit(0 if ok else 1)
