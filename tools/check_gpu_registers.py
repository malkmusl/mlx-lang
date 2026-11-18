#!/usr/bin/env python3
# Holds every register number and field mask of the kernel's Radeon display
# code (projects/brix/kernel/gpu_display.mlx and gpu_mode.mlx), every
# number of its video BIOS reading and table interpreter (gpu_rom.mlx and
# atom.mlx), and every offset and value of the parameter blocks its mode
# set gives the card's tables (gpu_blocks.mlx), against Linux's files, so
# that none is from memory:
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
# and for the video BIOS and its tables:
#   include/asic_reg/smu/smu_7_1_1_d.h         the ROM registers
#   amdgpu/atom.h amdgpu/atom.c                the interpreter's numbers,
#                                              its opcode table
#   include/atombios.h include/atomfirmware.h  the ROM's structures
#   amdgpu/ObjectID.h                          the objects' numbers
#   include/uapi/linux/pci_regs.h              (of the kernel's tree) the
#                                              PCI configuration's ROM word
# and for a TMDS output's mode set (gpu_mode.mlx's part of it and
# gpu_blocks.mlx):
#   include/asic_reg/bif/bif_5_0_d.h           the firmware's scratch words
#   display/dc/dce112/dce112_resource.c        the HDMI strap's number and
#                                              bit
#   display/include/signal_types.h             a TMDS link's clocks
#   display/dc/dce/dce_stream_encoder.c        where HDMI scrambles from
#   display/dc/dce/dce_link_encoder.c          where HDMI needs the ROM's
#                                              word
#   display/dc/inc/hw/clk_mgr_internal.h       the display clock's dividers
#   display/dc/clk_mgr/dce100/dce_clk_mgr.c    the clock generator's default
# and for the hot plug lines and the EDID over DDC (gpu_plug.mlx and
# gpu_ddc.mlx):
#   display/dc/link/protocols/link_hpd.c       the hot plug filter's times
#   display/dc/dce/dce_i2c_hw.h dce_i2c_hw.c   the I2C engine's numbers
#   display/dc/gpio/hw_ddc.c                   the pad's time to settle
#   drivers/gpu/drm/drm_edid.c                 (of the kernel's tree) the
#   include/drm/drm_edid.h                     EDID's addresses and tries
# and for a DisplayPort link (gpu_aux.mlx, gpu_link.mlx):
#   include/drm/display/drm_dp.h               (of the kernel's tree) the
#                                              monitor's registers
#   include/drm/drm_edid.h, drivers/gpu/drm/drm_edid.c,
#   drivers/gpu/drm/display/drm_dp_helper.c    (of the kernel's tree) the
#                                              EDID's addresses, the tries
#   display/dc/dc_ddc_types.h                  the AUX commands
#   display/dc/dc_dp_types.h                   the rates, the drive's levels
#   display/dc/dce/dce_aux.c, dce_aux.h        who holds a channel, its wait
#   display/dc/link/protocols/link_dp_training.c, link_dp_training_8b_10b.h,
#   display/dc/link/link_dpms.c                the training's counts
#   display/include/link_service_types.h       a rate's 27 MHz
#
# Checked: each `const NAME: usize = 0x...` that is a register is the
# header's mmNAME; a controller's register is at each controller's offset
# the header's per-instance register of that name; an encoder block's
# register likewise in all seven blocks; each `const REGISTER__FIELD` is
# the header's REGISTER__FIELD_MASK; the fields gpu_display.mlx decodes
# with numbers written in place (listed below) are the header's masks and
# shifts; every number of the raw dump's runs is a register in each
# controller's block. Of the video BIOS code: each constant named as
# Linux names it is Linux's number; each `STRUCT__field` is the field's
# offset in atombios.h's packed structure (`STRUCT__size` its size); the
# tables' places in the ROM's two lists are the fields' places in
# ATOM_MASTER_LIST_OF_COMMAND_TABLES and ..._DATA_TABLES; each opcode is
# where atom.c's opcode_table has its function, with the destinations,
# the jumps' conditions and the tables of parts in that file's order. Of
# a TMDS output's mode set: the words for each PHY's PLL and each hot plug
# line, with their fields the same in all six; the per-controller clock
# words' fields in all six; the scratch words and the strap; the bits of
# the scratch words, the link's clocks and the dividers as Linux's files
# have them; and gpu_blocks.mlx's `STRUCT__field` offsets and its values
# named as atombios.h names them. Of the hot plug lines and the EDID over
# DDC: each register of a hot plug line's block at all six blocks, the
# pins' mask bits, the filter's times; the I2C engine's registers (each
# line's words at all six lines, each message's word), every field (the
# same bits in each line's word and each message's), the pins' words,
# the engine's numbers, the EDID's addresses and how often Linux tries,
# and the ROM's table of DDC lines.
# Of a DisplayPort link: the six AUX
# channels' registers and fields, the pads' calibration words, the DDC
# lines' pin words and the hot plug lines' words in all six, the back
# end's link words in all seven blocks (the one that is not in the
# header by dce112_resource.c's numbers); every address and bit of the
# monitor's registers as drm_dp.h has it; the AUX commands, the tries
# and waits, the rates, the training's counts and the two values
# set_dp_phy_pattern_passthrough_mode writes, each as Linux's file has
# it. Of the picture brought to another output (gpu_mode.mlx's "THE
# PICTURE BROUGHT TO ANOTHER OUTPUT", the transmitter's "init" and the
# PLL's power-down in gpu_blocks.mlx): the words of each PHY's PLL and
# each PHY's symbol clock that are read around the power-down and the new
# clock, at all six.
# Prints what it held and ends with 1 when anything differs.
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
    "STREAM_GONE", "ENCODER_PRIMED", "STEPS", "SIGNAL_GONE", "BLOCKS", "NO_BLOCK", "TIMING_WORDS",
    "NO_CONNECTOR", "STILL"}

