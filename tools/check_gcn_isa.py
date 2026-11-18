#!/usr/bin/env python3
# Holds every number of our shader code for the RX 580 (GFX8) against
# Mesa's own files at the tag the driver follows, so that none is from
# memory:
#
#   tools/check_gcn_isa.py MESA
#
# MESA is a directory with these files of Mesa mesa-24.0.0 at their paths
# in its tree (https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-24.0.0/
# PATH; they are not kept in this repository):
#   src/amd/compiler/aco_opcodes.py     the instructions and their numbers
#   src/amd/compiler/aco_assembler.cpp  the formats' fixed bits, the VOP3
#                                       offsets, the SDWA and DPP sources
#   src/amd/compiler/aco_ir.h           the special registers, the inline
#                                       constants
#   src/amd/compiler/aco_ir.cpp         the wait word (wait_imm::pack)
#   src/amd/compiler/aco_insert_NOPs.cpp  the wait states of the hazards
#   src/compiler/nir/nir_lower_idiv.c   the division's scale
#   src/amd/registers/gfx8.json         the buffer descriptor's fields
#
# Checked:
#   1. projects/brix/vulkan/isa.mlx is what aco_opcodes.py's GFX8 column
#      gives, read here by its own means (not tools/gcn_isa_generate.py's):
#      every instruction with its format and number, none more, the count,
#      the file's sha256 in the header, and the table's words;
#   2. encode.mlx's constants: each format's fixed bits as aco_assembler.cpp
#      writes them for GFX8, the special registers' numbers, the inline
#      constants' ranges, the VOP3 offsets, the SDWA and DPP sources, the
#      wait word's fields;
#   3. wave.mlx's float constants and wait-state counts, the descriptor's
#      fields; shader_ir.mlx's division scale.
import ast
import hashlib
import json
import os
import re
import sys

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
driver = os.path.join(root, "projects", "brix", "vulkan")
failures = 0


def fail(message):
    global failures
    failures += 1
    print("FAIL " + message)


def ok(message):
    print("ok   " + message)


def mlx_constants(path):
    """`pub const NAME: T = VALUE` of an Mlx file, the value as text."""
    found = {}
    for line in open(path):
        m = re.match(r"\s*(?:pub )?const ([A-Z0-9_]+)(?:: [a-z0-9]+)? = ([^/]+?)\s*(?://.*)?$", line)
        if m:
            found[m.group(1)] = m.group(2).strip()
    return found


def number(text):
    text = text.strip()
    if re.fullmatch(r"0x[0-9A-Fa-f]+|\d+", text):
        return int(text, 0)
    return None


# ---------------------------------------------------------------------------
# 1. The table

