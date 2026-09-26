#!/usr/bin/env python3
"""Checks that the compositor's shell lists every handler it registers.

When the shell module is loaded again (examples/wayland-compositor/modules.mlx),
each handler the Wayland server holds is replaced by the new code's function of
the same name, from the tables handlerAddress/handlerName at the end of
shell.mlx, data.mlx and dmabuf.mlx. A function registered with setHandler,
setDestroyHandler, createGlobal or a client handler but missing from its file's
table would keep running old code after a reload. Exit status 1 names it.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / "examples" / "wayland-compositor"
FILES = ["shell.mlx", "data.mlx", "dmabuf.mlx"]


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


def main():
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
