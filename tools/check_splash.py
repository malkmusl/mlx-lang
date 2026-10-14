#!/usr/bin/env python3
"""mlx-splash end to end, with no screen, card or terminal of the machine's:

- headless (--output none --no-tty): the Plymouth protocol as systemd and
  the plymouth tool speak it (an update sent without reading the answer,
  arguments, a malformed request, a second splash on the taken socket,
  passwords and questions refused, a non-root sender refused when setpriv
  can switch to nobody), mlx-splashctl's commands, quit answered only when
  done, the boot's length kept, the snapshot drawn;
- on an emulated KMS card (tools/fake_drm_session.py's FakeCard, handed
  in with --card-fd): the mode set on the connected connector with the
  splash's own framebuffer, page flips, the frame's pixels (the indigo
  night and the lavender letters), stepping aside while a password is
  asked on the console (the console's framebuffer back on the CRTC) and
  coming back, deactivate and reactivate (DRM master given away and taken
  back, no flips in between), quit restoring the console, and quit
  --retain-splash leaving the last frame on the CRTC (DRM_IOCTL_MODE_CLOSEFB)
  with the console not put back, and the card going away under it (the
  splash says so and goes on answering; given a card's descriptor it
  looks for no other);
- a plymouthd holding the socket (a process of that name) asked to quit
  retaining its picture, and the splash taking over.

Usage: check_splash.py MLX_SPLASH MLX_SPLASHCTL WORKDIR
"""
import os
import socket
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fake_drm_session as fake  # noqa: E402

CLOSEFB = 3221775568
SPLASH, SPLASHCTL, WORK = sys.argv[1:4]


def fail(message):
    print(f"FAIL {message}", file=sys.stderr)
    raise SystemExit(1)


def ok(message):
    print(f"ok   {message}")


def connect(name):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect("\0" + name)
    return s


def request(name, command, argument=None, read=True):
    s = connect(name)
    if argument is None:
        data = command + b"\0"
    else:
        data = command + b"\x02" + bytes([len(argument) + 1]) + argument + b"\0"
    s.sendall(data)
    if not read:
        s.close()
        return None
    s.settimeout(10)
    reply = s.recv(1)
    s.close()
    return reply


def wait_socket(name, process, timeout=10):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if process.poll() is not None:
            fail(f"mlx-splash ended early ({process.returncode}): {open(process.log_path).read()}")
        try:
            connect(name).close()
            return
        except OSError:
            time.sleep(0.02)
    fail("mlx-splash never listened")


def start(name, tag, extra, pass_fds=()):
    log_path = os.path.join(WORK, f"{tag}.log")
    log = open(log_path, "w")
    process = subprocess.Popen([SPLASH, "--no-tty", "--socket", name,
                                "--duration-file", os.path.join(WORK, f"{tag}-duration"),
                                "--ask-password-dir", os.path.join(WORK, f"{tag}-ask")] + extra,
                               stdout=log, stderr=log, pass_fds=pass_fds)
    process.log_path = log_path
    os.makedirs(os.path.join(WORK, f"{tag}-ask"), exist_ok=True)
    return process


def ctl(name, *words):
    return subprocess.run([SPLASHCTL, "--socket", name] + list(words), capture_output=True, text=True, timeout=20)


ACK, NAK = b"\x06", b"\x15"


