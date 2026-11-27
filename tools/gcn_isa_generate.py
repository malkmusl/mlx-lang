#!/usr/bin/env python3
# Writes projects/brix/vulkan/isa.mlx, the instruction set of the Radeon
# RX 580's shader cores (GFX8, Polaris) as data of ours, from Mesa's own
# table of the instructions at a fixed tag:
#
#   tools/gcn_isa_generate.py ACO_OPCODES_PY [OUTPUT]
#
# ACO_OPCODES_PY is src/amd/compiler/aco_opcodes.py of Mesa mesa-24.0.0
# (https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-24.0.0/src/amd/
# compiler/aco_opcodes.py). The file is read as data: Python's ast module
# parses it and nothing of it runs. It is not kept in this repository.
#
# What is taken, table by table, is the GFX8 column of the file, never
# ACO's merged column (ACO passes the GFX9 number for GFX8 and GFX9 alike,
# opcode()'s second argument after gfx7): a row whose GFX8 column is -1 is
# not an instruction of this card. Where a table has no column of its own
# for GFX8 the comment the file writes above its loop says which column
# stands for it (VINTRP, IMAGE, IMAGE_SAMPLE, IMAGE_GATHER4: "code";
# IMAGE_ATOMIC: "gfx89"; FLAT: its second column). The GLOBAL and
# SCRATCH tables ("GFX89") are left out: ACO emits them only from GFX9 on
# (the second reading of the driver's study, correction 6), so their 70
# rows are counted and said, never written. Tables of later generations
# only (VOPP, VINTERP, LDSDIR, IMAGE_SAMPLE_G16) and the single opcode()
# calls outside the tables (which name no GFX8 number) are left out too.
# The comparisons, which the file makes in loops (v_cmp_<condition>_<type>),
# are evaluated with the loop's own variable over its own range.
#
# tools/check_gcn_isa.py reads the same file by its own means and holds
# what this wrote against it.
import ast
import hashlib
import os
import sys

TAG = "mesa-24.0.0"
SIX_COLUMNS = ["SOP2", "SOPK", "SOP1", "SOPC", "SOPP", "SMEM", "VOP2", "VOP1",
               "VOPC_CLASS", "VOP3", "DS", "MUBUF", "MTBUF"]
LATER_ONLY = ["VOPP", "VINTERP", "LDSDIR", "IMAGE_SAMPLE_G16"]
LEFT_OUT = ["GLOBAL", "SCRATCH"]
# The table each format's rows come from, and the format's name in ours.
FORMAT_OF = {
    "SOP2": "sop2", "SOPK": "sopk", "SOP1": "sop1", "SOPC": "sopc",
    "SOPP": "sopp", "SMEM": "smem", "VOP2": "vop2", "VOP1": "vop1",
    "VOPC_CLASS": "vopc", "VOP3": "vop3", "DS": "ds", "MUBUF": "mubuf",
    "MTBUF": "mtbuf", "VINTRP": "vintrp", "IMAGE": "mimg",
    "IMAGE_ATOMIC": "mimg", "IMAGE_SAMPLE": "mimg", "IMAGE_GATHER4": "mimg",
    "FLAT": "flat",
}
FORMATS = ["sop2", "sopk", "sop1", "sopc", "sopp", "smem", "vop2", "vop1",
           "vopc", "vop3", "vintrp", "ds", "mubuf", "mtbuf", "mimg", "flat",
           "exp"]


class Reader:
    """Evaluates the few kinds of expressions the table is written in."""

    def __init__(self, tree):
        self.names = {}
        for node in tree.body:
            if (isinstance(node, ast.Assign) and len(node.targets) == 1
                    and isinstance(node.targets[0], ast.Name)
                    and isinstance(node.value, (ast.Constant, ast.List))):
                self.names[node.targets[0].id] = self.value(node.value, {})

    def value(self, node, local):
        if isinstance(node, ast.Constant):
            return node.value
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
            return -self.value(node.operand, local)
        if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Add):
            return self.value(node.left, local) + self.value(node.right, local)
        if isinstance(node, ast.Name):
            if node.id in local:
                return local[node.id]
            if node.id in self.names:
                return self.names[node.id]
            raise ValueError("unknown name %s at line %d" % (node.id, node.lineno))
        if isinstance(node, ast.List):
            return [self.value(e, local) for e in node.elts]
        if isinstance(node, ast.Tuple):
            return tuple(self.value(e, local) for e in node.elts)
        if isinstance(node, ast.Subscript):
            return self.value(node.value, local)[self.value(node.slice, local)]
        if isinstance(node, ast.Attribute):
            # InstrClass.X and Format.X: kept as their names.
            return node.attr
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id in ("dst", "src"):
            # The file's own dst() and src(): four bytes, the first operand lowest.
            parts = [self.value(a, local) for a in node.args]
            word = 0
            for at, part in enumerate(parts):
                word |= part << (8 * at)
            return word
        raise ValueError("unknown expression at line %d" % node.lineno)


