#!/usr/bin/env python3
# Holds every number of the kernel's tables of display timings
# (projects/brix/kernel/mode_tables.mlx: the timings a monitor's EDID
# names by a bit or a code instead of writing them out) against Linux's
# own tables, so that none is from memory:
#
#   tools/check_display_modes.py DRM_EDID_C
#
# DRM_EDID_C is drivers/gpu/drm/drm_edid.c of Linux 6.6 (it is not in this
# repository:
#   curl -sS https://raw.githubusercontent.com/torvalds/linux/v6.6/drivers/gpu/drm/drm_edid.c
# ). Its tables:
#   drm_dmt_modes      VESA's monitor timings (DMT)        our DMT
#   edid_est_modes     the base block's established        our ESTABLISHED
#                      timings, by their bit
#   est3_modes         the further established timings     our ESTABLISHED_3
#                      (a descriptor), each a size, a
#                      rate and whether it is the one
#                      with reduced blanking
#   edid_cea_modes_1   CTA-861's video codes 1 to 127      our CTA
#
# Checked: each table has as many rows as Linux's, in Linux's order, and
# every row's numbers are Linux's: the pixel clock in kHz, across and down
# the size, where the sync pulse begins and ends and the total, and the
# flags (the pulses' polarities, interlaced, a line's pixels sent twice)
# with Linux's values for them; the flags' values themselves
# (include/uapi/drm/drm_mode.h's numbers, which drm_edid.c only names, are
# written here as that file has them). Of ESTABLISHED_3: the size, the
# rate and the reduced-blanking mark of each row.
#
# Prints what it held and ends with 1 when anything differs. With
# --print it writes the tables' rows as mode_tables.mlx has them (what the
# file was made with), from Linux's file alone.
import os, re, sys

args = [a for a in sys.argv[1:] if a != "--print"]
printing = "--print" in sys.argv[1:]
if len(args) != 1:
    sys.exit("usage: check_display_modes.py [--print] DRM_EDID_C (see the file's head)")
source = open(args[0], errors="replace").read()
ours_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "projects", "brix", "kernel", "mode_tables.mlx")

# include/uapi/drm/drm_mode.h (Linux 6.6).
FLAGS = {
    "DRM_MODE_FLAG_PHSYNC": 1 << 0,
    "DRM_MODE_FLAG_NHSYNC": 1 << 1,
    "DRM_MODE_FLAG_PVSYNC": 1 << 2,
    "DRM_MODE_FLAG_NVSYNC": 1 << 3,
    "DRM_MODE_FLAG_INTERLACE": 1 << 4,
    "DRM_MODE_FLAG_DBLSCAN": 1 << 5,
    "DRM_MODE_FLAG_DBLCLK": 1 << 12,
}

def body(name, kind):
    m = re.search(r"static const struct %s %s\[\] = \{" % (kind, name), source)
    if not m:
        sys.exit("no table %s in %s" % (name, args[0]))
    depth, k = 1, m.end()
    while depth:
        if source[k] == "{": depth += 1
        if source[k] == "}": depth -= 1
        k += 1
    return re.sub(r"/\*.*?\*/", "", source[m.end():k - 1], flags=re.S)

def modes(name):
    rows = []
    for m in re.finditer(r"DRM_MODE\(\s*\"[^\"]*\"\s*,\s*DRM_MODE_TYPE_DRIVER\s*,([^)]*)\)", body(name, "drm_display_mode")):
        parts = [p.strip() for p in m.group(1).split(",")]
        # (clock, hdisplay, hsync_start, hsync_end, htotal, hskew,
        # vdisplay, vsync_start, vsync_end, vtotal, vscan, flags.)
        if len(parts) != 12:
            sys.exit("a row of %s with %d parts" % (name, len(parts)))
        numbers = [int(p, 0) for p in parts[:11]]
        if numbers[5] != 0 or numbers[10] != 0:
            sys.exit("a row of %s with a skew or a scan count: the table here has no place for one" % name)
        flags = 0
        for word in parts[11].split("|"):
            word = word.strip()
            if word not in FLAGS:
                sys.exit("a flag %s in %s that is not known here" % (word, name))
            flags |= FLAGS[word]
        rows.append(numbers[0:5] + numbers[6:10] + [flags])
    return rows