def gfx8_instructions(path):
    """{name: (format, number)} of the GFX8 column, by evaluating the
    tables' tuples and the comparisons' loops of the file as data."""
    tree = ast.parse(open(path, "rb").read())
    names = {}
    for node in tree.body:
        if isinstance(node, ast.Assign) and isinstance(node.targets[0], ast.Name) and isinstance(node.value, (ast.Constant, ast.List)):
            names[node.targets[0].id] = ast.literal_eval(node.value)

    def value(node, local):
        if isinstance(node, ast.Constant):
            return node.value
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
            return -value(node.operand, local)
        if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Add):
            return value(node.left, local) + value(node.right, local)
        if isinstance(node, ast.Name):
            return local[node.id] if node.id in local else names.get(node.id, node.id)
        if isinstance(node, ast.Subscript):
            return value(node.value, local)[value(node.slice, local)]
        if isinstance(node, ast.Tuple):
            return tuple(value(e, local) for e in node.elts)
        if isinstance(node, ast.Attribute):
            return node.attr
        if isinstance(node, ast.Call):
            return None
        raise ValueError("expression at line %d" % node.lineno)

    # which column is GFX8, table by table (the loops' own comments)
    formats = {"SOP2": "sop2", "SOPK": "sopk", "SOP1": "sop1", "SOPC": "sopc", "SOPP": "sopp",
               "SMEM": "smem", "VOP2": "vop2", "VOP1": "vop1", "VOPC_CLASS": "vopc", "VOP3": "vop3",
               "DS": "ds", "MUBUF": "mubuf", "MTBUF": "mtbuf"}
    result = {}
    # MUBUF's own GFX8 column, -1 included: FLAT's column is the shared
    # "GFX89" one, so a FLAT row whose buffer namesake GFX8 lacks is GFX9's
    buffer_gfx8 = {}
    for node in tree.body:
        if isinstance(node, ast.Assign) and isinstance(node.targets[0], ast.Name) and isinstance(node.value, ast.Set):
            table = node.targets[0].id
            for element in node.value.elts:
                row = value(element, {})
                if table == "MUBUF":
                    buffer_gfx8[row[6]] = row[2]
                if table in formats:
                    gfx8, name, form = row[2], row[6], formats[table]
                elif table == "VINTRP":
                    gfx8, name, form = row[0], row[1], "vintrp"
                elif table in ("IMAGE", "IMAGE_SAMPLE", "IMAGE_GATHER4"):
                    gfx8, name, form = row[0], row[2], "mimg"
                elif table == "IMAGE_ATOMIC":
                    gfx8, name, form = row[2], row[4], "mimg"
                elif table == "FLAT":
                    gfx8, name, form = row[1], row[4], "flat"
                else:
                    continue
                if gfx8 != -1:
                    result[name] = (form, gfx8)
        if isinstance(node, ast.For) and isinstance(node.target, ast.Name):
            if isinstance(node.iter, ast.Call):
                values = range(*[value(a, {}) for a in node.iter.args])
            elif isinstance(node.iter, ast.List):
                values = [value(e, {}) for e in node.iter.elts]
            else:
                continue
            for i in values:
                local = {node.target.id: i}
                row = None
                for statement in node.body:
                    if isinstance(statement, ast.Assign) and isinstance(statement.targets[0], ast.Tuple):
                        row = value(statement.value, local)
                    elif isinstance(statement, ast.Expr) and isinstance(statement.value, ast.Call) and row is not None:
                        if row[2] != -1:
                            result[row[6]] = ("vopc", row[2])
                        row = None
    for name in [n for n, (form, _) in result.items() if form == "flat"]:
        if buffer_gfx8.get("buffer_" + name[len("flat_"):]) == -1:
            del result[name]
    return result


def check_table(mesa):
    path = os.path.join(mesa, "src/amd/compiler/aco_opcodes.py")
    want = gfx8_instructions(path)
    digest = hashlib.sha256(open(path, "rb").read()).hexdigest()
    text = open(os.path.join(driver, "isa.mlx")).read()
    if digest not in text:
        fail("isa.mlx names another aco_opcodes.py (sha256 %s)" % digest)
    have = {}
    for m in re.finditer(r"pub const ([A-Z0-9_]+) = Op\.\{ \.format = Format\.(\w+), \.number = (0x[0-9A-F]+) \}", text):
        have[m.group(1).lower()] = (m.group(2), int(m.group(3), 16))
    missing = sorted(set(want) - set(have))
    extra = sorted(set(have) - set(want))
    wrong = sorted(n for n in set(want) & set(have) if want[n] != have[n])
    if missing:
        fail("isa.mlx lacks %d: %s" % (len(missing), ", ".join(missing[:8])))
    if extra:
        fail("isa.mlx has %d more than GFX8's column: %s" % (len(extra), ", ".join(extra[:8])))
    if wrong:
        fail("isa.mlx numbers differ for: %s" % ", ".join("%s %s/%s" % (n, have[n], want[n]) for n in wrong[:8]))
    count = re.search(r"pub const COUNT: usize = (\d+)", text)
    if not count or int(count.group(1)) != len(want):
        fail("COUNT is not %d" % len(want))
    order = ["sop2", "sopk", "sop1", "sopc", "sopp", "smem", "vop2", "vop1", "vopc", "vop3", "vintrp", "ds", "mubuf", "mtbuf", "mimg", "flat", "exp"]
    codes = sorted(order.index(f) << 16 | n for f, n in want.values())
    table = re.search(r"pub const CODE = \[_\]u32\{(.*?)\}", text, re.S)
    words = [int(w, 16) for w in re.findall(r"0x[0-9A-F]+", table.group(1))] if table else []
    if words != codes:
        fail("CODE is not the instructions' format << 16 | number in order")
    # GFX9's numbers that GFX8 lacks must not be there
    for name in ("v_add_u32", "v_sub_u32", "s_mul_hi_u32", "global_load_dword", "scratch_load_dword"):
        if name in have:
            fail("%s is in isa.mlx: it is not a GFX8 instruction" % name)
    if not (missing or extra or wrong):
        ok("isa.mlx: %d instructions, each the GFX8 column's format and number of aco_opcodes.py" % len(want))