def headless():
    name = f"/mlx-splash-check-{os.getpid()}"
    snapshot = os.path.join(WORK, "headless.png")
    splash = start(name, "headless", ["--output", "none", "--size", "1280x720", "--snapshot", snapshot])
    wait_socket(name, splash)
    if request(name, b"P") != ACK:
        fail("ping not answered ACK")
    # systemd: the unit's name, nonblocking, the answer never read.
    request(name, b"U", b"systemd-udevd.service", read=False)
    request(name, b"U", b"NetworkManager.service", read=False)
    if request(name, b"M", b"Checking the disk") != ACK:
        fail("display-message not answered ACK")
    for command, what in ((b"*", "a password"), (b"c", "a cached password"), (b"W", "a question"), (b"K", "a keystroke"), (b"Z", "an unknown request")):
        if request(name, command, b"Passphrase" if command in (b"*", b"W") else None) != NAK:
            fail(f"{what} not refused")
    ok("the protocol: ping, updates sent as systemd sends them, a message; passwords, questions, keys refused")
    # A malformed request drops that connection only.
    s = connect(name)
    s.sendall(b"P\x05")
    s.settimeout(5)
    if s.recv(1) != b"":
        fail("a malformed request was answered")
    s.close()
    if request(name, b"P") != ACK:
        fail("the splash stopped answering after a malformed request")
    ok("a malformed request drops its connection, the splash goes on")
    # A second splash finds the socket taken.
    second = start(name, "second", ["--output", "none"])
    if second.wait(timeout=10) != 1 or "taken" not in open(second.log_path).read():
        fail("a second splash did not give up on the taken socket")
    ok("a second splash gives up on the taken socket")
    # Not root: NAK (setpriv drops to nobody, no groups).
    nobody = subprocess.run(["setpriv", "--reuid=65534", "--regid=65534", "--clear-groups", sys.executable, "-c",
                             "import socket,sys; s=socket.socket(socket.AF_UNIX); s.connect('\\0'+sys.argv[1]); "
                             "s.sendall(b'Q\\x02\\x01\\x00'); print(s.recv(1).hex())", name],
                            capture_output=True, text=True, timeout=20)
    if nobody.returncode == 0:
        if nobody.stdout.strip() != "15":
            fail(f"a request from nobody was answered {nobody.stdout.strip()!r}")
        ok("a request from another user than root is refused")
    else:
        print("skip a request from another user (setpriv could not switch):", nobody.stderr.strip())
    # mlx-splashctl.
    for words in (("ping",), ("update", "--status=dbus.service"), ("change-mode", "--updates"), ("system-update", "--progress=40"),
                  ("change-mode", "boot-up"), ("message", "Welcome"), ("hide-message", "Welcome"), ("pause-progress",),
                  ("unpause-progress",), ("--sysinit",)):
        result = ctl(name, *words)
        if result.returncode != 0:
            fail(f"mlx-splashctl {' '.join(words)}: {result.returncode} {result.stderr}")
    if ctl(name, "has-active-vt").returncode != 1:
        fail("has-active-vt said yes without a terminal")
    if ctl(name, "bogus").returncode != 2:
        fail("mlx-splashctl took an unknown command")
    ok("mlx-splashctl: ping, update, change-mode, system-update, messages, progress pause, has-active-vt")
    time.sleep(1.2)
    # quit is answered once the splash is done: wait finds it gone.
    waiter = subprocess.Popen([SPLASHCTL, "--socket", name, "wait"])
    began = time.time()
    result = ctl(name, "quit")
    if result.returncode != 0:
        fail(f"quit: {result.returncode} {result.stderr}")
    if splash.poll() is None:
        try:
            splash.wait(timeout=2)
        except subprocess.TimeoutExpired:
            fail("quit was answered before the splash ended")
    if splash.returncode != 0:
        fail(f"mlx-splash ended {splash.returncode}: {open(splash.log_path).read()}")
    if waiter.wait(timeout=5) != 0:
        fail("mlx-splashctl wait did not return when the splash ended")
    if time.time() - began < 0.3:
        fail("quit did not fade the picture out first")
    duration = open(os.path.join(WORK, "headless-duration")).read()
    if not duration.endswith("\n") or not duration[:-1].replace(".", "", 1).isdigit() or float(duration) < 1.0:
        fail(f"the boot's length was kept as {duration!r}")
    if os.path.getsize(snapshot) < 40000:
        fail("the snapshot is (nearly) empty")
    ok(f"quit: answered when done, wait returns, the boot's length kept ({duration.strip()} s), the snapshot drawn")


class Harness:
    def __init__(self):
        self.recorder = fake.Recorder()
        self.memory = None

    def record(self, *call):
        self.recorder.add(*call)


def fake_plymouthd(name):
    """A process called plymouthd on the socket, answering one request
    (quit, as mlx-splash should send it) and ending."""
    script = (
        "import ctypes, socket, sys\n"
        "ctypes.CDLL(None).prctl(15, b'plymouthd', 0, 0, 0)\n"
        "s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)\n"
        "s.bind('\\0' + sys.argv[1]); s.listen(4)\n"
        "print('listening', flush=True)\n"
        "while True:\n"
        "    c, _ = s.accept()\n"
        "    data = c.recv(64)\n"
        "    if not data:\n"
        "        c.close(); continue\n"
        "    print(data.hex(), flush=True)\n"
        "    c.sendall(b'\\x06'); c.close()\n"
        "    if data[:1] == b'Q':\n"
        "        break\n"
    )
    return subprocess.Popen([sys.executable, "-c", script, name], stdout=subprocess.PIPE, text=True)


