#!/usr/bin/env python3
# Holds the numbers of our Vulkan driver's stages (a), (b) and (c) (projects/
# brix/vulkan: driver.mlx; packets.mlx, card_host.mlx, device.mlx,
# pipeline.mlx) against the reference's own files at the tags the driver
# follows, so that none is from memory:
#
#   tools/check_vulkan_driver.py MESA LINUX
#
# MESA is a directory with these files of Mesa mesa-24.0.0 at their paths
# in its tree (https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-24.0.0/
# PATH), LINUX one with Linux v6.6's (https://raw.githubusercontent.com/
# torvalds/linux/v6.6/PATH); neither is kept in this repository:
#   src/amd/vulkan/radv_physical_device.c   the limits (radv_get_physical_
#                                           device_properties), the
#                                           descriptor counts' formula
#   src/amd/vulkan/radv_constants.h         the numbers the limits name
#   drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c the Polaris 10 device numbers
# and for stage (b) the files the section "Stage (b)" below names.
#
# Checked:
#   1. every limit driver.mlx's `limits` writes is the value RADV's limits
#      give it on this generation (GFX8, a Polaris 10: max_shared_size of
#      GFX7 and later, the descriptor counts' formula worked out from the
#      constants), and every limit RADV gives that driver.mlx leaves at 0 is
#      one the stage says it leaves (the timestamps: none at this stage);
#   2. `serves` takes exactly the device numbers amdgpu_drv.c lists as
#      CHIP_POLARIS10 (not a range: the list has gaps);
#   3. every opcode, register range, register, field, enumeration and
#      value of a compute stream in packets.mlx, the address spaces' start
#      and the numbers of RSRC1's formula (below);
#   4. (stage (c)) every opcode, register, field and enumeration value of
#      graphics_packets.mlx, the graphics preamble's values (queue.mlx) by
#      their fields, RADV's own numbers (radv_emit_graphics, the raster
#      configuration of Polaris 10, ac_compute_late_alloc's RSRC3_VS and
#      late allocation), the linear tile mode's entry in the kernel's table
#      and Linux's, and the clear state's PA_SC_EDGERULE (below).
import os
import re
import sys

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
driver = os.path.join(root, "projects", "brix", "vulkan", "driver.mlx")
failures = 0


def fail(message):
    global failures
    failures += 1
    print("FAIL " + message)


def ok(message):
    print("ok   " + message)


def constants(text):
    """#define NAME VALUE of a C header, the value as text."""
    found = {}
    for m in re.finditer(r"^#define\s+([A-Z0-9_]+)\s+(.+?)\s*(?://.*|/\*.*)?$", text, re.M):
        found[m.group(1)] = m.group(2).strip()
    return found


def c_value(text, names, local):
    """A C expression of the limits block as a number (or a pair)."""
    text = text.strip().rstrip(",")
    if text in local:
        return local[text]
    if text.startswith("{") and text.endswith("}"):
        return tuple(c_value(part, names, local) for part in text[1:-1].split(","))
    expression = text
    # (A constant's value may name others: until none is left.)
    for _ in range(8):
        before = expression
        for name in sorted(names, key=len, reverse=True):
            expression = re.sub(r"\b%s\b" % name, "(%s)" % names[name], expression)
        if expression == before:
            break
    expression = expression.replace("UINT32_MAX", "0xFFFFFFFF").replace("INT16_MIN", "-32768").replace("INT16_MAX", "32767")
    expression = " ".join(expression.split())
    expression = re.sub(r"\b(0x[0-9A-Fa-f]+|\d+)u?ll\b", r"\1", expression)
    expression = re.sub(r"\b(0x[0-9A-Fa-f]+|\d+)u\b", r"\1", expression)
    expression = expression.replace("true", "1").replace("false", "0")
    # (C's division of whole numbers.)
    if "." not in expression:
        expression = expression.replace("/", "//")
    try:
        return eval(expression, {"__builtins__": {}}, {})
    except Exception:
        return None


def radv_limits(mesa):
    source = open(os.path.join(mesa, "src/amd/vulkan/radv_physical_device.c")).read()
    header = open(os.path.join(mesa, "src/amd/vulkan/radv_constants.h")).read()
    names = constants(header)
    # The formula of radv_max_descriptor_set_size, with its constants.
    body = re.search(r"radv_max_descriptor_set_size\(\)\s*\{(.*?)\n\}", source, re.S).group(1)
    formula = re.search(r"return (.*?);", body, re.S).group(1)
    formula = re.sub(r"/\*.*?\*/", "", formula, flags=re.S)
    names.setdefault("MAX_DYNAMIC_BUFFERS", "(%s + %s)" % (names["MAX_DYNAMIC_UNIFORM_BUFFERS"], names["MAX_DYNAMIC_STORAGE_BUFFERS"]))
    descriptors = c_value(formula, names, {})
    # The device's own facts: GFX8 (max_shared_size of GFX7 and later), a
    # Polaris 10; sample counts 0xf.
    shared = re.search(r"max_shared_size = device->rad_info.gfx_level >= GFX7 \? (\d+) : (\d+);", source)
    # radv_uniform_buffer_offset_alignment: the hardware's least, the
    # driconf override being 0 unless set.
    uniform = re.search(r"return MAX2\(uniform_offset_alignment, (\d+)\);", source)
    local = {"max_descriptor_set_size": descriptors, "sample_counts": 0xF, "pdevice->max_shared_size": int(shared.group(1)), "radv_uniform_buffer_offset_alignment(pdevice)": int(uniform.group(1))}
    block = re.search(r"pdevice->vk.properties = \(struct vk_properties\)\{(.*?)\n   \};", source, re.S).group(1)
    limits = {}
    for m in re.finditer(r"^\s*\.([A-Za-z0-9]+) = (.+?),?\s*(?:/\*.*\*/)?$", block, re.M):
        limits[m.group(1)] = c_value(m.group(2), names, local)
    return limits, descriptors


