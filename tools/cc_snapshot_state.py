#!/usr/bin/env python3
"""The variables of mlx-cc's modules that a prefix snapshot holds.

A prefix snapshot (compiler/cc/snapshot.mlx) is the arena's used part and
every module-level `var` of the compiler's modules, by address and size.
Each module names its own in a `keepState()` function at its end, and
snapshot.mlx calls them all; this script writes those lists from the
declarations, so that a variable added to a module is in the snapshot
without anyone remembering to add it. A variable whose line ends with
`// (per run)` is left out: it belongs to the run, not to the state after
the prefix (the arena's place, the command line's options, the counts).

    tools/cc_snapshot_state.py           # rewrite the lists
    tools/cc_snapshot_state.py --check   # fail when a list is out of date
"""
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "compiler", "cc")
# Programs of their own (mlx-cheaders, mlx-ar) and the two that hold no
# state of the compilation.
SKIP = {"cheaders.mlx", "ar.mlx", "main.mlx", "snapshot.mlx"}
BEGIN = "// --- prefix snapshot state (tools/cc_snapshot_state.py writes this) ---"
END = "// --- end of prefix snapshot state ---"
VAR = re.compile(r"^(?:pub )?var ([A-Za-z_][A-Za-z0-9_]*)\b")


def strip_block(text):
    start = text.find(BEGIN)
    if start < 0:
        return text
    end = text.index(END, start) + len(END)
    return text[:start].rstrip("\n") + "\n" + text[end:].lstrip("\n")


def block(module, names):
    call = "keepState" if module == "base.mlx" else "base.keepState"
    function = "keepBaseState" if module == "base.mlx" else "keepState"
    lines = [BEGIN, "pub fn %s() -> void {" % function]
    for name in names:
        lines.append("    %s(@intFromPtr(&%s), @sizeOf(@typeOf(%s)))" % (call, name, name))
    lines.append("}")
    lines.append(END)
    return "\n".join(lines) + "\n"


def main():
    check = "--check" in sys.argv[1:]
    stale = []
    modules = []
    for name in sorted(os.listdir(ROOT)):
        if not name.endswith(".mlx") or name in SKIP:
            continue
        path = os.path.join(ROOT, name)
        with open(path) as f:
            original = f.read()
        text = strip_block(original)
        names = []
        for line in text.split("\n"):
            m = VAR.match(line)
            if m and not line.rstrip().endswith("// (per run)"):
                names.append(m.group(1))
        if names or name == "base.mlx":
            text = text.rstrip("\n") + "\n\n" + block(name, names)
            modules.append(name)
        if text != original:
            stale.append(name)
            if not check:
                with open(path, "w") as f:
                    f.write(text)
    # snapshot.mlx: every module's list, in one function.
    path = os.path.join(ROOT, "snapshot.mlx")
    with open(path) as f:
        original = f.read()
    text = strip_block(original)
    lines = [BEGIN]
    for name in modules:
        if name != "base.mlx":
            lines.append('const state_%s = @import("./%s")' % (name[:-4], name))
    lines.append("fn keepAllState() -> void {")
    for name in modules:
        if name == "base.mlx":
            lines.append("    base.keepBaseState()")
        else:
            lines.append("    state_%s.keepState()" % name[:-4])
    lines.append("}")
    lines.append(END)
    text = text.rstrip("\n") + "\n\n" + "\n".join(lines) + "\n"
    if text != original:
        stale.append("snapshot.mlx")
        if not check:
            with open(path, "w") as f:
                f.write(text)
    if check and stale:
        print("cc_snapshot_state.py: out of date (run tools/cc_snapshot_state.py): " + " ".join(stale))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