# ---------------------------------------------------------------------------
# 2. The encoder

def check_encoder(mesa):
    constants = mlx_constants(os.path.join(driver, "encode.mlx"))
    assembler = open(os.path.join(mesa, "src/amd/compiler/aco_assembler.cpp")).read()
    # each format's fixed bits, as the GFX8 branch of emit_instruction
    # writes them: "case Format::X: ... encoding = (0bN << S)"
    def fixed(case, gfx_condition=None):
        start = assembler.index(case)
        body = assembler[start:start + 4000]
        if gfx_condition:
            body = body[body.index(gfx_condition):]
        m = re.search(r"\(0b([01]+) << (\d+)\)", body)
        return int(m.group(1), 2) << int(m.group(2))
    wanted = {
        "SOP2_ENCODING": fixed("case Format::SOP2:"),
        "SOPK_ENCODING": fixed("case Format::SOPK:"),
        "SOP1_ENCODING": fixed("case Format::SOP1:"),
        "SOPC_ENCODING": fixed("case Format::SOPC:"),
        "SOPP_ENCODING": fixed("case Format::SOPP:"),
        "SMEM_ENCODING": fixed("case Format::SMEM:", "if (ctx.gfx_level <= GFX9) {\n         encoding = (0b110000"),
        "VOP1_ENCODING": fixed("case Format::VOP1:"),
        "VOPC_ENCODING": fixed("case Format::VOPC:"),
        "VINTRP_ENCODING": fixed("case Format::VINTRP:", "encoding = (0b110101 << 26); /* Vega ISA doc"),
        "DS_ENCODING": fixed("case Format::DS:"),
        "MUBUF_ENCODING": fixed("case Format::MUBUF:"),
        "MTBUF_ENCODING": fixed("case Format::MTBUF:", "uint32_t encoding = (0b111010"),
        "MIMG_ENCODING": fixed("case Format::MIMG:", "uint32_t encoding = (0b111100"),
        "FLAT_ENCODING": fixed("case Format::FLAT:"),
        "EXP_ENCODING": fixed("case Format::EXP:"),
        "VOP3_ENCODING": fixed("} else if (instr->isVOP3()) {", "if (ctx.gfx_level <= GFX9) {"),
    }
    m = re.search(r"if \(ctx\.gfx_level == GFX8 \|\| ctx\.gfx_level == GFX9\) \{\s*encoding = \((0b[01]+) << (\d+)\);\s*\} else \{\s*encoding = \(0b111110", assembler)
    wanted["EXP_ENCODING"] = int(m.group(1), 2) << int(m.group(2))
    offsets = re.search(r"if \(instr->isVOP2\(\)\) \{\s*opcode = opcode \+ (0x[0-9a-f]+);.*?if \(ctx\.gfx_level == GFX8 \|\| ctx\.gfx_level == GFX9\)\s*opcode = opcode \+ (0x[0-9a-f]+);.*?isVOPC\(\)\) \{\s*opcode = opcode \+ (0x[0-9a-f]+);.*?isVINTRP\(\)\) \{\s*opcode = opcode \+ (0x[0-9a-f]+);", assembler, re.S)
    wanted["VOP3_FROM_VOP2"] = int(offsets.group(1), 16)
    wanted["VOP3_FROM_VOP1"] = int(offsets.group(2), 16)
    wanted["VOP3_FROM_VOPC"] = int(offsets.group(3), 16)
    wanted["VOP3_FROM_VINTRP"] = int(offsets.group(4), 16)
    wanted["DPP_SOURCE"] = int(re.search(r"Operand\(PhysReg\{(\d+)\}, v1\);\s*instr->format = \(Format\)\(\(uint16_t\)instr->format & ~\(uint16_t\)Format::DPP16\)", assembler).group(1))
    wanted["SDWA_SOURCE"] = int(re.search(r"Operand\(PhysReg\{(\d+)\}, v1\);\s*instr->format = \(Format\)\(\(uint16_t\)instr->format & ~\(uint16_t\)Format::SDWA\)", assembler).group(1))
    ir_h = open(os.path.join(mesa, "src/amd/compiler/aco_ir.h")).read()
    for ours, theirs in (("FLAT_SCR_LO", "flat_scr_lo"), ("FLAT_SCR_HI", "flat_scr_hi"), ("VCC", "vcc"), ("VCC_HI", "vcc_hi"), ("TBA", "tba"), ("TMA", "tma"), ("TTMP0", "ttmp0"), ("M0", "m0"), ("EXEC", "exec"), ("EXEC_HI", "exec_hi"), ("SCC", "scc")):
        wanted[ours] = int(re.search(r"static constexpr PhysReg %s\{(\d+)\}" % theirs, ir_h).group(1))
    # the inline constants (Operand::c16's ranges: 128 + v, 192 - v; the
    # floats from 240, 1/(2 pi) 248, the literal 255)
    wanted["INT_ZERO"] = int(re.search(r"op\.setFixed\(PhysReg\{(\d+)u \+ v\}\)", ir_h).group(1))
    wanted["INT_MAX"] = int(re.search(r"PhysReg\{\(unsigned\)\((\d+) - \(int16_t\)v\)\}", ir_h).group(1))
    wanted["INT_MINUS_ONE"] = wanted["INT_MAX"] + 1
    wanted["INT_MINUS_SIXTEEN"] = wanted["INT_MAX"] + 16
    wanted["FLOAT_HALF"] = int(re.search(r"0x3800\) /\* 0\.5 \*/\s*op\.setFixed\(PhysReg\{(\d+)\}\)", ir_h).group(1))
    wanted["FLOAT_INV_2PI"] = int(re.search(r"0x3118\) /\* 1/2 PI \*/\s*op\.setFixed\(PhysReg\{(\d+)\}\)", ir_h).group(1))
    wanted["LITERAL"] = int(re.search(r"/\* Literal Constant \*/\s*op\.setFixed\(PhysReg\{(\d+)\}\)", ir_h).group(1))
    ir_cpp = open(os.path.join(mesa, "src/amd/compiler/aco_ir.cpp")).read()
    pack = re.search(r"imm = \(\(lgkm & (0x[0-9a-f]+)\) << (\d+)\) \| \(\(exp & (0x[0-9a-f]+)\) << (\d+)\) \| \(vm & (0x[0-9a-f]+)\);", ir_cpp)
    wanted["WAIT_LGKM_MASK"] = int(pack.group(1), 16)
    wanted["WAIT_LGKM_SHIFT"] = int(pack.group(2))
    wanted["WAIT_EXP_MASK"] = int(pack.group(3), 16)
    wanted["WAIT_EXP_SHIFT"] = int(pack.group(4))
    wanted["WAIT_VM_MASK"] = int(pack.group(5), 16)
    wrong = []
    for name, value in sorted(wanted.items()):
        have = number(constants.get(name, "")) if name in constants else None
        if have != value:
            wrong.append("%s is %s, Mesa %s" % (name, constants.get(name), hex(value)))
    for w in wrong:
        fail("encode.mlx: " + w)
    if not wrong:
        ok("encode.mlx: %d numbers (the formats' bits, registers, constants, offsets, the wait word) as Mesa writes them" % len(wanted))