# What a TMDS output's mode set has from other files than the display's
# two headers: registers by their mm names, fields by their _MASK names,
# and plain values (the name in the code: what Linux's file gives).
tmds_needed = ("bif_5_0_d.h", "dce112_resource.c", "signal_types.h", "dce_stream_encoder.c", "dce_link_encoder.c",
               "clk_mgr_internal.h", "dce_clk_mgr.c", "atombios.h")
tmds_missing = [name for name in tmds_needed if not os.path.exists(os.path.join(hdr, name))]
if tmds_missing:
    sys.exit("a TMDS output's mode set is not held: not in %s: %s" % (hdr, " ".join(tmds_missing)))

def source(name):
    return open(os.path.join(hdr, name), errors="replace").read()

def one(pattern, text):
    m = re.search(pattern, text)
    return int(m.group(1), 0) if m else None

elsewhere = dict(defines(os.path.join(hdr, "bif_5_0_d.h")))
elsewhere.update(defines(os.path.join(hdr, "dce112_resource.c")))
scratch_bits = {}
for m in re.finditer(r"#define\s+(ATOM_S\d_\w+)\s+(0x[0-9a-fA-F]+)L?\b", source("atombios.h")):
    scratch_bits.setdefault(m.group(1), int(m.group(2), 0))
LINUX_VALUES = dict(scratch_bits)
LINUX_VALUES.update(defines(os.path.join(hdr, "signal_types.h")))
for m in re.finditer(r"^\s*(DENTIST_\w+)\s*=\s*(0x[0-9a-fA-F]+|\d+)", source("clk_mgr_internal.h"), re.M):
    LINUX_VALUES[m.group(1)] = int(m.group(2), 0)
LINUX_VALUES["HDMI_SCRAMBLED"] = one(r"#define\s+HDMI_CLOCK_CHANNEL_RATE_MORE_340M\s+(\d+)", source("dce_stream_encoder.c"))
LINUX_VALUES["HDMI_FAST"] = one(r"HDMI_6GB_EN[^;]*?adjusted_pix_clk_khz >= (\d+)", source("dce_link_encoder.c"))
LINUX_VALUES["GENERATOR_DEFAULT"] = one(r"dentist_vco_freq_khz == 0\)\s*clk_mgr_dce->base\.dentist_vco_freq_khz = (\d+);", source("dce_clk_mgr.c"))

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
        if name in LINUX_VALUES or name.startswith(("ATOM_S", "DENTIST_DIVIDER", "DENTIST_BASE", "DENTIST_MAX")):
            hold(name, value, LINUX_VALUES.get(name))
            continue
        if "__" in name:
            fields += 1
            hold(name, value, sh.get(name + "_MASK", elsewhere.get(name + "_MASK")))
            # A field of one of several words alike is at the same bits
            # in each: the PHYs' PLLs' (A to F), the controllers' clock
            # words (0 to 5).
            if name.startswith("PHYPLLA_"):
                for letter in "BCDEF":
                    hold(name.replace("PHYPLLA", "PHYPLL" + letter), value, sh.get(name.replace("PHYPLLA", "PHYPLL" + letter) + "_MASK"))
            if re.match(r"CRTC0_(PIXEL_RATE|PHYPLL_PIXEL_RATE)_CNTL__", name):
                for n in range(1, 6):
                    other = name.replace("CRTC0", "CRTC%d" % n).replace("DTO0", "DTO%d" % n)
                    hold(other, value, sh.get(other + "_MASK"))
            continue
        registers += 1
        if name == "MC_VM_FB_LOCATION":
            hold(name, value, gmc.get("mm" + name))
        elif name in ("CC_DC_HDMI_STRAPS", "BIOS_SCRATCH_2", "BIOS_SCRATCH_6"):
            hold(name, value, elsewhere.get("mm" + name))
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

# The words that are one for each PHY's PLL, and for each hot plug line.
mode_text = open(os.path.join(here, "gpu_mode.mlx")).read()
for n, number in enumerate(table(mode_text, "PHYPLL_PIXCLK_RESYNC_CNTL")):
    hold("PHYPLL%s_PIXCLK_RESYNC_CNTL" % "ABCDEF"[n], number, d.get("mmPHYPLL%s_PIXCLK_RESYNC_CNTL" % "ABCDEF"[n]))
for n, number in enumerate(table(mode_text, "DC_HPD_INT_STATUS")):
    hold("HPD%d_DC_HPD_INT_STATUS" % n, number, d.get("mmHPD%d_DC_HPD_INT_STATUS" % n))
hold("the PHYs' PLLs' words", len(table(mode_text, "PHYPLL_PIXCLK_RESYNC_CNTL")), 6)
hold("the hot plug lines' words", len(table(mode_text, "DC_HPD_INT_STATUS")), 6)
# A move's PLL words (DC_COMBOPHYPLLREGS0_ to 5_) and symbol clock words
# (SYMCLKA_ to F_), by the back end's number.
for word in ("PLL_WRAP_CNTRL", "FREQ_CTRL0", "FREQ_CTRL3"):
    numbers = table(mode_text, "DC_COMBOPHYPLLREGS_" + word)
    hold("the PLLs' %s words" % word, len(numbers), 6)
    for n, number in enumerate(numbers):
        hold("DC_COMBOPHYPLLREGS%d_%s" % (n, word), number, d.get("mmDC_COMBOPHYPLLREGS%d_%s" % (n, word)))