def driver_limits():
    text = open(driver).read()
    body = re.search(r"fn limits\(into: \*vk.PhysicalDeviceLimits\) -> void \{(.*?)\n\}", text, re.S).group(1)
    local = {"all": 0xFFFFFFFF, "samples": 0xF, "vk.FALSE": 0, "vk.TRUE": 1}
    for m in re.finditer(r"const (\w+): u32 = (\w+)", body):
        local[m.group(1)] = int(m.group(2), 0)
    found = {}
    for m in re.finditer(r"into\.\*\.(\w+)(?:\[(\d)\])? = (\S+)", body):
        value = m.group(3)
        number = local[value] if value in local else float(value) if "." in value else int(value, 0)
        if m.group(2) is None:
            found[m.group(1)] = number
        else:
            found.setdefault(m.group(1), {})[int(m.group(2))] = number
    for key, value in list(found.items()):
        if isinstance(value, dict):
            found[key] = tuple(value[i] for i in sorted(value))
    return found


# The limits the stage gives otherwise, and why.
OURS = {
    # No timestamps at this stage: the queue family's valid bits are 0.
    "timestampComputeAndGraphics": 0,
    "timestampPeriod": 1.0,
    # Stage (c)'s own: 16 vertex locations and bindings, offsets the typed
    # fetch's field takes (the registry's least each), one viewport (no
    # multiViewport), one sample.
    "maxVertexInputAttributes": 16,
    "maxVertexInputBindings": 16,
    "maxVertexInputAttributeOffset": 2047,
    "maxViewports": 1,
    "framebufferColorSampleCounts": 1,
}


def check_limits(mesa):
    radv, descriptors = radv_limits(mesa)
    ours = driver_limits()
    if descriptors != 8388606:
        fail("radv_max_descriptor_set_size works out at %r, the driver writes 8388606" % descriptors)
    checked = 0
    for name, value in ours.items():
        if name in OURS:
            if value != OURS[name]:
                fail("%s is %r, the stage's own is %r" % (name, value, OURS[name]))
            continue
        if name not in radv:
            fail("%s: no such limit in RADV's" % name)
            continue
        want = radv[name]
        if want is None or (isinstance(want, tuple) and None in want):
            fail("%s: RADV's value could not be worked out" % name)
            continue
        if isinstance(want, tuple) or isinstance(value, tuple):
            if tuple(float(x) for x in (want if isinstance(want, tuple) else (want,))) != tuple(float(x) for x in (value if isinstance(value, tuple) else (value,))):
                fail("%s is %r, RADV's %r" % (name, value, want))
        elif float(value) != float(want):
            fail("%s is %r, RADV's %r" % (name, value, want))
        checked += 1
    # Every limit RADV has: written by the driver (the sparse residency
    # fields are VkPhysicalDeviceSparseProperties', not limits, and stay
    # false: sparse binding is not offered).
    properties = {"apiVersion", "driverVersion", "vendorID", "deviceID", "deviceType", "sparseResidencyNonResidentStrict", "sparseResidencyStandard2DBlockShape", "sparseResidencyStandard3DBlockShape"}
    missing = sorted(set(radv) - set(ours) - properties)
    if missing:
        fail("limits RADV gives that the driver does not write: %s" % ", ".join(missing))
    ok("%d limits as RADV gives them on GFX8, the timestamps' two the stage's own" % checked)


def check_serves(linux):
    text = open(os.path.join(linux, "drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c")).read()
    listed = sorted(int(m.group(1), 16) for m in re.finditer(r"\{0x1002, (0x[0-9A-Fa-f]+), PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_POLARIS10\}", text))
    source = open(driver).read()
    m = re.search(r"const POLARIS10 = \[_\]u32\{([^}]*)\}", source)
    if not m:
        fail("serves: its list is not where it was")
        return
    ours = sorted(int(x.strip(), 16) for x in m.group(1).split(","))
    if ours != listed:
        fail("serves takes %s, amdgpu_drv.c's Polaris 10 are %s" % ([hex(x) for x in ours], [hex(x) for x in listed]))
    else:
        ok("serves takes the %d device numbers amdgpu_drv.c lists as CHIP_POLARIS10" % len(listed))


