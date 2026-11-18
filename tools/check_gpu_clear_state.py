#!/usr/bin/env python3
# Holds the graphics ring's first submission as the kernel writes it
# (stage 6, kernel/gpu_graphics.mlx through gpu_compute.mlx's
# graphicsStart: the study's 5.7) against Linux's own, word for word: the
# words gfx_v8_0_cp_gfx_start writes, in its order, made here from Linux
# 6.6's files alone (the ring writes of the function, vid.h's PACKET3 and
# its operations, clearstate_vi.h's runs and values, gfx_8_0_d.h's
# PA_SC_RASTER_CONFIG, gfx_v8_0_raster_config's Polaris 10 words by vid.h's
# macros), so that none of the 916 words is from memory or from the
# kernel's own table:
#
#   t506 words | tools/check_gpu_clear_state.py HEADERS
#
# t506 is tests/506_gpu_engines_graphics_ring_runtime.mlx built for the
# host; with the argument `words` it prints the 916 words one a line.
# HEADERS is a directory with gfx_v8_0.c, vid.h, clearstate_vi.h,
# clearstate_defs.h and gfx_8_0_d.h of Linux 6.6 (drivers/gpu/drm/amd/).
# Prints what differs and ends with 1 when anything does.
import os, re, sys

if len(sys.argv) != 2:
    sys.exit("usage: t506 words | check_gpu_clear_state.py HEADERS (see the file's head)")
hdr = sys.argv[1]

def text_of(name):
    return re.sub(r"/\*.*?\*/", "", open(os.path.join(hdr, name), errors="replace").read(), flags=re.S)

gfx_c = text_of("gfx_v8_0.c")
vid = text_of("vid.h")
clear = re.sub(r"//[^\n]*", "", text_of("clearstate_vi.h"))
regs = {m.group(1): int(m.group(2), 0) for m in re.finditer(r"#define\s+(\w+)\s+(0x[0-9a-fA-F]+|\d+)\b", text_of("gfx_8_0_d.h"))}

def define(name):
    m = re.search(r"#\s*define\s+%s\s+(0x[0-9a-fA-F]+|\d+)\b" % name, vid)
    if m:
        return int(m.group(1), 0)
    m = re.search(r"#\s*define\s+%s\s+\((\d+)\s*<<\s*(\d+)\)" % name, vid)
    if m:
        return int(m.group(1)) << int(m.group(2))
    sys.exit("vid.h has no %s" % name)

def shift(name):
    m = re.search(r"#\s*define\s+%s\(x\)\s+\(\(x\)\s*<<\s*(\d+)\)" % name, vid)
    if not m:
        sys.exit("vid.h has no macro %s(x)" % name)
    return int(m.group(1))

# PACKET3(op, n) of vid.h: its type in bits 31:30, the operation, the count.
m = re.search(r"#define\s+PACKET3\(op, n\)\s+\(\(PACKET_TYPE3 << 30\)\s*\|\s*\\?\s*\(\(\(op\) & 0xFF\) << 8\)\s*\|\s*\\?\s*\(\(n\) & 0x3FFF\) << 16\)", vid)
if not m:
    sys.exit("vid.h's PACKET3 is not as this script reads it")
def packet3(op, n):
    return (define("PACKET_TYPE3") << 30) | ((define(op) & 0xFF) << 8) | ((n & 0x3FFF) << 16)

def body(text, signature):
    at = text.index(signature)
    at = text.index("{", at)
    return text[at:text.index("\n}\n", at)]

start = body(gfx_c, "static int gfx_v8_0_cp_gfx_start(struct amdgpu_device *adev)")
start = start[start.index("r = amdgpu_ring_alloc(ring, gfx_v8_0_get_csb_size(adev) + 4);"):]
raster = body(gfx_c, "\ngfx_v8_0_raster_config(struct amdgpu_device *adev, u32 *rconf, u32 *rconf1)")
case = re.search(r"case CHIP_TONGA:\n\tcase CHIP_POLARIS10:\n(.*?)break;", raster, re.S).group(1)
def raster_value(expression):
    return eval(re.sub(r"(\w+)\((\d+)\)", lambda m: "(%d << %d)" % (int(m.group(2)), shift(m.group(1))), " ".join(expression.split())))