symbols = table(mode_text, "SYMCLK_CLOCK_ENABLE")
hold("the symbol clocks' words", len(symbols), 6)
for n, number in enumerate(symbols):
    hold("SYMCLK%s_CLOCK_ENABLE" % "ABCDEF"[n], number, d.get("mmSYMCLK%s_CLOCK_ENABLE" % "ABCDEF"[n]))

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
display_bad = bad
bad = 0
held = 0

# ---- The video BIOS reading and the table interpreter.
needed = ("smu_7_1_1_d.h", "atom.h", "atom.c", "atombios.h", "atomfirmware.h", "ObjectID.h", "pci_regs.h")
missing = [name for name in needed if not os.path.exists(os.path.join(hdr, name))]
if missing:
    print("the video BIOS code is not held: not in %s: %s" % (hdr, " ".join(missing)))
    sys.exit(1)

def text_of(name):
    return open(os.path.join(hdr, name), errors="replace").read()

atom_h = defines(os.path.join(hdr, "atom.h"))
atom_c = text_of("atom.c")
bios_h = text_of("atombios.h")
bios_defines = {}
for m in re.finditer(r"#define\s+(\w+)\s+(0x[0-9a-fA-F]+|\d+)L?\b", bios_h):
    bios_defines.setdefault(m.group(1), int(m.group(2), 0))
objects = defines(os.path.join(hdr, "ObjectID.h"))
smu = defines(os.path.join(hdr, "smu_7_1_1_d.h"))
pci = {}
for m in re.finditer(r"#define\s+(\w+)\s+\(?(~?)(0x[0-9a-fA-F]+|\d+)U?\)?", text_of("pci_regs.h")):
    pci.setdefault(m.group(1), (~int(m.group(3), 0) & 0xFFFFFFFF) if m.group(2) else int(m.group(3), 0))
image = {}
for m in re.finditer(r"^\s*(\w+)\s*=\s*(0x[0-9a-fA-F]+|\d+),", text_of("atomfirmware.h"), re.M):
    image.setdefault(m.group(1), int(m.group(2), 0))
for m in re.finditer(r"#define\s+(BIOS_STRING_LENGTH)\s+(\d+)", text_of("atomfirmware.h")):
    image[m.group(1)] = int(m.group(2))

def constants(text):
    # (name, value, the struct it stands in) of each number.
    out = []
    inside = None
    for line in text.split("\n"):
        m = re.match(r"pub const (\w+) = struct \{", line)
        if m:
            inside = m.group(1)
        elif line.startswith("}"):
            inside = None
        m = re.match(r"\s*(?:pub )?const (\w+): usize = (0x[0-9A-Fa-f]+|\d+)\b", line)
        if m:
            out.append((m.group(1), int(m.group(2), 0), inside))
    return out

# The packed structures of atombios.h: a field's offset, a structure's
# size.
bios_plain = re.sub(r"//[^\n]*", "", bios_h)
SIZES = {"UCHAR": 1, "USHORT": 2, "ULONG": 4, "ATOM_FIRMWARE_CAPABILITY_ACCESS": 2, "PRODUCT_BRANDING": 1, "ATOM_I2C_ID_CONFIG": 1,
         "ATOM_MODE_MISC_INFO_ACCESS": 2, "ATOM_I2C_ID_CONFIG_ACCESS": 1}

def body_of(name):
    # The text between the braces of `typedef struct ... { ... } name;`
    # (the braces of unions inside it counted).
    m = re.search(r"\}\s*%s\s*;" % name, bios_plain)
    if not m:
        return None
    depth, at = 0, m.start()
    while True:
        if bios_plain[at] == "}":
            depth += 1
        elif bios_plain[at] == "{":
            depth -= 1
            if depth == 0:
                break
        at -= 1
    if not re.search(r"typedef\s+struct\s*\w*\s*$", bios_plain[:at]):
        return None
    return bios_plain[at + 1:m.start()]

def members(text, at):
    # The fields of a structure's text from offset `at` on: (field,
    # offset, kind), and the offset behind them. The members of a union
    # without a name all lie where the union begins, which is as long as
    # its longest.
    fields = []
    while True:
        u = re.search(r"\bunion\s*\{", text)
        f = re.search(r"(\w+)\s+(\w+)\s*(?:\[\s*(\w*)\s*\])?\s*;", text)
        if u and (not f or u.start() < f.start()):
            depth, end = 1, u.end()
            while depth:
                depth += {"{": 1, "}": -1}.get(text[end], 0)
                end += 1
            longest = at
            for statement in text[u.end():end - 1].split(";"):
                inner, behind = members(statement + ";", at)
                fields += inner
                longest = max(longest, behind)
            at = longest
            text = text[end:].lstrip().lstrip(";")
            continue
        if not f:
            return fields, at
        kind, field, count = f.group(1), f.group(2), f.group(3)
        size = SIZES.get(kind)
        if size is None:
            size = layout(kind)[1]
        if count is not None:
            # (A count that is an expression of the header: the fields
            # before the array are placed all the same.)
            size *= 0 if count == "" else (int(count) if count.isdigit() else bios_defines.get(count, 1))
        fields.append((field, at, kind))
        at += size
        text = text[f.end():]