# ---- Stage (b): the packets and registers of a compute stream
# (projects/brix/vulkan/packets.mlx), the host card's and the device's
# address space (card_host.mlx, device.mlx) and the numbers the driver
# writes by RADV's rules (pipeline.mlx, queue.mlx, commands.mlx). Beside
# the files above, of Mesa:
#   src/amd/common/sid.h                 the opcodes, PKT3_NOP_PAD, the ranges
#   src/amd/registers/gfx8.json          the registers, their fields, the
#                                        enumerations (SQ_SEL_*, FLOAT_MODE,
#                                        VGT_EVENT_TYPE, BUF_*_FORMAT)
#   src/amd/vulkan/si_cmd_buffer.c       ACQUIRE_MEM's words, the CP DMA
#                                        alignment, CS_PARTIAL_FLUSH's index
#   src/amd/vulkan/radv_descriptor_set.c a buffer descriptor's 16 bytes
#   src/amd/vulkan/radv_constants.h      the shader code's alignment
#   src/amd/common/ac_gpu_info.c         the compute engine's padding
#   src/amd/compiler/aco_live_var_analysis.cpp  VCC's two registers
# and of Linux:
#   drivers/gpu/drm/amd/amdgpu/vid.h     the opcodes, the ranges, DMA_DATA's
#                                        fields
#   drivers/gpu/drm/amd/include/asic_reg/gca/gfx_8_0_d.h and
#   gfx_8_0_sh_mask.h                    the registers and their masks
#   drivers/gpu/drm/amd/amdgpu/amdgpu_vm.h  the reserved first 2 MiB
def check_packets(mesa, linux):
    import json
    vulkan = os.path.join(root, "projects", "brix", "vulkan")
    text = open(os.path.join(vulkan, "packets.mlx")).read()
    ours = {}
    for m in re.finditer(r"pub const ([A-Z0-9_]+): (?:u32|usize) = (0x[0-9A-Fa-f]+|\d+)", text):
        ours[m.group(1)] = int(m.group(2), 0)
    sid_text = open(os.path.join(mesa, "src/amd/common/sid.h")).read()
    sid = constants(sid_text)
    linux_text = open(os.path.join(linux, "drivers/gpu/drm/amd/amdgpu/vid.h")).read()

    def macro(source_text, name):
        # A #define's body, of a function-like one too (vid.h indents its
        # fields' defines).
        m = re.search(r"^#\s*define\s+%s(?:\(x\))?\s+(.+?)\s*(?:/\*.*)?$" % name, source_text, re.M)
        return m.group(1).strip() if m else None
    vid = constants(open(os.path.join(linux, "drivers/gpu/drm/amd/amdgpu/vid.h")).read())
    regs = {m.group(1): m.group(2) for m in re.finditer(r"^#define\s+(mm\w+)\s+(0x[0-9a-fA-F]+)", open(os.path.join(linux, "drivers/gpu/drm/amd/include/asic_reg/gca/gfx_8_0_d.h")).read(), re.M)}
    masks = constants(open(os.path.join(linux, "drivers/gpu/drm/amd/include/asic_reg/gca/gfx_8_0_sh_mask.h")).read())
    database = json.load(open(os.path.join(mesa, "src/amd/registers/gfx8.json")))
    at = {r["name"]: r["map"]["at"] for r in database["register_mappings"]}
    kinds = {r["name"]: r.get("type", r["name"]) for r in database["register_mappings"]}
    types = database["register_types"]
    enums = {name: {e["name"]: e["value"] for e in value["entries"]} for name, value in database["enums"].items()}
    held = 0

    def hold(what, got, want):
        nonlocal held
        held += 1
        if got != want:
            fail("%s: packets.mlx has %s, the source %s" % (what, got, want))

    def number(text_value):
        return int(re.sub(r"[uUlL]+$", "", text_value.strip("()")), 0)

    # The opcodes, the same in both.
    for name, value in ours.items():
        if name.startswith("PKT3_"):
            hold(name + " (sid.h)", value, number(sid[name]))
            hold(name + " (vid.h)", value, number(vid["PACKET3" + name[4:]]))
    hold("NOP_PAD is PKT3(PKT3_NOP, 0x3fff, 0)", ours["NOP_PAD"], 3 << 30 | 0x3FFF << 16 | number(sid["PKT3_NOP"]) << 8)
    hold("the compute bit (PKT3_SHADER_TYPE_S)", ours["COMPUTE_BIT"], 2 if macro(sid_text, "PKT3_SHADER_TYPE_S") == "(((unsigned)(x) & 0x1) << 1)" else None)
    # The ranges: Linux's ends in words times 4, its starts as Mesa's.
    for ours_name, linux_name, mesa_name in (("CONFIG", "CONFIG_REG", "SI_CONFIG_REG_OFFSET"), ("SH", "SH_REG", "SI_SH_REG_OFFSET"), ("CONTEXT", "CONTEXT_REG", "SI_CONTEXT_REG_OFFSET"), ("UCONFIG", "UCONFIG_REG", "CIK_UCONFIG_REG_OFFSET")):
        hold(ours_name + "_START", ours[ours_name + "_START"], number(vid["PACKET3_SET_%s_START" % linux_name]) * 4)
        hold(ours_name + "_START (sid.h)", ours[ours_name + "_START"], number(sid[mesa_name]))
        hold(ours_name + "_END", ours[ours_name + "_END"], number(vid["PACKET3_SET_%s_END" % linux_name]) * 4)
    # The registers by name, and their fields.
    for name, value in ours.items():
        if "__" in name:
            register, field = name.split("__", 1)
            if register == "DMA_DATA":
                continue
            if register == "COMPUTE_NUM_THREAD_X" or register == "COMPUTE_STATIC_THREAD_MGMT_SE0" or register in types or register in kinds:
                kind = kinds.get(register, register)
                fields = {f["name"]: f["bits"] for f in types[kind]["fields"]}
                low, high = fields[field]
                hold(name + " (gfx8.json)", value, ((1 << (high - low + 1)) - 1) << low)
                hold(name + " (gfx_8_0_sh_mask.h)", value, number(masks[name + "_MASK"]))
        elif (name.startswith("COMPUTE_") and name != "COMPUTE_BIT") or name == "CP_COHER_CNTL":
            hold(name + " (gfx8.json)", value, at.get(name))
            hold(name + " (gfx_8_0_d.h)", value, number(regs["mm" + name]) * 4)
    # DMA_DATA's fields (vid.h's) and its count's field (gfx8.json's
    # CP_DMA_ME_COMMAND).
    hold("DMA_DATA__CP_SYNC", ours["DMA_DATA__CP_SYNC"], 1 << 31 if macro(linux_text, "PACKET3_DMA_DATA_CP_SYNC") == "(1 << 31)" else None)
    hold("DMA_DATA__SRC_SEL", ours["DMA_DATA__SRC_SEL"], 3 << 29 if macro(linux_text, "PACKET3_DMA_DATA_SRC_SEL") == "((x) << 29)" else None)
    hold("DMA_DATA__DST_SEL", ours["DMA_DATA__DST_SEL"], 3 << 20 if macro(linux_text, "PACKET3_DMA_DATA_DST_SEL") == "((x) << 20)" else None)
    hold("DMA_DATA__DIS_WC", ours["DMA_DATA__DIS_WC"], 1 << 21 if macro(linux_text, "PACKET3_DMA_DATA_DIS_WC") == "(1 << 21)" else None)
    hold("DMA_DATA__RAW_WAIT", ours["DMA_DATA__RAW_WAIT"], 1 << 30 if macro(linux_text, "PACKET3_DMA_DATA_CMD_RAW_WAIT") == "(1 << 30)" else None)
    command = {f["name"]: f["bits"] for f in types["CP_DMA_ME_COMMAND"]["fields"]}
    low, high = command["BYTE_COUNT"]
    hold("DMA_DATA__BYTE_COUNT (CP_DMA_ME_COMMAND)", ours["DMA_DATA__BYTE_COUNT"], ((1 << (high - low + 1)) - 1) << low)
    hold("SRC_SEL_DATA (vid.h: 2 - DATA)", ours["SRC_SEL_DATA"], 2 if re.search(r"PACKET3_DMA_DATA_SRC_SEL\(x\).*?\n.*?2 - DATA", linux_text, re.S) else None)
    hold("DST_SEL_L2 (vid.h: 3 - DST_ADDR using L2)", ours["DST_SEL_L2"], 3 if "3 - DST_ADDR using L2" in linux_text else None)
    hold("SRC_SEL_L2 (vid.h: 3 - SRC_ADDR using L2)", ours["SRC_SEL_L2"], 3 if "3 - SRC_ADDR using L2" in linux_text else None)
    cmd = open(os.path.join(mesa, "src/amd/vulkan/si_cmd_buffer.c")).read()
    alignment = int(re.search(r"#define SI_CPDMA_ALIGNMENT (\d+)", cmd).group(1))
    hold("DMA_MOST_BYTES (cp_dma_max_byte_count)", ours["DMA_MOST_BYTES"], ours["DMA_DATA__BYTE_COUNT"] & ~(alignment - 1))
    # The enumerations.
    for name, enum, entry in (("SQ_SEL_X", "SQ_SEL_XYZW01", "SQ_SEL_X"), ("SQ_SEL_Y", "SQ_SEL_XYZW01", "SQ_SEL_Y"), ("SQ_SEL_Z", "SQ_SEL_XYZW01", "SQ_SEL_Z"), ("SQ_SEL_W", "SQ_SEL_XYZW01", "SQ_SEL_W"),
                              ("BUF_NUM_FORMAT_FLOAT", "BUF_NUM_FORMAT", "BUF_NUM_FORMAT_FLOAT"), ("BUF_DATA_FORMAT_32", "BUF_DATA_FORMAT", "BUF_DATA_FORMAT_32"),
                              ("FP_16_64_DENORMS", "FLOAT_MODE", "FP_16_64_DENORMS"), ("CS_PARTIAL_FLUSH", "VGT_EVENT_TYPE", "CS_PARTIAL_FLUSH")):
        hold(name, ours[name], enums[enum][entry])
    hold("EVENT_INDEX_PARTIAL (radv_cs_emit_cache_flush)", ours["EVENT_INDEX_PARTIAL"], 4 if "EVENT_TYPE(V_028A90_CS_PARTIAL_FLUSH) | EVENT_INDEX(4)" in cmd else None)
    acquire = cmd[cmd.index("radv_emit_acquire_mem(struct radeon_cmdbuf"):]
    acquire = acquire[:acquire.index("\n}\n")]
    hold("COHER_SIZE_ALL", ours["COHER_SIZE_ALL"], 0xFFFFFFFF if "radeon_emit(cs, 0xffffffff);    /* CP_COHER_SIZE */" in acquire else None)
    hold("COHER_SIZE_HI_ALL", ours["COHER_SIZE_HI_ALL"], 0xFF if "is_gfx9 ? 0xffffff : 0xff" in acquire else None)
    hold("COHER_POLL", ours["COHER_POLL"], 0xA if "radeon_emit(cs, 0x0000000A);    /* POLL_INTERVAL */" in acquire else None)
    descriptor = open(os.path.join(mesa, "src/amd/vulkan/radv_descriptor_set.c")).read()
    size_of = re.search(r"case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:\n(?:\s*case [A-Z_]+:\n)*\s*return (\d+);", descriptor)
    hold("DESCRIPTOR_BYTES (a storage buffer's descriptor)", ours["DESCRIPTOR_BYTES"], int(size_of.group(1)) if size_of else None)
    info = open(os.path.join(mesa, "src/amd/common/ac_gpu_info.c")).read()
    pad = re.search(r"info->ip\[AMD_IP_COMPUTE\]\.ib_pad_dw_mask = (0x[0-9a-f]+);", info)
    hold("PAD_WORDS (the compute engine's ib_pad_dw_mask + 1)", ours["PAD_WORDS"], int(pad.group(1), 0) + 1 if pad else None)
    # The address spaces' first 2 MiB (AMDGPU_VA_RESERVED_SIZE) and the
    # code's alignment (RADV_SHADER_ALLOC_ALIGNMENT: COMPUTE_PGM_LO's >> 8).
    vm = open(os.path.join(linux, "drivers/gpu/drm/amd/amdgpu/amdgpu_vm.h")).read()
    reserved = re.search(r"#define AMDGPU_VA_RESERVED_SIZE\s+\((\d+)ULL << (\d+)\)", vm)
    for name in ("card_host.mlx", "device.mlx"):
        first = re.search(r"pub const FIRST_ADDRESS: u64 = (0x[0-9A-Fa-f]+)", open(os.path.join(vulkan, name)).read())
        hold("FIRST_ADDRESS of " + name, int(first.group(1), 0), int(reserved.group(1)) << int(reserved.group(2)))
    constants_h = constants(open(os.path.join(mesa, "src/amd/vulkan/radv_constants.h")).read())
    hold("the code's alignment: COMPUTE_PGM_LO's shift", 1 << 8, int(constants_h["RADV_SHADER_ALLOC_ALIGNMENT"]))
    # RSRC1's counts (pipeline.mlx rsrc1Of): VCC's two (get_extra_sgprs on
    # GFX8 without xnack), the entry's arguments + 5 (radv_shader.c's "+2
    # for the ring offsets, +3 for scratch wave offset and VCC").
    pipeline_text = open(os.path.join(vulkan, "pipeline.mlx")).read()
    aco = open(os.path.join(mesa, "src/amd/compiler/aco_live_var_analysis.cpp")).read()
    radv_shader = open(os.path.join(mesa, "src/amd/vulkan/radv_shader.c")).read()
    hold("VCC's two scalar registers (get_extra_sgprs)", 2 if "facts.*.sgprs + 2, 16), 16)" in pipeline_text else 0, 2 if re.search(r"else if \(program->needs_vcc\)\s*return 2;", aco) else None)
    # The host card's addressable scalar registers: the allocation less
    # those (get_addr_sgpr_from_waves).
    hold("VCC_SGPRS (get_addr_sgpr_from_waves takes get_extra_sgprs off)", ours["VCC_SGPRS"], 2 if re.search(r"else if \(program->needs_vcc\)\s*return 2;", aco) and "sgprs -= get_extra_sgprs(program);" in aco else None)
    hold("the entry's arguments + 5 (radv_postprocess_binary_config)", 5 if "entry + 5)" in pipeline_text else 0, 5 if "args->ac.num_sgprs_used + 2 + 3" in radv_shader else None)
    ok("stage (b): %d numbers of packets.mlx, card_host.mlx, device.mlx and pipeline.mlx held against Mesa's and Linux's files" % held)


