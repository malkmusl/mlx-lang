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
# and for the picture brought to and from a DisplayPort output and that
# output's hot plug line (gpu_mode.mlx's "A MOVE WITH A DISPLAYPORT END",
# gpu_dpwatch.mlx):
#   display/dc/inc/hw/stream_encoder.h         the stream's pixel depth
#   display/dc/dc_dp_types.h                   the short pulse's bytes
#   include/drm/display/drm_dp.h               (of the kernel's tree) the
#                                              sink count's bits
#   display/dc/link/protocols/link_dp_irq_handler.c  where they are read
# and for a passive adapter behind a DisplayPort connector (gpu_adapter.mlx):
#   drivers/gpu/drm/display/drm_dp_dual_mode_helper.c,
#   include/drm/display/drm_dp_dual_mode_helper.h  (of the kernel's tree)
#   display/dc/link/link_detection.c           how often the display core
#                                              reads an adapter's ID
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
# clock, at all six. Of a move with a DisplayPort end and that output's
# hot plug line (gpu_dpwatch.mlx): the stream's pixel depth, each line's
# interrupt word at all six and its two bits, the sink count's bits as
# drm_dp.h takes them, the bytes read at a short pulse as dc_dp_types.h
# has them (the 2 s between looks at the lanes are ours, not Linux's).
# Of the card's engines (gpu_engines.mlx: the stages toward a card that
# takes commands): every register, SMC address and field against the
# memory controller's, the system's, the bus interface's, the graphics
# block's and the SMC's headers, Linux's numbers where its files have
# them (the GART's place and size, an entry's bits, the interrupt ring,
# the waits, the golden tables in their order, the firmware files' names
# and header versions), the display's runs of tests/support/
# engine_card.mlx made again from dce_11_2_d.h, and that no register a
# step writes is the display's (the files are named at that section).
# Of stages 3 and 4 (gpu_smc_sdma.mlx: the SMC's firmware, the engines'
# firmware through it, the copy engines): every register, field, SMC
# address, message, kind and bit of the engines' list, packet word and
# Linux's value against the SMC's, the list's, the packets' and the
# copy engines' files, the list's order and the copies', the golden
# table, and that no register those steps write is the display's.
# Of stage 5 (gpu_compute.mlx, gpu_ring.mlx, gpu_clear_state.mlx: the
# graphics block, the RLC, the KIQ and compute ring 0, the ring as an
# interface): every register, field, packet and macro's shift, the golden
# tables row for row, the tiling words and the raster configuration worked
# out from gfx_v8_0.c's macros, the queue descriptor's words from
# vi_structs.h, the KIQ's queue by amdgpu_gfx_kiq_acquire's rule, Linux's
# values where its files have them, the clear state value for value, and
# that no register stage 5 writes is the display's.
# Of stages 6 and 7 (gpu_graphics.mlx, gpu_space.mlx, gpu_first_space.mlx
# and gpu_ring.mlx's frame for a program's buffer: the graphics ring, the
# programs' address spaces as an interface, V1): every register, field,
# event and Linux's value (the first submission's size, the polls, the
# context control's words, the reserved bottom, the DMA's bits, the
# address spaces handed out), gpu_map's flags against the spec, the order
# of a job's frame in amdgpu_ib_schedule and amdgpu_vm_flush and the same
# in submitIn, the entries' bits and addresses as amdgpu_vm.c,
# amdgpu_ttm.c, amdgpu_vram_mgr.c and amdgpu_gmc.c make them, the fault
# taken as gmc_v8_0_process_interrupt takes it, and that no register
# these stages write is the display's but DMIF_ADDR_CALC, written only
# where it differs. (tools/check_gpu_clear_state.py holds the graphics
# ring's first submission, as tests/506 prints it, word for word.)
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
    "NO_CONNECTOR", "STILL",
    # (A move's pixel on a DisplayPort link: three colours of eight bits,
    # the depth the stream is told.)
    "MOVE_BITS"}

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
LINUX_VALUES["DP_COMPONENT_PIXEL_DEPTH_8BPC"] = one(r"DP_COMPONENT_PIXEL_DEPTH_8BPC\s*=\s*(0x[0-9a-fA-F]+|\d+)", source("stream_encoder.h"))
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
            "I2C_SPEED_KHZ", "FALLBACKS", "PLANS_AGAIN"}
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

# ---- The hot plug line of a DisplayPort output (gpu_dpwatch.mlx): the
# short pulse's latch and its acknowledgement in each line's words, the
# sink count's bits (DP_GET_SINK_COUNT), the six bytes read from 200h.
watch_before, watch_bad_before = held, bad
watch_text = open(os.path.join(here, "gpu_dpwatch.mlx")).read()
watch = dict((m.group(1), int(m.group(2), 0)) for m in re.finditer(r"^(?:pub )?const (\w+): usize = (0x[0-9A-Fa-f]+|\d+)\s*$", watch_text, re.M))
for name in ("DC_HPD_INT_CONTROL",):
    hold(name, watch[name], d.get("mm" + name))
    for n in range(6):
        hold("HPD%d_%s" % (n, name), watch[name] + hpd_step * n, d.get("mmHPD%d_%s" % (n, name)))
for name in ("DC_HPD_INT_CONTROL__DC_HPD_RX_INT_ACK", "DC_HPD_INT_STATUS__DC_HPD_RX_INT_STATUS"):
    hold(name, watch[name], sh.get(name + "_MASK"))
sink = re.search(r"#\s*define\s+DP_GET_SINK_COUNT\(x\)\s*\(\(\(\(x\) & (0x[0-9a-fA-F]+)\) >> 1\) \| \(\(x\) & (0x[0-9a-fA-F]+)\)\)", source("drm_dp.h"))
hold("DP_SINK_COUNT_HIGH", watch["DP_SINK_COUNT_HIGH"], int(sink.group(1), 16) if sink else None)
hold("DP_SINK_COUNT_MASK", watch["DP_SINK_COUNT_MASK"], int(sink.group(2), 16) if sink else None)
hold("the short pulse's bytes", int(re.search(r"var six: \[(\d+)\]u8", watch_text).group(1)), one(r"union hpd_irq_data \{.*?uint8_t raw\[(\d+)\];", re.sub(r"\n", " ", source("dc_dp_types.h"))))
# (From DisplayPort 1.4 on: the event status indicators, as
# link_dp_irq_handler.c's dp_read_hpd_rx_irq_data reads and takes them.)
if not os.path.exists(os.path.join(hdr, "link_dp_irq_handler.c")):
    sys.exit("the short pulse's bytes are not held: not in %s: link_dp_irq_handler.c" % hdr)
irq = re.sub(r"\s+", " ", source("link_dp_irq_handler.c"))
ESI = ("DP_SINK_COUNT_ESI", "DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0", "DP_LANE0_1_STATUS_ESI", "DP_LANE2_3_STATUS_ESI", "DP_LANE_ALIGN_STATUS_UPDATED_ESI", "DP_SINK_STATUS_ESI")
for name in ("DP_DPCD_REV_14",) + ESI:
    hold(name, watch[name], dp.get(name))
    if name != "DP_DPCD_REV_14":
        hold("%s taken" % name, int(("esi[%s - DP_SINK_COUNT_ESI]" % name) in watch_text), int(("tmp[%s - DP_SINK_COUNT_ESI]" % name) in irq))
hold("the event status indicators read", int(re.search(r"var esi: \[(\d+)\]u8", watch_text).group(1)), dp["DP_SINK_STATUS_ESI"] - dp["DP_SINK_COUNT_ESI"] + 1 if "uint8_t tmp[DP_SINK_STATUS_ESI - DP_SINK_COUNT_ESI + 1]" in irq else None)
hold("read there from revision 1.4", int("if revision < DP_DPCD_REV_14 {" in watch_text), int("dpcd_rev.raw < DPCD_REV_14" in irq))
print("DisplayPort hot plug: %d numbers of gpu_dpwatch.mlx held against Linux's files, %d wrong" % (held - watch_before, bad - watch_bad_before))

# ---- A passive adapter behind a DisplayPort connector (gpu_adapter.mlx):
# its address and registers, the HDMI ID's text and the clocks
# (drivers/gpu/drm/display/drm_dp_dual_mode_helper.c and include/drm/
# display/drm_dp_dual_mode_helper.h of the kernel's tree).
adapter_before, adapter_bad_before = held, bad
adapter_missing = [name for name in ("drm_dp_dual_mode_helper.h", "drm_dp_dual_mode_helper.c", "link_detection.c") if not os.path.exists(os.path.join(hdr, name))]
if adapter_missing:
    sys.exit("a passive adapter is not held: not in %s: %s" % (hdr, " ".join(adapter_missing)))
adapter_text = open(os.path.join(here, "gpu_adapter.mlx")).read()
dual = values_of("drm_dp_dual_mode_helper.h")
for key, value in values_of("drm_dp_dual_mode_helper.c").items():
    dual.setdefault(key, value)
dual_c = re.sub(r"\s+", " ", source("drm_dp_dual_mode_helper.c"))
dual["TYPE1_MAX_TMDS_CLOCK"] = one(r"if \(type < DRM_DP_DUAL_MODE_TYPE2_DVI\) return (\d+);", dual_c)
# (How often the HDMI ID is read: once, and again while the display
# core's retry_count lasts, link_detection.c's query_dp_dual_mode_adaptor.)
retries = one(r"int retry_count = (\d+);", re.sub(r"\s+", " ", source("link_detection.c")))
dual["ID_READS"] = None if retries is None else retries + 1
for name, value, inside in constants(adapter_text):
    if name != "NONE":
        hold(name, value, dual.get(name))
hdmi_id = re.search(r'pub const HDMI_ID = "([^"]*)"', adapter_text)
want_id = re.search(r'dp_dual_mode_hdmi_id\[DP_DUAL_MODE_HDMI_ID_LEN\] = "([^"]*)"', dual_c)
hold("the HDMI ID's text", int(hdmi_id is not None and want_id is not None and hdmi_id.group(1) == want_id.group(1)), 1)
hold("a type 2 adapter's clock in 2.5 MHz", int("a.clockByte * 5000 / 2" in adapter_text), int("return max_tmds_clock * 5000 / 2;" in dual_c))
print("a passive adapter: %d numbers of gpu_adapter.mlx held against Linux's files, %d wrong" % (held - adapter_before, bad - adapter_bad_before))

# ---- The card's engines: gpu_engines.mlx (the stages toward a card that
# takes commands: S0, M1, M2, I1). Beside the files above, of Linux 6.6's
# drivers/gpu/drm/amd:
#   include/asic_reg/gmc/gmc_8_1_sh_mask.h, oss/oss_3_0_d.h,
#   oss/oss_3_0_sh_mask.h, bif/bif_5_0_sh_mask.h, gca/gfx_8_0_d.h,
#   gca/gfx_8_0_sh_mask.h, smu/smu_7_1_3_d.h, smu/smu_7_1_3_sh_mask.h
#                                       the registers, the SMC's, fields
#   amdgpu/cikd.h                       the memory's kind in MC_SEQ_MISC0
#   amdgpu/gmc_v8_0.c amdgpu/amdgpu_gmc.c amdgpu/amdgpu_gmc.h
#                                       the memory start, the GART's place
#                                       and enable, its golden words
#   amdgpu/amdgpu_vm.h amdgpu/amdgpu_gart.h amdgpu/amdgpu.h
#                                       an entry's bits, a page, the waits
#   amdgpu/tonga_ih.c amdgpu/amdgpu_ih.h the interrupt ring
#   amdgpu/gfx_v8_0.c amdgpu/sdma_v3_0.c the golden tables read at S0,
#                                       the address configuration
#   amdgpu/amdgpu_ucode.h               a firmware file's header
#   amdgpu/amdgpu_cgs.c                 which SMC file a card takes
#   pm/powerplay/smumgr/smu7_smumgr.c, polaris10_smumgr.c
#                                       the SMC running, its RAM, the fuse
# and the display's runs of tests/support/engine_card.mlx made again from
# dce_11_2_d.h; and that no register the steps write is the display's.
engines_needed = ("gmc_8_1_sh_mask.h", "oss_3_0_d.h", "oss_3_0_sh_mask.h", "bif_5_0_sh_mask.h", "gfx_8_0_d.h", "gfx_8_0_sh_mask.h",
                  "smu_7_1_3_d.h", "smu_7_1_3_sh_mask.h", "cikd.h", "gmc_v8_0.c", "amdgpu_gmc.c", "amdgpu_gmc.h", "amdgpu_vm.h",
                  "amdgpu_gart.h", "amdgpu.h", "tonga_ih.c", "amdgpu_ih.h", "gfx_v8_0.c", "sdma_v3_0.c", "amdgpu_ucode.h",
                  "amdgpu_cgs.c", "smu7_smumgr.c", "polaris10_smumgr.c")
engines_missing = [name for name in engines_needed if not os.path.exists(os.path.join(hdr, name))]
if engines_missing:
    sys.exit("the card's engines are not held: not in %s: %s" % (hdr, " ".join(engines_missing)))
engines_before = held
engines_bad_before = bad
engines_text = open(os.path.join(here, "gpu_engines.mlx")).read()
regs = {}
for name in ("gmc_8_1_d.h", "oss_3_0_d.h", "bif_5_0_d.h", "gfx_8_0_d.h", "smu_7_1_3_d.h"):
    for key, value in defines(os.path.join(hdr, name)).items():
        regs.setdefault(key, value)
