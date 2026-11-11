#!/usr/bin/env python3
# Holds every register number and field mask of the kernel's Radeon display
# code (projects/brix/kernel/gpu_display.mlx and gpu_mode.mlx) against
# Linux's headers, so that none is from memory:
#
#   tools/check_gpu_registers.py HEADERS
#
# HEADERS is a directory with these files of Linux 6.6 (they are not in
# this repository; from drivers/gpu/drm/amd/ of the kernel's tree):
#   include/asic_reg/dce/dce_11_2_d.h          the registers' numbers
#   include/asic_reg/dce/dce_11_2_sh_mask.h    their fields
#   include/asic_reg/gmc/gmc_8_1_d.h           MC_VM_FB_LOCATION
#   amdgpu/vid.h                               the controllers' and
#                                              encoders' offsets
#
# Checked: each `const NAME: usize = 0x...` that is a register is the
# header's mmNAME; a controller's register is at each controller's offset
# the header's per-instance register of that name; an encoder block's
# register likewise in all seven blocks; each `const REGISTER__FIELD` is
# the header's REGISTER__FIELD_MASK; the fields gpu_display.mlx decodes
# with numbers written in place (listed below) are the header's masks and
# shifts; every number of the raw dump's runs is a register in each
# controller's block. Prints what it held and ends with 1 when anything
# differs.
import os, re, sys

if len(sys.argv) != 2:
    sys.exit("usage: check_gpu_registers.py HEADERS (see the file's head)")
hdr = sys.argv[1]
here = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "projects", "brix", "kernel")

def defines(path):
    out = {}
    with open(path, errors="replace") as f:
        for line in f:
            m = re.match(r"#define\s+(\w+)\s+(0x[0-9a-fA-F]+|\d+)\b", line)
            if m:
                out[m.group(1)] = int(m.group(2), 0)
    return out

d = defines(os.path.join(hdr, "dce_11_2_d.h"))
sh = defines(os.path.join(hdr, "dce_11_2_sh_mask.h"))
gmc = defines(os.path.join(hdr, "gmc_8_1_d.h"))
vid = open(os.path.join(hdr, "vid.h")).read()

# Constants that are no registers: counts, an encoder's modes, waits, the
# placing of a surface, Linux's values.
NOT_REGISTERS = {"CONTROLLERS", "BACK_ENDS", "FRONT_ENDS", "MODE_DISPLAYPORT", "MODE_LVDS", "MODE_DVI", "MODE_HDMI",
    "MODE_DISPLAYPORT_MST", "FIRST_FRAME", "COUNTED", "FRAMES", "PLACED", "ROW", "MAX_WATERMARK", "VID_N", "TAKEN",
    "STREAM_GONE", "ENCODER_PRIMED", "STEPS"}

bad = 0
held = 0

def hold(what, got, want):
    global bad, held
    held += 1
    if got != want:
        bad += 1
        print("WRONG %s: the code has %#x, the header %s" % (what, got, "nothing" if want is None else hex(want)))

def table(text, name):
    m = re.search(r"const %s = \[_\]usize\{([^}]*)\}" % name, text)
    return [int(x, 0) for x in m.group(1).replace(" ", "").split(",")]

display = open(os.path.join(here, "gpu_display.mlx")).read()
offsets = table(display, "OFFSETS")
digs = table(display, "DIG")
for n in range(6):
    m = re.search(r"#define\s+CRTC%d_REGISTER_OFFSET\s+\(([^)]*)\)" % n, vid)
    hold("CRTC%d_REGISTER_OFFSET" % n, offsets[n], eval(m.group(1)))
for n in range(7):
    m = re.search(r"#define\s+DIG%d_REGISTER_OFFSET\s+\(([^)]*)\)" % n, vid)
    hold("DIG%d_REGISTER_OFFSET" % n, digs[n] - 0x4A00, eval(m.group(1)))
    hold("DIG%d_DIG_FE_CNTL" % n, digs[n], d.get("mmDIG%d_DIG_FE_CNTL" % n))
vga = table(display, "VGA_CONTROL")
for n in range(6):
    hold("D%dVGA_CONTROL" % (n + 1), vga[n], d.get("mmD%dVGA_CONTROL" % (n + 1)))

def instances(name, first):
    # The per-instance registers named ..._NAME whose instance 0 is at
    # `first`: (prefix, how many).
    for key, value in d.items():
        m = re.match(r"mm([A-Z_]+?)0_" + re.escape(name) + "$", key)
        if m and value == first:
            return m.group(1)
    return None