def layout(name):
    text = body_of(name)
    if text is None:
        return None
    return members(text, 0)

def offset_of(name, field):
    got = layout(name)
    if got is None:
        return None
    for one, at, kind in got[0]:
        if one == field:
            return at
    for one, at, kind in got[0]:
        if kind not in SIZES:
            inner = offset_of(kind, field)
            if inner is not None:
                return at + inner
    return None

def list_index(struct, field):
    got = layout(struct)
    for n, (one, at, kind) in enumerate(got[0]):
        if one == field:
            return n
    return None

# gpu_rom.mlx: numbers of its own (a ROM's most bytes, "none", how many
# connectors are kept, amdgpu_atombios_allocate_fb_scratch's 20 KiB).
OWN = {"ROOM", "NONE", "CONNECTORS", "DEFAULT_SCRATCH", "DDC_LINES", "DDC_LINE_STEP"}
rom_text = open(os.path.join(here, "gpu_rom.mlx")).read()
# The hot plug pins' register and the six lines' bits in it.
for n, shift in enumerate(table(rom_text, "HPD_PIN_SHIFTS")):
    hold("DC_GPIO_HPD%d_A's bit" % (n + 1), shift, sh.get("DC_GPIO_HPD_A__DC_GPIO_HPD%d_A__SHIFT" % (n + 1)))
hold("the hot plug pins", len(table(rom_text, "HPD_PIN_SHIFTS")), 6)
for name, value, inside in constants(rom_text):
    if name in OWN:
        continue
    if name == "DC_GPIO_HPD_A":
        hold(name, value, d.get("mm" + name))
    elif name == "DC_GPIO_DDC1_A":
        # The six DDC lines' clock pins' words, a step apart.
        step = dict((n, v) for n, v, i in constants(rom_text))["DDC_LINE_STEP"]
        for k in range(6):
            hold("DC_GPIO_DDC%d_A" % (k + 1), value + step * k, d.get("mmDC_GPIO_DDC%d_A" % (k + 1)))
    elif inside == "Command":
        hold("Command." + name, value, list_index("ATOM_MASTER_LIST_OF_COMMAND_TABLES", name))
    elif inside == "Data":
        hold("Data." + name, value, list_index("ATOM_MASTER_LIST_OF_DATA_TABLES", name))
    elif "__" in name:
        struct, field = name.split("__")
        hold(name, value, layout(struct)[1] if field == "size" else offset_of(struct, field))
    elif name.startswith("PCI_"):
        hold(name, value, pci.get(name))
    elif name.startswith("SMC_IND_"):
        hold(name, value, smu.get("mm" + name))
    elif name in ("ROM_INDEX", "ROM_DATA"):
        hold(name, value, smu.get("ix" + name))
    elif name.startswith("OFFSET_TO_") or name == "BIOS_STRING_LENGTH":
        hold(name, value, image.get(name))
    elif name == "STRLEN_LONG":
        hold(name, value, atom_h.get(name))
    elif name in objects:
        hold(name, value, objects[name])
    else:
        hold(name, value, bios_defines.get(name))

# gpu_blocks.mlx: the parameter structures' offsets and sizes, and the
# values named as atombios.h names them (its own numbers: the longest
# block's words and a TMDS link's lanes).
blocks_text = open(os.path.join(here, "gpu_blocks.mlx")).read()
blocks_held = held
for name, value, inside in constants(blocks_text):
    if name in ("WORDS", "LANES"):
        continue
    if name == "LINK_RATE_REF_FREQ_IN_KHZ":
        hold(name, value, one(r"LINK_RATE_REF_FREQ_IN_KHZ\s*=\s*(\d+)", source("link_service_types.h")))
        continue
    if "__" in name:
        struct, field = name.split("__")
        got = layout(struct)
        hold(name, value, None if got is None else (got[1] if field == "size" else offset_of(struct, field)))
    else:
        hold(name, value, bios_defines.get(name))
blocks_held = held - blocks_held
# (Each block's structure is as long as its bytes are laid out: nothing
# of a structure is behind what the code fills.)
for struct in sorted(set(name.split("__")[0] for name, value, inside in constants(blocks_text) if "__" in name)):
    got = layout(struct)
    filled = [value for name, value, inside in constants(blocks_text) if name.startswith(struct + "__") and not name.endswith("__size")]
    hold(struct + "'s fields lie inside it", int(got is not None and max(filled) < got[1]), 1)

# atom.mlx: atom.h's numbers and marks.
atom_text = open(os.path.join(here, "atom.mlx")).read()
ops = {}
for name, value, inside in constants(atom_text):
    if name.startswith("ATOM_"):
        hold(name, value, atom_h.get(name))
    elif name.startswith("OP_"):
        ops[name] = value
for name in ("ATOM_ATI_MAGIC", "ATOM_ROM_MAGIC"):
    ours = re.search(r'pub const %s = "([^"]*)"' % name, atom_text)
    theirs = re.search(r'#define\s+%s\s+"([^"]*)"' % name, text_of("atom.h"))
    held += 1
    if not ours or not theirs or ours.group(1) != theirs.group(1):
        bad += 1
        print("WRONG %s: not atom.h's text" % name)