# ---- Stage (c): the graphics ring's packets and registers (projects/brix/
# vulkan/graphics_packets.mlx), the numbers of RADV's own the driver writes
# (queue.mlx's preamble, graphics.mlx's pipeline words), the linear tile
# mode entry and the linear layout (image.mlx) and the clear state's edge
# rule. Beside the files above, of Mesa: src/amd/vulkan/si_cmd_buffer.c
# (radv_emit_graphics), radv_pipeline_graphics.c, radv_cmd_buffer.c,
# radv_cs.h, src/amd/common/ac_shader_util.c (ac_compute_late_alloc's
# formula), ac_gpu_info.c (the raster configuration of Polaris 10), src/
# amd/addrlib/src/r800/siaddrlib.cpp and egbaddrlib.cpp (the linear
# alignments), and of Linux: gfx_v8_0.c (its tiling table, Polaris 10's
# GB_ADDR_CONFIG), clearstate_vi.h, include/asic_reg/gca/gfx_8_0_enum.h.
def check_graphics(mesa, linux):
    import json
    import struct
    vulkan = os.path.join(root, "projects", "brix", "vulkan")
    ours = {}
    for m in re.finditer(r"pub const ([A-Z0-9_]+): (?:u32|usize) = (0x[0-9A-Fa-f]+|\d+)", open(os.path.join(vulkan, "graphics_packets.mlx")).read()):
        ours[m.group(1)] = int(m.group(2), 0)
    sid = constants(open(os.path.join(mesa, "src/amd/common/sid.h")).read())
    vid = constants(open(os.path.join(linux, "drivers/gpu/drm/amd/amdgpu/vid.h")).read())
    regs = {m.group(1): m.group(2) for m in re.finditer(r"^#define\s+(mm\w+)\s+(0x[0-9a-fA-F]+)", open(os.path.join(linux, "drivers/gpu/drm/amd/include/asic_reg/gca/gfx_8_0_d.h")).read(), re.M)}
    database = json.load(open(os.path.join(mesa, "src/amd/registers/gfx8.json")))
    at = {r["name"]: r["map"]["at"] for r in database["register_mappings"]}
    kinds = {r["name"]: r.get("type_ref", r["name"]) for r in database["register_mappings"]}
    types = database["register_types"]
    enums = {}
    for name, value in database["enums"].items():
        for e in value["entries"]:
            enums.setdefault(e["name"], set()).add(e["value"])
    held = 0

    def hold(what, got, want):
        nonlocal held
        held += 1
        if got != want:
            fail("%s: ours %r, the source %r" % (what, got, want))

    def number(text_value):
        return int(re.sub(r"[uUlL]+$", "", text_value.strip("()")), 0)

    def mask(register, field):
        kind = kinds.get(register, register)
        if kind not in types:
            return None
        fields = {f["name"]: f["bits"] for f in types[kind]["fields"]}
        if field not in fields:
            return None
        low, high = fields[field]
        return ((1 << (high - low + 1)) - 1) << low
    # The opcodes.
    for name, value in ours.items():
        if name.startswith("PKT3_"):
            hold(name + " (sid.h)", value, number(sid[name]))
            hold(name + " (vid.h)", value, number(vid["PACKET3" + name[4:]]))
    # The registers: gfx8.json's offset and gfx_8_0_d.h's word times 4.
    fields_of = {"SPI_SHADER_PGM_RSRC1": "SPI_SHADER_PGM_RSRC1_VS", "SPI_SHADER_PGM_RSRC2": "SPI_SHADER_PGM_RSRC2_VS", "SPI_SHADER_PGM_RSRC3": "SPI_SHADER_PGM_RSRC3_VS", "SPI_SHADER_PGM_RSRC3_HS": "SPI_SHADER_PGM_RSRC3_HS", "SPI_SHADER_PGM_HI": "SPI_SHADER_PGM_HI_VS", "SPI_PS_INPUT_CNTL": "SPI_PS_INPUT_CNTL_0", "SCISSOR": "PA_SC_GENERIC_SCISSOR_TL", "SCREEN_SCISSOR": "PA_SC_SCREEN_SCISSOR_TL", "GB_TILE_MODE": "GB_TILE_MODE0"}
    renamed = {("SCISSOR", "X"): ("PA_SC_GENERIC_SCISSOR_TL", "TL_X"), ("SCISSOR", "Y"): ("PA_SC_GENERIC_SCISSOR_TL", "TL_Y"), ("SCREEN_SCISSOR", "X"): ("PA_SC_SCREEN_SCISSOR_TL", "TL_X"), ("SCREEN_SCISSOR", "Y"): ("PA_SC_SCREEN_SCISSOR_TL", "TL_Y"), ("CB_COLOR_CONTROL", "MODE"): ("CB_COLOR_CONTROL", "MODE"), ("VGT_SHADER_STAGES_EN", "ALL"): None, ("CP_COHER_CNTL", "CB_DEST_BASE_ALL"): None, ("PA_CL_VTE_CNTL", "ALL_SCALES_OFFSETS"): None, ("SPI_SHADER_POS_FORMAT", "POS0_EXPORT_FORMAT"): ("SPI_SHADER_POS_FORMAT", "POS0_EXPORT_FORMAT")}
    for name, value in ours.items():
        if "__" in name:
            register, field = name.split("__", 1)
            key = renamed.get((register, field), (fields_of.get(register, register), field))
            if key is None:
                continue
            want = mask(key[0], key[1])
            if want is None and register in ("VGT_DRAW_INITIATOR", "EOP"):
                continue
            hold(name + " (gfx8.json %s.%s)" % key, value, want)
        elif name.startswith(("PA_", "CB_", "DB_", "SPI_", "VGT_", "IA_")) and name not in ("CB_COLOR_STRIDE", "SPI_SHADER_ZERO", "SPI_SHADER_32_ABGR", "SPI_SHADER_4COMP", "CB_NORMAL", "SPI_PNT_SPRITE_SEL_0", "SPI_PNT_SPRITE_SEL_1", "SPI_PNT_SPRITE_SEL_S", "SPI_PNT_SPRITE_SEL_T", "VGT_INDEX_16", "VGT_INDEX_32") and value >= 0x8000:
            hold(name + " (gfx8.json)", value, at.get(name))
            if "mm" + name in regs:
                hold(name + " (gfx_8_0_d.h)", value, number(regs["mm" + name]) * 4)
    hold("CB_COLOR_STRIDE", ours["CB_COLOR_STRIDE"], at["CB_COLOR1_BASE"] - at["CB_COLOR0_BASE"])
    hold("CP_COHER_CNTL__CB_DEST_BASE_ALL", ours["CP_COHER_CNTL__CB_DEST_BASE_ALL"], sum(mask("CP_COHER_CNTL", "CB%d_DEST_BASE_ENA" % k) for k in range(8)))
    # The enumerations' values by their names in gfx8.json.
    for name in ("COLOR_INVALID", "COLOR_8_8_8_8", "SWAP_STD", "SWAP_ALT", "CB_NORMAL", "BLEND_ZERO", "BLEND_ONE", "BLEND_SRC_COLOR", "BLEND_ONE_MINUS_SRC_COLOR", "BLEND_SRC_ALPHA", "BLEND_ONE_MINUS_SRC_ALPHA", "BLEND_DST_ALPHA", "BLEND_ONE_MINUS_DST_ALPHA", "BLEND_DST_COLOR", "BLEND_ONE_MINUS_DST_COLOR", "BLEND_CONSTANT_COLOR", "BLEND_ONE_MINUS_CONSTANT_COLOR", "BLEND_CONSTANT_ALPHA", "BLEND_ONE_MINUS_CONSTANT_ALPHA", "COMB_DST_PLUS_SRC", "COMB_SRC_MINUS_DST", "COMB_MIN_DST_SRC", "COMB_MAX_DST_SRC", "COMB_DST_MINUS_SRC", "SPI_SHADER_ZERO", "SPI_SHADER_32_ABGR", "SPI_SHADER_4COMP", "SPI_PNT_SPRITE_SEL_0", "SPI_PNT_SPRITE_SEL_1", "SPI_PNT_SPRITE_SEL_S", "SPI_PNT_SPRITE_SEL_T", "X_ROUND_TO_EVEN", "X_16_8_FIXED_POINT_1_256TH", "DI_PT_TRILIST", "DI_PT_TRISTRIP", "DI_SRC_SEL_DMA", "DI_SRC_SEL_AUTO_INDEX", "VGT_INDEX_16", "VGT_INDEX_32", "VS_PARTIAL_FLUSH", "PS_PARTIAL_FLUSH", "PIPELINESTAT_START", "FLUSH_AND_INV_CB_DATA_TS", "FLUSH_AND_INV_CB_META", "FLUSH_AND_INV_DB_META", "FORCE_DISABLE", "ARRAY_LINEAR_ALIGNED", "NUMBER_UNORM", "EARLY_Z_THEN_LATE_Z"):
            found = enums.get(name, set())
            hold(name + " (gfx8.json)", ours[name] if len(found) <= 1 or ours[name] in found else None, next(iter(found)) if len(found) == 1 else (ours[name] if ours[name] in found else None))
    # RADV's own numbers (radv_emit_graphics and the pipeline's).
    cmd = open(os.path.join(mesa, "src/amd/vulkan/si_cmd_buffer.c")).read()
    graphics_text = cmd[cmd.index("radv_emit_graphics(struct radv_device"):]
    graphics_text = graphics_text[:graphics_text.index("\n}\n")]
    fui = lambda x: struct.unpack("<I", struct.pack("<f", x))[0]
    hold("MAX_TESS_LEVEL fui(64)", ours["MAX_TESS_LEVEL"], fui(64.0) if "R_028A18_VGT_HOS_MAX_TESS_LEVEL, fui(64)" in graphics_text else None)
    hold("ES_PER_GS", ours["ES_PER_GS"], 0x40 if "R_028A58_VGT_ES_PER_GS, 0x40" in graphics_text else None)
    constants_h = constants(open(os.path.join(mesa, "src/amd/vulkan/radv_constants.h")).read())
    hold("GS_PER_ES (SI_GS_PER_ES)", ours["GS_PER_ES"], number(constants_h["SI_GS_PER_ES"]))
    hold("LINE_STIPPLE_SCALE", ours["LINE_STIPPLE_SCALE"], 0x3F800000 if "R_028828_PA_SU_LINE_STIPPLE_SCALE, 0x3f800000" in graphics_text else None)
    hold("POINT_SIZE (unsigned)(1.0 * 8.0)", ours["POINT_SIZE"], 8 if "unsigned tmp = (unsigned)(1.0 * 8.0);" in graphics_text else None)
    hold("POINT_MAX_SIZE radv_pack_float_12p4(8191.875 / 2)", ours["POINT_MAX_SIZE"], min(int(8191.875 / 2 * 16), 0xFFFF) if "radv_pack_float_12p4(8191.875 / 2)" in graphics_text else None)
    hold("ES_VERTS_PER_SUBGRP", ours["ES_VERTS_PER_SUBGRP"], 64 if "S_028A44_ES_VERTS_PER_SUBGRP(64) | S_028A44_GS_PRIMS_PER_SUBGRP(4)" in graphics_text else None)
    hold("GS_PRIMS_PER_SUBGRP", ours["GS_PRIMS_PER_SUBGRP"], 4 if "S_028A44_ES_VERTS_PER_SUBGRP(64) | S_028A44_GS_PRIMS_PER_SUBGRP(4)" in graphics_text else None)
    info = open(os.path.join(mesa, "src/amd/common/ac_gpu_info.c")).read()
    polaris = re.search(r"case CHIP_POLARIS10:\s*raster_config = (0x[0-9a-f]+);\s*raster_config_1 = (0x[0-9a-f]+);", info)
    hold("RASTER_CONFIG_POLARIS10", ours["RASTER_CONFIG_POLARIS10"], int(polaris.group(1), 16) if polaris else None)
    hold("RASTER_CONFIG_1_POLARIS10", ours["RASTER_CONFIG_1_POLARIS10"], int(polaris.group(2), 16) if polaris else None)
    sid_text = open(os.path.join(mesa, "src/amd/common/sid.h")).read()
    for name in ("CC0_UPDATE_LOAD_ENABLES", "CC1_UPDATE_SHADOW_ENABLES"):
        hold(name, ours[name], 1 << 31 if re.search(r"#define\s+%s\(x\)\s+\(\(\(unsigned\)\(x\)&0x1\) << 31\)" % name, sid_text) else None)
    hold("PA_CL_VTE_CNTL__ALL_SCALES_OFFSETS (the six ENA fields)", ours["PA_CL_VTE_CNTL__ALL_SCALES_OFFSETS"], sum(mask("PA_CL_VTE_CNTL", f) for f in ("VPORT_X_SCALE_ENA", "VPORT_X_OFFSET_ENA", "VPORT_Y_SCALE_ENA", "VPORT_Y_OFFSET_ENA", "VPORT_Z_SCALE_ENA", "VPORT_Z_OFFSET_ENA")))
    # The queue's preamble values (queue.mlx) by their fields.
    queue = {}
    for m in re.finditer(r"pub const ([A-Z0-9_]+): u32 = (0x[0-9A-Fa-f]+|[0-9 |<]+)", open(os.path.join(vulkan, "queue.mlx")).read()):
        queue[m.group(1)] = eval(m.group(2))
    put = lambda register, field, value: (value << ((mask(register, field) & -mask(register, field)).bit_length() - 1)) & mask(register, field)
    hold("DB_RENDER_OVERRIDE (FORCE_HIS_ENABLE0, 1: FORCE_DISABLE)", queue["DB_RENDER_OVERRIDE_VALUE"], put("DB_RENDER_OVERRIDE", "FORCE_HIS_ENABLE0", ours["FORCE_DISABLE"]) | put("DB_RENDER_OVERRIDE", "FORCE_HIS_ENABLE1", ours["FORCE_DISABLE"]))
    hold("RSRC3 CU_EN 0xffff, WAVE_LIMIT 0x3F", queue["RSRC3_ALL"], put("SPI_SHADER_PGM_RSRC3_LS", "CU_EN", 0xFFFF) | put("SPI_SHADER_PGM_RSRC3_LS", "WAVE_LIMIT", 0x3F))
    hold("RSRC3_HS WAVE_LIMIT 0x3F", queue["RSRC3_HS"], put("SPI_SHADER_PGM_RSRC3_HS", "WAVE_LIMIT", 0x3F))
    hold("VGT_GS_ONCHIP_CNTL", queue["GS_ONCHIP"], put("VGT_GS_ONCHIP_CNTL", "ES_VERTS_PER_SUBGRP", 64) | put("VGT_GS_ONCHIP_CNTL", "GS_PRIMS_PER_SUBGRP", 4))
    hold("PA_SU_POINT_SIZE", queue["POINT_SIZE"], put("PA_SU_POINT_SIZE", "HEIGHT", 8) | put("PA_SU_POINT_SIZE", "WIDTH", 8))
    hold("PA_SU_POINT_MINMAX", queue["POINT_MINMAX"], put("PA_SU_POINT_MINMAX", "MAX_SIZE", 0xFFFF))
    hold("SPI_INTERP_CONTROL_0", queue["INTERP_CONTROL"], put("SPI_INTERP_CONTROL_0", "FLAT_SHADE_ENA", 1) | put("SPI_INTERP_CONTROL_0", "PNT_SPRITE_ENA", 1) | put("SPI_INTERP_CONTROL_0", "PNT_SPRITE_OVRD_X", ours["SPI_PNT_SPRITE_SEL_S"]) | put("SPI_INTERP_CONTROL_0", "PNT_SPRITE_OVRD_Y", ours["SPI_PNT_SPRITE_SEL_T"]) | put("SPI_INTERP_CONTROL_0", "PNT_SPRITE_OVRD_Z", ours["SPI_PNT_SPRITE_SEL_0"]) | put("SPI_INTERP_CONTROL_0", "PNT_SPRITE_OVRD_W", ours["SPI_PNT_SPRITE_SEL_1"]))
    hold("PA_SU_VTX_CNTL", queue["VTX_CNTL"], put("PA_SU_VTX_CNTL", "PIX_CENTER", 1) | put("PA_SU_VTX_CNTL", "ROUND_MODE", ours["X_ROUND_TO_EVEN"]) | put("PA_SU_VTX_CNTL", "QUANT_MODE", ours["X_16_8_FIXED_POINT_1_256TH"]))
    hold("PA_CL_VTE_CNTL", queue["VTE_CNTL"], sum(put("PA_CL_VTE_CNTL", f, 1) for f in ("VTX_W0_FMT", "VPORT_X_SCALE_ENA", "VPORT_X_OFFSET_ENA", "VPORT_Y_SCALE_ENA", "VPORT_Y_OFFSET_ENA", "VPORT_Z_SCALE_ENA", "VPORT_Z_OFFSET_ENA")))
    hold("PA_SU_PRIM_FILTER_CNTL", queue["PRIM_FILTER"], put("PA_SU_PRIM_FILTER_CNTL", "XMAX_RIGHT_EXCLUSION", 1) | put("PA_SU_PRIM_FILTER_CNTL", "YMAX_BOTTOM_EXCLUSION", 1))
    # The vertex shader's RSRC3 and late allocation (ac_compute_late_alloc
    # for 36 compute units in 4 engines of 1 array: 9 an array).
    late = open(os.path.join(mesa, "src/amd/common/ac_shader_util.c")).read()
    graphics_mlx = open(os.path.join(vulkan, "graphics.mlx")).read()
    # (ac_compute_late_alloc's GFX8 branch, as graphics.mlx's lateAlloc
    # computes it from the record's compute units a shader array)
    def constant(name):
        return int(re.search(r"pub const %s: u32 = (0x[0-9A-F]+|\d+)" % name, graphics_mlx).group(1), 0)
    hold("late allocation: none at min_good_cu_per_sa <= %d" % constant("LATE_ALLOC_NONE_AT"), "if (info->min_good_cu_per_sa <= %d)\n      return;" % constant("LATE_ALLOC_NONE_AT") in late, True)
    hold("late allocation: %d at min_good_cu_per_sa <= %d" % (constant("LATE_ALLOC_FEW"), constant("LATE_ALLOC_FEW_AT")), "if (info->min_good_cu_per_sa <= %d) {" % constant("LATE_ALLOC_FEW_AT") in late and "*late_alloc_wave64 = %d;" % constant("LATE_ALLOC_FEW") in late, True)
    hold("late allocation: (min_good_cu_per_sa - %d) * %d" % (constant("LATE_ALLOC_SPARED"), constant("LATE_ALLOC_PER_CU")), "*late_alloc_wave64 = (info->min_good_cu_per_sa - %d) * %d;" % (constant("LATE_ALLOC_SPARED"), constant("LATE_ALLOC_PER_CU")) in late, True)
    hold("RSRC3_VS's CU_EN 0x%X above %d waves" % (constant("CU_EN_ONE_OFF"), constant("LATE_ALLOC_FEW")), "if (*late_alloc_wave64 > %d)\n         *cu_mask = 0x%x; /* 1 CU disabled */" % (constant("LATE_ALLOC_FEW"), constant("CU_EN_ONE_OFF")) in late, True)
    hold("RSRC3_VS's CU_EN 0x%X else" % constant("CU_EN_ALL"), "*cu_mask = 0x%x;" % constant("CU_EN_ALL") in late, True)
    hold("RSRC3_VS's WAVE_LIMIT 0x3F", constant("RSRC3_VS_WAVE_LIMIT"), put("SPI_SHADER_PGM_RSRC3_VS", "WAVE_LIMIT", 0x3F))
    hold("RSRC3_VS's CU_EN field", put("SPI_SHADER_PGM_RSRC3_VS", "CU_EN", 0xFFFF), constant("CU_EN_ALL"))
    # The linear tile mode: the kernel's table's entry 8 is ARRAY_LINEAR_
    # ALIGNED, as Linux's Polaris 10 table's modearray[8].
    kernel = open(os.path.join(root, "projects", "brix", "kernel", "gpu_compute.mlx")).read()
    table = [int(x, 16) for x in re.search(r"pub const TILE_MODES = \[_\]usize\{([^}]*)\}", kernel).group(1).replace(" ", "").split(",")]
    index = int(re.search(r"pub const LINEAR_TILE_MODE: u32 = (\d+)", open(os.path.join(vulkan, "image.mlx")).read()).group(1))
    hold("the kernel's TILE_MODES[%d] ARRAY_MODE" % index, (table[index] & ours["GB_TILE_MODE__ARRAY_MODE"]) >> 2, ours["ARRAY_LINEAR_ALIGNED"])
    gfx = open(os.path.join(linux, "drivers/gpu/drm/amd/amdgpu/gfx_v8_0.c")).read()
    polaris_table = gfx[gfx.index("case CHIP_POLARIS10:", gfx.index("gfx_v8_0_tiling_mode_table_init")):]
    entry = re.search(r"modearray\[%d\] = \(ARRAY_MODE\((\w+)\)" % index, polaris_table)
    hold("Linux's Polaris 10 modearray[%d]" % index, entry.group(1) if entry else None, "ARRAY_LINEAR_ALIGNED")
    # The linear layout (image.mlx) against addrlib (src/amd/addrlib/src/
    # r800/siaddrlib.cpp, egbaddrlib.cpp) and Polaris 10's pipe interleave
    # (Linux's gfx_v8_0.c, the golden GB_ADDR_CONFIG its case takes).
    image_mlx = open(os.path.join(vulkan, "image.mlx")).read()
    def image_constant(name):
        return int(re.search(r"pub const %s: u(?:32|64) = (0x[0-9A-F]+|\d+)" % name, image_mlx).group(1), 0)
    si = open(os.path.join(mesa, "src/amd/addrlib/src/r800/siaddrlib.cpp")).read()
    eg = open(os.path.join(mesa, "src/amd/addrlib/src/r800/egbaddrlib.cpp")).read()
    masks = constants(open(os.path.join(linux, "drivers/gpu/drm/amd/include/asic_reg/gca/gfx_8_0_sh_mask.h")).read())
    early = gfx[gfx.index("gfx_v8_0_gpu_early_init"):]
    polaris = early[early.index("case CHIP_POLARIS10:"):]
    golden = re.search(r"gb_addr_config = (\w+);", polaris).group(1)
    config = int(re.search(r"#define %s (0x[0-9a-fA-F]+)" % golden, gfx).group(1), 16)
    interleave_mask = int(masks["GB_ADDR_CONFIG__PIPE_INTERLEAVE_SIZE_MASK"], 0)
    interleave_shift = int(masks["GB_ADDR_CONFIG__PIPE_INTERLEAVE_SIZE__SHIFT"], 0)
    interleave = 256 << ((config & interleave_mask) >> interleave_shift)
    hold("linear base alignment: the pipe interleave (%s)" % golden, image_constant("BASE_ALIGNMENT"), interleave if "*pBaseAlign     = m_pipeInterleaveBytes;" in eg else None)
    pitch_align = max(8, 64 // 4) if "pitchAlign = Max(8u, 64 / BITS_TO_BYTES(bpp));" in si else None
    hold("linear pitch alignment max(8, 64 / 4)", image_constant("PITCH_PIXELS"), pitch_align)
    slice_align = max(64, interleave // 4) if "UINT_32 sliceAlignInPixel = pixelsPerPipeInterleave < 64 ? 64 : pixelsPerPipeInterleave;" in si and "pitch += pitchAlign;" in si else None
    hold("linear slice alignment in pixels, the pitch grown", image_constant("SLICE_PIXELS"), slice_align)
    # The edge rule: the clear state's PA_SC_EDGERULE.
    clear = open(os.path.join(root, "projects", "brix", "kernel", "gpu_clear_state.mlx")).read()
    firsts = [int(x, 16) for x in re.search(r"RUN_FIRST = \[_\]usize\{([^}]*)\}", clear).group(1).replace(" ", "").split(",")]
    counts = [int(x) for x in re.search(r"RUN_COUNT = \[_\]usize\{([^}]*)\}", clear).group(1).replace(" ", "").split(",")]
    values = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", re.search(r"VALUES = \[_\]u32\{(.*?)\}", clear, re.S).group(1))]
    word = ours["PA_SC_EDGERULE"] // 4
    edge = None
    done = 0
    for first, count in zip(firsts, counts):
        if first <= word < first + count:
            edge = values[done + word - first]
        done += count
    hold("EDGE_RULE (the clear state's PA_SC_EDGERULE)", ours["EDGE_RULE"], edge)
    ok("stage (c): %d numbers of graphics_packets.mlx, queue.mlx, graphics.mlx and image.mlx held against Mesa's and Linux's files" % held)


def main():
    if len(sys.argv) != 3:
        print("usage: tools/check_vulkan_driver.py MESA LINUX")
        return 2
    check_limits(sys.argv[1])
    check_serves(sys.argv[2])
    check_packets(sys.argv[1], sys.argv[2])
    check_graphics(sys.argv[1], sys.argv[2])
    if failures:
        print("%d checks failed" % failures)
        return 1
    print("all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
