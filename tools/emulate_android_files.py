#!/usr/bin/env python3
"""Runs examples/mlx-files-android against the Android/Vulkan model.

The fixture is a real host directory exposed as ANativeActivity's
internalDataPath. The app must use Vulkan by default, paint the shared Files
UI, route the Vulkan demo's raw motion events into the toolbar and file rows,
navigate into two naturally sorted directories, refresh, and cleanly destroy
its activity state.

Usage: python3 tools/emulate_android_files.py [-v] [app.apk | libmain.so]
With no file, the script builds examples/mlx-files-android/main.mlx.
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
    ACTION_UP,
    MS,
    QUEUE,
    WINDOW,
)
from emulate_vulkan_android import App as VulkanApp  # noqa: E402


class FilesFramework(VulkanApp):
    def __init__(self, library, data_path, verbose=False, vulkan=True):
        self.data_path = os.fsencode(data_path) + b"\0"
        self.freed = []
        self.handled = []
        self.ime_shown = []
        self.ime_hidden = []
        super().__init__(library, available=vulkan, verbose=verbose, jni={"sdk": 34, "insets": (24, 0, 30, 0)})
        self.vulkan.first_pipeline = "paint"
        self.vulkan.extent = (360, 640)

    def _imports(self):
        imports = super()._imports()
        imports["free"] = lambda process, address, *_: self.freed.append(address) or 0
        imports["AInputQueue_finishEvent"] = self._finish_event
        imports["ANativeActivity_showSoftInput"] = self._show_soft_input
        imports["ANativeActivity_hideSoftInput"] = self._hide_soft_input
        return imports

    def _finish_event(self, process, queue, event, handled, *_):
        self.finished.append(event)
        self.handled.append(handled & 0xFFFFFFFF)

    def _show_soft_input(self, process, activity, flags, *_):
        self.ime_shown.append((activity, flags & 0xFFFFFFFF))

    def _hide_soft_input(self, process, activity, flags, *_):
        self.ime_hidden.append((activity, flags & 0xFFFFFFFF))

    def prepare_activity(self):
        super().prepare_activity()
        address = self.malloc(len(self.data_path))
        self.lib.write(address, self.data_path)
        self.put_u64(self.activity + 32, address)  # ANativeActivity.internalDataPath


def feed(framework, events, base):
    for action, x, y, at in events:
        framework.touch(action, x, y, base + at)
    return base + max(at for _, _, _, at in events) + 100 * MS


def log_lines(framework):
    return [text for tag, text in framework.logs if tag == "mlx"]


def frame_pixel(framework, x, y):
    (width, height), pixels = framework.vulkan.frames[-1]
    assert 0 <= x < width and 0 <= y < height
    return pixels[y * width + x]


def run_empty_toolbar_touch(library, fixture, verbose):
    """A fresh install has no rows, so its view buttons must still prove that
    raw Android touch is connected to the shared Files UI."""
    framework = FilesFramework(library, fixture, verbose)
    framework.create()
    runtime = framework.u64(framework.activity + 56)
    framework.show_window()
    framework.attach_input()

    # 360x640 with a 24 px top inset: grid/list are 268..308/308..348,
    # vertically 32..72. Sample away from their glyphs and rounded corners.
    selected = 0xFF544642
    unselected = 0xFF3A2F2C
    grid_sample = (274, 52)
    list_sample = (342, 52)
    problems = []
    if frame_pixel(framework, *grid_sample) != unselected or frame_pixel(framework, *list_sample) != selected:
        problems.append("initial list-view selection was not painted")

    base = feed(framework, [(ACTION_DOWN, 288, 52, 0), (ACTION_UP, 288, 52, 80 * MS)], 0)
    if frame_pixel(framework, *grid_sample) != selected or frame_pixel(framework, *list_sample) != unselected:
        problems.append("grid button touch did not switch the visible view")

    feed(framework, [(ACTION_DOWN, 328, 52, 0), (ACTION_UP, 328, 52, 80 * MS)], base)
    if frame_pixel(framework, *grid_sample) != unselected or frame_pixel(framework, *list_sample) != selected:
        problems.append("list button touch did not switch the visible view")

    lines = log_lines(framework)
    if "files: input queue attached" not in lines or "files: touch input active" not in lines:
        problems.append("Vulkan demo motion path did not reach the Files activity")
    if "files: grid view" not in lines or "files: list view" not in lines:
        problems.append("toolbar touches did not reach the Files UI actions")
    if framework.handled != [1, 1, 1, 1]:
        problems.append(f"toolbar motion events were not handled: {framework.handled}")

    framework.callback("onNativeWindowDestroyed", WINDOW)
    framework.callback("onInputQueueDestroyed", QUEUE)
    framework.callback("onDestroy")
    if runtime not in framework.freed:
        problems.append("empty-directory activity state was not released")
    if framework.lib.missing:
        problems.append(f"unmodeled imports called: {sorted(framework.lib.missing)}")

    ok = not problems
    if verbose or not ok:
        print(f"{'ok  ' if ok else 'FAIL'} empty-directory toolbar touch")
        for problem in problems:
            print(f"     {problem}")
    return ok


def run_vulkan(library, fixture, verbose):
    framework = FilesFramework(library, fixture, verbose)
    framework.create()
    runtime = framework.u64(framework.activity + 56)
    framework.show_window()
    framework.attach_input()
    base = 0

    def tap(x, y):
        return [(ACTION_DOWN, x, y, 0), (ACTION_UP, x, y, 80 * MS)]

    # 360x640, safe top 24/bottom 30: toolbar 24..80, rows begin at 80.
    grid_button = tap(288, 52)
    list_button = tap(328, 52)
    first_grid_item = tap(96, 140)
    first_row = tap(180, 106)
    second_row = tap(180, 158)
    back_button = tap(30, 52)
    long_press = [(ACTION_DOWN, 180, 106, 0), (ACTION_UP, 180, 106, 700 * MS)]

    # The shared toolbar changes to the real grid renderer; its first cell
    # opens Project 2 through the same down/up path as the Vulkan demo. Return
    # to list view, then exercise the existing direct-row gestures.
    base = feed(framework, grid_button, base)
    base = feed(framework, first_grid_item, base)
    base = feed(framework, back_button, base)
    base = feed(framework, list_button, base)
    base = feed(framework, first_row, base)
    base = feed(framework, long_press, base)
    base = feed(framework, back_button, base)
    base = feed(framework, second_row, base)

    lines = log_lines(framework)
    wanted = [
        "files: private directory loaded",
        "files: grid view",
        "files: opened directory",
        "files: parent directory",
        "files: list view",
        "files: opened directory",
        "files: refreshed",
        "files: parent directory",
        "files: opened directory",
    ]
    position = 0
    problems = []
    for expected in wanted:
        while position < len(lines) and lines[position] != expected:
            position += 1
        if position == len(lines):
            problems.append(f"missing log line: {expected}")
        else:
            position += 1
    if "files: Vulkan renderer ready" not in lines:
        problems.append("Vulkan was not selected as the default renderer")
    if framework.frames:
        problems.append("the default path posted an ANativeWindow CPU frame")
    if len(framework.vulkan.frames) < 5:
        problems.append(f"only {len(framework.vulkan.frames)} Vulkan frames were presented")
    else:
        # Android stores RGBA_8888 with red in the least-significant byte.
        # These are the shared Files background and toolbar palette entries,
        # proving the Android adapter went through filemanager-shared/view.mlx
        # instead of its former standalone dark-list renderer.
        colors = set(framework.vulkan.frames[0][1])
        background = 0xFF28201E
        toolbar = 0xFF2F2624
        if background not in colors or toolbar not in colors:
            problems.append("first frame did not use the shared Files palette")
    if framework.handled and set(framework.handled) != {1}:
        problems.append(f"motion events were returned as unhandled: {framework.handled}")
    if len(framework.handled) != 16:
        problems.append(f"expected 16 finished motion events, got {len(framework.handled)}")

    framework.callback("onNativeWindowDestroyed", WINDOW)
    framework.callback("onInputQueueDestroyed", QUEUE)
    framework.callback("onDestroy")
    if runtime not in framework.freed:
        problems.append("file-manager activity state was not released")
    if framework.lib.missing:
        problems.append(f"unmodeled imports called: {sorted(framework.lib.missing)}")

    ok = not problems
    if verbose or not ok:
        print(f"{'ok  ' if ok else 'FAIL'} private-directory navigation")
        if verbose:
            print("     log: " + " | ".join(lines))
        for problem in problems:
            print(f"     {problem}")
    return ok


def run_system_keyboard(library, fixture, verbose):
    """The compact responsive toolbar exposes Search. A direct tap must
    focus it through Android's NativeActivity IME API, and a tap outside it
    must dismiss the same system keyboard."""
    framework = FilesFramework(library, fixture, verbose)
    framework.vulkan.extent = (640, 360)
    framework.create()
    framework.show_window()
    framework.attach_input()

    # Compact layout at 640x360: Search is x=476..626, y=35..65.
    base = feed(framework, [(ACTION_DOWN, 550, 50, 0), (ACTION_UP, 550, 50, 80 * MS)], 0)
    feed(framework, [(ACTION_DOWN, 320, 120, 0), (ACTION_UP, 320, 120, 80 * MS)], base)

    problems = []
    if framework.ime_shown != [(framework.activity, 0)]:
        problems.append(f"search did not show the standard IME: {framework.ime_shown}")
    if framework.ime_hidden != [(framework.activity, 0)]:
        problems.append(f"leaving search did not hide the standard IME: {framework.ime_hidden}")
    lines = log_lines(framework)
    if "files: search focused" not in lines or "files: search dismissed" not in lines:
        problems.append("search focus changes were not routed through the Files UI")
    if framework.handled != [1, 1, 1, 1]:
        problems.append(f"search motion events were not handled: {framework.handled}")

    framework.callback("onNativeWindowDestroyed", WINDOW)
    framework.callback("onInputQueueDestroyed", QUEUE)
    framework.callback("onDestroy")
    if framework.lib.missing:
        problems.append(f"unmodeled imports called: {sorted(framework.lib.missing)}")

    ok = not problems
    if verbose or not ok:
        print(f"{'ok  ' if ok else 'FAIL'} Android system keyboard")
        for problem in problems:
            print(f"     {problem}")
    return ok


def run_cpu_fallback(library, fixture, verbose):
    framework = FilesFramework(library, fixture, verbose, vulkan=False)
    framework.create()
    runtime = framework.u64(framework.activity + 56)
    framework.show_window()
    framework.attach_input()
    framework.touch(ACTION_DOWN, 20, 16, 0)
    framework.touch(ACTION_UP, 20, 16, 80 * MS)
    lines = log_lines(framework)
    problems = []
    if "files: Vulkan unavailable" not in lines or "files: CPU window fallback active" not in lines:
        problems.append("missing Vulkan-to-CPU fallback log")
    if framework.vulkan.frames:
        problems.append("a Vulkan frame was presented without Vulkan")
    if len(framework.frames) < 2:
        problems.append(f"only {len(framework.frames)} CPU fallback frames were posted")
    if framework.handled != [1, 1]:
        problems.append(f"CPU fallback input was not handled: {framework.handled}")
    framework.callback("onNativeWindowDestroyed", WINDOW)
    framework.callback("onInputQueueDestroyed", QUEUE)
    framework.callback("onDestroy")
    if runtime not in framework.freed:
        problems.append("CPU fallback activity state was not released")
    if framework.lib.missing:
        problems.append(f"unmodeled imports called: {sorted(framework.lib.missing)}")
    ok = not problems
    if verbose or not ok:
        print(f"{'ok  ' if ok else 'FAIL'} CPU fallback")
        for problem in problems:
            print(f"     {problem}")
    return ok


def run(library, fixture, empty_fixture, verbose):
    passed = (
        int(run_empty_toolbar_touch(library, empty_fixture, verbose))
        + int(run_vulkan(library, fixture, verbose))
        + int(run_system_keyboard(library, fixture, verbose))
        + int(run_cpu_fallback(library, fixture, verbose))
    )
    print(f"{passed}/4 android file-manager scenarios passed")
    return passed == 4


def main():
    arguments = [argument for argument in sys.argv[1:] if argument != "-v"]
    verbose = "-v" in sys.argv[1:]
    with tempfile.TemporaryDirectory() as tmp:
        fixture = os.path.join(tmp, "files")
        empty_fixture = os.path.join(tmp, "empty-files")
        os.makedirs(empty_fixture)
        os.makedirs(os.path.join(fixture, "Project 2", "Nested"))
        os.makedirs(os.path.join(fixture, "Project 10"))
        open(os.path.join(fixture, "alpha.txt"), "wb").close()
        open(os.path.join(fixture, "Beta.txt"), "wb").close()

        library = os.path.join(tmp, "libmain.so")
        if not arguments:
            subprocess.run([
                os.path.join(ROOT, "zig-out/bin/mlx1"),
                "examples/mlx-files-android/main.mlx",
                "-o", library,
                "--target=aarch64-android",
                "--quiet",
            ], cwd=ROOT, check=True)
        elif arguments[0].endswith(".apk"):
            with zipfile.ZipFile(arguments[0]) as apk, open(library, "wb") as output:
                output.write(apk.read("lib/arm64-v8a/libmain.so"))
        else:
            library = arguments[0]
        return 0 if run(library, fixture, empty_fixture, verbose) else 1


if __name__ == "__main__":
    sys.exit(main())
