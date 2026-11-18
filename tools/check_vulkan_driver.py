#!/usr/bin/env python3
# Holds the numbers of our Vulkan driver's stages (a) and (b) (projects/
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
#      and the numbers of RSRC1's formula (below).
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


def main():
    if len(sys.argv) != 3:
        print("usage: tools/check_vulkan_driver.py MESA LINUX")
        return 2
    check_limits(sys.argv[1])
    check_serves(sys.argv[2])
    check_packets(sys.argv[1], sys.argv[2])
    if failures:
        print("%d checks failed" % failures)
        return 1
    print("all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