def minimodes(name):
    return [[int(v) for v in m.groups()] for m in re.finditer(r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}", body(name, "minimode"))]

def ours(name, width):
    text = open(ours_path).read()
    m = re.search(r"pub const %s = \[_\]u32\{(.*?)\n\}" % name, text, flags=re.S)
    if not m:
        sys.exit("no table %s in mode_tables.mlx" % name)
    inside = re.sub(r"//[^\n]*", "", m.group(1))
    numbers = [int(v, 0) for v in re.findall(r"0x[0-9a-fA-F]+|\d+", inside)]
    if len(numbers) % width != 0:
        sys.exit("%s has %d numbers, which is no whole count of rows of %d" % (name, len(numbers), width))
    return [numbers[k:k + width] for k in range(0, len(numbers), width)]

def constant(name):
    text = open(ours_path).read()
    m = re.search(r"pub const %s: usize = (0x[0-9a-fA-F]+|\d+)" % name, text)
    if not m:
        sys.exit("no constant %s in mode_tables.mlx" % name)
    return int(m.group(1), 0)

tables = (("DMT", modes("drm_dmt_modes"), 10), ("ESTABLISHED", modes("edid_est_modes"), 10), ("CTA", modes("edid_cea_modes_1"), 10), ("ESTABLISHED_3", minimodes("est3_modes"), 4))

if printing:
    for name, rows, width in tables:
        print("pub const %s = [_]u32{" % name)
        for row in rows:
            print("    " + ", ".join(("0x%x" % v) if width == 10 and k == 9 else str(v) for k, v in enumerate(row)) + ",")
        print("}")
        print()
    sys.exit(0)

wrong = 0
def differ(text):
    global wrong
    wrong += 1
    print("DIFFERS " + text)

for name, rows, width in tables:
    mine = ours(name, width)
    if len(mine) != len(rows):
        differ("%s has %d rows, Linux's table %d" % (name, len(mine), len(rows)))
        continue
    bad = 0
    for k, (a, b) in enumerate(zip(mine, rows)):
        if a != b:
            bad += 1
            differ("%s row %d is %s, Linux's %s" % (name, k, a, b))
    if bad == 0:
        print("ok   %s: %d rows, %d numbers, each Linux's" % (name, len(rows), len(rows) * width))

# The flags' values, and the widths of a row.
for mine, theirs in (("PHSYNC", "DRM_MODE_FLAG_PHSYNC"), ("NHSYNC", "DRM_MODE_FLAG_NHSYNC"), ("PVSYNC", "DRM_MODE_FLAG_PVSYNC"), ("NVSYNC", "DRM_MODE_FLAG_NVSYNC"), ("INTERLACE", "DRM_MODE_FLAG_INTERLACE"), ("DBLSCAN", "DRM_MODE_FLAG_DBLSCAN"), ("DBLCLK", "DRM_MODE_FLAG_DBLCLK")):
    if constant(mine) != FLAGS[theirs]:
        differ("%s is %#x, %s is %#x" % (mine, constant(mine), theirs, FLAGS[theirs]))
if wrong == 0:
    print("ok   the flags' values are drm_mode.h's")
# The first code of the CTA table, and how far it goes.
if constant("CTA_FIRST") != 1:
    differ("CTA_FIRST is %d, Linux's table begins with video code 1" % constant("CTA_FIRST"))
if wrong:
    print("%d differ" % wrong)
    sys.exit(1)
print("every number of mode_tables.mlx is Linux 6.6's")
