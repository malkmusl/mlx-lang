#!/usr/bin/env python3
# The tables tools/check_atom_reference.sh feeds to the kernel's
# interpreter for a Radeon's firmware tables and to Linux's, and the
# holding of what the two wrote down against each other. Called by that
# script (which says how to use it); a development comparison.
#
# The tables are made from a seed, so that the same numbers give the same
# tables again. A ROM image holds some thirty command tables on four
# levels (a table calls only tables of a deeper level), data tables of
# chance bytes and indirect register programs. A table is statements in
# sequence, each of them one of:
# - an operation of every family (MOVE, AND, OR, XOR, ADD, SUB, MUL, DIV,
#   COMPARE, TEST, SHL, SHR, MASK, the two shifts by a count in the code,
#   CLEAR, MUL32, DIV32) on every kind of destination (a register, the
#   parameter space, the work space with its nine special words, the
#   scratch, the PLL, the memory controller), from every kind of source
#   (those, the data block, a number in the code), every part of the
#   source's word (the long, three words, four bytes) into each of the
#   destination's parts;
# - a condition (a COMPARE or a TEST, or the flags as they stand) and one
#   of the seven jumps over statements; a switch of two to five cases; a
#   loop that counts down in a work space word; a call (also of a table
#   that is not there);
# - what belongs together and chance seldom puts together (`together`);
# - the register block, the IO port (the registers themselves, the ROM's
#   indirect programs or none, PCI, an IO port), the data block (none,
#   the table, a data table), the scratch's window (inside the scratch or
#   well beyond it), waits, and the opcodes that do nothing or pass over
#   bytes.
# Left out of these, because Linux reads outside what it was given there
# and what it then does is chance: a work space word beyond the table's
# own, a window that puts the scratch's end inside reach, a table or data
# block beyond the lists, a port with a program to write and none to read
# (see `programs`). Every fifth image holds instead tables that end
# in one such thing, or in an opcode that is none, a switch that is not
# one, an indirect program with a step that is none, calls nested twenty
# deep or a loop without end: there the kernel's interpreter must stop
# and say so, having done exactly what Linux did up to that place.
import os, random, re, struct, subprocess, sys

SIZE = 0x10400
STRINGS, PCI_DATA, HEADER, COMMANDS, DATA, FIRST = 0x100, 0x1C0, 0x200, 0x240, 0x300, 0x400
COMMAND_COUNT, DATA_COUNT = 81, 35
SCRATCH_BYTES = 5120 * 4

REG, PS, WS, FB, ID, IMM, PLL, MC = range(8)
DEST = [REG, PS, WS, FB, PLL, MC]
KIND_NAMES = ["REG", "PS", "WS", "FB", "ID", "IMM", "PLL", "MC"]
PART_NAMES = ["31:0", "15:0", "23:8", "31:16", "7:0", "15:8", "23:16", "31:24"]
# The first opcode of each family of six (one for each destination).
SIX = {"move": 1, "and": 7, "or": 13, "shift_left": 19, "shift_right": 25, "mul": 31, "div": 37, "add": 43, "sub": 49,
       "compare": 60, "test": 74, "clear": 84, "mask": 92, "xor": 103, "shl": 109, "shr": 115}
SETPORT, SETREGBLOCK, SETFBBASE, SWITCH, JUMP = 55, 58, 59, 66, 67
DELAY, CALLTABLE, REPEAT, NOP, EOT = 80, 82, 83, 90, 91
POSTCARD, BEEP, SAVEREG, RESTOREREG, SETDATABLOCK, DEBUG, PROCESSDS, MUL32, DIV32 = 98, 99, 100, 101, 102, 121, 122, 123, 125
JUMP_NOTEQUAL = 73
PART_BYTES = [4, 2, 2, 2, 1, 1, 1, 1]
REGISTER_POOL = [0, 1, 2, 0x38, 0x39, 0x100, 0x140, 0x141, 0x143, 0x1B80, 0x1B9C, 0x4A00, 0x4E47, 0x4E48, 0xFFFF, 0x8000]


