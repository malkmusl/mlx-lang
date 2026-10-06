#!/usr/bin/env python3
"""Runs Files on Android: projects/desktop/files/main.mlx, the same source
as the desktop's mlx-files, built for aarch64-android (std.ui.host) and run
against the Android/Vulkan model.

The fixture is a real host directory exposed as ANativeActivity's
internalDataPath (the app's home). The app must draw with Vulkan by
default (no CPU frame), lay out a phone, compact or wide view by the
window's size and density, open a folder with a tap, open its menu with a
long press and make a folder from it, rename that folder with typed keys
(the on-screen keyboard shown and hidden), go back, filter by the search,
scroll with a drag without opening anything, and fall back to the CPU
without Vulkan.

Usage: python3 tools/emulate_android_files.py [-v] [app.apk | libmain.so]
With no file, the script builds projects/desktop/files/main.mlx.
"""
import os
import subprocess
import sys
import tempfile
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from emulate_android_app import (  # noqa: E402
    ACTION_DOWN,
    ACTION_MOVE,
    ACTION_UP,
    MS,
    QUEUE,
    WINDOW,
    compiler,
)
from emulate_vulkan_android import App as VulkanApp  # noqa: E402

# Colours as an RGBA_8888 frame holds them (red in the low byte).
FOLDER = 0xFFF0A05C       # the folder icon's front (92, 160, 240)
BACKGROUND = 0xFF28201E   # the content (filemanager-shared/ui.mlx BACKGROUND)
TOOLBAR = 0xFF2F2624      # the toolbar (36, 38, 47)

# Android key codes and meta state (AKEYCODE_*, AMETA_SHIFT_ON).
KEYCODE_ENTER = 66
META_SHIFT_ON = 1
KEY_ACTION_DOWN, KEY_ACTION_UP = 0, 1


def keycode(letter):
    return 29 + ord(letter.lower()) - ord("a")