registers = 0
fields = 0
for path in ("gpu_display.mlx", "gpu_mode.mlx"):
    text = open(os.path.join(here, path)).read()
    for m in re.finditer(r"^(?:pub )?const (\w+): usize = (0x[0-9A-Fa-f]+|\d+)\s*$", text, re.M):
        name, value = m.group(1), int(m.group(2), 0)
        if name in NOT_REGISTERS:
            continue
        if "__" in name:
            fields += 1
            hold(name, value, sh.get(name + "_MASK"))
            continue
        registers += 1
        if name == "MC_VM_FB_LOCATION":
            hold(name, value, gmc.get("mm" + name))
        elif re.match(r"(CRTC0_|DP_DTO0_|PIPE0_)", name):
            # One register for each controller, not at the offsets: a
            # step of 4 (the clock words) or of 1 (the buffers).
            hold(name, value, d.get("mm" + name))
            step = 1 if name.startswith("PIPE0_") else 4
            for n in range(6):
                hold(name.replace("0_", "%d_" % n, 1), value + step * n, d.get("mm" + name.replace("0_", "%d_" % n, 1)))
        elif value < 0x100:
            # In an encoder's block.
            pre = "DIG" if ("mmDIG0_" + name) in d else "DP"
            for n in range(7):
                hold("%s%d_%s" % (pre, n, name), digs[n] + value, d.get("mm%s%d_%s" % (pre, n, name)))
        else:
            hold(name, value, d.get("mm" + name))
            pre = instances(name, value)
            if pre is not None:
                for n in range(6):
                    hold("%s%d_%s" % (pre, n, name), value + offsets[n], d.get("mm%s%d_%s" % (pre, n, name)))
            elif value >= 0x1A00 and value < 0x1C00:
                bad += 1
                print("WRONG %s: no register of each controller by that name" % name)

# Every number of the raw dump's runs is a register, in each controller's
# block.
byval = {}
for k, v in d.items():
    if k.startswith("mm"):
        byval.setdefault(v, []).append(k)
for runs in ("SURFACE_RUNS", "SCALER_RUNS", "TIMING_RUNS"):
    numbers = table(display, runs)
    for k in range(0, len(numbers), 2):
        for number in range(numbers[k], numbers[k + 1] + 1):
            for n in range(6):
                held += 1
                if not byval.get(number + offsets[n]):
                    bad += 1
                    print("WRONG run number %#x at controller %d: no register" % (number, n))