class Rom:
    # A ROM image laid out as tests/support/atom_rom.mlx lays one out,
    # and what is not laid out is chance.
    def __init__(self, rng):
        self.b = bytearray(rng.randbytes(SIZE))
        self.b[0:FIRST] = bytes(FIRST)
        self.at = FIRST
        b = self.b
        struct.pack_into("<H", b, 0, 0xAA55)
        struct.pack_into("<H", b, 0x18, PCI_DATA)
        b[PCI_DATA:PCI_DATA + 4] = b"PCIR"
        struct.pack_into("<HH", b, PCI_DATA + 4, 0x1002, 0x67DF)
        b[PCI_DATA + 21] = 0x80
        b[0x30:0x3A] = b" 761295520"
        struct.pack_into("<H", b, 0x48, HEADER)
        b[0x50:0x5E] = b"11/11/26 12:34"
        b[0x2F] = 2
        struct.pack_into("<H", b, 0x6E, STRINGS)
        b[STRINGS:STRINGS + 12] = b"113-TEST-001"
        b[STRINGS + 13:STRINGS + 22] = b"POLARIS10"
        b[STRINGS + 23:STRINGS + 25] = b"\r\n"
        b[STRINGS + 25:STRINGS + 44] = b"GENERATED FOR TESTS"
        struct.pack_into("<HBB", b, HEADER, 0x24, 1, 1)
        b[HEADER + 4:HEADER + 8] = b"ATOM"
        struct.pack_into("<HH", b, HEADER + 0x1E, COMMANDS, DATA)
        struct.pack_into("<HBB", b, COMMANDS, 4 + 2 * COMMAND_COUNT, 1, 1)
        struct.pack_into("<HBB", b, DATA, 4 + 2 * DATA_COUNT, 1, 1)

    def command(self, index, work, parameters, code):
        start = self.at
        struct.pack_into("<H", self.b, COMMANDS + 4 + 2 * index, start)
        body = struct.pack("<HBBBB", len(code) + 6, 1, 1, work, parameters) + bytes(code)
        self.b[start:start + len(body)] = body
        self.at += len(body)
        return start

    def data(self, index, content):
        start = self.at
        struct.pack_into("<H", self.b, DATA + 4 + 2 * index, start)
        body = struct.pack("<HBB", len(content) + 4, 1, 1) + bytes(content)
        self.b[start:start + len(body)] = body
        self.at += len(body)
        return start

    def image(self):
        assert self.at < 0xE000, "the tables do not fit"
        b = self.b
        b[2] = SIZE // 512
        struct.pack_into("<H", b, PCI_DATA + 16, SIZE // 512)
        b[0x21] = 0
        b[0x21] = (256 - sum(b) % 256) % 256
        return bytes(b)


class Code:
    # A table's code as pieces, with places that are known only when the
    # pieces are laid out (jump targets, counted from the table's start).
    def __init__(self):
        self.pieces = []  # (bytes, [(where in the piece, label)], what it is)
        self.labels = {}
        self.count = 0

    def label(self):
        self.count += 1
        return self.count

    def here(self, label):
        self.labels[label] = len(self.pieces)

    def emit(self, data, fixes=(), what=None):
        self.pieces.append((bytearray(data), list(fixes), what))

    def lay(self, first=6):
        # (`first`: where in the table the first piece lies.)
        offsets, at = [], first
        for data, fixes, what in self.pieces:
            offsets.append(at)
            at += len(data)
        offsets.append(at)
        out, whats = bytearray(), {}
        for n, (data, fixes, what) in enumerate(self.pieces):
            for where, label in fixes:
                struct.pack_into("<H", data, where, offsets[self.labels[label]])
            if what is not None:
                whats[offsets[n]] = what
            out += data
        return out, whats


class Maker:
    def __init__(self, rng, level, work, callable_tables, absent_tables, data_tables):
        self.rng = rng
        self.level = level
        self.work = work
        self.callable = callable_tables
        self.absent = absent_tables
        self.data_tables = data_tables
        self.code = Code()
        self.counters = []  # work space words that count loops now
        self.early_end = True
        self.any_port = True
        self.free_counters = list(range(max(0, work - 2), work))

    # --- operands
    def number(self, size):
        r = self.rng
        pick = r.random()
        top = (1 << (8 * size)) - 1
        if pick < 0.15:
            value = 0
        elif pick < 0.45:
            value = r.randrange(0, 40)
        elif pick < 0.55:
            value = top
        elif pick < 0.65:
            value = 1 << r.randrange(0, 8 * size)
        else:
            value = r.randrange(0, top + 1)
        return value.to_bytes(size, "little")

    def register(self):
        r = self.rng
        return struct.pack("<H", r.choice(REGISTER_POOL) if r.random() < 0.9 else r.randrange(0x10000))

    def small(self):
        r = self.rng
        return bytes([r.randrange(0, 12) if r.random() < 0.85 else r.randrange(256)])

    def work_word(self, written):
        # A work space word: the table's own (not one that counts a loop
        # when it is written) or a special one (the window only read).
        r = self.rng
        own = [w for w in range(self.work) if (w < 0x40 or w > 0x48) and not (written and w in self.counters)]
        special = [0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x47, 0x48] + ([] if written else [0x46])
        if own and r.random() < 0.7:
            return bytes([r.choice(own)])
        return bytes([r.choice(special)])

    def destination(self, kind):
        if kind == REG:
            return self.register()
        if kind == PS:
            return self.small()
        if kind == WS:
            return self.work_word(True)
        return self.small()

    def source(self, kind, part):
        if kind == REG:
            return self.register()
        if kind == ID:
            return struct.pack("<H", self.rng.randrange(0, 0x200))
        if kind == IMM:
            return self.number(PART_BYTES[part])
        if kind == WS:
            return self.work_word(False)
        return self.small()

    def window(self, size):
        # A place for the scratch's window: inside (every index in reach
        # stays inside) or well beyond the end.
        r = self.rng
        inside = r.random() < 0.8
        top = (1 << (8 * size)) - 1
        if inside or top < SCRATCH_BYTES + 8:
            return r.randrange(0, min(top, SCRATCH_BYTES - 1028) + 1)
        return r.randrange(SCRATCH_BYTES + 8, min(top, SCRATCH_BYTES + 0x10000) + 1)

    # --- statements
    def operation(self):
        r = self.rng
        family = r.choice(list(SIX) + ["move", "move", "mask", "mul32", "div32"])
        if family in ("mul32", "div32"):
            which = r.randrange(2)
            op = (MUL32 if family == "mul32" else DIV32) + which
            kind = PS if which == 0 else WS
        else:
            which = r.randrange(6)
            op = SIX[family] + which
            kind = DEST[which]
        source, part, select = r.randrange(8), r.randrange(8), r.randrange(4)
        attribute = source | (part << 3) | (select << 6)
        what = (op, source, part, select, kind)
        if family in ("shift_left", "shift_right"):
            count = r.choice([0, 1, 4, 8, 15, 16, 31, 32, 33, r.randrange(256)])
            self.code.emit(bytes([op, attribute]) + self.destination(kind) + bytes([count]), what=(op, None, part, None, kind))
        elif family == "clear":
            self.code.emit(bytes([op, attribute]) + self.destination(kind), what=(op, None, part, None, kind))
        elif family == "mask":
            self.code.emit(bytes([op, attribute]) + self.destination(kind) + self.number(PART_BYTES[part]) + self.source(source, part), what=what)
        else:
            self.code.emit(bytes([op, attribute]) + self.destination(kind) + self.source(source, part), what=what)

    def setting(self):
        r = self.rng
        pick = r.randrange(12)
        if pick == 0:
            port = r.choice([0, 0, 0, 1, 2, 3, 4, 5, 0x81, 0x102, 0x8003, r.randrange(0x10000)] if self.any_port else [0, 0, 1, 2, 3, 5])
            self.code.emit(bytes([SETPORT]) + struct.pack("<H", port), what=(SETPORT,))
        elif pick == 1:
            op = SETPORT + r.randrange(1, 3)
            self.code.emit(bytes([op, r.randrange(256)]), what=(op,))
            # (Not for long: most of what follows should reach the card.)
            if r.random() < 0.7:
                self.operation()
                self.code.emit(bytes([SETPORT, 0, 0]), what=(SETPORT,))
        elif pick == 2:
            block = r.choice([0, 0, 0, 0x100, 0x1000, 0xFFFF, r.randrange(0x10000)])
            self.code.emit(bytes([SETREGBLOCK]) + struct.pack("<H", block), what=(SETREGBLOCK,))
        elif pick == 3:
            part = r.randrange(8)
            size = PART_BYTES[part]
            self.code.emit(bytes([SETFBBASE, IMM | (part << 3) | (r.randrange(4) << 6)]) + self.window(size).to_bytes(size, "little"), what=(SETFBBASE,))
        elif pick == 4:
            # (The same through the work space's word for it.)
            self.code.emit(bytes([SIX["move"] + 2, IMM, 0x46]) + self.window(4).to_bytes(4, "little"), what=(SIX["move"] + 2, IMM, 0, 0, WS))
        elif pick == 5:
            block = r.choice([0, 255] + self.data_tables + [r.randrange(1, DATA_COUNT)])
            self.code.emit(bytes([SETDATABLOCK, block]), what=(SETDATABLOCK,))
        elif pick == 6:
            op = DELAY + r.randrange(2)
            self.code.emit(bytes([op, r.randrange(256)]), what=(op,))
        elif pick == 7:
            op = r.choice([NOP, BEEP, REPEAT, SAVEREG, RESTOREREG])
            self.code.emit(bytes([op]), what=(op,))
        elif pick == 8:
            op = r.choice([POSTCARD, DEBUG])
            self.code.emit(bytes([op, r.randrange(256)]), what=(op,))
        elif pick == 9:
            data = r.randbytes(r.randrange(0, 9))
            self.code.emit(bytes([PROCESSDS]) + struct.pack("<H", len(data)) + data, what=(PROCESSDS,))
        else:
            tables = self.callable if self.callable and r.random() < 0.85 else self.absent
            if tables:
                self.code.emit(bytes([CALLTABLE, r.choice(tables)]), what=(CALLTABLE,))

    def together(self):
        # Statements that belong together, which chance seldom puts
        # together: a product of 64 bits divided again and both halves of
        # the outcome kept; a division and its quotient and remainder; a
        # register written and read through a port's programs.
        r = self.rng
        move_ps = SIX["move"] + 1
        pick = r.randrange(3)
        if pick == 0:
            which = r.randrange(2)
            kind = PS if which == 0 else WS
            self.code.emit(bytes([MUL32 + which, IMM]) + self.destination(kind) + self.number(4), what=(MUL32 + which, IMM, 0, 0, kind))
            self.code.emit(bytes([DIV32 + which, IMM]) + self.destination(kind) + self.number(4), what=(DIV32 + which, IMM, 0, 0, kind))
        elif pick == 1:
            which = r.randrange(6)
            source, part, select = r.randrange(8), r.randrange(8), r.randrange(4)
            self.code.emit(bytes([SIX["div"] + which, source | (part << 3) | (select << 6)]) + self.destination(DEST[which]) + self.source(source, part),
                           what=(SIX["div"] + which, source, part, select, DEST[which]))
        else:
            source, part, select = r.randrange(8), r.randrange(8), r.randrange(4)
            self.code.emit(bytes([SETPORT]) + struct.pack("<H", r.choice([1, 2, 3, 5])), what=(SETPORT,))
            self.code.emit(bytes([SIX["move"], source | (part << 3) | (select << 6)]) + self.register() + self.source(source, part), what=(SIX["move"], source, part, select, REG))
            part, select = r.randrange(8), r.randrange(4)
            self.code.emit(bytes([move_ps, REG | (part << 3) | (select << 6)]) + self.small() + self.register(), what=(move_ps, REG, part, select, PS))
            self.code.emit(bytes([SETPORT, 0, 0]), what=(SETPORT,))
            return
        self.code.emit(bytes([move_ps, WS]) + self.small() + bytes([0x40]), what=(move_ps, WS, 0, 0, PS))
        self.code.emit(bytes([move_ps, WS]) + self.small() + bytes([0x41]), what=(move_ps, WS, 0, 0, PS))

    def condition(self, depth):
        # Perhaps a COMPARE or a TEST first, then a jump over statements.
        r = self.rng
        if r.random() < 0.8:
            family = r.choice(["compare", "test"])
            which = r.randrange(6)
            source, part, select = r.randrange(8), r.randrange(8), r.randrange(4)
            op = SIX[family] + which
            self.code.emit(bytes([op, source | (part << 3) | (select << 6)]) + self.destination(DEST[which]) + self.source(source, part),
                           what=(op, source, part, select, DEST[which]))
        op = JUMP + r.randrange(7)
        end = self.code.label()
        self.code.emit(bytes([op, 0, 0]), [(1, end)], what=(op,))
        self.sequence(r.randrange(1, 4), depth + 1)
        if self.early_end and r.random() < 0.05:
            self.code.emit(bytes([EOT]), what=(EOT,))
        self.code.here(end)

    def switch(self, depth):
        r = self.rng
        source, part = r.randrange(8), r.randrange(8)
        size = PART_BYTES[part]
        cases = r.randrange(2, 6)
        labels = [self.code.label() for _ in range(cases)]
        end = self.code.label()
        data = bytearray([SWITCH, source | (part << 3) | (r.randrange(4) << 6)]) + self.source(source, part)
        fixes = []
        for n in range(cases):
            data += bytes([0x63]) + self.number(size)
            fixes.append((len(data), labels[n]))
            data += b"\0\0"
        data += b"\x5a\x5a"
        self.code.emit(data, fixes, what=(SWITCH, source, part, None, None))
        self.sequence(r.randrange(0, 3), depth + 1)
        self.code.emit(bytes([JUMP, 0, 0]), [(1, end)], what=(JUMP,))
        for n in range(cases):
            self.code.here(labels[n])
            self.sequence(r.randrange(0, 3), depth + 1)
            if n + 1 < cases and r.random() < 0.7:
                self.code.emit(bytes([JUMP, 0, 0]), [(1, end)], what=(JUMP,))
        self.code.here(end)

    def loop(self, depth):
        r = self.rng
        counter = self.free_counters.pop()
        self.counters.append(counter)
        top = self.code.label()
        self.code.emit(bytes([SIX["move"] + 2, IMM, counter]) + struct.pack("<I", r.randrange(1, 5)), what=(SIX["move"] + 2, IMM, 0, 0, WS))
        self.code.here(top)
        self.sequence(r.randrange(1, 4), depth + 1)
        self.code.emit(bytes([SIX["sub"] + 2, IMM, counter, 1, 0, 0, 0]), what=(SIX["sub"] + 2, IMM, 0, 0, WS))
        self.code.emit(bytes([SIX["compare"] + 2, IMM, counter, 0, 0, 0, 0]), what=(SIX["compare"] + 2, IMM, 0, 0, WS))
        self.code.emit(bytes([JUMP_NOTEQUAL, 0, 0]), [(1, top)], what=(JUMP_NOTEQUAL,))
        self.counters.pop()
        self.free_counters.append(counter)

    def sequence(self, count, depth):
        r = self.rng
        for _ in range(count):
            pick = r.random()
            if pick < 0.55 or depth >= 3:
                self.operation()
            elif pick < 0.62:
                self.together()
            elif pick < 0.80:
                self.setting()
            elif pick < 0.90:
                self.condition(depth)
            elif pick < 0.95:
                self.switch(depth)
            elif self.free_counters and self.level < 3:
                self.loop(depth)
            else:
                self.operation()


def shaped(rng, number):
    # A program of the shape a ROM's have: the register's number (and
    # perhaps the IO attributes) into an index register, then the word
    # read from the data register or, for a program that writes, the data
    # into it.
    index, data = rng.choice([(0x38, 0x39), (0x100, 0x141), (0x1B80, 0x1B9C)])
    out = bytearray([1, number])
    if rng.random() < 0.7:
        out += bytes([4, 32, 0])
    out += bytes([6, rng.choice([8, 16, 16, 32, 20]), rng.choice([0, 0, 2]), rng.choice([0, 0, 2, 8])])
    if rng.random() < 0.3:
        out += bytes([7, rng.choice([4, 8]), 0, rng.choice([16, 24])])
    out += bytes([3]) + struct.pack("<H", index)
    if number & 0x80:
        out += bytes([8, rng.choice([32, 32, 16, 8]), rng.choice([0, 0, 8]), rng.choice([0, 0, 8, 16])])
        if rng.random() < 0.3:
            out += bytes([5, 1, rng.randrange(32)])
        out += bytes([3]) + struct.pack("<H", data)
    else:
        out += bytes([2]) + struct.pack("<H", data)
    return out + bytes([9, 0, 0])


def programs(rng):
    # The indirect register programs: a port has none, one to read, or one
    # to read and one to write (its number with bit 7 set). Never one to
    # write alone: Linux then puts what a variable it did not set happens
    # to hold into the bits a table leaves alone (atom_op_move's `saved`
    # after "Undefined indirect IO read method"), which is chance; the
    # kernel's interpreter puts 0 there.
    out = bytearray()
    numbers = []
    for port in [1, 2, 3, 5]:
        pick = rng.random()
        if pick < 0.75:
            numbers.append(port)
        if pick < 0.55:
            numbers.append(0x80 | port)
    for number in numbers:
        if number & 0x7F < 3 and rng.random() < 0.8:
            out += shaped(rng, number)
            continue
        out += bytes([1, number])
        for _ in range(rng.randrange(2, 9)):
            step = rng.choice([0, 2, 2, 3, 3, 4, 5, 6, 6, 7, 8, 8])
            width = rng.choice([1, 4, 8, 16, 32, 32, rng.randrange(0, 41), 255])
            first = rng.choice([0, 0, 4, 8, 16, 24, 31, rng.randrange(0, 64), 255])
            second = rng.choice([0, 0, 4, 8, 16, 24, 31, rng.randrange(0, 64)])
            if step == 0:
                out += bytes([0])
            elif step in (2, 3):
                out += bytes([step]) + struct.pack("<H", rng.choice(REGISTER_POOL))
            elif step in (4, 5):
                out += bytes([step, width, first])
            else:
                out += bytes([step, width, first, second])
        out += bytes([9, rng.randrange(256), rng.randrange(256)])
    out += bytes([0])
    return out


def run_line(table, seed, fresh, steps, words):
    return "%d %d %d %d %d %s\n" % (table, seed, fresh, steps, len(words), " ".join(str(w) for w in words))


def agreeing_image(rng):
    # Tables Linux runs without reading outside what it has: every line
    # of the two must be the same. (image, runs, what each run expects,
    # what lies at each offset.)
    rom = Rom(rng)
    data_tables = rng.sample([n for n in range(1, DATA_COUNT) if n != 23], 5)
    for index in data_tables:
        rom.data(index, rng.randbytes(rng.randrange(8, 64)))
    rom.data(23, programs(rng))
    indices = rng.sample(range(COMMAND_COUNT), 34)
    absent = indices[30:]
    levels = [indices[0:10], indices[10:18], indices[18:25], indices[25:30]]
    whats = {}
    for level in (3, 2, 1, 0):
        deeper = [t for l in range(level + 1, 4) for t in levels[l]]
        for index in levels[level]:
            work = rng.choice([0, 1, 2, 4, 8, 16, 64, 255])
            maker = Maker(rng, level, work, deeper, absent, data_tables)
            maker.sequence(rng.randrange(3, 12 if level < 2 else 7), 0)
            maker.code.emit(bytes([EOT]), what=(EOT,))
            code, what = maker.code.lay()
            # (The parameter space's size has seven bits; the eighth is a flag of the ROM's tools.)
            start = rom.command(index, work, rng.choice([0, 0, 4, 8, 12, 16, 0x7C, rng.randrange(128)]) | (0x80 if rng.random() < 0.2 else 0), code)
            for offset, value in what.items():
                whats[start + offset] = value
    runs, expect = [], []
    first = True
    for index in levels[0] * 4 + levels[1] * 2 + levels[2] + levels[3] + absent[:2]:
        words = [rng.choice([0, 1, 5, rng.randrange(1 << 32), rng.randrange(256)]) for _ in range(rng.randrange(0, 17))]
        runs.append(run_line(index, rng.randrange(1 << 32), 1 if first or rng.random() < 0.3 else 0, 0, words))
        expect.append(None)
        first = False
    return rom.image(), runs, expect, whats


def stopping_image(rng):
    # Tables that end in something the kernel's interpreter stops at.
    rom = Rom(rng)
    data_tables = [4, 9]
    for index in data_tables:
        rom.data(index, rng.randbytes(32))
    # Program 6 reads with a step that is none in its middle (a START);
    # no other statement of these tables asks for port 6.
    rom.data(23, bytes([1, 6, 4, 32, 0, 2, 0x39, 0, 1, 9, 9, 0, 0, 0]))
    free = list(range(COMMAND_COUNT))
    rng.shuffle(free)
    runs, expect = [], []

    def table(tail, work=4, after=True):
        maker = Maker(rng, 3, work, [], [], data_tables)
        maker.free_counters = []
        maker.early_end = False
        maker.any_port = False
        maker.sequence(rng.randrange(2, 7), 0)
        # (The place itself is reached through the registers themselves
        # and with no register block: what was set before is chance.)
        code, _ = maker.code.lay()
        code += bytes([SETPORT, 0, 0, SETREGBLOCK, 0, 0]) + bytes(tail)
        if after:
            follow = Maker(rng, 3, work, [], [], data_tables)
            follow.free_counters = []
            follow.any_port = False
            follow.sequence(rng.randrange(1, 4), 0)
            more, _ = follow.code.lay(6 + len(code))
            code += more + bytes([EOT])
        index = free.pop()
        rom.command(index, work, 0, code)
        return index

    def add(index, kind, how, steps=0):
        words = [rng.randrange(1 << 32) for _ in range(4)]
        runs.append(run_line(index, rng.randrange(1 << 32), 1, steps, words))
        expect.append((kind, how))

    move_ps = SIX["move"] + 1
    for _ in range(3):
        # An opcode that is none ends the run; Linux ends the table there.
        add(table([rng.choice([0, 127, 128, 200, 255])], after=False), "unknown_opcode", "same")
        # The same in a table that was called: Linux's caller goes on.
        inner = table([rng.choice([0, 127, 255])], after=False)
        add(table([CALLTABLE, inner]), "unknown_opcode", "prefix")
        # A switch whose case does not begin with its mark (the byte there
        # is the table's end, which is what Linux then runs).
        add(table([SWITCH, IMM | (4 << 3), 7, EOT], after=False), "bad_case", "same")
        add(table([SETPORT, 6, 0, move_ps, REG, 0, 0x10, 0, SETPORT, 0, 0]), "bad_program", "prefix")
        # Data beyond the ROM's end.
        add(table([SIX["move"] + 2, IMM, 0x42, 0xFF, 0xFF, 0, 0, move_ps, ID, 0, 0x00, 0xFF]), "outside", "prefix")
        # A work space word the table did not ask for: read, written.
        add(table([move_ps, WS, 0, rng.randrange(4, 0x40)], work=4), "outside", "prefix")
        add(table([SIX["move"] + 2, IMM, rng.randrange(0x49, 0x100), 1, 0, 0, 0], work=4), "outside", "prefix")
        # A data block and a table beyond the lists (where the bytes
        # behind the lists are 0, so that Linux finds nothing there).
        add(table([SETDATABLOCK, rng.randrange(40, 120)]), "outside", "prefix")
        add(table([CALLTABLE, rng.randrange(134, 221)]), "outside", "prefix")
    # Calls nested twenty deep.
    chain = [free.pop() for _ in range(20)]
    for n, index in enumerate(chain):
        maker = Maker(rng, 3, 2, [], [], data_tables)
        maker.free_counters = []
        maker.early_end = False
        maker.any_port = False
        maker.sequence(2, 0)
        code, _ = maker.code.lay()
        rom.command(index, 2, 4, code + (bytes([CALLTABLE, chain[n + 1]]) if n + 1 < 20 else b"") + bytes([EOT]))
    add(chain[0], "depth", "prefix")
    # A loop without end: a write, a jump back to it.
    index = free.pop()
    rom.command(index, 0, 0, bytes([SIX["add"], IMM, 0x40, 0x01, 1, 0, 0, 0, JUMP, 6, 0]))
    add(index, "count", "stuck", 3000)
    return rom.image(), runs, expect, {}


def blocks(text):
    # What a program wrote, a run at a time: (events, end, the rest).
    out = []
    for line in text.splitlines():
        if line.startswith("#"):
            continue
        if line.startswith("run "):
            out.append({"head": line, "events": [], "end": "", "rest": []})
        elif not out:
            out.append({"head": line, "events": [], "end": line, "rest": []})
        elif line.startswith("end "):
            out[-1]["end"] = line
        elif out[-1]["end"]:
            out[-1]["rest"].append(line)
        else:
            out[-1]["events"].append(line)
    return out


def hold(ours, theirs, expect, where, report):
    # One run's two accounts held against each other; what differs, or
    # None. `expect`: None, or (how the kernel's must stop, how much of
    # Linux's account it must match).
    if ours["head"] != theirs["head"]:
        return "the runs are not the same run: %s / %s" % (ours["head"], theirs["head"])
    stopped = ours["end"].startswith("end stopped ")
    if expect is None and stopped and report is not None:
        # (A real ROM's table: no expectation was made.)
        expect = (ours["end"].split()[2], "prefix")
    if expect is None:
        for n, (a, b) in enumerate(zip(ours["events"], theirs["events"])):
            if a != b:
                return "%s: at step %d of what was done to the card the kernel's has '%s', Linux's '%s'" % (where, n, a, b)
        if len(ours["events"]) != len(theirs["events"]):
            return "%s: the kernel's did %d things to the card, Linux's %d" % (where, len(ours["events"]), len(theirs["events"]))
        if ours["end"] != theirs["end"]:
            return "%s: the kernel's says '%s', Linux's '%s'" % (where, ours["end"], theirs["end"])
        for a, b in zip(ours["rest"], theirs["rest"]):
            if a != b:
                return "%s: afterwards the kernel's has '%s', Linux's '%s'" % (where, a, b)
        return None
    kind, how = expect
    if not ours["end"].startswith("end stopped " + kind + " "):
        return "%s: the kernel's must stop as %s and says '%s'" % (where, kind, ours["end"])
    if ours["events"] != theirs["events"][:len(ours["events"])]:
        # (A table that does not end, a real ROM's that waits for
        # something the card that is a table never shows: each gives up
        # in its own way, Linux's after 20000 jumps to one place, the
        # kernel's by its count. The one that gave up first must be how
        # the other began.)
        if not (kind == "count" and theirs["end"] == "end stuck" and theirs["events"] == ours["events"][:len(theirs["events"])]):
            return "%s: what the kernel's did before it stopped is not how Linux's began" % where
    if how == "same" and (len(ours["events"]) != len(theirs["events"]) or ours["rest"] != theirs["rest"] or theirs["end"] != "end done"):
        return "%s: Linux's ends the table there too, and the two differ" % where
    if how == "stuck" and theirs["end"] != "end stuck":
        return "%s: Linux's does not say it was stuck ('%s')" % (where, theirs["end"])
    return None


def capped(args, errors=False, most=96 << 20):
    # A program run, and what it wrote (to the error output when
    # `errors`), which is `most` bytes at most and ten minutes: a table
    # that does not end must not fill the memory or the disk.
    program = subprocess.Popen(["timeout", "600"] + args, stdout=subprocess.DEVNULL if errors else subprocess.PIPE,
                               stderr=subprocess.PIPE if errors else subprocess.DEVNULL)
    stream = program.stderr if errors else program.stdout
    pieces, size = [], 0
    while True:
        piece = stream.read(1 << 16)
        if not piece:
            break
        size += len(piece)
        if size > most:
            program.kill()
            raise SystemExit("%s wrote more than %d MiB: a table that does not end" % (args[0], most >> 20))
        pieces.append(piece)
    return program.wait(), b"".join(pieces).decode("latin-1")


def both(work, image, runs, name, verbose=False):
    with open(os.path.join(work, name + ".rom"), "wb") as f:
        f.write(image)
    with open(os.path.join(work, name + ".runs"), "w") as f:
        f.write("".join(runs))
    args = [os.path.join(work, name + ".rom"), os.path.join(work, name + ".runs")]
    answered, ours = capped([os.path.join(work, "ours")] + args)
    if answered != 0:
        raise SystemExit("the kernel's interpreter ended with %d" % answered)
    answered, theirs = capped([os.path.join(work, "reference")] + args)
    if answered != 0:
        raise SystemExit("Linux's interpreter ended with %d" % answered)
    # (Linux's own account of what it ran, when asked for: a run more.)
    said = capped([os.path.join(work, "reference")] + args + ["-v"], True)[1] if verbose else ""
    os.remove(args[0])
    os.remove(args[1])
    return ours, theirs, said


def check(work, images, seed, quiet=False):
    # `images` images from `seed`; (runs held, runs that differ, the
    # first differences).
    held = differ = events = stopping = 0
    stopped_events = 0
    first = []
    done = {}
    for n in range(images):
        rng = random.Random(seed * 1000003 + n)
        stops = n % 5 == 4
        image, runs, expect, whats = stopping_image(rng) if stops else agreeing_image(rng)
        account = not stops and n < 10 and not quiet
        ours, theirs, said = both(work, image, runs, "image%d" % n, account)
        if account:
            # What Linux's own account says it ran, by where it lies.
            for m in re.finditer(r"@ 0x([0-9A-F]+)", said):
                what = whats.get(int(m.group(1), 16))
                if what is not None:
                    done[what] = done.get(what, 0) + 1
        a, b = blocks(ours), blocks(theirs)
        if len(a) != len(runs) or len(b) != len(runs):
            differ += 1
            first.append("image %d: %d runs asked for, the kernel's wrote %d down, Linux's %d" % (n, len(runs), len(a), len(b)))
            continue
        for k in range(len(runs)):
            held += 1
            if expect[k] is None:
                events += len(b[k]["events"])
            else:
                stopping += 1
                stopped_events += len(a[k]["events"])
            what = hold(a[k], b[k], expect[k], "image %d (seed %d) run %d, table %s" % (n, seed, k, runs[k].split()[0]), None)
            if what is not None:
                differ += 1
                if len(first) < 5:
                    first.append(what)
    return held, differ, (events, stopped_events), stopping, first, done


def coverage(done):
    # What Linux's account of the first images says was run.
    ops = {}
    for what, count in done.items():
        ops[what[0]] = ops.get(what[0], 0) + count
    missing = [op for op in range(1, 127) if op not in ops]
    print("of the 126 opcodes, %d were run in the first images (Linux's own account of what it ran)%s" %
          (126 - len(missing), "" if not missing else "; not run: " + " ".join(str(op) for op in missing)))
    # Each family of six with every kind of source, every part of the
    # source and each of the destination's parts.
    combos = set((what[0], what[1], what[2], what[3]) for what in done if len(what) == 5 and what[1] is not None and what[3] is not None)
    kinds = set((what[4], what[1]) for what in done if len(what) == 5 and what[1] is not None and what[4] is not None)
    parts = set((what[2], what[3]) for what in done if len(what) == 5 and what[3] is not None)
    print("operations run: %d different (opcode, source kind, source part, destination part); every destination kind with every source kind: %d of 48; every source part into each destination part: %d of 32" %
          (len(combos), len(kinds), len(parts)))


# Deliberate mistakes for the comparison to find: (what it is, the text
# of atom.mlx as it stands, the text with the mistake).
MISTAKES = [
    ("an ADD that is one too many", "self.put(hands, kind, attribute, from, (was + value) & M32, whole)", "self.put(hands, kind, attribute, from, (was + value + 1) & M32, whole)"),
    ("a MOVE of a part that forgets the rest of the word", "_ = self.destination(hands, kind, attribute)\n                    whole = self.*.whole", "_ = self.destination(hands, kind, attribute)\n                    whole = 0"),
    ("a destination part taken from the wrong column", "const DST_TO_SRC = [_]usize{ 0, 0, 0, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 4, 5, 6, 7,", "const DST_TO_SRC = [_]usize{ 0, 0, 0, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 4, 5, 7, 6,"),
    ("a shift by 32 and more that gives 0", "fn shiftLeft(value: usize, count: usize) -> usize { return (value << @intCast(u6, count & 31)) & M32 }", "fn shiftLeft(value: usize, count: usize) -> usize { return if count > 31 { 0 } else { (value << @intCast(u6, count & 31)) & M32 } }"),
    ("a jump when below that also jumps when equal", "taken = !(self.*.above || self.*.equal)", "taken = !self.*.above"),
    ("a write that forgets the register block", "const number = self.wordAt(at) + self.*.regBlock", "const number = self.wordAt(at)"),
    ("a 64-bit division that forgets the high half", "const quotient = (left | (self.*.remainder << 32)) / right", "const quotient = left / right"),
    ("a called table that gets its caller's parameter space", "self.enter(table, start, self.*.at, frame.*.parameters + frame.*.shift)", "self.enter(table, start, self.*.at, frame.*.parameters)"),
    ("flags forgotten from one run to the next", "self.*.failed = End.done\n        self.*.finished = false", "self.*.failed = End.done\n        self.*.equal = false\n        self.*.above = false\n        self.*.finished = false"),
    ("an indirect program that takes the register's number for the data", "const source = if step == ATOM_IIO_MOVE_INDEX { number } else if step == ATOM_IIO_MOVE_DATA { data }", "const source = if step == ATOM_IIO_MOVE_INDEX { number } else if step == ATOM_IIO_MOVE_DATA { number }"),
    ("a MASK that sets the source before it masks", "self.put(hands, kind, attribute, from, (was & mask) | value, whole)", "self.put(hands, kind, attribute, from, (was | value) & mask, whole)"),
    ("register 0 written as any other", "if number == 0 { (word << 2) & M32 } else { word }", "word"),
    ("a work space that is not zeroed for a table", "self.*.work[self.*.workUsed + k] = 0", "self.*.work[self.*.workUsed + k] = self.*.work[self.*.workUsed + k]"),
    ("a TEST that also sets which is greater", "self.*.equal = left & right == 0", "self.*.equal = left & right == 0\n                self.*.above = left > right"),
    ("a CLEAR that clears the whole word", "_ = self.destination(hands, kind, attribute)\n            self.put(hands, kind, attribute, from, 0, self.*.whole)", "_ = self.destination(hands, kind, attribute)\n            self.put(hands, kind, attribute, from, 0, 0)"),
    ("a wait in milliseconds taken for microseconds", "const time = if op == OP_DELAY { count * 1000 } else { count }", "const time = count"),
]


def mistakes(root, work, compiler):
    source = open(os.path.join(root, "projects/brix/kernel/atom.mlx")).read()
    tree = os.path.join(work, "tree")
    for folder in ("projects/brix/kernel", "tests/support", "tools/atom_reference"):
        os.makedirs(os.path.join(tree, folder))
    os.symlink(os.path.join(root, "std"), os.path.join(tree, "std"))
    for name in ("tests/support/atom_card.mlx", "tools/atom_reference/ours.mlx"):
        with open(os.path.join(tree, name), "w") as f:
            f.write(open(os.path.join(root, name)).read())
    found = 0
    for n, (what, right, wrong) in enumerate([("no mistake", "", "")] + MISTAKES):
        if right and source.count(right) != 1:
            raise SystemExit("mistake '%s': the text it changes is in atom.mlx %d times (tables.py's MISTAKES must follow the file)" % (what, source.count(right)))
        with open(os.path.join(tree, "projects/brix/kernel/atom.mlx"), "w") as f:
            f.write(source.replace(right, wrong) if right else source)
        built = subprocess.run([compiler, "--quiet", "tools/atom_reference/ours.mlx", "-o", os.path.join(work, "ours")], cwd=tree, capture_output=True, text=True)
        if built.returncode != 0:
            raise SystemExit("mistake '%s' does not build: %s" % (what, built.stdout[-400:] + built.stderr[-400:]))
        held, differ, events, stopping, first, done = check(work, 6, 1, True)
        if n == 0:
            if differ:
                raise SystemExit("the interpreter as it stands differs from Linux's: %s" % first[0])
            print("as it stands: %d runs, none differs" % held)
            continue
        if differ:
            found += 1
            print("found: %s (%d of %d runs differ; the first: %s)" % (what, differ, held, first[0][:150]))
        else:
            print("NOT FOUND: %s" % what)
    print("%d of %d deliberate mistakes found" % (found, len(MISTAKES)))
    return 0 if found == len(MISTAKES) else 1


def main():
    mode = sys.argv[1]
    if mode == "run-line":
        table, _, rest = sys.argv[2].partition(":")
        words = [int(w, 0) for w in rest.split(",") if w]
        # (A table that waits for ever is given 200000 instructions, more
        # than Linux's takes to give up on a short loop: not the four
        # million a run in the kernel gets, each written down.)
        sys.stdout.write(run_line(int(table, 0), 1, 1, 200000, words))
        return 0
    if mode == "compare":
        a, b = blocks(open(sys.argv[2]).read()), blocks(open(sys.argv[3]).read())
        bad = 0
        if len(a) != len(b):
            print("the kernel's interpreter wrote %d runs down, Linux's %d" % (len(a), len(b)))
            return 1
        for k in range(len(a)):
            what = hold(a[k], b[k], None, a[k]["head"], True)
            if what is not None:
                bad += 1
                print("DIFFERS " + what)
            elif a[k]["end"].startswith("end stopped count ") and b[k]["end"] == "end stuck":
                print("%s: the table does not end on this card that is a table (it waits for something a real card shows): both gave up, the kernel's interpreter by its count (%s), Linux's after 20000 jumps to one place; the %d things both did to the card are the same" %
                      (a[k]["head"], a[k]["end"][4:], min(len(a[k]["events"]), len(b[k]["events"]))))
            elif a[k]["end"].startswith("end stopped"):
                print("%s: the kernel's interpreter stopped (%s) having done what Linux's did up to there (%d things to the card); Linux's went on (%s)" %
                      (a[k]["head"], a[k]["end"][4:], len(a[k]["events"]), b[k]["end"]))
            else:
                print("%s: the same in both, %d things done to the card, %s" % (a[k]["head"], len(a[k]["events"]), a[k]["end"]))
        return 1 if bad else 0
    if mode == "mistakes":
        return mistakes(sys.argv[2], sys.argv[3], sys.argv[4])
    if mode == "write":
        # One image and its runs written out (to look at a difference).
        work, n, seed = sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
        rng = random.Random(seed * 1000003 + n)
        image, runs, expect, whats = stopping_image(rng) if n % 5 == 4 else agreeing_image(rng)
        with open(os.path.join(work, "image%d.rom" % n), "wb") as f:
            f.write(image)
        with open(os.path.join(work, "image%d.runs" % n), "w") as f:
            f.write("".join(runs))
        return 0
    if mode == "check":
        work, images, seed = sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
        held, differ, events, stopping, first, done = check(work, images, seed)
        coverage(done)
        for what in first:
            print("DIFFERS " + what)
        print("%d runs of generated tables in %d images (seed %d) through both interpreters: %d runs that must be the same in both to the end (%d things done to the card, each line held, and the parameter space, the state and the scratch afterwards), %d runs of tables the kernel's interpreter must stop in (%d things done to the card before it stops, each as Linux's did); %d runs differ" %
              (held, images, seed, held - stopping, events[0], stopping, events[1], differ))
        return 1 if differ else 0
    raise SystemExit("tables.py: called by tools/check_atom_reference.sh")


if __name__ == "__main__":
    sys.exit(main())