def rows(path):
    """(table, name, gfx8, defs, ops) for every row of every table."""
    source = open(path, "rb").read()
    tree = ast.parse(source, path)
    reader = Reader(tree)
    found = []
    tables = {}
    for node in tree.body:
        if (isinstance(node, ast.Assign) and len(node.targets) == 1
                and isinstance(node.targets[0], ast.Name)
                and isinstance(node.value, ast.Set)):
            tables[node.targets[0].id] = [reader.value(e, {}) for e in node.value.elts]
    for table, entries in tables.items():
        for entry in entries:
            if table in SIX_COLUMNS:
                gfx8, name = entry[2], entry[6]
                rest = [e for e in entry[7:] if isinstance(e, int) and not isinstance(e, bool)]
                defs = rest[0] if len(rest) >= 2 else None
                ops = rest[1] if len(rest) >= 2 else None
            elif table == "VINTRP":
                gfx8, name, defs, ops = entry[0], entry[1], entry[2], entry[3]
            elif table in ("IMAGE", "IMAGE_SAMPLE", "IMAGE_GATHER4"):
                gfx8, name, defs, ops = entry[0], entry[2], None, None
            elif table == "IMAGE_ATOMIC":
                gfx8, name, defs, ops = entry[2], entry[4], None, None
            elif table == "FLAT":
                gfx8, name, defs, ops = entry[1], entry[4], None, None
            elif table in LEFT_OUT:
                gfx8, name, defs, ops = entry[0], entry[3], None, None
            elif table in LATER_ONLY:
                continue
            else:
                raise ValueError("a table this script does not know: " + table)
            found.append((table, name, gfx8, defs, ops))
    # The comparisons: loops whose bodies assign a tuple of seven and call
    # opcode() with it.
    for node in tree.body:
        if not isinstance(node, ast.For):
            continue
        if not (isinstance(node.target, ast.Name) and isinstance(node.iter, (ast.Call, ast.List))):
            continue
        variable = node.target.id
        if isinstance(node.iter, ast.Call):
            if not (isinstance(node.iter.func, ast.Name) and node.iter.func.id == "range"):
                continue
            values = list(range(*[reader.value(a, {}) for a in node.iter.args]))
        else:
            values = reader.value(node.iter, {})
        for value in values:
            local = {variable: value}
            pending = None
            for statement in node.body:
                if isinstance(statement, ast.Assign) and isinstance(statement.targets[0], ast.Tuple):
                    pending = reader.value(statement.value, local)
                elif (isinstance(statement, ast.Expr) and isinstance(statement.value, ast.Call)
                      and isinstance(statement.value.func, ast.Name)
                      and statement.value.func.id == "opcode" and pending is not None):
                    keywords = {k.arg: k.value for k in statement.value.keywords}
                    defs = reader.value(keywords["definitions"], local) if "definitions" in keywords else 0
                    ops = reader.value(keywords["operands"], local) if "operands" in keywords else 0
                    form = reader.value(statement.value.args[5], local)
                    if form != "VOPC":
                        raise ValueError("a loop of another format at line %d" % node.lineno)
                    found.append(("VOPC", pending[6], pending[2], defs, ops))
                    pending = None
    return source, found