def take_over():
    name = f"/mlx-splash-plymouthd-{os.getpid()}"
    holder = fake_plymouthd(name)
    if holder.stdout.readline().strip() != "listening":
        fail("the stand-in plymouthd did not start")
    splash = start(name, "takeover", ["--output", "none"])
    asked = holder.stdout.readline().strip()
    if asked != "51020201" + "00":
        fail(f"plymouthd was asked {asked!r}, not quit --retain-splash")
    holder.wait(timeout=5)
    wait_socket(name, splash)
    if request(name, b"P") != ACK:
        fail("the splash did not answer after taking over")
    if ctl(name, "quit").returncode != 0 or splash.wait(timeout=10) != 0:
        fail("the splash did not quit after taking over")
    if "taking over from plymouthd" not in open(splash.log_path).read():
        fail("the splash did not say it took over")
    ok("plymouthd on the socket: asked to quit retaining its picture, the splash takes over")


class SplashCard(fake.FakeCard):
    """The fake card with DRM_IOCTL_MODE_CLOSEFB (Linux 6.8): the
    framebuffer is the program's no more but stays on the CRTC."""

    def __init__(self, harness):
        super().__init__(harness)
        self.retained = None
        self.gone = False

    def ioctl(self, request, argument):
        if self.gone:
            # Unplugged (simpledrm when the GPU's driver takes over).
            self.harness.record("drm", "gone", request)
            return -19, argument, None
        if request == CLOSEFB:
            if len(argument) != 8:
                fail(f"CLOSEFB with {len(argument)} argument bytes")
            fb = struct.unpack_from("<I", argument)[0]
            if fb not in self.framebuffers:
                return -fake.EINVAL, argument, None
            # What is on screen stays readable after the buffer's handle
            # is destroyed.
            memfd, size, pitch = self.buffers[self.framebuffers[fb]]
            self.retained = (fb, os.pread(memfd, size, 0), pitch)
            self.framebuffers.pop(fb)
            self.harness.record("drm", "closefb", fb)
            return 0, argument, None
        return super().ioctl(request, argument)

    def frame(self):
        handle = self.framebuffers[self.shown]
        memfd, size, pitch = self.buffers[handle]
        return os.pread(memfd, size, 0), pitch