masks = {}
for name in ("gmc_8_1_sh_mask.h", "oss_3_0_sh_mask.h", "bif_5_0_sh_mask.h", "gfx_8_0_sh_mask.h", "smu_7_1_3_sh_mask.h", "dce_11_2_sh_mask.h"):
    for key, value in defines(os.path.join(hdr, name)).items():
        masks.setdefault(key, value)
DISPLAYS = {"VGA_RENDER_CONTROL", "VGA_HDP_CONTROL", "DMIF_STATUS", "DMIF_ADDR_CALC"}
cikd = defines(os.path.join(hdr, "cikd.h"))
gmc_c = text_of("gmc_v8_0.c")
gmc_common = text_of("amdgpu_gmc.c")
ih_c = text_of("tonga_ih.c")
gfx_c = text_of("gfx_v8_0.c")
sdma_c = text_of("sdma_v3_0.c")
smu7_c = text_of("smu7_smumgr.c")
p10_c = text_of("polaris10_smumgr.c")
ucode_h = re.sub(r"/\*.*?\*/", "", text_of("amdgpu_ucode.h"), flags=re.S)

def bits(expression):
    # "(1ULL << 4)" and plain numbers.
    m = re.match(r"\(?\s*1ULL\s*<<\s*(\d+)\s*\)?$", expression.strip())
    if m:
        return 1 << int(m.group(1))
    return int(expression.strip().rstrip("ULl"), 0)

def define_in(name, define):
    m = re.search(r"#define\s+%s\s+([^/\n]+)" % define, text_of(name))
    return None if m is None else bits(m.group(1))

def ucode_offset(struct, field):
    # A field's offset in amdgpu_ucode.h's structures (uint32_t and
    # uint16_t fields, a struct common_firmware_header first).
    m = re.search(r"struct\s+%s\s*\{(.*?)\};" % struct, ucode_h, re.S)
    at = 0
    for kind, name in re.findall(r"(struct\s+\w+|uint32_t|uint16_t)\s+(\w+)\s*;", m.group(1)):
        if name == field:
            return at
        at += {"uint32_t": 4, "uint16_t": 2}.get(kind, 32)
    return at if field == "size" else None

# Linux's numbers that are no register, each where Linux has it.
blocks = re.search(r"amdgpu_vm_adjust_size\(adev,\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\)", gmc_c)
levels = 2  # what amdgpu_vm_adjust_size's tmp holds when it asks for the block size (the study's 2.5)
level_bits = (levels.bit_length() - 1) + 18
LINUX = {
    "MC_MASK": int(re.search(r"mc_mask = (0x[0-9a-fA-F]+)ULL", gmc_c).group(1), 0),
    "AMDGPU_GMC_HOLE_START": define_in("amdgpu_gmc.h", "AMDGPU_GMC_HOLE_START"),
    "GART_BYTES": int(re.search(r"case CHIP_POLARIS10:[^\n]*\n(?:[^\n]*\n)*?\s*adev->gmc.gart_size = (\d+)ULL << 20", gmc_c).group(1)) << 20,
    "FOUR_GB": int(re.search(r"four_gb = (0x[0-9a-fA-F]+)ULL", gmc_common).group(1), 0),
    "AMDGPU_GPU_PAGE_SIZE": define_in("amdgpu_gart.h", "AMDGPU_GPU_PAGE_SIZE"),
    "ENTRY_BYTES": int(re.search(r"table_size = adev->gart.num_gpu_pages \* (\d+)", gmc_c).group(1)),
    "PTE_ADDRESS": int(re.search(r"value = addr & (0x[0-9A-Fa-f]+)ULL", gmc_common).group(1), 0),
    "AMDGPU_MAX_USEC_TIMEOUT": define_in("amdgpu.h", "AMDGPU_MAX_USEC_TIMEOUT"),
    "MAX_PFN": int(blocks.group(2 - 1)) << 18,
    "FRAGMENT_SIZE": int(blocks.group(2)),
    "BLOCK_SIZE": level_bits - 9 if levels <= 8 else (level_bits + 3) // 2,
    "SYSTEM_ACCESS_MODE": int(re.search(r"SYSTEM_ACCESS_MODE, (\d+)\)", gmc_c).group(1)),
    "EFFECTIVE_L2_QUEUE_SIZE": int(re.search(r"EFFECTIVE_L2_QUEUE_SIZE, (\d+)\)", gmc_c).group(1)),
    "CONTEXT1_IDENTITY_ACCESS_MODE": int(re.search(r"CONTEXT1_IDENTITY_ACCESS_MODE, (\d+)\)", gmc_c).group(1)),
    "AGP_EMPTY": int(re.search(r"WREG32\(mmMC_VM_AGP_TOP, (0x[0-9A-Fa-f]+)\)", gmc_c).group(1), 0),
    "IH_RING_BYTES": eval(re.search(r"amdgpu_ih_ring_init\(adev, &adev->irq.ih, ([\d *]+), true\)", ih_c).group(1)),
    "IH_ENTRY_BYTES": int(re.search(r"ih->rptr \+= (\d+);", ih_c).group(1)),
    "AMDGPU_IH_MAX_NUM_IVS": define_in("amdgpu_ih.h", "AMDGPU_IH_MAX_NUM_IVS"),
    "SMC_RAM_RUNNING": int(re.search(r"(0x[0-9a-fA-F]+) <= cgs_read_ind_register\(hwmgr->device, CGS_IND_REG__SMC, ixSMC_PC_C\)", smu7_c).group(1), 0),
    "AVFS_FUSE_WORD": int(re.search(r"ixSMU_EFUSE_0 \+ \((\d+)\*4\)", p10_c).group(1)),
    "TONGA_GB_ADDR_CONFIG_GOLDEN": int(re.search(r"#define TONGA_GB_ADDR_CONFIG_GOLDEN (0x[0-9a-fA-F]+)", gfx_c).group(1), 0),
    "SMU7_SMC_SIZE": int(re.search(r"#define SMU7_SMC_SIZE (0x[0-9a-fA-F]+)", smu7_c).group(1), 0),
    "HDP_SURFACE_FIRST": int(re.search(r"WREG32\(\((0x[0-9a-fA-F]+) \+ j\), 0x00000000\);", gmc_c).group(1), 0),
    "HDP_SURFACE_STEP": int(re.search(r"i < \d+; i\+\+, j \+= (0x[0-9a-fA-F]+)\)", gmc_c).group(1), 0),
    "HDP_SURFACE_WORDS": len(re.findall(r"WREG32\(\(0x[0-9a-fA-F]+ \+ j\), 0x00000000\);", gmc_c)),
    "HDP_SURFACES": int(re.search(r"for \(i = 0, j = 0; i < (\d+); i\+\+, j \+= 0x6\)", gmc_c).group(1)),
    "GFX_Info": list_index("ATOM_MASTER_LIST_OF_DATA_TABLES", "GFX_Info"),
    "ATOM_GFX_INFO_V2_1__GfxIpMinVer": offset_of("ATOM_GFX_INFO_V2_1", "GfxIpMinVer"),
    "ATOM_GFX_INFO_V2_1__size": layout("ATOM_GFX_INFO_V2_1")[1],
    "ATOM_FIRMWARE_INFO_V2_2__ulDefaultEngineClock": offset_of("ATOM_FIRMWARE_INFO_V2_2", "ulDefaultEngineClock"),
    "ATOM_FIRMWARE_INFO_V2_2__ulDefaultMemoryClock": offset_of("ATOM_FIRMWARE_INFO_V2_2", "ulDefaultMemoryClock"),
    "HEADER__size_bytes": ucode_offset("common_firmware_header", "size_bytes"),
    "HEADER__header_version_major": ucode_offset("common_firmware_header", "header_version_major"),
    "HEADER__header_version_minor": ucode_offset("common_firmware_header", "header_version_minor"),
    "HEADER__ucode_version": ucode_offset("common_firmware_header", "ucode_version"),
    "HEADER__ucode_size_bytes": ucode_offset("common_firmware_header", "ucode_size_bytes"),
    "HEADER__ucode_array_offset_bytes": ucode_offset("common_firmware_header", "ucode_array_offset_bytes"),
    "HEADER__size": ucode_offset("common_firmware_header", "size"),
    "GFX_HEADER__jt_offset": ucode_offset("gfx_firmware_header_v1_0", "jt_offset"),
    "GFX_HEADER__jt_size": ucode_offset("gfx_firmware_header_v1_0", "jt_size"),
    "ATOM_S7_ASIC_INIT_COMPLETE_MASK": scratch_bits.get("ATOM_S7_ASIC_INIT_COMPLETE_MASK"),
    "MC_SEQ_MISC0__MT__MASK": cikd.get("MC_SEQ_MISC0__MT__MASK"),
    "MC_SEQ_MISC0__MT__GDDR5": cikd.get("MC_SEQ_MISC0__MT__GDDR5"),
}
for flag in ("VALID", "SYSTEM", "SNOOPED", "EXECUTABLE", "READABLE", "WRITEABLE"):
    LINUX["AMDGPU_PTE_" + flag] = define_in("amdgpu_vm.h", "AMDGPU_PTE_" + flag)
# The version the register list of polaris10_mc.bin leaves (its own
# ucode_version, read from the repository's file).
firmware_dir = os.path.join(here, "..", "firmware", "amdgpu")
with open(os.path.join(firmware_dir, "polaris10_mc.bin"), "rb") as f:
    mc = f.read()
LINUX["MC_LIST_VERSION"] = int.from_bytes(mc[16:20], "little")
# This driver's own numbers (the file's head says what each is), and
# DMA_PAGES, which is gpu.mlx's.
ENGINES_OWN = {"DRIVER_PART", "PROVED", "SCRATCH_TRIAL", "TABLE_AT", "DEFAULT_PAGE_AT", "BEYOND_AT", "SILENT_FOR", "LOOK_EVERY", "STORM",
               "RATE_SLACK", "KEPT", "NONE", "MEC_FILE", "ON_RING", "ON_COPY", "ON_QUEUES", "ON_PROCESSORS", "ON_RLC",
               "ON_DOORBELLS", "ON_GART", "ON_SMC"}
gpu_text = open(os.path.join(here, "gpu.mlx")).read()
hold("DMA_PAGES is gpu.mlx's", int(re.search(r"const DMA_PAGES: usize = (\d+)", engines_text).group(1)),
     int(re.search(r"const DMA_PAGES: usize = (\d+)", gpu_text).group(1)))