# The opcodes: atom.c's opcode_table, entry by entry (function,
# argument).
body = atom_c[atom_c.index("opcode_table[ATOM_OP_CNT] = {"):]
body = body[:body.index("};")]
entries = re.findall(r"(NULL|atom_op_\w+)\s*,\s*(\w+)\s*\}", body)
hold("the opcode table's length", len(entries), atom_h.get("ATOM_OP_CNT"))
cond = dict((m.group(1), int(m.group(2))) for m in re.finditer(r"#define\s+(ATOM_COND_\w+)\s+(\d+)", atom_c))
SIX_ARGS = ["ATOM_ARG_REG", "ATOM_ARG_PS", "ATOM_ARG_WS", "ATOM_ARG_FB", "ATOM_ARG_PLL", "ATOM_ARG_MC"]
RUNS = {"SETPORT": ["ATOM_PORT_ATI", "ATOM_PORT_PCI", "ATOM_PORT_SYSIO"], "DELAY": ["ATOM_UNIT_MILLISEC", "ATOM_UNIT_MICROSEC"],
        "MUL32": ["ATOM_ARG_PS", "ATOM_ARG_WS"], "DIV32": ["ATOM_ARG_PS", "ATOM_ARG_WS"]}
destination = table(atom_text, "DESTINATION")
jumps = table(atom_text, "JUMP_CONDITION")
for name, first in sorted(ops.items(), key=lambda item: item[1]):
    function = "atom_op_" + name[3:].lower()
    count = sum(1 for entry in entries if entry[0] == function)
    held += 1
    if first >= len(entries) or entries[first][0] != function or entries[first - 1][0] == function:
        bad += 1
        print("WRONG %s: atom.c's opcode_table does not begin %s at %d" % (name, function, first))
        continue
    arguments = [entry[1] for entry in entries[first:first + count]]
    if count == 6:
        hold(name + "'s destinations", int(arguments == SIX_ARGS and destination == [atom_h[a] for a in SIX_ARGS]), 1)
    elif name == "OP_JUMP":
        hold("the jumps' conditions", int(jumps == [cond[a] for a in arguments]), 1)
    elif name[3:] in RUNS:
        hold(name + "'s run", int(arguments == RUNS[name[3:]]), 1)
    else:
        hold(name + " alone", count, 1)
hold("opcodes named", sum(sum(1 for entry in entries if entry[0] == "atom_op_" + name[3:].lower()) for name in ops), len(entries) - 1)

# The tables of parts.
def c_table(name):
    m = re.search(r"%s(?:\[\d*\])+\s*=\s*\{(.*?)\};" % name, atom_c, re.S)
    return [int(x, 0) for x in re.findall(r"0x[0-9A-Fa-f]+|\d+", re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S))]

for ours, theirs in (("PART_MASK", "atom_arg_mask"), ("PART_SHIFT", "atom_arg_shift"), ("DST_TO_SRC", "atom_dst_to_src"), ("DEF_DST", "atom_def_dst")):
    hold(ours, int(table(atom_text, ours) == c_table(theirs)), 1)
lengths = c_table("atom_iio_len")
steps = dict((atom_h[name], name) for name in atom_h if name.startswith("ATOM_IIO_"))
m = re.search(r"fn programStep\(step: usize\) -> usize \{(.*?)\n\}", atom_text, re.S)
def step_length(step):
    for line in m.group(1).split("\n"):
        f = re.match(r"\s*if (.*) \{ return (\d) \}", line)
        if not f:
            continue
        for clause in f.group(1).split("||"):
            g = re.match(r"\s*step (==|<=) (ATOM_IIO_\w+)\s*", clause)
            if (g.group(1) == "==" and atom_h[g.group(2)] == step) or (g.group(1) == "<=" and step <= atom_h[g.group(2)]):
                return int(f.group(2))
    return 0
hold("the indirect programs' step lengths", int([step_length(k) for k in range(len(lengths))] == lengths), 1)

print("video BIOS and tables: %d numbers of gpu_rom.mlx, gpu_blocks.mlx (%d of them) and atom.mlx held against Linux's files, %d wrong" % (held, blocks_held, bad))
rom_bad = bad
bad = 0
held = 0

# ---- The hot plug lines and the EDID over DDC (gpu_plug.mlx and
# gpu_ddc.mlx). Beside the display's two headers and atombios.h, from
# drivers/gpu/drm/amd/display/dc of the kernel's tree and drivers/gpu/drm:
#   link/protocols/link_hpd.c     the hot plug filter's times
#   dce/dce_i2c_hw.h dce_i2c_hw.c the I2C engine's numbers
#   gpio/hw_ddc.c                 the pad's time to settle
#   dce112/dce112_resource.c      the speed, how many DDC lines
#   drm_edid.c include/drm/drm_edid.h   the EDID's addresses and tries
plug_needed = ("link_hpd.c", "dce_i2c_hw.h", "dce_i2c_hw.c", "hw_ddc.c", "drm_edid.c", "drm_edid.h")
plug_missing = [name for name in plug_needed if not os.path.exists(os.path.join(hdr, name))]
if plug_missing:
    print("the hot plug lines and the EDID over DDC are not held: not in %s: %s" % (hdr, " ".join(plug_missing)))
    sys.exit(1)
plug_text = open(os.path.join(here, "gpu_plug.mlx")).read()
ddc_text = open(os.path.join(here, "gpu_ddc.mlx")).read()

def number_of(text, name):
    m = re.search(r"^pub const %s: usize = (0x[0-9A-Fa-f]+|\d+)\s*$" % name, text, re.M)
    return int(m.group(1), 0) if m else None

