#!/usr/bin/env python3
"""Checks that the compositor's shell lists every handler it registers.

When the shell module is loaded again (examples/wayland-compositor/modules.mlx),
each handler the Wayland server holds is replaced by the new code's function of
the same name, from the tables handlerAddress/handlerName at the end of
shell.mlx, protocols.mlx, data.mlx, dmabuf.mlx and pointer.mlx. A function registered with setHandler,
setDestroyHandler, createGlobal or a client handler but missing from its file's
table would keep running old code after a reload. Exit status 1 names it.

With --fix it rewrites each file's table (the section after its "The handlers
this file registers" comment) from the handlers the file registers.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / "examples" / "wayland-compositor"
FILES = ["shell.mlx", "protocols.mlx", "data.mlx", "dmabuf.mlx", "pointer.mlx"]


def registered(text):
    found = set()
    found |= set(re.findall(r"(?<!wl\.server)\.set(?:Destroy)?Handler\((\w+),", text))
    found |= set(re.findall(r"wl\.server\.set(?:Destroy)?Handler\([^,]+, (\w+),", text))
    found |= set(re.findall(r"createGlobal\(wl\.Interface\.\w+, \d+, (\w+),", text))
    found |= set(re.findall(r"setClient(?:Created|Destroyed)Handler\((\w+),", text))
    return found


def listed(text, module):
    addresses = re.findall(r"if index == (\d+) \{ return @intFromPtr\((\w+)\) \}", text)
    names = re.findall(r'if index == (\d+) \{ return "%s\.(\w+)" \}' % module, text)
    count = re.search(r"pub const HANDLER_COUNT: usize = (\d+)", text)
    return addresses, names, int(count.group(1)) if count else -1


TABLE_MARKER = "\n// ---------------------------------------------------------------------------\n// The handlers this file registers"


def table(module, names):
    lines = [
        "",
        "// ---------------------------------------------------------------------------",
        "// The handlers this file registers with the Wayland server, by name: when",
        "// the shell is loaded again (modules.mlx), each resource's handler is",
        "// replaced by the new code's function of the same name. Every function",
        "// passed to setHandler, setDestroyHandler, createGlobal or a client",
        "// handler here must be listed (tools/check_compositor_modules.py checks;",
        "// --fix rewrites this table).",
        "",
        f"pub const HANDLER_COUNT: usize = {len(names)}",
        "",
        "pub fn handlerAddress(index: usize) -> usize {",
    ]
    lines += [f"    if index == {i} {{ return @intFromPtr({n}) }}" for i, n in enumerate(names)]
    lines += ["    return 0", "}", "", "pub fn handlerName(index: usize) -> []const u8 {"]
    lines += [f'    if index == {i} {{ return "{module}.{n}" }}' for i, n in enumerate(names)]
    lines += ['    return ""', "}", ""]
    return "\n".join(lines)


def fix():
    for name in FILES:
        path = ROOT / name
        text = path.read_text()
        start = text.find(TABLE_MARKER)
        body = text if start < 0 else text[:start]
        path.write_text(body.rstrip("\n") + "\n" + table(name[:-4], sorted(registered(body))))
        print("rewrote the handler table of", path.relative_to(ROOT.parent.parent))


def main():
    if "--fix" in sys.argv[1:]:
        fix()
    problems = []
    for name in FILES:
        module = name[:-4]
        text = (ROOT / name).read_text()
        addresses, names, count = listed(text, module)
        by_index = dict((int(i), f) for i, f in addresses)
        named = dict((int(i), f) for i, f in names)
        if count != len(by_index) or sorted(by_index) != list(range(count)):
            problems.append(f"{name}: HANDLER_COUNT is {count} but handlerAddress lists {len(by_index)}")
        if by_index != named:
            problems.append(f"{name}: handlerAddress and handlerName disagree")
        for function in sorted(registered(text) - set(by_index.values())):
            problems.append(f"{name}: {function} is registered as a handler but not in the handler table")
    for problem in problems:
        print("check_compositor_modules.py:", problem, file=sys.stderr)
    if problems:
        return 1
    print("ok   every handler the shell registers is in its reload table")
    return 0


if __name__ == "__main__":
    sys.exit(main())