def wait_after(record, mark, predicate, timeout, what):
    """A call matching `predicate` among those after the first `mark`."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        with record.lock:
            found = [c for c in record.calls[mark:] if predicate(c)]
        if found:
            return found
        time.sleep(0.02)
    fail(f"timed out waiting for {what}; calls: {record.calls[-12:]}")


def flips_after(record, mark):
    with record.lock:
        return len([c for c in record.calls[mark:] if c[:2] == ("drm", "page_flip")])


def look(frame, pitch, width, height):
    """The night's darkness at the corners and the letters' lavender in
    the middle."""
    def pixel(x, y):
        value = struct.unpack_from("<I", frame, y * pitch + x * 4)[0]
        return (value >> 16) & 255, (value >> 8) & 255, value & 255
    corner = pixel(4, 4)
    if max(corner) > 40 or corner[2] < corner[1]:
        fail(f"the corner is {corner}, not the dark indigo night")
    lavender = 0
    for y in range(height // 2 - 40, height // 2 + 40, 4):
        for x in range(width // 2 - 160, width // 2 + 160, 4):
            r, g, b = pixel(x, y)
            if r > 150 and b > 200 and g > 120:
                lavender += 1
    if lavender < 60:
        fail(f"only {lavender} lavender pixels where the letters are")
    return lavender


def card_gone():
    harness = Harness()
    card = SplashCard(harness)
    name = f"/mlx-splash-gone-{os.getpid()}"
    descriptor = card.take()
    splash = start(name, "gone", ["--card-fd", str(descriptor)], pass_fds=(descriptor,))
    os.close(descriptor)
    harness.memory = fake.Memory(splash.pid)
    card.start()
    harness.recorder.wait(lambda c: c[:2] == ("drm", "page_flip"), 10, "page flips")
    card.gone = True
    deadline = time.time() + 5
    while "the card went away" not in open(splash.log_path).read():
        if time.time() > deadline or splash.poll() is not None:
            fail(f"the splash did not notice the card going: {open(splash.log_path).read()}")
        time.sleep(0.05)
    wait_socket(name, splash)
    if request(name, b"P") != ACK:
        fail("the splash stopped answering without its card")
    time.sleep(1.5)
    if "showing on the new card" in open(splash.log_path).read():
        fail("given a card's descriptor, the splash looked for another")
    if ctl(name, "quit").returncode != 0 or splash.wait(timeout=10) != 0:
        fail("the splash did not quit without its card")
    card.revoke()
    ok("the card going away: said, answering on, no other card looked for (a descriptor was given)")


def kms(retain):
    harness = Harness()
    card = SplashCard(harness)
    name = f"/mlx-splash-kms-{os.getpid()}-{int(retain)}"
    descriptor = card.take()
    splash = start(name, f"kms{int(retain)}", ["--card-fd", str(descriptor)], pass_fds=(descriptor,))
    os.close(descriptor)
    harness.memory = fake.Memory(splash.pid)
    card.start()
    record = harness.recorder
    calls = record.wait(lambda c: c[:2] == ("drm", "setcrtc") and c[3] != fake.CONSOLE_FB and c[3] != 0, 10, "the mode set")
    _, _, crtc, fb, connectors, valid, width, height = calls[0]
    if connectors != [42] or (width, height) != (fake.WIDTH, fake.HEIGHT):
        fail(f"modeset on {connectors} at {width}x{height}")
    wait_socket(name, splash)
    record.wait(lambda c: c[:2] == ("drm", "page_flip"), 5, "page flips")
    time.sleep(1.6)
    frame, pitch = card.frame()
    lavender = look(frame, pitch, fake.WIDTH, fake.HEIGHT)
    if not retain:
        ok(f"KMS: the mode set on the eDP connector ({width}x{height}), page flips, the night and the letters on screen ({lavender} lavender samples)")
        # A password asked on the console: the console back on the CRTC.
        ask = os.path.join(WORK, "kms0-ask", "ask.check")
        open(ask, "w").close()
        mark = len(record.calls)
        wait_after(record, mark, lambda c: c[:2] == ("drm", "setcrtc") and c[3] == fake.CONSOLE_FB, 5, "the console shown for a password")
        time.sleep(0.1)
        mark = len(record.calls)
        time.sleep(0.6)
        if flips_after(record, mark) > 0:
            fail("frames went on while the console was shown")
        os.unlink(ask)
        wait_after(record, mark, lambda c: c[:2] == ("drm", "setcrtc") and c[3] not in (fake.CONSOLE_FB, 0), 5, "the splash back after the password")
        ok("a password asked on the console: the console shown, no frames, the splash back after")
        # Deactivate and reactivate.
        if ctl(name, "deactivate").returncode != 0:
            fail("deactivate not answered ACK")
        record.wait(lambda c: c[:2] == ("drm", "drop_master"), 5, "DRM master dropped")
        time.sleep(0.1)
        mark = len(record.calls)
        time.sleep(0.5)
        if flips_after(record, mark) > 0:
            fail("frames went on while deactivated")
        if ctl(name, "reactivate").returncode != 0:
            fail("reactivate not answered ACK")
        wait_after(record, mark, lambda c: c[:2] == ("drm", "set_master"), 5, "DRM master taken back")
        wait_after(record, mark, lambda c: c[:2] == ("drm", "setcrtc") and c[3] not in (fake.CONSOLE_FB, 0), 5, "the mode set again")
        deadline = time.time() + 5
        while flips_after(record, mark) < 3:
            if time.time() > deadline:
                fail("no frames after reactivate")
            time.sleep(0.02)
        ok("deactivate gives DRM master away and stops frames, reactivate takes it back")
        if ctl(name, "quit").returncode != 0:
            fail("quit not answered ACK")
        if splash.wait(timeout=10) != 0:
            fail(f"mlx-splash ended {splash.returncode}: {open(splash.log_path).read()}")
        if card.shown != fake.CONSOLE_FB:
            fail(f"after quit the CRTC shows {card.shown}, not the console")
        if card.framebuffers or card.buffers:
            fail(f"buffers left: {card.framebuffers} {card.buffers}")
        ok("quit: the console's framebuffer back on the CRTC, every buffer freed")
    else:
        if ctl(name, "quit", "--retain-splash").returncode != 0:
            fail("quit --retain-splash not answered ACK")
        if splash.wait(timeout=10) != 0:
            fail(f"mlx-splash ended {splash.returncode}: {open(splash.log_path).read()}")
        if card.retained is None or card.retained[0] != card.shown:
            fail(f"the frame on screen ({card.shown}) was not closed with CLOSEFB: {record.calls[-8:]}")
        if record.count(lambda c: c[:2] == ("drm", "setcrtc") and c[3] == fake.CONSOLE_FB):
            fail("the console was put back though the splash was to stay")
        look(card.retained[1], card.retained[2], fake.WIDTH, fake.HEIGHT)
        record.wait(lambda c: c[:2] == ("drm", "drop_master"), 5, "DRM master dropped")
        ok("quit --retain-splash: the last frame stays on the CRTC (CLOSEFB), the console is not put back")
    card.revoke()


headless()
take_over()
kms(False)
kms(True)
card_gone()