# gpu_plug.mlx. Its own numbers: "none", the count of lines, the step
# from one line's block to the next (held below, block by block), its
# looks' times and counts.
PLUG_OWN = {"NONE", "LINES", "HPD_STEP", "INTERVAL", "BOOT_SETTLE", "BOOT_LOOKS", "ASK_AGAIN", "AGREE", "PIECES"}
m = re.search(r"case SIGNAL_TYPE_HDMI_TYPE_A:.*?delay_on_connect_in_ms = (\d+);\s*delay_on_disconnect_in_ms = (\d+);.*?"
              r"case SIGNAL_TYPE_DISPLAY_PORT_MST:.*?delay_on_connect_in_ms = (\d+);\s*delay_on_disconnect_in_ms = (\d+);", source("link_hpd.c"), re.S)
PLUG_LINUX = dict(zip(("CONNECT_TMDS", "DISCONNECT_TMDS", "CONNECT_DP", "DISCONNECT_DP"), [int(x) for x in m.groups()])) if m else {}
hpd_step = number_of(plug_text, "HPD_STEP")
for name, value, inside in constants(plug_text):
    if name in PLUG_OWN:
        continue
    if name in ("CONNECT_TMDS", "DISCONNECT_TMDS", "CONNECT_DP", "DISCONNECT_DP"):
        hold(name, value, PLUG_LINUX.get(name))
    elif "__" in name:
        hold(name, value, sh.get(name + "_MASK"))
    elif name == "DC_GPIO_HPD_MASK":
        hold(name, value, d.get("mm" + name))
    else:
        # A register of a hot plug line's block: at each of the six.
        hold(name, value, d.get("mm" + name))
        for n in range(6):
            hold("HPD%d_%s" % (n, name), value + hpd_step * n, d.get("mmHPD%d_%s" % (n, name)))
pin_masks = table(plug_text, "HPD_PIN_MASKS")
for n, mask in enumerate(pin_masks):
    hold("DC_GPIO_HPD%d_MASK" % (n + 1), mask, sh.get("DC_GPIO_HPD_MASK__DC_GPIO_HPD%d_MASK_MASK" % (n + 1)))
hold("the hot plug pins' mask bits", len(pin_masks), 6)
hold("LINES of gpu_plug.mlx", number_of(plug_text, "LINES"), 6)
# (The one shift gpu_plug.mlx writes in place, for the log.)
hold("DC_HPD_DISCONNECT_INT_DELAY's shift", 20, sh.get("DC_HPD_TOGGLE_FILT_CNTL__DC_HPD_DISCONNECT_INT_DELAY__SHIFT"))
hold("DC_HPD_CONNECT_INT_DELAY's shift", 0, sh.get("DC_HPD_TOGGLE_FILT_CNTL__DC_HPD_CONNECT_INT_DELAY__SHIFT"))

# gpu_ddc.mlx. Its own numbers: "none", how many blocks and bytes are
# kept, the reading's time, how many messages a transfer has.
DDC_OWN = {"NONE", "BLOCKS", "ROOM", "BUDGET", "MESSAGES"}
i2c_h = source("dce_i2c_hw.h")
i2c_c = source("dce_i2c_hw.c")
resource_c = source("dce112_resource.c")
edid_c = source("drm_edid.c")
edid_h = source("drm_edid.h")
DDC_LINUX = {}
for m in re.finditer(r"^\s*(I2C_\w+|TRANSACTION_TIMEOUT_IN_I2C_CLOCKS)\s*=\s*(\d+),", i2c_h, re.M):
    DDC_LINUX[m.group(1)] = int(m.group(2))
# (The status fields' values: an enum in its order.)
body = re.search(r"enum dc_i2c_status \{(.*?)\};", i2c_h, re.S).group(1)
at = 0
for entry in [e.strip() for e in body.split(",") if e.strip()]:
    m = re.match(r"(\w+)\s*(?:=\s*(\d+))?$", entry)
    if m.group(2) is not None:
        at = int(m.group(2))
    DDC_LINUX[m.group(1)] = at
    at += 1
DDC_LINUX["SPEED"] = one(r"dc->caps\.i2c_speed_in_khz = (\d+);", resource_c)
DDC_LINUX["LINES"] = one(r"polaris_10_resource_cap = \{[^}]*?\.num_ddc = (\d+)", resource_c)
DDC_LINUX["LINES_SMALL"] = one(r"polaris_11_resource_cap = \{[^}]*?\.num_ddc = (\d+)", resource_c)
DDC_LINUX["THRESHOLD"] = one(r"DC_I2C_DDC1_THRESHOLD\), (\d+),", i2c_c)
DDC_LINUX["START_STOP_TIMING"] = one(r"DC_I2C_DDC1_START_STOP_TIMING_CNTL\), speed > 50 \? (\d+):", i2c_c)
settle = one(r"clock_en_bit_present\)\s*(?:/\*.*?\*/\s*)?msleep\((\d+)\);", re.sub(r"\n", " ", source("hw_ddc.c")))
DDC_LINUX["PAD_SETTLE"] = None if settle is None else settle * 1000
DDC_LINUX["DDC_ADDR"] = one(r"#define\s+DDC_ADDR\s+(0x[0-9a-fA-F]+)", edid_h)
DDC_LINUX["DDC_SEGMENT_ADDR"] = one(r"#define\s+DDC_SEGMENT_ADDR\s+(0x[0-9a-fA-F]+)", edid_c)
DDC_LINUX["EDID_LENGTH"] = one(r"#define\s+EDID_LENGTH\s+(\d+)", edid_h)
DDC_LINUX["TRANSFER_TRIES"] = one(r"int ret, retries = (\d+);", edid_c)
DDC_LINUX["BLOCK_READS"] = one(r"for \(try = 0; try < (\d+); try\+\+\)", edid_c)
# (The table of DDC lines: an I2C record's byte is a union of one byte,
# and the table's count of lines is an expression in atombios.h:
# ATOM_DEVICE_RESERVEDF_INDEX+1.)
SIZES["ATOM_I2C_ID_CONFIG_ACCESS"] = 1
if re.search(r"#define\s+ATOM_MAX_SUPPORTED_DEVICE\s+\(ATOM_DEVICE_RESERVEDF_INDEX\+1\)", bios_h):
    bios_defines.setdefault("ATOM_MAX_SUPPORTED_DEVICE", bios_defines["ATOM_DEVICE_RESERVEDF_INDEX"] + 1)