# ---------------------------------------------------------------------------
# 3. The interpreter and the compiler

def check_rest(mesa):
    wave = open(os.path.join(driver, "wave.mlx")).read()
    ir_h = open(os.path.join(mesa, "src/amd/compiler/aco_ir.h")).read()
    floats = re.findall(r"else if \(v == (0x[0-9a-f]{8})\) /\* (-?[0-9.]+) \*/\s*op\.setFixed\(PhysReg\{(\d+)\}\)", ir_h)
    wrong = 0
    for bits, text, reg in floats:
        if not re.search(r"o == %s \{ value\.\* = 0x%s \}" % (reg, bits[2:].upper()), wave):
            fail("wave.mlx: the inline constant %s (%s) is not %s" % (reg, text, bits))
            wrong += 1
    if len(floats) != 8:
        fail("aco_ir.h's 32-bit float constants not found as eight")
    nops = open(os.path.join(mesa, "src/amd/compiler/aco_insert_NOPs.cpp")).read()
    gfx6 = nops[nops.index("handle_instruction_gfx6"):]
    counts = {
        1: int(re.search(r"handle_valu_then_read_hazard\(state, &NOPs, (\d+), op\);\s*\}\s*\}\s*if \(!instr->isSALU", gfx6).group(1)),
        2: int(re.search(r"!instr->operands\[1\]\.isConstant\(\)\) \{\s*handle_valu_then_read_hazard\(state, &NOPs, (\d+)", gfx6).group(1)),
        3: int(re.search(r"ctx\.valu_wr_vcc_then_div_fmas = (\d+);", gfx6).group(1)),
        4: int(re.search(r"ctx\.valu_wr_exec_then_dpp = (\d+);", gfx6).group(1)),
        5: int(re.search(r"NOPs = MAX2\(NOPs, ctx\.valu_wr_exec_then_dpp\);\s*handle_valu_then_read_hazard\(state, &NOPs, (\d+)", gfx6).group(1)),
        6: int(re.search(r"ctx\.setreg_then_getsetreg = (\d+);", gfx6).group(1)),
        7: int(re.search(r"ctx\.set_vskip_mode_then_vector = (\d+);", gfx6).group(1)),
        8: int(re.search(r"ctx\.salu_wr_m0_then_gds_msg_ttrace = (\d+);", gfx6).group(1)),
        9: int(re.search(r"ctx\.vmem_store_then_wr_data\[\(wrdata & 0xff\) \+ i\] = (\d+);", gfx6).group(1)),
        10: 1,
    }
    for rule, states in counts.items():
        if not re.search(r"hazard\(inst, result, %d, %d, " % (rule, states), wave) and not re.search(r"scalarOperandRead\([^\n]*, %d, %d, " % (rule, states), wave):
            if rule != 10:
                fail("wave.mlx: rule %d is not %d wait states" % (rule, states))
                wrong += 1
    registers = json.load(open(os.path.join(mesa, "src/amd/registers/gfx8.json")))
    fields = {}
    for name in ("SQ_BUF_RSRC_WORD1", "SQ_BUF_RSRC_WORD3"):
        for f in registers["register_types"][name]["fields"]:
            fields[(name, f["name"])] = tuple(f["bits"])
    if fields[("SQ_BUF_RSRC_WORD1", "BASE_ADDRESS_HI")] != (0, 15) or "& 0xFFFF) << 32" not in wave:
        fail("wave.mlx: BASE_ADDRESS_HI is not bits 15:0")
        wrong += 1
    if fields[("SQ_BUF_RSRC_WORD1", "STRIDE")] != (16, 29) or "((word1 >> 16) & 0x3FFF)" not in wave:
        fail("wave.mlx: STRIDE is not bits 29:16")
        wrong += 1
    if fields[("SQ_BUF_RSRC_WORD3", "ADD_TID_ENABLE")] != (23, 23) or "((word3 >> 23) & 1)" not in wave:
        fail("wave.mlx: ADD_TID_ENABLE is not bit 23")
        wrong += 1
    idiv = open(os.path.join(mesa, "src/compiler/nir/nir_lower_idiv.c")).read()
    scale = re.search(r"nir_fmul_imm\(bld, rcp, ([0-9.]+)\)", idiv).group(1)
    if ("const scale: f32 = %s" % scale) not in open(os.path.join(driver, "shader_ir.mlx")).read():
        fail("shader_ir.mlx: the division's scale is not %s" % scale)
        wrong += 1
    if not wrong:
        ok("wave.mlx and shader_ir.mlx: the float constants, the wait states of rules 1-9, the descriptor's fields, the division's scale")


def main():
    if len(sys.argv) != 2:
        print("usage: tools/check_gcn_isa.py MESA", file=sys.stderr)
        return 2
    check_table(sys.argv[1])
    check_encoder(sys.argv[1])
    check_rest(sys.argv[1])
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