# The fields gpu_display.mlx decodes with numbers in place: (register,
# field, mask before the shift, shift).
IN_PLACE = [
 ("CRTC_CONTROL", "CRTC_MASTER_EN", 0x1, 0),
 ("CRTC_H_TOTAL", "CRTC_H_TOTAL", 0x3FFF, 0),
 ("CRTC_V_TOTAL", "CRTC_V_TOTAL", 0x3FFF, 0),
 ("CRTC_H_BLANK_START_END", "CRTC_H_BLANK_START", 0x3FFF, 0),
 ("CRTC_H_BLANK_START_END", "CRTC_H_BLANK_END", 0x3FFF, 16),
 ("CRTC_V_BLANK_START_END", "CRTC_V_BLANK_START", 0x3FFF, 0),
 ("CRTC_V_BLANK_START_END", "CRTC_V_BLANK_END", 0x3FFF, 16),
 ("CRTC_H_SYNC_A", "CRTC_H_SYNC_A_START", 0x3FFF, 0),
 ("CRTC_H_SYNC_A", "CRTC_H_SYNC_A_END", 0x3FFF, 16),
 ("CRTC_V_SYNC_A", "CRTC_V_SYNC_A_START", 0x3FFF, 0),
 ("CRTC_V_SYNC_A", "CRTC_V_SYNC_A_END", 0x3FFF, 16),
 ("CRTC_H_SYNC_A_CNTL", "CRTC_H_SYNC_A_POL", 0x1, 0),
 ("CRTC_V_SYNC_A_CNTL", "CRTC_V_SYNC_A_POL", 0x1, 0),
 ("CRTC_BLANK_CONTROL", "CRTC_BLANK_DATA_EN", 0x1, 8),
 ("CRTC_INTERLACE_CONTROL", "CRTC_INTERLACE_ENABLE", 0x1, 0),
 ("CRTC_STATUS_FRAME_COUNT", "CRTC_FRAME_COUNT", 0xFFFFFF, 0),
 ("GRPH_ENABLE", "GRPH_ENABLE", 0x1, 0),
 ("GRPH_CONTROL", "GRPH_DEPTH", 0x3, 0),
 ("GRPH_CONTROL", "GRPH_FORMAT", 0x7, 8),
 ("GRPH_CONTROL", "GRPH_ARRAY_MODE", 0xF, 20),
 ("GRPH_PRIMARY_SURFACE_ADDRESS", "GRPH_PRIMARY_SURFACE_ADDRESS", 0xFFFFFF, 8),
 ("GRPH_PRIMARY_SURFACE_ADDRESS_HIGH", "GRPH_PRIMARY_SURFACE_ADDRESS_HIGH", 0xFF, 0),
 ("GRPH_PITCH", "GRPH_PITCH", 0x7FFF, 0),
 ("GRPH_X_START", "GRPH_X_START", 0x3FFF, 0),
 ("GRPH_Y_START", "GRPH_Y_START", 0x3FFF, 0),
 ("GRPH_X_END", "GRPH_X_END", 0x7FFF, 0),
 ("GRPH_Y_END", "GRPH_Y_END", 0x7FFF, 0),
 ("VIEWPORT_START", "VIEWPORT_X_START", 0x3FFF, 16),
 ("VIEWPORT_START", "VIEWPORT_Y_START", 0x3FFF, 0),
 ("VIEWPORT_SIZE", "VIEWPORT_WIDTH", 0x3FFF, 16),
 ("VIEWPORT_SIZE", "VIEWPORT_HEIGHT", 0x3FFF, 0),
 ("SCL_MODE", "SCL_MODE", 0x3, 0),
 ("SCL_MODE", "SCL_PSCL_EN", 0x1, 4),
 ("SCL_BYPASS_CONTROL", "SCL_BYPASS_MODE", 0x3, 0),
 ("SCL_TAP_CONTROL", "SCL_V_NUM_OF_TAPS", 0x7, 0),
 ("SCL_TAP_CONTROL", "SCL_H_NUM_OF_TAPS", 0xF, 8),
 ("SCL_HORZ_FILTER_SCALE_RATIO", "SCL_H_SCALE_RATIO", 0x3FFFFFF, 0),
 ("SCL_VERT_FILTER_SCALE_RATIO", "SCL_V_SCALE_RATIO", 0x3FFFFFF, 0),
 ("EXT_OVERSCAN_LEFT_RIGHT", "EXT_OVERSCAN_LEFT", 0x1FFF, 16),
 ("EXT_OVERSCAN_LEFT_RIGHT", "EXT_OVERSCAN_RIGHT", 0x1FFF, 0),
 ("EXT_OVERSCAN_TOP_BOTTOM", "EXT_OVERSCAN_TOP", 0x1FFF, 16),
 ("EXT_OVERSCAN_TOP_BOTTOM", "EXT_OVERSCAN_BOTTOM", 0x1FFF, 0),
 ("LB_MEMORY_CTRL", "LB_MEMORY_CONFIG", 0x3, 20),
 ("LB_MEMORY_CTRL", "LB_MEMORY_SIZE", 0x1FFF, 0),
 ("PIPE0_DMIF_BUFFER_CONTROL", "DMIF_BUFFERS_ALLOCATED", 0x7, 0),
 ("CRTC0_PIXEL_RATE_CNTL", "DP_DTO0_ENABLE", 0x1, 4),
 ("DIG_BE_EN_CNTL", "DIG_ENABLE", 0x1, 0),
 ("DIG_BE_CNTL", "DIG_FE_SOURCE_SELECT", 0x7F, 8),
 ("DIG_BE_CNTL", "DIG_MODE", 0x7, 16),
 ("DIG_FE_CNTL", "DIG_SOURCE_SELECT", 0x7, 0),
 ("DP_CONFIG", "DP_UDI_LANES", 0x3, 0),
 ("DP_LINK_CNTL", "DP_LINK_TRAINING_COMPLETE", 0x1, 4),
 ("DP_VID_STREAM_CNTL", "DP_VID_STREAM_ENABLE", 0x1, 0),
 # gpu_mode.mlx's shifts written in place.
 ("DPG_WATERMARK_MASK_CONTROL", "URGENCY_WATERMARK_MASK", 0x7, 8),
 ("DPG_PIPE_URGENCY_CONTROL", "URGENCY_HIGH_WATERMARK", 0xFFFF, 16),
 ("DP_VID_STREAM_CNTL", "DP_VID_STREAM_DIS_DEFER", 0x3, 8),
 ("DP_PIXEL_FORMAT", "DP_COMPONENT_DEPTH", 0x7, 24),
 ("CRTC_START_LINE_CONTROL", "CRTC_ADVANCED_START_LINE_POSITION", 0xFF, 12),
 ("CRTC_CONTROL", "CRTC_HBLANK_EARLY_CONTROL", 0x7, 20),
 ("CRTC0_PIXEL_RATE_CNTL", "DP_DTO0_ENABLE", 0x1, 4),
 ("VIEWPORT_SIZE", "VIEWPORT_WIDTH", 0x3FFF, 16),
]
wrong = 0
for register, field, mask, shift in IN_PLACE:
    mk = "%s__%s_MASK" % (register, field)
    sk = "%s__%s__SHIFT" % (register, field)
    if mk not in sh or sk not in sh:
        wrong += 1
        print("FIELD not in the header: %s" % mk)
    elif sh[mk] != (mask << shift) or sh[sk] != shift:
        wrong += 1
        print("FIELD differs %s: the header has mask %#x shift %d, the code mask %#x shift %d" % (mk, sh[mk], sh[sk], mask << shift, shift))

print("%d register constants and %d field constants read; %d numbers held against the headers, %d wrong; %d fields decoded in place held, %d differ" % (registers, fields, held, bad, len(IN_PLACE), wrong))
sys.exit(1 if bad or wrong else 0)