ddc_registers = 0
for name, value, inside in constants(ddc_text):
    if name in DDC_OWN:
        continue
    if name in DDC_LINUX:
        hold(name, value, DDC_LINUX[name])
    elif name == "GPIO_I2C_Info":
        hold(name, value, list_index("ATOM_MASTER_LIST_OF_DATA_TABLES", name))
    elif name.startswith("ATOM_"):
        struct, field = name.split("__")
        hold(name, value, layout(struct)[1] if field == "size" else offset_of(struct, field))
    elif "__" in name:
        hold(name, value, sh.get(name + "_MASK"))
        # A field of a line's word is at the same bits in each line's
        # (the pad's mode bit is named after the line too); a message's
        # in each message's.
        if "DDC1" in name:
            for n in range(2, 7):
                other = name.replace("DDC1", "DDC%d" % n).replace("AUX_PAD1", "AUX_PAD%d" % n)
                hold(other, value, sh.get(other + "_MASK"))
        if name.startswith("DC_I2C_TRANSACTION0__"):
            for k in range(1, 4):
                other = "DC_I2C_TRANSACTION%d__%s%d" % (k, name.split("__")[1][:-1], k)
                hold(other, value, sh.get(other + "_MASK"))
    else:
        ddc_registers += 1
        hold(name, value, d.get("mm" + name))
        # One word for each line: a number apart (the hardware's status),
        # two (speed and setup), four (the pins').
        if "DDC1" in name:
            step = 4 if name.startswith("DC_GPIO_") else (1 if name.endswith("HW_STATUS") else 2)
            for n in range(1, 7):
                hold(name.replace("DDC1", "DDC%d" % n), value + step * (n - 1), d.get("mm" + name.replace("DDC1", "DDC%d" % n)))
        if name == "DC_I2C_TRANSACTION0":
            for k in range(4):
                hold("DC_I2C_TRANSACTION%d" % k, value + k, d.get("mmDC_I2C_TRANSACTION%d" % k))
print("hot plug lines and DDC: %d numbers of gpu_plug.mlx and gpu_ddc.mlx held against Linux's files, %d wrong" % (held, bad))

# ---- A DisplayPort link: gpu_aux.mlx and gpu_link.mlx.
link_needed = ("drm_dp.h", "drm_edid.h", "drm_edid.c", "drm_dp_helper.c", "dc_ddc_types.h", "dc_dp_types.h", "dce_aux.c", "dce_aux.h",
               "link_dp_training.c", "link_dp_training_8b_10b.h", "link_dpms.c", "link_service_types.h")
link_missing = [name for name in link_needed if not os.path.exists(os.path.join(hdr, name))]
if link_missing:
    sys.exit("a DisplayPort link is not held: not in %s: %s" % (hdr, " ".join(link_missing)))
link_before = held
link_bad_before = bad