class FilesFramework(VulkanApp):
    def __init__(self, library, data_path, verbose=False, vulkan=True, size=(360, 640)):
        self.data_path = os.fsencode(data_path) + b"\0"
        self.handled = []
        self.ime_shown = []
        self.ime_hidden = []
        self.key_events = {}
        super().__init__(library, available=vulkan, verbose=verbose, jni={"sdk": 34, "insets": (24, 0, 30, 0)})
        self.vulkan.first_pipeline = "paint"
        self.vulkan.extent = size
        self.width, self.height = size
        self.framebuffer = self.malloc(size[0] * size[1] * 4)

    def _imports(self):
        imports = super()._imports()
        imports["AInputQueue_finishEvent"] = self._finish_event
        imports["ANativeActivity_showSoftInput"] = lambda p, activity, flags, *_: self.ime_shown.append(flags & 0xFFFFFFFF)
        imports["ANativeActivity_hideSoftInput"] = lambda p, activity, flags, *_: self.ime_hidden.append(flags & 0xFFFFFFFF)
        imports["AInputEvent_getType"] = lambda p, event, *_: 1 if event in self.key_events else 2
        imports["AKeyEvent_getAction"] = lambda p, event, *_: self.key_events[event][0]
        imports["AKeyEvent_getKeyCode"] = lambda p, event, *_: self.key_events[event][1]
        imports["AKeyEvent_getMetaState"] = lambda p, event, *_: self.key_events[event][2]
        imports["AKeyEvent_getRepeatCount"] = lambda p, event, *_: 0
        return imports

    def _finish_event(self, process, queue, event, handled, *_):
        self.finished.append(event)
        self.handled.append(handled & 0xFFFFFFFF)

    def prepare_activity(self):
        super().prepare_activity()
        address = self.malloc(len(self.data_path))
        self.lib.write(address, self.data_path)
        self.put_u64(self.activity + 32, address)  # ANativeActivity.internalDataPath

    def key(self, action, code, meta=0):
        event = 0x4B00_0000_0000 + len(self.key_events)
        self.key_events[event] = (action, code, meta)
        self.pending.append(event)
        callback, data = self.input_callback
        keep = self.lib.call_address(callback, 0, 1, data, name="input")
        assert keep & 0xFFFFFFFF == 1, "input callback unregistered itself"

    def type_text(self, text):
        for letter in text:
            meta = META_SHIFT_ON if letter.isupper() else 0
            self.key(KEY_ACTION_DOWN, keycode(letter), meta)
            self.key(KEY_ACTION_UP, keycode(letter), meta)
        self.settle()

    def press_key(self, code):
        self.key(KEY_ACTION_DOWN, code)
        self.key(KEY_ACTION_UP, code)
        self.settle()

    def tap(self, x, y):
        self.touch(ACTION_DOWN, x, y, self.now + 10 * MS)
        self.touch(ACTION_UP, x, y, self.now + 80 * MS)
        self.settle()

    def hold(self, x, y):
        self.touch(ACTION_DOWN, x, y, self.now + 10 * MS)
        self.advance(self.now + 700 * MS)
        self.touch(ACTION_UP, x, y, self.now + 10 * MS)
        self.settle()

    def drag(self, x, y, to_y):
        self.touch(ACTION_DOWN, x, y, self.now + 10 * MS)
        steps = 6
        for step in range(1, steps + 1):
            self.touch(ACTION_MOVE, x, y + (to_y - y) * step // steps, self.now + 16 * MS)
        self.touch(ACTION_UP, x, to_y, self.now + 16 * MS)
        self.settle()

    def settle(self):
        self.advance(self.now + 100 * MS)

    def pixel(self, x, y):
        if self.vulkan.frames:
            (width, height), pixels = self.vulkan.frames[-1]
        else:
            width, pixels = self.width, self.pixels()
        return pixels[y * width + x]

    def close(self):
        self.callback("onNativeWindowDestroyed", WINDOW)
        self.callback("onInputQueueDestroyed", QUEUE)
        self.callback("onDestroy")


def report(name, problems, verbose):
    ok = not problems
    if verbose or not ok:
        print(f"{'ok  ' if ok else 'FAIL'} {name}" + "".join(f"\n     {p}" for p in problems))
    return ok


def start(library, fixture, verbose, vulkan=True, size=(360, 640)):
    framework = FilesFramework(library, fixture, verbose, vulkan=vulkan, size=size)
    framework.create()
    framework.show_window()
    framework.attach_input()
    framework.settle()
    return framework


# A phone at 360 x 640 (density 1): the status bar takes 24 pixels, the
# navigation bar 30. The toolbar is 24 .. 128 (the search 84 .. 124), the
# grid's cells 112 x 116 from (12, 140), three across; a cell's folder icon
# shows its front at (cell x + 56, 185). Back is at 54 .. 94, 32 .. 72.
def cell(index):
    return 12 + 112 * (index % 3) + 56, 140 + 116 * (index // 3) + 45


def run_phone(library, fixture, verbose):
    framework = start(library, fixture, verbose)
    problems = []
    if framework.frames:
        problems.append("a CPU frame although Vulkan is there")
    if not framework.vulkan.frames:
        problems.append("no Vulkan frame")
        return report("phone: Vulkan", problems, verbose)
    # Home: Project 2, Project 10 (folders first, numbers by value), then
    # alpha.txt and Beta.txt.
    if framework.pixel(*cell(0)) != FOLDER or framework.pixel(*cell(1)) != FOLDER:
        problems.append(f"home: the folders are not in the first cells ({framework.pixel(*cell(0)):#x}, {framework.pixel(*cell(1)):#x})")
    if framework.pixel(180, 100) != TOOLBAR and framework.pixel(20, 40) != TOOLBAR:
        problems.append("home: no phone toolbar under the status bar")
    # A tap opens Project 2 (one folder in it: Nested).
    framework.tap(*cell(0))
    if framework.pixel(*cell(0)) != FOLDER or framework.pixel(*cell(1)) != BACKGROUND:
        problems.append(f"a tap on Project 2 did not open it ({framework.pixel(*cell(1)):#x} beside Nested)")
    # A long press on empty space: the menu; its first entry makes a folder.
    framework.hold(180, 400)
    framework.tap(230, 420)
    made = os.path.join(fixture, "Project 2", "untitled folder")
    if not os.path.isdir(made):
        problems.append(f"the menu's New Folder made nothing in Project 2: {sorted(os.listdir(os.path.join(fixture, 'Project 2')))}")
    if 2 not in framework.ime_shown:
        problems.append(f"renaming the new folder did not show the keyboard: {framework.ime_shown}")
    # Typed keys rename it.
    framework.type_text("Docs")
    framework.press_key(KEYCODE_ENTER)
    if not os.path.isdir(os.path.join(fixture, "Project 2", "Docs")):
        problems.append(f"typing Docs and Enter did not rename it: {sorted(os.listdir(os.path.join(fixture, 'Project 2')))}")
    if not framework.ime_hidden:
        problems.append("the keyboard stayed up after the rename")
    # Back home, then the search: "alp" leaves alpha.txt alone.
    framework.tap(74, 52)
    if framework.pixel(*cell(1)) != FOLDER:
        problems.append("Back did not go home")
    shown_before = len(framework.ime_shown)
    framework.tap(180, 104)
    if len(framework.ime_shown) <= shown_before:
        problems.append("a tap on the search did not show the keyboard")
    framework.type_text("alp")
    if framework.pixel(*cell(0)) == FOLDER or framework.pixel(*cell(1)) != BACKGROUND:
        problems.append("the search for alp still shows the folders")
    if framework.handled and set(framework.handled) != {1}:
        problems.append(f"input events were left to the system: {framework.handled}")
    framework.close()
    if framework.lib.missing:
        problems.append(f"unmodeled imports called: {sorted(framework.lib.missing)}")
    return report("phone: tap opens, long press menu, typed rename, Back, search (Vulkan)", problems, verbose)


def run_scroll(library, fixture, verbose):
    framework = start(library, fixture, verbose)
    problems = []
    before = framework.pixel(*cell(0))
    first = framework.vulkan.frames[-1][1]
    framework.drag(180, 520, 200)
    after = framework.vulkan.frames[-1][1]
    width = framework.vulkan.frames[-1][0][0]
    rows = range(140, 560)
    if all(first[y * width:(y + 1) * width] == after[y * width:(y + 1) * width] for y in rows):
        problems.append("a drag did not scroll the grid")
    if before != FOLDER:
        problems.append("the first cell is not the folder")
    if os.listdir(os.path.join(fixture, "Folder")) != []:
        problems.append("the drag opened or changed something")
    framework.close()
    if framework.lib.missing:
        problems.append(f"unmodeled imports called: {sorted(framework.lib.missing)}")
    return report("phone: a drag scrolls, opens nothing", problems, verbose)


def run_views(library, fixture, verbose):
    """The same app at other sizes and densities: a phone at density 2
    (720 x 1280: the toolbar twice as tall), a landscape tablet at density
    2 (1280 x 800: 640 logical pixels, the compact view), and a wide
    tablet (1600 x 1000: 800 logical pixels, the sidebar)."""
    problems = []
    cases = [((720, 1280), "phone"), ((1280, 800), "compact"), ((1600, 1000), "wide")]
    for size, mode in cases:
        framework = start(library, fixture, verbose, size=size)
        if not framework.vulkan.frames:
            problems.append(f"{size}: no Vulkan frame")
            continue
        # Below the status bar by 150 pixels: still the toolbar on a phone
        # at density 2 (208 tall), the content in the compact view (104).
        toolbar_deep = framework.pixel(size[0] - 40, 24 + 150) == TOOLBAR
        # The wide view's sidebar (400 pixels): not the content's colour.
        sidebar = framework.pixel(100, size[1] // 2) != BACKGROUND
        if mode == "phone" and not toolbar_deep:
            problems.append(f"{size}: not the phone view at density 2 (toolbar)")
        if mode == "compact" and (toolbar_deep or sidebar):
            problems.append(f"{size}: not the compact view (toolbar {toolbar_deep}, sidebar {sidebar})")
        if mode == "wide" and not sidebar:
            problems.append(f"{size}: no sidebar in the wide view")
        framework.close()
        if framework.lib.missing:
            problems.append(f"unmodeled imports called: {sorted(framework.lib.missing)}")
    return report("views by size and density: phone, compact, wide", problems, verbose)


def run_cpu_fallback(library, fixture, verbose):
    framework = start(library, fixture, verbose, vulkan=False)
    problems = []
    if framework.vulkan.frames:
        problems.append("a Vulkan frame was presented without Vulkan")
    if not framework.frames:
        problems.append("no CPU frame")
    elif framework.pixel(*cell(0)) != FOLDER:
        problems.append("the CPU frame does not show the folders")
    framework.tap(*cell(0))
    if framework.pixel(*cell(1)) != BACKGROUND:
        problems.append("a tap did not open Project 2 on the CPU path")
    framework.close()
    if framework.lib.missing:
        problems.append(f"unmodeled imports called: {sorted(framework.lib.missing)}")
    return report("CPU fallback without Vulkan", problems, verbose)


def fixture_home(root):
    os.makedirs(os.path.join(root, "Project 2", "Nested"))
    os.makedirs(os.path.join(root, "Project 10"))
    open(os.path.join(root, "alpha.txt"), "wb").close()
    open(os.path.join(root, "Beta.txt"), "wb").close()


def run(library, tmp, verbose):
    homes = [os.path.join(tmp, name) for name in ("phone", "scroll", "views", "cpu")]
    for home in homes:
        os.makedirs(home)
    fixture_home(homes[0])
    os.makedirs(os.path.join(homes[1], "Folder"))
    for index in range(40):
        open(os.path.join(homes[1], f"file {index:02d}.txt"), "wb").close()
    fixture_home(homes[2])
    fixture_home(homes[3])
    results = [
        run_phone(library, homes[0], verbose),
        run_scroll(library, homes[1], verbose),
        run_views(library, homes[2], verbose),
        run_cpu_fallback(library, homes[3], verbose),
    ]
    print(f"{sum(results)}/{len(results)} android file-manager scenarios passed")
    return all(results)


def main():
    arguments = [argument for argument in sys.argv[1:] if argument != "-v"]
    verbose = "-v" in sys.argv[1:]
    with tempfile.TemporaryDirectory() as tmp:
        library = os.path.join(tmp, "libmain.so")
        if not arguments:
            subprocess.run([compiler(), "projects/desktop/files/main.mlx", "-o", library,
                            "--target=aarch64-android", "--quiet"], cwd=ROOT, check=True)
        elif arguments[0].endswith(".apk"):
            with zipfile.ZipFile(arguments[0]) as apk, open(library, "wb") as output:
                output.write(apk.read("lib/arm64-v8a/libmain.so"))
        else:
            library = arguments[0]
        return 0 if run(library, tmp, verbose) else 1


if __name__ == "__main__":
    sys.exit(main())