def generate(path):
    source, found = rows(path)
    digest = hashlib.sha256(source).hexdigest()
    kept = []
    left = {}
    vopc_class = sum(1 for f in found if f[0] == "VOPC_CLASS" and f[2] != -1)
    # FLAT's column is the file's shared one for GFX8 and GFX9 ("GFX89"):
    # a FLAT row whose buffer namesake has no GFX8 number in MUBUF's own
    # GFX8 column (the d16 loads and stores) is GFX9's only.
    buffer_gfx8 = dict((f[1], f[2]) for f in found if f[0] == "MUBUF")
    for table, name, gfx8, defs, ops in found:
        if table in LEFT_OUT:
            if gfx8 != -1:
                left[table] = left.get(table, 0) + 1
            continue
        if gfx8 == -1:
            continue
        if table == "FLAT" and buffer_gfx8.get("buffer_" + name[len("flat_"):]) == -1:
            left["FLAT"] = left.get("FLAT", 0) + 1
            continue
        form = "vopc" if table == "VOPC" else FORMAT_OF[table]
        kept.append((FORMATS.index(form), gfx8, name, form, defs, ops))
    kept.sort()
    names = [k[2] for k in kept]
    if len(set(names)) != len(names):
        raise ValueError("a name twice")
    counts = {}
    for k in kept:
        counts[k[3]] = counts.get(k[3], 0) + 1
    out = []
    w = out.append
    w("// The instruction set of the Radeon RX 580's shader cores (GFX8,")
    w("// Polaris): every instruction this generation has, by format, with its")
    w("// number. GENERATED by tools/gcn_isa_generate.py from Mesa %s's" % TAG)
    w("// src/amd/compiler/aco_opcodes.py, its GFX8 column only (do not edit:")
    w("// generate it again); tools/check_gcn_isa.py holds it against the same")
    w("// file. The file's sha256:")
    w("//   %s" % digest)
    w("//")
    looped = sum(1 for k in kept if k[3] == "vopc") - vopc_class
    w("// %d instructions: the %d rows of the file's tables and the %d comparisons"
      % (len(kept), len(kept) - looped, looped))
    w("// its loops make (v_cmp_<condition>_<type>), by format:")
    line = "//"
    for form in FORMATS:
        if form in counts:
            part = " %s %d" % (form, counts[form])
            if len(line) + len(part) > 76:
                w(line)
                line = "//"
            line += part
    w(line)
    w("// Left out: the GLOBAL and SCRATCH forms (%d and %d rows headed \"GFX89\" in"
      % (left.get("GLOBAL", 0), left.get("SCRATCH", 0)))
    w("// the file), which the reference emits only from GFX9 on; %d FLAT rows" % left.get("FLAT", 0))
    w("// (FLAT's column is the file's shared \"GFX89\" one) whose buffer namesake")
    w("// MUBUF's GFX8 column does not have (the d16 forms); every row whose")
    w("// GFX8 column is -1. A comparison's number is the file's loop's, worked")
    w("// out for each condition and type. EXP has no opcode: its format is here.")
    w("// A VOP1, VOP2 or VOPC instruction keeps its own number in the VOP3 form;")
    w("// encode.mlx adds the offset.")
    w("")
    w("pub const Format = enum(u8) {")
    for form in FORMATS:
        w("    %s," % form)
    w("}")
    w("")
    w("// An instruction: its format and its number in that format.")
    w("pub const Op = struct {")
    w("    format: Format")
    w("    number: u16")
    w("}")
    w("")
    w("pub const COUNT: usize = %d" % len(kept))
    w("")
    for _, gfx8, name, form, _, _ in kept:
        w("pub const %s = Op.{ .format = Format.%s, .number = 0x%03X }" % (name.upper(), form, gfx8))
    w("")
    # The names, one string; each entry of CODE is format << 16 | number,
    # of NAME_AT offset << 8 | length, of SHAPE the file's dst() << 32 | src().
    blob = []
    at = 0
    name_at = []
    for _, _, name, _, _, _ in kept:
        name_at.append((at << 8) | len(name))
        blob.append(name)
        at += len(name)
    w("// The names one after the other; NAME_AT[i] is offset << 8 | length.")
    text = "".join(blob)
    w("pub const NAMES = \"%s\"" % text)
    w("")
    w("// CODE[i] is format << 16 | number, in the order of the formats and the")
    w("// numbers.")
    w("pub const CODE = [_]u32{")
    for i in range(0, len(kept), 8):
        w("    " + ", ".join("0x%06X" % (FORMATS.index(k[3]) << 16 | k[1]) for k in kept[i:i + 8]) + ",")
    w("}")
    w("")
    w("pub const NAME_AT = [_]u32{")
    for i in range(0, len(kept), 8):
        w("    " + ", ".join("0x%06X" % v for v in name_at[i:i + 8]) + ",")
    w("}")
    w("")
    w("// What the file says an instruction writes and reads, where it says it")
    w("// (its dst() and src(): a byte an operand, the first lowest: a count of")
    w("// 32-bit registers, or the number of the register it is: 106 vcc,")
    w("// 124 m0, 127 exec, 253 scc); 0xFFFFFFFF where the file says nothing.")
    w("pub const DEFS = [_]u32{")
    for i in range(0, len(kept), 8):
        w("    " + ", ".join("0x%08X" % (0xFFFFFFFF if k[4] is None else k[4]) for k in kept[i:i + 8]) + ",")
    w("}")
    w("")
    w("pub const OPS = [_]u32{")
    for i in range(0, len(kept), 8):
        w("    " + ", ".join("0x%08X" % (0xFFFFFFFF if k[5] is None else k[5]) for k in kept[i:i + 8]) + ",")
    w("}")
    return "\n".join(out) + "\n", len(kept), left


def main():
    if len(sys.argv) < 2:
        print(__doc__ or "usage: gcn_isa_generate.py ACO_OPCODES_PY [OUTPUT]", file=sys.stderr)
        return 2
    text, count, left = generate(sys.argv[1])
    output = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "projects", "brix", "vulkan", "isa.mlx")
    with open(output, "w") as handle:
        handle.write(text)
    print("%d instructions written; left out: %s" % (count, ", ".join("%s %d" % kv for kv in sorted(left.items()))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