rconf = raster_value(re.search(r"\*rconf \|= (.*?);", case, re.S).group(1))
rconf1 = raster_value(re.search(r"\*rconf1 \|= (.*?);", case, re.S).group(1))

# clearstate_vi.h: the context section's runs (array, first register,
# count) in vi_SECT_CONTEXT_defs' order, each array's values.
arrays = {m.group(1): [int(v, 0) for v in m.group(2).replace("\n", " ").split(",") if v.strip()]
          for m in re.finditer(r"static const unsigned int (vi_SECT_CONTEXT_def_\d+)\[\] =\s*\{(.*?)\};", clear, re.S)}
runs = re.findall(r"\{\s*(vi_SECT_CONTEXT_def_\d+),\s*(0x[0-9a-fA-F]+),\s*(\d+)\s*\}", clear)
if not re.search(r"\{\s*vi_SECT_CONTEXT_defs,\s*SECT_CONTEXT\s*\}", clear):
    sys.exit("clearstate_vi.h's vi_cs_data has no context section as this script reads it")

# The words, statement by statement of the function.
words = []
for statement in re.findall(r"(for \(sect = vi_cs_data;.*?\n\t\}|amdgpu_ring_write\(ring,.*?\);)", start, re.S):
    if statement.startswith("for"):
        if "PACKET3(PACKET3_SET_CONTEXT_REG,\n\t\t\t\t\t       ext->reg_count)" not in statement or "ext->reg_index - PACKET3_SET_CONTEXT_REG_START" not in statement:
            sys.exit("the clear state's loop is not as this script reads it")
        for name, first, count in runs:
            values = arrays[name]
            if len(values) != int(count):
                sys.exit("%s has %d values, its run says %s" % (name, len(values), count))
            words.append(packet3("PACKET3_SET_CONTEXT_REG", int(count)))
            words.append(int(first, 0) - define("PACKET3_SET_CONTEXT_REG_START"))
            words += values
        continue
    argument = " ".join(statement[len("amdgpu_ring_write(ring,"):-2].split())
    m = re.match(r"PACKET3\((\w+), (\d+)\)$", argument)
    if m:
        words.append(packet3(m.group(1), int(m.group(2))))
    elif re.match(r"(0x[0-9a-fA-F]+|\d+)$", argument):
        words.append(int(argument, 0))
    elif argument == "mmPA_SC_RASTER_CONFIG - PACKET3_SET_CONTEXT_REG_START":
        words.append(regs["mmPA_SC_RASTER_CONFIG"] - define("PACKET3_SET_CONTEXT_REG_START"))
    elif argument == "adev->gfx.config.rb_config[0][0].raster_config":
        words.append(rconf)
    elif argument == "adev->gfx.config.rb_config[0][0].raster_config_1":
        words.append(rconf1)
    elif argument == "PACKET3_BASE_INDEX(CE_PARTITION_BASE)":
        words.append(define("CE_PARTITION_BASE") << shift("PACKET3_BASE_INDEX"))
    elif re.match(r"PACKET3_\w+$", argument):
        words.append(define(argument))
    else:
        sys.exit("a word of gfx_v8_0_cp_gfx_start this script does not read: %s" % argument)

given = [int(line, 0) for line in sys.stdin.read().split()]
bad = 0
if len(given) != len(words):
    print("WRONG the kernel's first submission has %d words, Linux's %d" % (len(given), len(words)))
    bad += 1
for k in range(min(len(given), len(words))):
    if given[k] != words[k]:
        print("WRONG word %d: the kernel's %#x, Linux's %#x" % (k, given[k], words[k]))
        bad += 1
print("the graphics ring's first submission: %d words of the kernel's held against gfx_v8_0_cp_gfx_start and clearstate_vi.h (%d runs, %d values), %d wrong"
      % (len(given), len(runs), sum(int(c) for _, _, c in runs), bad))
sys.exit(1 if bad else 0)