hold("DEFAULT_PAGE_AT is behind the GART's table", int(LINUX["GART_BYTES"] // LINUX["AMDGPU_GPU_PAGE_SIZE"] * LINUX["ENTRY_BYTES"]),
     LINUX["GART_BYTES"] // LINUX["AMDGPU_GPU_PAGE_SIZE"] * LINUX["ENTRY_BYTES"])
engine_names = {}
for name, value, inside in constants(engines_text):
    engine_names[name] = value
    if name in ENGINES_OWN or name == "DMA_PAGES":
        continue
    if name in LINUX:
        hold(name, value, LINUX[name])
    elif name.startswith("ix"):
        hold(name, value, regs.get(name))
    elif "__" in name:
        hold(name, value, masks.get(name + "_MASK"))
    elif name in DISPLAYS:
        hold(name, value, d.get("mm" + name))
    else:
        hold(name, value, regs.get("mm" + name))
# The golden tables, register for register in Linux's order.
def golden(text, table):
    body = re.search(r"static const u32 %s\[\] =\s*\{(.*?)\};" % table, text, re.S).group(1)
    rows = [row.strip() for row in body.replace("\n", " ").split(",") if row.strip()]
    return rows
def resolve(name):
    if name.startswith("mm"):
        return regs.get(name, d.get(name))
    return int(name, 0)
memory_rows = golden(gmc_c, "golden_settings_polaris10_a11")
code_memory = [int(x, 0) if x.startswith("0x") or x.isdigit() else engine_names.get(x) for x in
               re.search(r"pub const GOLDEN_MEMORY = \[_\]usize\{([^}]*)\}", engines_text).group(1).replace(" ", "").split(",")]
hold("the memory controller's golden words, as many", len(code_memory), len(memory_rows))
for k, row in enumerate(memory_rows[:len(code_memory)]):
    hold("golden memory word %d" % k, code_memory[k], resolve(row))
for table, source, mine in (("golden_settings_polaris10_a11", gfx_c, "GOLDEN_GRAPHICS"), ("polaris10_golden_common_all", gfx_c, "GOLDEN_COMMON"),
                            ("golden_settings_polaris10_a11", sdma_c, "GOLDEN_COPY")):
    rows = golden(source, table)
    wanted = [resolve(rows[k]) for k in range(0, len(rows), 3)]
    code = table_of = [int(x, 0) for x in re.search(r"pub const %s = \[_\]usize\{([^}]*)\}" % mine, engines_text).group(1).replace(" ", "").split(",")]
    hold("%s's registers, as many" % mine, len(code), len(wanted))
    for k in range(min(len(code), len(wanted))):
        hold("%s %d" % (mine, k), code[k], wanted[k])
# The firmware files' names as Linux asks for them, and the header
# versions the repository's files have.
names = re.findall(r'"(polaris10_\w+\.bin)"', re.search(r"pub const FILES = \[_\]\[\]const u8\{([^}]*)\}", engines_text).group(1))
linux_names = set(re.findall(r'"amdgpu/(polaris10_\w+\.bin)"', gfx_c + sdma_c + text_of("amdgpu_cgs.c")))
majors = [int(x) for x in re.search(r"pub const FILE_MAJOR = \[_\]usize\{([^}]*)\}", engines_text).group(1).replace(" ", "").split(",")]
minors = [int(x) for x in re.search(r"pub const FILE_MINOR = \[_\]usize\{([^}]*)\}", engines_text).group(1).replace(" ", "").split(",")]
for k, name in enumerate(names):
    hold("Linux asks for " + name, int(name in linux_names), 1)
    with open(os.path.join(firmware_dir, name), "rb") as f:
        head = f.read(12)
    hold(name + "'s header version", (majors[k] << 16) | minors[k], (int.from_bytes(head[8:10], "little") << 16) | int.from_bytes(head[10:12], "little"))
for name in re.findall(r'pub const SMC_\w+ = "(polaris10_\w+\.bin)"', engines_text):
    hold("Linux asks for " + name, int(name in linux_names), 1)
# The display's runs of the test's card, made again from the header (its
# numbers but for the audio controller's: below 0xc0, and the aliases).
display_numbers = sorted(set(v for k, v in d.items() if k.startswith("mm") and v >= 0xC0 and not k.endswith("_ALIAS")))
runs = []
first = last = display_numbers[0]
for v in display_numbers[1:]:
    if v - last > 16:
        runs += [first, last]
        first = v
    last = v
runs += [first, last]
card_text = open(os.path.join(here, "..", "..", "..", "tests", "support", "engine_card.mlx")).read()
card_runs = [int(x, 0) for x in re.search(r"pub const DISPLAY_RUNS = \[_\]usize\{([^}]*)\}", card_text).group(1).replace(" ", "").split(",")]
hold("the display's runs of engine_card.mlx are dce_11_2_d.h's", int(card_runs == runs), 1)
# No register a step writes is the display's.
written_names = set(re.findall(r"\b(?:put|alter|set|whereDiffers|change)\(e, (\w+)", engines_text)) | set(re.findall(r"\bwr\(e, (\w+)", engines_text))
for name in sorted(written_names):
    if name not in engine_names:
        continue
    number = engine_names[name]
    in_display = any(runs[k] <= number <= runs[k + 1] for k in range(0, len(runs), 2))
    hold("%s (%#x) written by a step is no register of the display's" % (name, number), int(in_display), 0)
print("the card's engines: %d numbers of gpu_engines.mlx (and engine_card.mlx's display runs) held against Linux's files, %d wrong" % (held - engines_before, bad - engines_bad_before))
# ---- Stages 3 and 4: gpu_smc_sdma.mlx (the SMC's firmware, the engines'
# firmware loaded through it, the two copy engines). Beside the files
# above, of Linux 6.6's drivers/gpu/drm/amd:
#   pm/powerplay/inc/smu7_ppsmc.h       the SMC's messages and answers
#   pm/powerplay/inc/smu7.h, smu74.h    the firmware's place in the SMC's
#                                       RAM, its header, its soft registers
#   pm/powerplay/inc/smu_ucode_xfer_vi.h  the engines' kinds, their bits,
#                                       the list (struct SMU_DRAMData_TOC)
#   pm/powerplay/smumgr/smu7_smumgr.h   the SMC's RAM's end
#   pm/powerplay/hwmgr/hwmgr.c          how long the SMC's waits are
#   include/amd_shared.h                AMD_MAX_USEC_TIMEOUT
#   include/ivsrcid/ivsrcid_vislands30.h  the copy engines' trap
#   amdgpu/tonga_sdma_pkt_open.h        the copy engines' packets
#   amdgpu/amdgpu_doorbell.h            their doorbells
#   amdgpu/amdgpu_ib.c amdgpu/amdgpu_drv.c  the indirect buffer test's
#                                       time, the ring's submissions, the
#                                       phase quantum
# and that no register these steps write is the display's.
smc_sdma_needed = ("smu7_ppsmc.h", "smu7.h", "smu74.h", "smu_ucode_xfer_vi.h", "smu7_smumgr.h", "hwmgr.c", "amd_shared.h",
                   "ivsrcid_vislands30.h", "tonga_sdma_pkt_open.h", "amdgpu_doorbell.h", "amdgpu_ib.c", "amdgpu_drv.c", "vid.h")
smc_sdma_missing = [name for name in smc_sdma_needed if not os.path.exists(os.path.join(hdr, name))]
if smc_sdma_missing:
    sys.exit("stages 3 and 4 are not held: not in %s: %s" % (hdr, " ".join(smc_sdma_missing)))
loaded_before = held
loaded_bad_before = bad
loaded_text = open(os.path.join(here, "gpu_smc_sdma.mlx")).read()
ppsmc = {m.group(1): int(m.group(2), 0) for m in re.finditer(r"#define\s+(PPSMC_\w+)\s+\(\(uint16_t\)\s*(0x[0-9a-fA-F]+)\)", text_of("smu7_ppsmc.h"))}
xfer = defines(os.path.join(hdr, "smu_ucode_xfer_vi.h"))
pkt = defines(os.path.join(hdr, "tonga_sdma_pkt_open.h"))
vid = defines(os.path.join(hdr, "vid.h"))
smu7_h = defines(os.path.join(hdr, "smu7.h"))
smu7_smumgr_h = defines(os.path.join(hdr, "smu7_smumgr.h"))
amd_shared = defines(os.path.join(hdr, "amd_shared.h"))
ivsrc = defines(os.path.join(hdr, "ivsrcid_vislands30.h"))
doorbell_h = text_of("amdgpu_doorbell.h")
ib_c = text_of("amdgpu_ib.c")
drv_c = text_of("amdgpu_drv.c")
def c_function(text, name):
    # A function's body in a C file (from its name to the next line that
    # is a lone closing brace).
    at = text.index(name + "(")
    return text[at:text.index("\n}\n", at)]

hdp_flush_c = c_function(sdma_c, "static void sdma_v3_0_ring_emit_hdp_flush")
smu74_h = re.sub(r"/\*.*?\*/", "", text_of("smu74.h"), flags=re.S)
xfer_text = re.sub(r"/\*.*?\*/", "", text_of("smu_ucode_xfer_vi.h"), flags=re.S)

def c_struct_offset(text, struct, field):
    # A field's offset in a structure of uint8_t, uint16_t and uint32_t
    # fields and arrays of them, each aligned to its size (as the host's
    # compiler lays them out); `size` the whole, rounded to the largest.
    m = re.search(r"struct\s+%s\s*\{(.*?)\};" % struct, text, re.S)
    body = m.group(1)
    body = re.sub(r"#else.*?#endif", "", body, flags=re.S)
    body = re.sub(r"#\w+[^\n]*", "", body)
    at = 0
    widest = 1
    for kind, name, count in re.findall(r"(uint8_t|uint16_t|uint32_t)\s+(\w+)\s*(?:\[(\w+)\])?\s*;", body):
        size = {"uint8_t": 1, "uint16_t": 2, "uint32_t": 4}[kind]
        widest = max(widest, size)
        at = (at + size - 1) // size * size
        if name == field:
            return at
        n = 1 if not count else (int(count, 0) if count[0].isdigit() else xfer[count])
        at += size * n
    if field == "size":
        return (at + widest - 1) // widest * widest
    return None

entry_size = c_struct_offset(xfer_text, "SMU_Entry", "size")
toc_m = re.search(r"struct\s+SMU_DRAMData_TOC\s*\{(.*?)\};", xfer_text, re.S)
toc_size = 8 + entry_size * xfer["SMU_MAX_ENTRIES"] if toc_m and "struct SMU_Entry entry[SMU_MAX_ENTRIES]" in toc_m.group(1) else None
golden_rows = golden(sdma_c, "golden_settings_polaris10_a11")
LOADED_LINUX = {
    "SMU7_FIRMWARE_HEADER_LOCATION": smu7_h.get("SMU7_FIRMWARE_HEADER_LOCATION"),
    "SMU74_Firmware_Header__Version": c_struct_offset(smu74_h, "SMU74_Firmware_Header", "Version"),
    "SMU74_Firmware_Header__SoftRegisters": c_struct_offset(smu74_h, "SMU74_Firmware_Header", "SoftRegisters"),
    "SMU74_SoftRegisters__UcodeLoadStatus": c_struct_offset(smu74_h, "SMU74_SoftRegisters", "UcodeLoadStatus"),
    "SMC_RAM_END": smu7_smumgr_h.get("SMC_RAM_END"),
    "AMD_MAX_USEC_TIMEOUT": amd_shared.get("AMD_MAX_USEC_TIMEOUT") if "hwmgr->usec_timeout = AMD_MAX_USEC_TIMEOUT;" in text_of("hwmgr.c") else None,
    "TEST_TRIGGER": int(re.search(r"PPSMC_MSG_Test, (0x[0-9a-fA-F]+), NULL", smu7_c).group(1), 0),
    "JUMP_ON_START": int.from_bytes(bytes(int(x, 0) for x in re.search(r"static const unsigned char data\[4\] = \{([^}]*)\}", smu7_c).group(1).split(",")), "big"),
    "SMC_HEADER__ucode_start_addr": ucode_offset("smc_firmware_header_v1_0", "ucode_start_addr"),
    "SMU_Entry__size": entry_size,
    "SMU_DRAMData_TOC__size": toc_size,
    "HEADER_BUFFER_BYTES": ((toc_size or 0) // 4096 + 1) * 4096 if "((sizeof(struct SMU_DRAMData_TOC) / 4096) + 1) * 4096" in smu7_c else None,
    "HEADER__ucode_feature_version": ucode_offset("sdma_firmware_header_v1_0", "ucode_feature_version") if ucode_offset("sdma_firmware_header_v1_0", "ucode_feature_version") == ucode_offset("gfx_firmware_header_v1_0", "ucode_feature_version") else None,
    "BURST_NOP_FEATURE": int(re.search(r"feature_version >= (\d+)\)\s*\n\s*adev->sdma.instance\[i\].burst_nop = true", sdma_c).group(1)),
    "POLL_HDP_FLUSH": int(re.search(r"SDMA_PKT_POLL_REGMEM_HEADER_HDP_FLUSH\((\d+)\)", hdp_flush_c).group(1)),
    "POLL_FUNC_EQUAL": int(re.search(r"SDMA_PKT_POLL_REGMEM_HEADER_FUNC\((\d+)\)", hdp_flush_c).group(1)),
    "POLL_RETRY_COUNT": int(re.search(r"SDMA_PKT_POLL_REGMEM_DW5_RETRY_COUNT\((0x[0-9a-fA-F]+)\)", hdp_flush_c).group(1), 0),
    "POLL_INTERVAL": int(re.search(r"SDMA_PKT_POLL_REGMEM_DW5_INTERVAL\((\d+)\)", hdp_flush_c).group(1)),
    "SRBM_WRITE_BYTES": int(re.search(r"SDMA_PKT_SRBM_WRITE_HEADER_BYTE_EN\((0x[0-9a-fA-F]+)\)", sdma_c).group(1), 0),
    "IB_BASE_ALIGN": int(re.search(r"lower_32_bits\(ib->gpu_addr\) & (0x[0-9a-fA-F]+)\);", sdma_c).group(1), 0),
    "TILING_FROM_ADDRESS": int(re.search(r"gb_addr_config & (0x[0-9a-fA-F]+)\);", sdma_c).group(1), 0),
    "PHASE_QUANTUM": int(re.search(r"uint amdgpu_sdma_phase_quantum = (\d+);", drv_c).group(1)),
    "RING_ALIGN": int(re.search(r"static const struct amdgpu_ring_funcs sdma_v3_0_ring_funcs = \{.*?\.align_mask = (0x[0-9a-fA-F]+),", sdma_c, re.S).group(1), 0) + 1,
    "IB_END_ALIGN": int(re.search(r"\(2 - lower_32_bits\(ring->wptr\)\) & (\d+)\);", sdma_c).group(1)) + 1,
    "AMDGPU_DOORBELL_sDMA_ENGINE0": int(re.search(r"AMDGPU_DOORBELL_sDMA_ENGINE0\s*=\s*(0x[0-9A-Fa-f]+)", doorbell_h).group(1), 0),
    "AMDGPU_DOORBELL_sDMA_ENGINE1": int(re.search(r"AMDGPU_DOORBELL_sDMA_ENGINE1\s*=\s*(0x[0-9A-Fa-f]+)", doorbell_h).group(1), 0),
    "VISLANDS30_IV_SRCID_SDMA_TRAP": ivsrc.get("VISLANDS30_IV_SRCID_SDMA_TRAP"),
    "TEST_BEFORE": int(re.search(r"tmp = (0x[0-9A-Fa-f]+);\s*\n\s*adev->wb.wb\[index\] = cpu_to_le32\(tmp\);\s*\n\s*\n?\s*r = amdgpu_ring_alloc", sdma_c).group(1), 0),
    "TEST_WRITTEN": int(re.search(r"amdgpu_ring_write\(ring, (0x[0-9A-Fa-f]+)\);\s*\n\s*amdgpu_ring_commit\(ring\);", sdma_c).group(1), 0),
    "IB_TEST_TIMEOUT_MS": int(re.search(r"#define AMDGPU_IB_TEST_TIMEOUT\s+msecs_to_jiffies\((\d+)\)", ib_c).group(1)),
    "IB_BYTES": int(re.search(r"amdgpu_ib_get\(adev, NULL, (\d+),\s*AMDGPU_IB_POOL_DIRECT, &ib\);", sdma_c).group(1)),
    "SDMA1_REGISTER_OFFSET": vid.get("SDMA1_REGISTER_OFFSET"),
}
for name in list(pkt):
    if name.startswith("SDMA_OP_") or name.startswith("SDMA_SUBOP_") or name.startswith("SDMA_PKT_"):
        LOADED_LINUX.setdefault(name, pkt[name])
for name, value in ppsmc.items():
    LOADED_LINUX.setdefault(name, value)
for name, value in xfer.items():
    if name.startswith("UCODE_") or name.startswith("SMU_"):
        LOADED_LINUX.setdefault(name, value)
# (Written as products, held below.)
LOADED_PRODUCTS = {"SMU_BUFFER_BYTES", "SDMA_RING_BYTES"}
LOADED_OWN = {"MARK", "LIST_AT", "SMU_BUFFER_AT", "TRIAL_BYTES", "FILL_VALUE", "RECORD", "RPTR_SLOT", "WPTR_SLOT", "FENCE_SLOT",
              "RING_TEST_AT", "IB_TEST_AT", "IB_TEST", "IB_FILL", "IB_COPY", "KEPT", "TRAPS", "NONE", "IB_VMID"}
loaded_names = {}
for name, value, inside in constants(loaded_text):
    loaded_names[name] = value
    if name in LOADED_OWN or name in LOADED_PRODUCTS:
        continue
    if name in LOADED_LINUX:
        hold(name, value, LOADED_LINUX[name])
    elif "__" in name:
        hold(name, value, masks.get(name + "_MASK"))
    else:
        hold(name, value, regs.get("mm" + name))
# The constants written as sums: the SMC's working memory (smu7_init), the
# ring's bytes (the 1024 words sdma_v3_0_sw_init asks, 4 bytes each, times
# amdgpu_sched_hw_submission), the load mask (smu7_request_smu_load_fw's
# fw_to_load past Topaz), the places in the driver's part of the aperture.
smu_buffer = re.search(r"smu_data->smu_buffer.data_size = (\d+)\*(\d+);", smu7_c)
hold("SMU_BUFFER_BYTES is smu7_init's", int(re.search(r"pub const SMU_BUFFER_BYTES: usize = (\d+) \* (\d+)", loaded_text).group(1)) * 4096, int(smu_buffer.group(1)) * int(smu_buffer.group(2)))
ring_words = int(re.search(r"r = amdgpu_ring_init\(adev, ring, (\d+), &adev->sdma.trap_irq,", sdma_c).group(1))
submissions = int(re.search(r"int amdgpu_sched_hw_submission = (\d+);", drv_c).group(1))
mine = re.search(r"pub const SDMA_RING_BYTES: usize = (\d+) \* (\d+) \* (\d+)", loaded_text)
hold("SDMA_RING_BYTES is amdgpu_ring_init's for 1024 words", int(mine.group(1)) * int(mine.group(2)) * int(mine.group(3)), 1 << (ring_words * 4 * submissions - 1).bit_length())
fw_to_load = re.search(r"if \(hwmgr->chip_id > CHIP_TOPAZ\) \{.*?fw_to_load = (.*?);", smu7_c, re.S).group(1)
linux_mask = sum(xfer[name] for name in re.findall(r"UCODE_ID_\w+_MASK", fw_to_load))
code_mask = sum(loaded_names.get(name, 0) for name in re.findall(r"UCODE_ID_\w+_MASK", re.search(r"pub const LOAD_MASK: usize = ([^\n]*)", loaded_text).group(1)))
hold("LOAD_MASK is smu7_request_smu_load_fw's", code_mask, linux_mask)
fill_at = loaded_names["LIST_AT"] + loaded_names["HEADER_BUFFER_BYTES"] + int(smu_buffer.group(1)) * int(smu_buffer.group(2))
proved = 1
for factor in re.search(r"pub const PROVED: usize = ([\d *]+)", engines_text).group(1).split("*"):
    proved *= int(factor)
hold("the SMC's list after the GART's table and the default page", int(loaded_names["LIST_AT"] >= LINUX["GART_BYTES"] // LINUX["AMDGPU_GPU_PAGE_SIZE"] * LINUX["ENTRY_BYTES"] + LINUX["AMDGPU_GPU_PAGE_SIZE"]), 1)
hold("the SMC's working memory after its list", int(loaded_names["SMU_BUFFER_AT"] >= loaded_names["LIST_AT"] + loaded_names["HEADER_BUFFER_BYTES"]), 1)
hold("the fill's and the copy's pages within what M1 proved", int(fill_at + 3 * 4096 <= proved and re.search(r"pub const FILL_AT: usize = LIST_AT \+ HEADER_BUFFER_BYTES \+ SMU_BUFFER_BYTES", loaded_text) is not None), 1)
# The copy engines' golden table, row for row in Linux's order.
code_golden = [int(x, 0) for x in re.search(r"pub const GOLDEN_COPY_WORDS = \[_\]usize\{([^}]*)\}", loaded_text).group(1).replace(" ", "").split(",")]
hold("the copy engines' golden rows, as many", len(code_golden), len(golden_rows))
for k in range(min(len(code_golden), len(golden_rows))):
    hold("the copy engines' golden word %d" % k, code_golden[k], resolve(golden_rows[k]))
# The list's order (smu7_request_smu_load_fw) and each kind's file
# (gpu_engines.mlx's FILES: pfp, me, ce, mec, rlc, sdma, sdma1).
order = re.findall(r"smu7_populate_single_firmware_entry\(hwmgr,\s*(UCODE_ID_\w+), &toc->entry", smu7_c)
if "UCODE_ID_MEC_STORAGE" in order:
    order.remove("UCODE_ID_MEC_STORAGE")  # (a guest's virtual function only)
code_kinds = re.search(r"pub const LIST_KINDS = \[_\]usize\{([^}]*)\}", loaded_text).group(1).replace(" ", "").split(",")
hold("the list's kinds, as many", len(code_kinds), len(order))
for k in range(min(len(code_kinds), len(order))):
    hold("the list's entry %d is %s" % (k, order[k]), loaded_names.get(code_kinds[k], -1), xfer[order[k]])
kind_file = {"UCODE_ID_RLC_G": "rlc", "UCODE_ID_CP_CE": "ce", "UCODE_ID_CP_PFP": "pfp", "UCODE_ID_CP_ME": "me", "UCODE_ID_CP_MEC": "mec",
             "UCODE_ID_CP_MEC_JT1": "mec", "UCODE_ID_CP_MEC_JT2": "mec", "UCODE_ID_SDMA0": "sdma", "UCODE_ID_SDMA1": "sdma1"}
files_of = [re.sub(r"polaris10_(\w+?)(_2)?\.bin", r"\1", n) for n in names]
code_files = [int(x) for x in re.search(r"pub const LIST_FILES = \[_\]usize\{([^}]*)\}", loaded_text).group(1).replace(" ", "").split(",")]
for k in range(min(len(code_files), len(order))):
    hold("the list's entry %d's file is polaris10_%s" % (k, kind_file[order[k]]), int(files_of[code_files[k]] == kind_file[order[k]]), 1)
# The copies' order: enum AMDGPU_UCODE_ID's.
enum_body = re.search(r"enum AMDGPU_UCODE_ID \{(.*?)\};", ucode_h, re.S).group(1)
enum_order = re.findall(r"AMDGPU_UCODE_ID_(\w+)", enum_body)
file_enum = {"sdma": "SDMA0", "sdma1": "SDMA1", "ce": "CP_CE", "pfp": "CP_PFP", "me": "CP_ME", "mec": "CP_MEC1", "rlc": "RLC_G"}
code_copies = [int(x) for x in re.search(r"pub const COPY_ORDER = \[_\]usize\{([^}]*)\}", loaded_text).group(1).replace(" ", "").split(",")]
positions = [enum_order.index(file_enum[files_of[k]]) for k in code_copies]
hold("the copies in the order of enum AMDGPU_UCODE_ID", int(positions == sorted(positions) and len(code_copies) == 7), 1)
hold("MEC2's room between MEC's jump table and RLC (enum AMDGPU_UCODE_ID)", int(enum_order.index("CP_MEC1") < enum_order.index("CP_MEC2") < enum_order.index("RLC_G")), 1)
# The unhalt flag on RLC's and MEC's entries alone (smu7_populate_single_firmware_entry).
hold("the unhalt flag's engines", int(re.search(r"if \(\(fw_type == UCODE_ID_RLC_G\)\s*\|\| \(fw_type == UCODE_ID_CP_MEC\)\)\s*entry->flags = 1;", smu7_c) is not None and "kind == UCODE_ID_RLC_G || kind == UCODE_ID_CP_MEC { UCODE_FLAG_UNHALT_MASK }" in loaded_text), 1)
# No register these steps write is the display's (engine 1's as engine
# 0's + 0x200 as well).
written = set(re.findall(r"\b(?:put\(e, l,|wr\(e,|regField\(e,) (?:engines\.)?(\w+)", loaded_text))
for name in sorted(written):
    number = loaded_names.get(name, engine_names.get(name))
    if number is None:
        continue
    for at in (number, number + 0x200 if name.startswith("SDMA0") else number):
        in_display = any(runs[k] <= at <= runs[k + 1] for k in range(0, len(runs), 2))
        hold("%s (%#x) written by stage 3 or 4 is no register of the display's" % (name, at), int(in_display), 0)
print("stages 3 and 4: %d numbers of gpu_smc_sdma.mlx held against Linux's files, %d wrong" % (held - loaded_before, bad - loaded_bad_before))
# ---- Stage 5: gpu_compute.mlx, gpu_ring.mlx, gpu_clear_state.mlx (the
# graphics block's settings, the RLC, the KIQ and compute ring 0, the ring
# as an interface, the clear state). Beside the files above, of Linux
# 6.6's drivers/gpu/drm/amd:
#   include/asic_reg/gca/gfx_8_0_enum.h  the tiling words' names, the
#                                       memory types, the shader memory
#                                       modes, the end of pipe's event
#   include/vi_structs.h                the queue descriptor (struct vi_mqd,
#                                       struct vi_mqd_allocation)
#   amdgpu/clearstate_vi.h              the clear state
#   amdgpu/amdgpu_gfx.c, amdgpu_ids.h   the KIQ's queue, the address spaces
#   amdgpu/amdgpu_ring.c                a ring's size and record
# and that no register these steps write is the display's.
stage5_needed = ("gfx_8_0_enum.h", "vi_structs.h", "clearstate_vi.h", "amdgpu_gfx.c", "amdgpu_ids.h", "amdgpu_ring.c", "vid.h",
                 "gfx_v8_0.c", "gmc_v8_0.c", "amdgpu_doorbell.h", "ivsrcid_vislands30.h", "amdgpu_drv.c")
stage5_missing = [name for name in stage5_needed if not os.path.exists(os.path.join(hdr, name))]
if stage5_missing:
    sys.exit("stage 5 is not held: not in %s: %s" % (hdr, " ".join(stage5_missing)))
stage5_before = held
stage5_bad_before = bad
compute_text = open(os.path.join(here, "gpu_compute.mlx")).read()
ring_text = open(os.path.join(here, "gpu_ring.mlx")).read()
clear_text = open(os.path.join(here, "gpu_clear_state.mlx")).read()
enum8 = {m.group(1): int(m.group(2), 0) for m in re.finditer(r"^\s*(\w+)\s*=\s*(0x[0-9a-fA-F]+|\d+)\s*,", text_of("gfx_8_0_enum.h"), re.M)}
vid_text = re.sub(r"/\*.*?\*/", "", text_of("vid.h"), flags=re.S)
gfx_shifts = {m.group(1): int(m.group(2), 0) for m in re.finditer(r"#define\s+(\w+)__SHIFT\s+(0x[0-9a-fA-F]+|\d+)", text_of("gfx_8_0_sh_mask.h"))}
amdgpu_gfx_c = text_of("amdgpu_gfx.c")
ring_c = text_of("amdgpu_ring.c")
gmc8_c = text_of("gmc_v8_0.c")

def vid_value(name):
    # A define of vid.h: a number, (1 << n), (x << n) for a macro that takes
    # a value (its shift), PACKET3's opcodes.
    m = re.search(r"#\s*define\s+%s\(x\)\s+\(\(x\)\s*<<\s*(\d+)\)" % name, vid_text)
    if m:
        return int(m.group(1))
    m = re.search(r"#\s*define\s+%s\s+\((\d+)\s*<<\s*(\d+)\)" % name, vid_text)
    if m:
        return int(m.group(1)) << int(m.group(2))
    m = re.search(r"#\s*define\s+%s\s+(0x[0-9a-fA-F]+|\d+)\b" % name, vid_text)
    if m:
        return int(m.group(1), 0)
    return None

def function_body(text, name):
    at = text.index(name + "(")
    at = text.index("{", at)
    return text[at:text.index("\n}\n", at)]

# The descriptor's words (every field 32 bits) and the allocation's bytes.
structs_text = re.sub(r"/\*.*?\*/", "", text_of("vi_structs.h"), flags=re.S)
mqd_body = re.search(r"struct vi_mqd \{(.*?)\};", structs_text, re.S).group(1)
mqd_words = {}
at = 0
for name, count in re.findall(r"uint32_t\s+(\w+)\s*(?:\[(\d+)\])?\s*;", mqd_body):
    mqd_words[name] = at
    at += int(count) if count else 1
mqd_size = at * 4
allocation = {"size": mqd_size}
alloc_body = re.search(r"struct vi_mqd_allocation \{(.*?)\};", structs_text, re.S).group(1)
at = mqd_size
for name in re.findall(r"uint32_t\s+(\w+)\s*;", alloc_body):
    allocation[name] = at
    at += 4
allocation["size"] = at

gfx_c_plain = re.sub(r"/\*.*?\*/", "", gfx_c, flags=re.S)
mqd_init = function_body(gfx_c_plain, "static int gfx_v8_0_mqd_init")
early = re.search(r"case CHIP_POLARIS10:\n\tcase CHIP_VEGAM:\n(.*?)break;", function_body(gfx_c_plain, "static int gfx_v8_0_gpu_early_init"), re.S).group(1)
def config(name):
    return int(re.search(r"adev->gfx.config.%s = (0x[0-9a-fA-F]+|\d+);" % name, early).group(1), 0)
constants_init = function_body(gfx_c_plain, "static void gfx_v8_0_constants_init")
compute_funcs = re.search(r"static const struct amdgpu_ring_funcs gfx_v8_0_ring_funcs_compute = \{(.*?)\};", gfx_c_plain, re.S).group(1)
kiq_funcs = re.search(r"static const struct amdgpu_ring_funcs gfx_v8_0_ring_funcs_kiq = \{(.*?)\};", gfx_c_plain, re.S).group(1)
gfx_funcs = re.search(r"static const struct amdgpu_ring_funcs gfx_v8_0_ring_funcs_gfx = \{(.*?)\};", gfx_c_plain, re.S).group(1)
aligns = {int(re.search(r"\.align_mask = (0x[0-9a-fA-F]+)", f).group(1), 0) for f in (compute_funcs, kiq_funcs, gfx_funcs)}
nops = {re.search(r"\.nop = PACKET3\(PACKET3_NOP, (0x[0-9a-fA-F]+)\)", f).group(1) for f in (compute_funcs, kiq_funcs, gfx_funcs)}
hold("the three rings pad to one fetch size", len(aligns), 1)
hold("the three rings' NOP covers 0x3FFF", int(nops == {"0x3FFF"}), 1)
fence_kiq = function_body(gfx_c, "static void gfx_v8_0_ring_emit_fence_kiq")
fence_compute = function_body(gfx_c_plain, "static void gfx_v8_0_ring_emit_fence_compute")
hdp_flush = function_body(gfx_c, "static void gfx_v8_0_ring_emit_hdp_flush")
mem_sync = function_body(gfx_c, "static void gfx_v8_0_emit_mem_sync_compute")
cond_exec = function_body(gfx_c, "static unsigned gfx_v8_0_ring_emit_init_cond_exec")
kiq_kcq = function_body(gfx_c_plain, "static int gfx_v8_0_kiq_kcq_enable")
kcq_disable = function_body(gfx_c_plain, "static int gfx_v8_0_kcq_disable")
test_ib = function_body(gfx_c_plain, "static int gfx_v8_0_ring_test_ib")
gfx_start = function_body(gfx_c_plain, "static int gfx_v8_0_cp_gfx_start")
compute_vmid = function_body(gfx_c_plain, "static void gfx_v8_0_init_compute_vmid")
early_gmc = function_body(gmc8_c, "static int gmc_v8_0_early_init")
doorbell_text = text_of("amdgpu_doorbell.h")
ivsrc8 = defines(os.path.join(hdr, "ivsrcid_vislands30.h"))

# The tiling words: the Polaris 10 case of gfx_v8_0_tiling_mode_table_init
# with its macros, the headers' shifts and the enum's names.
tiling_fn = function_body(gfx_c_plain, "static void gfx_v8_0_tiling_mode_table_init")
tiling_case = re.search(r"case CHIP_POLARIS10:\n(.*?)\n\t\tbreak;", tiling_fn, re.S).group(1)
tile_macros = {m.group(1): m.group(2) for m in re.finditer(r"#define\s+(ARRAY_MODE|PIPE_CONFIG|TILE_SPLIT|MICRO_TILE_MODE_NEW|SAMPLE_SPLIT|BANK_WIDTH|BANK_HEIGHT|MACRO_TILE_ASPECT|NUM_BANKS)\(x\)\s+\(\(x\) << (\w+)__SHIFT\)", gfx_c_plain)}
def tiling_value(expression):
    return eval(re.sub(r"(\w+)\((\w+)\)", lambda m: "((%d) << %d)" % (enum8[m.group(2)], gfx_shifts[tile_macros[m.group(1)]]), " ".join(expression.split())))
linux_tiles = [0] * 32
linux_macrotiles = [0] * 16
for which, index, expression in re.findall(r"(modearray|mod2array)\[(\d+)\] = \((.*?)\);", tiling_case, re.S):
    (linux_tiles if which == "modearray" else linux_macrotiles)[int(index)] = tiling_value(expression)
skipped = int(re.search(r"if \(reg_offset != (\d+)\)\s*\n\s*WREG32\(mmGB_MACROTILE_MODE0", tiling_case).group(1))

# The raster configuration of Polaris 10 (gfx_v8_0_raster_config) by
# vid.h's macros.
raster_fn = function_body(gfx_c_plain, "gfx_v8_0_raster_config")
raster_case = re.search(r"case CHIP_TONGA:\n\tcase CHIP_POLARIS10:\n(.*?)break;", raster_fn, re.S).group(1)
def raster_value(expression):
    return eval(re.sub(r"(\w+)\((\d+)\)", lambda m: "(%d << %d)" % (int(m.group(2)), vid_value(m.group(1))), " ".join(expression.split())))
raster0 = raster_value(re.search(r"\*rconf \|= (.*?);", raster_case, re.S).group(1))
raster1 = raster_value(re.search(r"\*rconf1 \|= (.*?);", raster_case, re.S).group(1))

# The KIQ's queue by amdgpu_gfx_kiq_acquire with compute ring 0 taken (its
# bit 0): the highest free bit whose queue is 0 and not on pipe 2 or 3 of
# the second processor, two processors of 4 pipes of 8 queues
# (gfx_v8_0_sw_init).
num_mec = 2 if re.search(r"case CHIP_POLARIS10:[^\n]*\n(?:[^\n]*\n)*?\s*adev->gfx.mec.num_mec = 2;", gfx_c_plain) else 1
pipes = int(re.search(r"adev->gfx.mec.num_pipe_per_mec = (\d+);", gfx_c_plain).group(1))
queues = int(re.search(r"adev->gfx.mec.num_queue_per_pipe = (\d+);", gfx_c_plain).group(1))
kiq_place = None
for bit in range(num_mec * pipes * queues - 1, -1, -1):
    if bit == 0:
        continue
    mec, rest = divmod(bit, pipes * queues)
    pipe, queue = divmod(rest, queues)
    if (mec == 1 and pipe > 1) or queue != 0:
        continue
    kiq_place = (mec + 1, pipe, queue)
    break
hold("the KIQ's queue rule in amdgpu_gfx.c is the one followed", int("(mec == 1 && pipe > 1) || queue != 0" in amdgpu_gfx_c), 1)

ring_words = int(re.search(r"r = amdgpu_ring_init\(adev, ring, (\d+), &adev->gfx.eop_irq, irq_type,", gfx_c_plain).group(1))
submissions = int(re.search(r"int amdgpu_sched_hw_submission = (\d+);", drv_c).group(1))
STAGE5_LINUX = {
    "RING_ALIGN": aligns.pop() + 1,
    "HDP_POLL_INTERVAL": int(re.search(r"amdgpu_ring_write\(ring, (0x[0-9a-fA-F]+)\); /\* poll interval \*/", hdp_flush).group(1), 0),
    "MEM_SYNC_POLL": int(re.search(r"amdgpu_ring_write\(ring, (0x[0-9a-fA-F]+)\);\s*/\* poll interval \*/", mem_sync).group(1), 0),
    "COND_EXEC_DUMMY": int(re.search(r"amdgpu_ring_write\(ring, (0x[0-9a-fA-F]+)\); /\* patch dummy value later \*/", cond_exec).group(1), 0),
    "CPC_INT_RAISE": int(re.search(r"amdgpu_ring_write\(ring, (0x[0-9a-fA-F]+)\); /\* src_id is 178 \*/", fence_kiq).group(1), 0),
    "DATA_32": int(re.search(r"DATA_SEL\(write64bit \? \d+ : (\d+)\)", fence_compute).group(1)),
    "INTERRUPT_CONFIRMED": int(re.search(r"INT_SEL\(int_sel \? (\d+) : 0\)", fence_compute).group(1)),
    "CACHE_FLUSH_AND_INV_TS_EVENT": enum8.get("CACHE_FLUSH_AND_INV_TS_EVENT"),
    "MTYPE_NC": enum8.get("MTYPE_NC"),
    "MTYPE_CC": enum8.get("MTYPE_CC"),
    "MTYPE_UC": enum8.get("MTYPE_UC"),
    "SH_MEM_ALIGNMENT_MODE_UNALIGNED": enum8.get("SH_MEM_ALIGNMENT_MODE_UNALIGNED"),
    "SH_MEM_ADDRESS_MODE_HSA64": enum8.get("SH_MEM_ADDRESS_MODE_HSA64"),
    "ACLK_DIVIDER": int(re.search(r"data \|= (0x[0-9a-fA-F]+) << CG_ACLK_CNTL__ACLK_DIVIDER__SHIFT;", gfx_c_plain).group(1), 0),
    "MACROTILE_SKIPPED": skipped,
    "RASTER_CONFIG": raster0,
    "RASTER_CONFIG_1": raster1,
    "SC_FRONTEND_PRIM_FIFO_SIZE": config("sc_prim_fifo_size_frontend"),
    "SC_BACKEND_PRIM_FIFO_SIZE": config("sc_prim_fifo_size_backend"),
    "SC_HIZ_TILE_FIFO_SIZE": config("sc_hiz_tile_fifo_size"),
    "SC_EARLYZ_TILE_FIFO_SIZE": config("sc_earlyz_tile_fifo_size"),
    "MAX_CONTEXT": config("max_hw_contexts") - 1 if "WREG32(mmCP_MAX_CONTEXT, adev->gfx.config.max_hw_contexts - 1);" in gfx_start else None,
    "PIPE_ORDER": int(re.search(r"SPI_ARB_PRIORITY, PIPE_ORDER_TS0, (\d+)\);", constants_init).group(1)),
    "SWIZZLE_ENABLE": int(re.search(r"SWIZZLE_ENABLE, (\d+)\);", constants_init).group(1)),
    "ELEMENT_SIZE": int(re.search(r"ELEMENT_SIZE, (\d+)\);", constants_init).group(1)),
    "INDEX_STRIDE": int(re.search(r"INDEX_STRIDE, (\d+)\);", constants_init).group(1)),
    "SHARED_APERTURE_BASES": int(re.search(r"adev->gmc.shared_aperture_start = (0x[0-9a-fA-F]+)ULL;", early_gmc).group(1), 0) >> 48 if "adev->gmc.shared_aperture_start >> 48" in constants_init else None,
    "DEFAULT_SH_MEM_BASES": int(re.search(r"#define DEFAULT_SH_MEM_BASES\s+\((0x[0-9a-fA-F]+)\)", gfx_c_plain).group(1), 0) if "DEFAULT_SH_MEM_BASES | (DEFAULT_SH_MEM_BASES << 16)" in compute_vmid else None,
    "FIRST_KFD_VMID": int(re.search(r"adev->vm_manager.first_kfd_vmid = (\d+);", gmc8_c).group(1)),
    "AMDGPU_NUM_VMID": defines(os.path.join(hdr, "amdgpu_ids.h")).get("AMDGPU_NUM_VMID"),
    # (gpu_ring.mlx: the address spaces a buffer may name.)
    "SPACES": defines(os.path.join(hdr, "amdgpu_ids.h")).get("AMDGPU_NUM_VMID"),
    "SPM_VMID": int(re.search(r"update_spm_vmid\(adev, (0x[0-9a-fA-F]+)\);", gfx_c_plain).group(1), 0),
    "KIQ_ME": kiq_place[0],
    "KIQ_PIPE": kiq_place[1],
    "KIQ_QUEUE": kiq_place[2],
    # gfx_v8_0_sw_init: ring 0 is the first processor's (ME 1), its first
    # pipe and queue.
    "RING0_ME": 1 if "ring->me = mec + 1;" in gfx_c_plain else None,
    "RING0_PIPE": 0,
    "RING0_QUEUE": 0,
    "GFX8_MEC_HPD_SIZE": int(re.search(r"#define GFX8_MEC_HPD_SIZE (\d+)", gfx_c_plain).group(1)),
    "MQD_HEADER": int(re.search(r"mqd->header = (0x[0-9a-fA-F]+);", mqd_init).group(1), 0),
    "PRELOAD_SIZE": int(re.search(r"PRELOAD_SIZE, (0x[0-9a-fA-F]+)\);", mqd_init).group(1), 0),
    "DEVICE_ID": int(re.search(r"WREG32\(mmCP_DEVICE_ID, (\d+)\);", gfx_start).group(1)),
    "CE_PARTITION": int(re.search(r"PACKET3_BASE_INDEX\(CE_PARTITION_BASE\)\);\s*\n\s*amdgpu_ring_write\(ring, (0x[0-9a-fA-F]+)\);", gfx_start).group(1), 0),
    "CONTEXT_CONTROL_WORD": int(re.search(r"PACKET3_CONTEXT_CONTROL, 1\)\);\s*\n\s*amdgpu_ring_write\(ring, (0x[0-9a-fA-F]+)\);", gfx_start).group(1), 0),
    "IDLE_STATUS2": int(re.search(r"RREG32\(mmGRBM_STATUS2\) != (0x[0-9a-fA-F]+)\)", gfx_c_plain).group(1), 0),
    "RESET_QUEUES": int(re.search(r"PACKET3_UNMAP_QUEUES_ACTION\((\d+)\) \| /\* RESET_QUEUES \*/", gfx_c).group(1)),
    "DST_MEMORY": int(re.search(r"ib.ptr\[1\] = WRITE_DATA_DST_SEL\((\d+)\) \| WR_CONFIRM;", test_ib).group(1)),
    "RING0_ID": (1 << 2) | 0 | (0 << 4) if "me_id = (entry->ring_id & 0x0c) >> 2;" in gfx_c_plain and "queue_id = (entry->ring_id & 0x70) >> 4;" in gfx_c_plain else None,
    "VISLANDS30_IV_SRCID_CP_END_OF_PIPE": ivsrc8.get("VISLANDS30_IV_SRCID_CP_END_OF_PIPE"),
    "VISLANDS30_IV_SRCID_CP_PRIV_REG_FAULT": ivsrc8.get("VISLANDS30_IV_SRCID_CP_PRIV_REG_FAULT"),
    "VISLANDS30_IV_SRCID_CP_PRIV_INSTR_FAULT": ivsrc8.get("VISLANDS30_IV_SRCID_CP_PRIV_INSTR_FAULT"),
}
for name in ("AMDGPU_DOORBELL_KIQ", "AMDGPU_DOORBELL_MEC_RING0", "AMDGPU_DOORBELL_MEC_RING7", "AMDGPU_DOORBELL_GFX_RING0"):
    STAGE5_LINUX[name] = int(re.search(r"%s\s*=\s*(0x[0-9A-Fa-f]+)" % name, doorbell_text).group(1), 0)
for name, word in mqd_words.items():
    STAGE5_LINUX["vi_mqd__" + name] = word
for name, offset in allocation.items():
    STAGE5_LINUX["vi_mqd_allocation__" + name] = offset
# (Their own: the records, the marks, the room.)
STAGE5_OWN = {"STOP_MARK", "WATCH_EVERY", "KEPT", "KIQ_RECORD", "RING0_RECORD", "GRAPHICS_RECORD", "IB_TEST_AT", "MOST_ENGINES",
              "MOST_ARRAYS", "MOST_UNITS", "NONE", "RECORD", "RPTR_SLOT", "WPTR_SLOT", "FENCE_SLOT", "COND_SLOT", "FRAME_WORDS", "FAULTS",
              # (the packet's length field, bits 19:0: the study's 5.7; no header of Linux's names it)
              "IB_LENGTH_MOST"}
STAGE5_PRODUCTS = {"RING_BYTES"}
# (gpu_ring.mlx's numbers of stages 6 and 7: held in their section below.)
STAGE67_RING = {"VS_PARTIAL_FLUSH", "VGT_FLUSH", "VISLANDS30_IV_SRCID_GFX_PAGE_INV_FAULT", "VISLANDS30_IV_SRCID_GFX_MEM_PROT_FAULT", "PIPELINE_SYNC_POLL",
                "VM_FLUSH_POLL", "CONTEXT_LOAD_ENABLE", "CONTEXT_LOAD_GLOBAL", "CONTEXT_LOAD_CS_SH", "CONTEXT_LOAD_PER_CONTEXT", "SPACE_FRAME_WORDS"}
stage5_names = {}
for text in (compute_text, ring_text):
    for name, value, inside in constants(text):
        stage5_names[name] = value
        if name in STAGE5_OWN or name in STAGE5_PRODUCTS or name in STAGE67_RING:
            continue
        if name in STAGE5_LINUX:
            hold(name, value, STAGE5_LINUX[name])
        elif name.endswith("_SHIFT") or name.startswith("PACKET3_") or name.startswith("PACKET_") or name.startswith("EOP_") or name in ("WR_CONFIRM", "INDIRECT_BUFFER_VALID", "CE_PARTITION_BASE"):
            hold(name, value, vid_value(name[:-len("_SHIFT")] if name.endswith("_SHIFT") else name))
        elif name.startswith("GPU_HDP_FLUSH_DONE__"):
            hold(name, value, masks.get(name + "_MASK"))
        elif "__" in name:
            hold(name, value, masks.get(name + "_MASK"))
        else:
            hold(name, value, regs.get("mm" + name))
# The ring's bytes: amdgpu_ring_init's rule for the 1024 words the compute
# and graphics rings ask (twice over, a power of two).
mine = re.search(r"pub const RING_BYTES: usize = (\d+) \* (\d+) \* (\d+)", compute_text)
hold("RING_BYTES is amdgpu_ring_init's for the rings' 1024 words", int(mine.group(1)) * int(mine.group(2)) * int(mine.group(3)), 1 << (ring_words * 4 * submissions - 1).bit_length())
hold("the ring's record slots are 32 bytes apart as amdgpu_device_wb_get's", stage5_names["RECORD"], 32 if "*wb = offset << 3; /* convert to dw offset */" in text_of("amdgpu_device.c") else None)
# The golden tables, row for row.
for table_name, mine_name in (("golden_settings_polaris10_a11", "GOLDEN_GRAPHICS_WORDS"), ("polaris10_golden_common_all", "GOLDEN_COMMON_WORDS")):
    rows = golden(gfx_c, table_name)
    code = [int(x, 0) for x in re.search(r"pub const %s = \[_\]usize\{([^}]*)\}" % mine_name, compute_text).group(1).replace(" ", "").split(",")]
    hold("%s's rows, as many" % mine_name, len(code), len(rows))
    for k in range(min(len(code), len(rows))):
        hold("%s word %d" % (mine_name, k), code[k], resolve(rows[k]))
# The tiling words.
code_tiles = [int(x, 0) for x in re.search(r"pub const TILE_MODES = \[_\]usize\{([^}]*)\}", compute_text).group(1).replace(" ", "").split(",")]
code_macrotiles = [int(x, 0) for x in re.search(r"pub const MACROTILE_MODES = \[_\]usize\{([^}]*)\}", compute_text).group(1).replace(" ", "").split(",")]
hold("TILE_MODES, as many", len(code_tiles), len(linux_tiles))
hold("MACROTILE_MODES, as many", len(code_macrotiles), len(linux_macrotiles))
for k in range(min(len(code_tiles), 32)):
    hold("tile mode %d" % k, code_tiles[k], linux_tiles[k])
for k in range(min(len(code_macrotiles), 16)):
    hold("macrotile mode %d" % k, code_macrotiles[k], linux_macrotiles[k])
# gfx_v8_0_mqd_commit's ranges: the registers from CP_MQD_BASE_ADDR are
# the descriptor's words from cp_mqd_base_addr_lo, in the same order.
for name in ("CP_HQD_VMID", "CP_HQD_PERSISTENT_STATE", "CP_HQD_QUANTUM", "CP_HQD_PQ_DOORBELL_CONTROL", "CP_HQD_PQ_CONTROL", "CP_HQD_IB_CONTROL",
             "CP_HQD_IQ_TIMER", "CP_MQD_CONTROL", "CP_HQD_EOP_CONTROL", "CP_HQD_EOP_RPTR", "CP_HQD_EOP_WPTR", "CP_HQD_EOP_EVENTS",
             "CP_HQD_CTX_SAVE_BASE_ADDR_LO", "CP_HQD_CTX_SAVE_BASE_ADDR_HI", "CP_HQD_CTX_SAVE_CONTROL", "CP_HQD_CNTL_STACK_OFFSET",
             "CP_HQD_CNTL_STACK_SIZE", "CP_HQD_WG_STATE_OFFSET", "CP_HQD_CTX_SAVE_SIZE", "CP_HQD_ERROR", "CP_HQD_EOP_WPTR_MEM", "CP_HQD_EOP_DONES"):
    field_name = {"CP_HQD_EOP_EVENTS": "cp_hqd_eop_done_events"}.get(name, name.lower())
    hold("%s is the descriptor's %s" % (name, field_name), regs["mm" + name] - regs["mmCP_MQD_BASE_ADDR"] + mqd_words["cp_mqd_base_addr_lo"], mqd_words.get(field_name))
hold("gfx_v8_0_mqd_commit's first range ends at CP_HQD_EOP_CONTROL, its second is CP_HQD_EOP_EVENTS to CP_HQD_ERROR", int("for (mqd_reg = mmCP_HQD_VMID; mqd_reg <= mmCP_HQD_EOP_CONTROL; mqd_reg++)" in gfx_c and "for (mqd_reg = mmCP_HQD_EOP_EVENTS; mqd_reg <= mmCP_HQD_ERROR; mqd_reg++)" in gfx_c and "for (mqd_reg = mmCP_MQD_BASE_ADDR; mqd_reg <= mmCP_HQD_ACTIVE; mqd_reg++)" in gfx_c), 1)
# The KIQ's packets' words as gfx_v8_0_kiq_kcq_enable and _kcq_disable
# write them.
hold("SET_RESOURCES carries 6 words", int("PACKET3(PACKET3_SET_RESOURCES, 6)" in kiq_kcq and "6))" in ring_text + "PACKET3_SET_RESOURCES, 6))" ), 1)
hold("MAP_QUEUES carries 5 words, the micro engine 1 as 0", int("PACKET3(PACKET3_MAP_QUEUES, 5)" in kiq_kcq and "PACKET3_MAP_QUEUES_ME(ring->me == 1 ? 0 : 1)" in kiq_kcq and "PACKET3_MAP_QUEUES, 5))" in compute_text), 1)
hold("UNMAP_QUEUES carries 4 words", int("PACKET3(PACKET3_UNMAP_QUEUES, 4)" in kcq_disable and "PACKET3_UNMAP_QUEUES, 4))" in compute_text), 1)
hold("the queue bit is micro engine x 32 + pipe x 8 + queue (two processors of 4 pipes of 8)", int(pipes * queues == 32 and queues == 8 and "(r.*.me - 1) * 32 + r.*.pipe * 8 + r.*.queue" in compute_text), 1)
# The clear state, value for value.
cs_text = re.sub(r"//[^\n]*", "", re.sub(r"/\*.*?\*/", "", text_of("clearstate_vi.h"), flags=re.S))
cs_arrays = {m.group(1): [int(v, 0) for v in m.group(2).replace("\n", " ").split(",") if v.strip()]
             for m in re.finditer(r"static const unsigned int (vi_SECT_CONTEXT_def_\d+)\[\] =\s*\{(.*?)\};", cs_text, re.S)}
cs_runs = re.findall(r"\{\s*(vi_SECT_CONTEXT_def_\d+),\s*(0x[0-9a-fA-F]+),\s*(\d+)\s*\}", cs_text)
code_first = [int(x, 0) for x in re.search(r"pub const RUN_FIRST = \[_\]usize\{([^}]*)\}", clear_text).group(1).replace(" ", "").split(",")]
code_count = [int(x, 0) for x in re.search(r"pub const RUN_COUNT = \[_\]usize\{([^}]*)\}", clear_text).group(1).replace(" ", "").split(",")]
code_values = [int(x, 0) for x in re.search(r"pub const VALUES = \[_\]u32\{([^}]*)\}", clear_text).group(1).replace("\n", " ").replace(" ", "").split(",") if x]
linux_values = []
for name, first, count in cs_runs:
    linux_values += cs_arrays[name]
hold("the clear state's runs, as many", len(code_first), len(cs_runs))
for k in range(min(len(code_first), len(cs_runs))):
    hold("clear state run %d's first register" % k, code_first[k], int(cs_runs[k][1], 0))
    hold("clear state run %d's count" % k, code_count[k], int(cs_runs[k][2]))
hold("the clear state's values, as many", len(code_values), len(linux_values))
hold("the clear state's values, value for value", int(code_values == linux_values), 1)
# No register these steps write is the display's.
written5 = set(re.findall(r"\b(?:put|putField)\(e, c, (?:engines\.|loaded\.|ring\.)?(\w+)", compute_text)) | set(re.findall(r"\bwr\(e, (?:engines\.|loaded\.|ring\.)?(\w+)", compute_text + ring_text))
for name in sorted(written5):
    number = stage5_names.get(name, loaded_names.get(name, engine_names.get(name)))
    if number is None:
        continue
    in_display = any(runs[k] <= number <= runs[k + 1] for k in range(0, len(runs), 2))
    hold("%s (%#x) written by stage 5 is no register of the display's" % (name, number), int(in_display), 0)
for k in range(0, len(code_tiles)):
    number = regs["mmGB_TILE_MODE0"] + k
    hold("GB_TILE_MODE%d written by stage 5 is no register of the display's" % k, int(any(runs[j] <= number <= runs[j + 1] for j in range(0, len(runs), 2))), 0)
print("stage 5: %d numbers of gpu_compute.mlx, gpu_ring.mlx and gpu_clear_state.mlx held against Linux's files, %d wrong" % (held - stage5_before, bad - stage5_bad_before))
# ---- Stages 6 and 7: gpu_graphics.mlx (the graphics ring), gpu_space.mlx
# (the programs' address spaces as an interface), gpu_first_space.mlx
# (V1), gpu_ring.mlx's frame for a program's buffer and the memory
# controller's faults. Beside the files above, of Linux 6.6's
# drivers/gpu/drm/amd:
#   amdgpu/amdgpu_vm.h, amdgpu_vm.c, amdgpu_vm_pt.c, amdgpu_vm_cpu.c  the
#                  reserved bottom, the entries' bits, the flush's order
#   amdgpu/amdgpu_ib.c                 a job's frame, in its order
#   amdgpu/amdgpu_ids.c                the flush at every job of an address
#                                      space the processor writes
#   amdgpu/amdgpu_ttm.c, amdgpu_vram_mgr.c  the entries' flags
#   amdgpu/amdgpu_gmc.c                a directory entry's address
#   include/asic_reg/oss/oss_3_0_sh_mask.h  the interrupt ring's number
# (the spec's gpu_map flags from spec/04-stdlib/os/brix.xml), and that no
# register these steps write is the display's but DMIF_ADDR_CALC, written
# only where it differs.
stage67_needed = ("amdgpu_vm.h", "amdgpu_vm.c", "amdgpu_vm_pt.c", "amdgpu_vm_cpu.c", "amdgpu_ib.c", "amdgpu_ids.c", "amdgpu_ttm.c",
                  "amdgpu_vram_mgr.c", "amdgpu_gmc.c", "oss_3_0_sh_mask.h")
stage67_missing = [name for name in stage67_needed if not os.path.exists(os.path.join(hdr, name))]
if stage67_missing:
    sys.exit("stages 6 and 7 are not held: not in %s: %s" % (hdr, " ".join(stage67_missing)))
stage67_before = held
stage67_bad_before = bad
graphics_text = open(os.path.join(here, "gpu_graphics.mlx")).read()
space_text = open(os.path.join(here, "gpu_space.mlx")).read()
first_text = open(os.path.join(here, "gpu_first_space.mlx")).read()
spec_text = open(os.path.join(here, "..", "..", "..", "spec", "04-stdlib", "os", "brix.xml")).read()
vm_h = text_of("amdgpu_vm.h")
vm_c = re.sub(r"/\*.*?\*/", "", text_of("amdgpu_vm.c"), flags=re.S)
ib_c = re.sub(r"/\*.*?\*/", "", text_of("amdgpu_ib.c"), flags=re.S)
ids_c = text_of("amdgpu_ids.c")
ttm_c = text_of("amdgpu_ttm.c")
vram_mgr_c = text_of("amdgpu_vram_mgr.c")
gmc_common_c = text_of("amdgpu_gmc.c")
oss_masks = defines(os.path.join(hdr, "oss_3_0_sh_mask.h"))
csb_at = gfx_c.index("static u32 gfx_v8_0_get_csb_size(struct amdgpu_device *adev)\n{")
csb_size = gfx_c[csb_at:gfx_c.index("\n}\n", csb_at)]
pipeline_sync = function_body(gfx_c, "static void gfx_v8_0_ring_emit_pipeline_sync")
vm_flush = function_body(gfx_c, "static void gfx_v8_0_ring_emit_vm_flush")
cntxcntl = function_body(gfx_c, "static void gfx_v8_ring_emit_cntxcntl")
vgt_flush = function_body(gfx_c, "static void gfx_v8_0_ring_emit_vgt_flush")
eop_irq = function_body(gfx_c_plain, "static int gfx_v8_0_eop_irq")
gfx_eop_state = function_body(gfx_c_plain, "static void gfx_v8_0_set_gfx_eop_interrupt_state")
fault_state = function_body(gmc8_c, "static int gmc_v8_0_vm_fault_interrupt_state")
fault_process = function_body(gmc8_c, "static int gmc_v8_0_process_interrupt")
flush_tlb = function_body(gmc8_c, "static uint64_t gmc_v8_0_emit_flush_gpu_tlb")
ib_schedule = function_body(ib_c, "int amdgpu_ib_schedule")
vm_flush_common = function_body(vm_c, "int amdgpu_vm_flush")
csb_counts = [int(x) for x in re.findall(r"count \+= (\d+);", csb_size)]
STAGE67_LINUX = {
    # gfx_v8_0_get_csb_size's words that are no clear state's run's, and the
    # four cp_gfx_start asks for beyond it (SET_BASE).
    "CSB_FRAME_WORDS": sum(csb_counts) if "count += 2 + ext->reg_count;" in csb_size else None,
    "SET_BASE_WORDS": int(re.search(r"amdgpu_ring_alloc\(ring, gfx_v8_0_get_csb_size\(adev\) \+ (\d+)\);", gfx_start).group(1)),
    # gfx_v8_0_eop_irq: the graphics ring is micro engine 0, pipe 0, queue 0.
    "GRAPHICS_ID": 0 if "case 0:\n\t\tamdgpu_fence_process(&adev->gfx.gfx_ring[0]);" in eop_irq else None,
    "VS_PARTIAL_FLUSH": enum8.get("VS_PARTIAL_FLUSH"),
    "VGT_FLUSH": enum8.get("VGT_FLUSH"),
    "VISLANDS30_IV_SRCID_GFX_PAGE_INV_FAULT": ivsrc8.get("VISLANDS30_IV_SRCID_GFX_PAGE_INV_FAULT"),
    "VISLANDS30_IV_SRCID_GFX_MEM_PROT_FAULT": ivsrc8.get("VISLANDS30_IV_SRCID_GFX_MEM_PROT_FAULT"),
    "PIPELINE_SYNC_POLL": int(re.search(r"amdgpu_ring_write\(ring, (\d+)\); /\* poll interval \*/", pipeline_sync).group(1)),
    "VM_FLUSH_POLL": int(re.search(r"amdgpu_ring_write\(ring, (0x[0-9a-fA-F]+)\); /\* poll interval \*/", vm_flush).group(1), 0),
    "CONTEXT_LOAD_ENABLE": int(re.search(r"dw2 \|= (0x[0-9a-fA-F]+); /\* set load_enable", cntxcntl).group(1), 0),
    "CONTEXT_LOAD_GLOBAL": int(re.search(r"/\* set load_global_config & load_global_uconfig \*/\s*dw2 \|= (0x[0-9a-fA-F]+);", cntxcntl).group(1), 0),
    "CONTEXT_LOAD_CS_SH": int(re.search(r"/\* set load_cs_sh_regs \*/\s*dw2 \|= (0x[0-9a-fA-F]+);", cntxcntl).group(1), 0),
    "CONTEXT_LOAD_PER_CONTEXT": int(re.search(r"/\* set load_per_context_state & load_gfx_sh_regs for GFX \*/\s*dw2 \|= (0x[0-9a-fA-F]+);", cntxcntl).group(1), 0),
    "IH_VMID_0_LUT__PASID": oss_masks.get("IH_VMID_0_LUT__PASID_MASK"),
    "AMDGPU_VA_RESERVED_SIZE": (lambda m: int(m.group(1)) << int(m.group(2)))(re.search(r"#define AMDGPU_VA_RESERVED_SIZE\s+\((\d+)ULL << (\d+)\)", vm_h)),
    "DMA_BITS": int(re.search(r"dma_set_mask_and_coherent\(adev->dev, DMA_BIT_MASK\((\d+)\)\)", gmc8_c).group(1)),
    # gpu_map's flags as the spec names them.
    "READ": int(re.search(r"flags read (\d+), write \d+, execute \d+", spec_text).group(1)),
    "WRITE": int(re.search(r"flags read \d+, write (\d+), execute \d+", spec_text).group(1)),
    "EXECUTE": int(re.search(r"flags read \d+, write \d+, execute (\d+)", spec_text).group(1)),
    # Linux's own clients' address spaces: 1 to first_kfd_vmid less one.
    "HANDED": int(re.search(r"adev->vm_manager.first_kfd_vmid = (\d+);", gmc8_c).group(1)) - 1,
}
for name in ("PROTECTIONS", "MEMORY_CLIENT_ID", "MEMORY_CLIENT_RW", "VMID"):
    STAGE67_LINUX["VM_CONTEXT1_PROTECTION_FAULT_STATUS__" + name] = masks.get("VM_CONTEXT1_PROTECTION_FAULT_STATUS__%s_MASK" % name)
for name in ("RANGE", "DUMMY_PAGE", "PDE0", "VALID", "READ", "WRITE", "EXECUTE"):
    whole = "VM_CONTEXT1_CNTL__%s_PROTECTION_FAULT_ENABLE_INTERRUPT" % name
    STAGE67_LINUX[whole] = masks.get(whole + "_MASK")
    hold("gmc_v8_0_vm_fault_interrupt_state sets %s in both address spaces' words" % whole, int(whole + "_MASK" in fault_state and "tmp = RREG32(mmVM_CONTEXT0_CNTL);\n\t\ttmp |= bits;" in fault_state and "tmp = RREG32(mmVM_CONTEXT1_CNTL);\n\t\ttmp |= bits;" in fault_state), 1)
    hold("%s is the same bit in VM_CONTEXT0_CNTL" % whole, masks.get(whole + "_MASK"), masks.get(whole.replace("CONTEXT1", "CONTEXT0") + "_MASK"))
STAGE67_OWN = {"BUFFER_AT", "TEST_AT", "NONE", "UNHELD", "POOL_AT", "KERNEL", "KERNEL_PASID", "KEPT_FAULTS", "RINGS", "SYSTEM_AT", "CARD_AT",
               "UNMAPPED_AT", "FIRST_AT", "SECOND_AT", "THIRD_AT", "WORD_AT", "FIRST_WORD", "CARD_WORD", "STRAY_WORD", "THIRD_WORD",
               "FAULT_LATER_MS", "SPACE_FRAME_WORDS"}
STAGE67_PRODUCTS = {"POOL_PAGES", "POOL_WORDS", "DIRECTORY_ENTRIES", "DIRECTORY_BYTES", "TABLE_ENTRIES", "TABLE_BYTES"}
stage67_names = {}
ring67 = [(name, value, inside) for name, value, inside in constants(ring_text) if name in STAGE67_RING]
for name, value, inside in constants(graphics_text) + constants(space_text) + constants(first_text) + ring67:
    stage67_names[name] = value
    if name in STAGE67_OWN or name in STAGE67_PRODUCTS:
        continue
    if name in STAGE67_LINUX:
        hold(name, value, STAGE67_LINUX[name])
    elif "__" in name:
        hold(name, value, masks.get(name + "_MASK"))
    else:
        hold(name, value, regs.get("mm" + name))
# The products and the frame's room.
hold("a directory's entries: MAX_PFN >> BLOCK_SIZE", int("DIRECTORY_ENTRIES: usize = engines.MAX_PFN >> @intCast(u6, engines.BLOCK_SIZE)" in space_text), 1)
hold("a table's entries: 2^BLOCK_SIZE (amdgpu_vm_pt_entries_mask)", int("TABLE_ENTRIES: usize = @intCast(usize, 1) << @intCast(u6, engines.BLOCK_SIZE)" in space_text), 1)
frame_words = 5 + 5 + 7 + 5 + 5 + 7 + 2 + 5 + 12 + 4 + 5 + 7 + 4 + 3 + 4 + 5 + 12 + 2
hold("the most a program's frame takes on the graphics ring (99 words) fits SPACE_FRAME_WORDS", int(frame_words <= stage67_names["SPACE_FRAME_WORDS"]), 1)
hold("the first submission: 916 words (gfx_v8_0_get_csb_size and SET_BASE)", STAGE67_LINUX["CSB_FRAME_WORDS"] + 2 * len(cs_runs) + len(linux_values) + STAGE67_LINUX["SET_BASE_WORDS"], 916)
# V1's addresses: above the kernel's bottom, inside the 64 GiB; its two
# pages in one table, the unmapped page in another.
for name in ("SYSTEM_AT", "CARD_AT", "UNMAPPED_AT"):
    hold("%s lies above the reserved bottom and inside the address space" % name, int(stage67_names[name] >= STAGE67_LINUX["AMDGPU_VA_RESERVED_SIZE"] and stage67_names[name] >> 12 < engine_names["MAX_PFN"]), 1)
hold("V1's two pages share a table", stage67_names["SYSTEM_AT"] >> (12 + engine_names["BLOCK_SIZE"]), stage67_names["CARD_AT"] >> (12 + engine_names["BLOCK_SIZE"]))
hold("V1's unmapped page is in no table it makes", int(stage67_names["UNMAPPED_AT"] >> (12 + engine_names["BLOCK_SIZE"]) != stage67_names["SYSTEM_AT"] >> (12 + engine_names["BLOCK_SIZE"])), 1)
# The order of the two words (gfx_v8_0_constants_init) and the graphics
# ring's end-of-pipe interrupt (gfx_v8_0_set_gfx_eop_interrupt_state).
hold("HDP_ADDR_CONFIG before DMIF_ADDR_CALC, as gfx_v8_0_constants_init writes them", int(constants_init.index("WREG32(mmHDP_ADDR_CONFIG") < constants_init.index("WREG32(mmDMIF_ADDR_CALC") and graphics_text.index("compute.put(e, c, engines.HDP_ADDR_CONFIG, config)") < graphics_text.index("compute.put(e, c, engines.DMIF_ADDR_CALC, config)")), 1)
hold("the graphics ring's end of pipe is CP_INT_CNTL_RING0's TIME_STAMP_INT_ENABLE", int("WREG32_FIELD(CP_INT_CNTL_RING0, TIME_STAMP_INT_ENABLE" in gfx_eop_state and "CP_INT_CNTL_RING0__TIME_STAMP_INT_ENABLE" in graphics_text), 1)
hold("the vertex grouper's flush: VS_PARTIAL_FLUSH index 4, VGT_FLUSH index 0", int("EVENT_TYPE(VS_PARTIAL_FLUSH) |\n\t\tEVENT_INDEX(4)" in vgt_flush and "EVENT_TYPE(VGT_FLUSH) |\n\t\tEVENT_INDEX(0)" in vgt_flush and "event(l, r, VS_PARTIAL_FLUSH, 4)" in ring_text and "event(l, r, VGT_FLUSH, 0)" in ring_text), 1)
# A job's frame in amdgpu_ib_schedule's and amdgpu_vm_flush's order, and
# submitIn's in the same.
def in_order(text, marks):
    places = [text.find(mark) for mark in marks]
    return int(all(p >= 0 for p in places) and places == sorted(places))
submit_in = ring_text[ring_text.index("pub fn submitIn"):]
hold("amdgpu_ib_schedule's order: memory sync, the address space's flush, the conditional execution, the host path's flush, the context control, the buffer, the invalidate, the fence, the count patched, the buffer switch",
     in_order(ib_schedule, ["ring->funcs->emit_mem_sync(ring);", "r = amdgpu_vm_flush(ring, job, need_pipe_sync);", "patch_offset = amdgpu_ring_init_cond_exec(ring);", "amdgpu_device_flush_hdp(adev, ring);", "amdgpu_ring_emit_cntxcntl(ring, status);", "amdgpu_ring_emit_ib(ring, job, ib, status);", "amdgpu_device_invalidate_hdp(adev, ring);", "r = amdgpu_fence_emit(ring, f, job, fence_flags);", "amdgpu_ring_patch_cond_exec(ring, patch_offset);", "amdgpu_ring_emit_switch_buffer(ring);"]), 1)
hold("submitIn's order is the same", in_order(submit_in, ["if s.*.sync { memSync(l, r) }", "pipelineSync(l, r)", "const second = if graphics { condExec(l, r) }", "hdpFlush(l, r)", "emit(l, r, packet3(PACKET3_CONTEXT_CONTROL, 1))", "indirect(l, r, address, length, s.*.space)", "writeRegister(l, r, loaded.HDP_DEBUG0, 1)", "fence(l, r, r.*.sent, true)\n    if graphics {\n        patchCondExec(l, r, second)", "        switchBuffer(l, r)\n    }\n    r.*.context"]), 1)
hold("amdgpu_vm_flush's order: the conditional execution, the wait for the last fence, the flush, the number, a fence, the count patched, two buffer switches",
     in_order(vm_flush_common, ["patch_offset = amdgpu_ring_init_cond_exec(ring);", "amdgpu_ring_emit_pipeline_sync(ring);", "amdgpu_ring_emit_vm_flush(ring, job->vmid, job->vm_pd_addr);", "amdgpu_gmc_emit_pasid_mapping(ring, job->vmid, job->pasid);", "r = amdgpu_fence_emit(ring, &fence, NULL, 0);", "amdgpu_ring_patch_cond_exec(ring, patch_offset);", "amdgpu_ring_emit_switch_buffer(ring);\n\t\tamdgpu_ring_emit_switch_buffer(ring);"]), 1)
hold("submitIn's flush part is in the same order", in_order(submit_in, ["const first = if graphics { condExec(l, r) }", "pipelineSync(l, r)", "writeRegister(l, r, baseRegister(s.*.space), s.*.directory >> 12)", "writeRegister(l, r, engines.VM_INVALIDATE_REQUEST,", "emit(l, r, packet3(PACKET3_PFP_SYNC_ME, 0))", "if s.*.lut { writeRegister(l, r, IH_VMID_0_LUT + s.*.space, s.*.pasid) }", "fence(l, r, r.*.sent, true)", "patchCondExec(l, r, first)", "switchBuffer(l, r)\n            switchBuffer(l, r)"]), 1)
hold("gmc_v8_0_emit_flush_gpu_tlb: the base register (0 to 7 from VM_CONTEXT0's, 8 to 15 from VM_CONTEXT8's) = the directory >> 12, then VM_INVALIDATE_REQUEST = 1 << n",
     int("reg = mmVM_CONTEXT0_PAGE_TABLE_BASE_ADDR + vmid;" in flush_tlb and "reg = mmVM_CONTEXT8_PAGE_TABLE_BASE_ADDR + vmid - 8;" in flush_tlb and "amdgpu_ring_emit_wreg(ring, reg, pd_addr >> 12);" in flush_tlb and "amdgpu_ring_emit_wreg(ring, mmVM_INVALIDATE_REQUEST, 1 << vmid);" in flush_tlb), 1)
hold("the wait for the flush: always, on VM_INVALIDATE_REQUEST; the pre-fetch parser's sync on the graphics ring", int("WAIT_REG_MEM_FUNCTION(0) |  /* always */" in vm_flush and "amdgpu_ring_write(ring, mmVM_INVALIDATE_REQUEST);" in vm_flush and "PACKET3(PACKET3_PFP_SYNC_ME, 0)" in vm_flush), 1)
hold("the wait for the last fence: memory, equal, the pre-fetch parser's on the graphics ring", int("WAIT_REG_MEM_MEM_SPACE(1) | /* memory */" in pipeline_sync and "WAIT_REG_MEM_FUNCTION(3) | /* equal */" in pipeline_sync and "WAIT_REG_MEM_ENGINE(usepfp)" in pipeline_sync), 1)
hold("Linux flushes at every job of an address space whose tables the processor writes", int("bool needs_flush = vm->use_cpu_for_update;" in ids_c and "job->vm_needs_flush = vm->use_cpu_for_update;" in ids_c and ".flush = true," in space_text), 1)
hold("a flush brings the wait for the last fence", int("if (job->vm_needs_flush || ring->has_compute_vm_bug)" in vm_c), 1)
# The entries (the study's 2.3, the source where it differs).
hold("a page of the card's memory: its offset in it (vram_base_offset 0 on a card that is no APU)", int("adev->vm_manager.vram_base_offset = 0;" in gmc8_c and "addr = vram_base + cursor.start;" in vm_c), 1)
hold("the card's memory is write-combined for a card not on XGMI: no snooped bit", int("vres->base.bus.caching = ttm_write_combined;" in vram_mgr_c and "mem->bus.caching == ttm_cached)\n\t\tflags |= AMDGPU_PTE_SNOOPED;" in ttm_c), 1)
hold("a system page: valid, system, snooped when cached (amdgpu_ttm_tt_pde_flags)", int("flags |= AMDGPU_PTE_SYSTEM;\n\n\t\tif (ttm->caching == ttm_cached)\n\t\t\tflags |= AMDGPU_PTE_SNOOPED;" in ttm_c), 1)
hold("readable, writeable as the mapping asks; executable from the mapping (amdgpu_vm_bo_update, gmc_v8_0_get_vm_pte)", int("update_flags &= ~AMDGPU_PTE_READABLE;" in vm_c and "update_flags &= ~AMDGPU_PTE_WRITEABLE;" in vm_c and "*flags |= mapping->flags & AMDGPU_PTE_EXECUTABLE;" in gmc8_c), 1)
hold("a directory entry: the table's card address (amdgpu_bo_gpu_offset) | valid", int("*addr = amdgpu_bo_gpu_offset(bo);" in gmc_common_c and "flags |= AMDGPU_PTE_VALID;" in ttm_c and "(s.*.vramStart + table) & engines.PTE_ADDRESS) | engines.AMDGPU_PTE_VALID" in space_text), 1)
hold("an entry written whole, its address & 0x0000FFFFFFFFF000 (amdgpu_gmc_set_pte_pde)", int("value = addr & 0x0000FFFFFFFFF000ULL;" in gmc_common_c and engine_names["PTE_ADDRESS"] == 0x0000FFFFFFFFF000), 1)
hold("the processor's writes then the host path flushed (amdgpu_vm_cpu_commit)", int("amdgpu_device_flush_hdp(p->adev, NULL);" in text_of("amdgpu_vm_cpu.c")), 1)
# Both kinds of fault interrupt in V1 (the study's 1.2, row 13): the
# memory's and the graphics block's (gfx_v8_0_late_init's priv_reg_irq and
# priv_inst_irq, a field each of CP_INT_CNTL_RING0).
hold("late_init's two graphics fault interrupts, each a field of CP_INT_CNTL_RING0, turned on by V1",
     int("r = amdgpu_irq_get(adev, &adev->gfx.priv_reg_irq, 0);" in gfx_c and "r = amdgpu_irq_get(adev, &adev->gfx.priv_inst_irq, 0);" in gfx_c and "WREG32_FIELD(CP_INT_CNTL_RING0, PRIV_REG_INT_ENABLE," in gfx_c and "WREG32_FIELD(CP_INT_CNTL_RING0, PRIV_INSTR_INT_ENABLE," in gfx_c and "compute.put(e, c, loaded.CP_INT_CNTL_RING0, rd(e, loaded.CP_INT_CNTL_RING0) | privateFaults())" in first_text), 1)
# A space runs on one ring at a time: this card flushes no address space
# another engine still uses (concurrent flushes only from Vega10 on; a space
# that needs a flush is taken idle on every other ring).
hold("no concurrent flush before Vega10 (amdgpu_vm_manager_init), a used space that needs a flush not taken (amdgpu_vmid_grab_used)",
     int("adev->vm_manager.concurrent_flush = !(adev->asic_type < CHIP_VEGA10 ||" in vm_c and "if (needs_flush && !adev->vm_manager.concurrent_flush)\n\t\t\tcontinue;" in ids_c and "if !idleBut(e, s, space, k) { return NONE }" in space_text), 1)
# A fault is the context's whose number its entry carries (Linux names the
# process by entry->pasid, the entry's bits 31:16 of its third word).
hold("a fault's process by the entry's number (gmc_v8_0_process_interrupt, tonga_ih_decode_iv)",
     int("amdgpu_vm_get_task_info(adev, entry->pasid, &task_info);" in gmc8_c and "entry->pasid = (dw[2] >> 16) & 0xffff;" in ih_c and "const number = (r.*.spaceWord[k] >> 16) & IH_VMID_0_LUT__PASID" in space_text and "pasidOf(s.*.owner[space]) != number" in space_text), 1)
# The fault taken as gmc_v8_0_process_interrupt takes it.
hold("a fault: page, status, client read, then VM_CONTEXT1_CNTL2 bit 0 set; nothing more when page and status are 0",
     int(in_order(fault_process, ["RREG32(mmVM_CONTEXT1_PROTECTION_FAULT_ADDR)", "RREG32(mmVM_CONTEXT1_PROTECTION_FAULT_STATUS)", "RREG32(mmVM_CONTEXT1_PROTECTION_FAULT_MCCLIENT)", "WREG32_P(mmVM_CONTEXT1_CNTL2, 1, ~1);", "if (!addr && !status)"]) == 1 and in_order(ring_text, ["rd(e, engines.VM_CONTEXT1_PROTECTION_FAULT_ADDR)", "rd(e, engines.VM_CONTEXT1_PROTECTION_FAULT_STATUS)", "rd(e, engines.VM_CONTEXT1_PROTECTION_FAULT_MCCLIENT)", "VM_CONTEXT1_CNTL2__CLEAR_PROTECTION_FAULT_STATUS_ADDR)", "if page == 0 && status == 0"]) == 1), 1)
hold("the client's four characters, byte 3 first", int("char block[5] = { mc_client >> 24," in gmc8_c), 1)
# No register these steps write is the display's but DMIF_ADDR_CALC.
written67 = set(re.findall(r"\b(?:put|putField)\(e, c, (?:engines\.|loaded\.|ring\.|compute\.)?(\w+)", graphics_text + first_text)) | set(re.findall(r"\bwr\(e, (?:engines\.|loaded\.|ring\.|compute\.)?(\w+)", graphics_text + space_text + first_text))
for name in sorted(written67):
    number = stage67_names.get(name, stage5_names.get(name, loaded_names.get(name, engine_names.get(name))))
    if number is None:
        continue
    in_display = any(runs[k] <= number <= runs[k + 1] for k in range(0, len(runs), 2))
    hold("%s (%#x) written by stage 6 or 7 is %s register of the display's" % (name, number, "the one" if name == "DMIF_ADDR_CALC" else "no"), int(in_display), int(name == "DMIF_ADDR_CALC"))
hold("DMIF_ADDR_CALC is written only where it differs", int("if g.*.dmifWas != config {\n            compute.put(e, c, engines.DMIF_ADDR_CALC, config)" in graphics_text), 1)
print("stages 6 and 7: %d numbers of gpu_graphics.mlx, gpu_space.mlx, gpu_first_space.mlx and gpu_ring.mlx held against Linux's files, %d wrong" % (held - stage67_before, bad - stage67_bad_before))
sys.exit(1 if bad or rom_bad or wrong or display_bad else 0)