def values_of(name):
    # Every `#define NAME value` of a file and every member of its
    # enumerations (one without a value of its own is one more than the
    # member before it), the value a number, a shift `(a << b)`, a sum
    # `(a + b)` or another name of the file.
    out = {}
    text = re.sub(r"/\*.*?\*/", "", source(name), flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    later = []

    def number(value):
        value = value.strip().strip("()").strip()
        f = re.match(r"^(0x[0-9a-fA-F]+|\d+)\s*(<<|\+)\s*(\d+)$", value)
        if f:
            return (int(f.group(1), 0) << int(f.group(3))) if f.group(2) == "<<" else int(f.group(1), 0) + int(f.group(3))
        if re.match(r"^(0x[0-9a-fA-F]+|\d+)[uUlL]*$", value):
            return int(re.match(r"^(0x[0-9a-fA-F]+|\d+)", value).group(1), 0)
        return out.get(value)

    for m in re.finditer(r"^\s*#\s*define\s+(\w+)\s+(.+?)\s*$", text, re.M):
        got = number(m.group(2))
        if got is not None:
            out.setdefault(m.group(1), got)
    for m in re.finditer(r"\benum\s*\w*\s*\{([^}]*)\}", text):
        last = -1
        for member in m.group(1).split(","):
            f = re.match(r"^\s*(\w+)\s*(?:=\s*(.+?))?\s*$", member, re.S)
            if not f:
                continue
            got = last + 1 if f.group(2) is None else number(f.group(2))
            if got is None:
                last = -1
                continue
            out.setdefault(f.group(1), got)
            last = got
    return out

dp = values_of("drm_dp.h")
link_values = {}
for name in ("drm_edid.h", "drm_edid.c", "drm_dp_helper.c", "dc_ddc_types.h", "dc_dp_types.h", "dce_aux.c", "dce_aux.h",
             "link_dp_training.c", "link_dp_training_8b_10b.h", "link_dpms.c"):
    for key, value in values_of(name).items():
        link_values.setdefault(key, value)
# (What set_dp_phy_pattern_passthrough_mode writes: that function's text.)
passthrough = source("dce_link_encoder.c")
passthrough = passthrough[passthrough.index("static void set_dp_phy_pattern_passthrough_mode("):]
passthrough = passthrough[:passthrough.index("\n}\n")]
link_values["IDLE_BS_INTERVAL"] = one(r"DP_IDLE_BS_INTERVAL,\s*(0x[0-9a-fA-F]+)", passthrough)
link_values["SCRAMBLER_BS_COUNT"] = one(r"DPHY_SCRAMBLER_BS_COUNT,\s*(0x[0-9a-fA-F]+)", passthrough)
for n in range(7):
    elsewhere["mmDP%d_DP_DPHY_INTERNAL_CTRL" % n] = one(r"#define\s+mmDP%d_DP_DPHY_INTERNAL_CTRL\s+(0x[0-9a-fA-F]+)" % n, source("dce112_resource.c"))
# Numbers that are this driver's own: counts of the card's parts, its own
# bounds and waits (the files' heads say what each is), the polling's
# 10 us (dce_aux.c's REG_WAIT), "none", and two bits that drm_dp.h has no
# name for (dc_dp_types.h has them as bit fields: bit 5 of 02h and bit 1
# of 204h; gpu_link.mlx says so where they stand).
LINK_OWN = {"CHANNELS", "DDC_STEP", "HPD_STEP", "HPD_LINES", "POLL", "TRIES", "I2C_TRIES", "I2C_DEFERS", "EDID_TRIES", "ACCESS_MOST",
            "NONE", "TRY_TIME", "STATE_TIME", "WOKEN", "HOLDS_AFTER", "WORDS", "POST_LT_ADJ_REQ_SUPPORTED", "POST_LT_ADJ_REQ_IN_PROGRESS",
            "I2C_SPEED_KHZ"}
aux_text = open(os.path.join(here, "gpu_aux.mlx")).read()
link_text = open(os.path.join(here, "gpu_link.mlx")).read()
aux_offsets = table(aux_text, "AUX_OFFSETS")
hold("the AUX channels", len(aux_offsets), 6)
aux_names = dict((name, value) for name, value, inside in constants(aux_text))
for name, value, inside in constants(aux_text):
    if name in LINK_OWN:
        continue
    if "__" in name:
        hold(name, value, sh.get(name + "_MASK"))
        if name.startswith("DC_GPIO_DDC1_MASK__"):
            for k in range(2, 7):
                other = name.replace("DDC1", "DDC%d" % k).replace("PAD1", "PAD%d" % k)
                hold(other, value, sh.get(other + "_MASK"))
    elif name.startswith("AUX_") and ("mmDP_AUX0_" + name) in d:
        for n in range(6):
            hold("DP_AUX%d_%s" % (n, name), value + aux_offsets[n], d.get("mmDP_AUX%d_%s" % (n, name)))
    elif name in ("AUXP_IMPCAL", "AUXN_IMPCAL"):
        hold(name, value, d.get("mm" + name))
    elif name in ("DC_GPIO_DDC1_MASK", "DC_GPIO_DDC1_A"):
        for k in range(6):
            hold(name.replace("DDC1", "DDC%d" % (k + 1)), value + aux_names["DDC_STEP"] * k, d.get("mm" + name.replace("DDC1", "DDC%d" % (k + 1))))
    elif name.startswith("DP_"):
        hold(name, value, dp.get(name))
    else:
        hold(name, value, link_values.get(name))
# (The fields of the first channel's words are every channel's.)
for name, value, inside in constants(aux_text):
    if "__" in name and name.startswith("AUX_") and not name.startswith(("AUXP", "AUXN")):
        for n in range(6):
            hold("DP_AUX%d_%s" % (n, name), value, sh.get(name + "_MASK"))
link_names = dict((name, value) for name, value, inside in constants(link_text))
for name, value, inside in constants(link_text):
    if name in LINK_OWN:
        continue
    if "__" in name:
        hold(name, value, sh.get(name + "_MASK"))
    elif name in ("DC_HPD_INT_STATUS", "DC_HPD_CONTROL"):
        for n in range(6):
            hold("HPD%d_%s" % (n, name), value + link_names["HPD_STEP"] * n, d.get("mmHPD%d_%s" % (n, name)))
    elif name.startswith("DP_DPHY") or name == "DP_LINK_FRAMING_CNTL":
        # In a back end's block (the one word that is not in the header:
        # dce112_resource.c's numbers).
        for n in range(7):
            hold("DP%d_%s" % (n, name), digs[n] + value, d.get("mmDP%d_%s" % (n, name), elsewhere.get("mmDP%d_%s" % (n, name))))
    elif name.startswith("DP_"):
        hold(name, value, dp.get(name))
    else:
        hold(name, value, link_values.get(name))
rates = re.search(r"pub const RATES = \[_\]usize\{([^}]*)\}", link_text).group(1).replace(" ", "").split(",")
hold("the rates there are", int(rates == ["LINK_RATE_LOW", "LINK_RATE_HIGH", "LINK_RATE_HIGH2", "LINK_RATE_HIGH3"]), 1)
print("DisplayPort link: %d numbers of gpu_aux.mlx and gpu_link.mlx held against Linux's files, %d wrong" % (held - link_before, bad - link_bad_before))
sys.exit(1 if bad or rom_bad or wrong or display_bad else 0)
