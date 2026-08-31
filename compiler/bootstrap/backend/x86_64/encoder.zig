/// x86_64 machine-code encoder for zin0.
///
/// Emits raw instruction bytes into an ArrayList(u8) buffer.
/// Tracks symbol definitions and fixup (relocation) records so that
/// forward references (branch targets, call targets, RIP-relative lea)
/// can be patched once all code has been emitted.
///
/// All instructions assume 64-bit (REX.W) operand size unless noted.
const std = @import("std");
const x86 = @import("target.zig");
pub const Reg = x86.Register;

pub const Condition = enum(u8) {
    equal = 0x4,
    not_equal = 0x5,
    below = 0x2,
    above_equal = 0x3,
    below_equal = 0x6,
    above = 0x7,
    less = 0xc,
    greater_equal = 0xd,
    less_equal = 0xe,
    greater = 0xf,
};

// ─────────────────────────────────────────────────────────────────────────────
//  Fixup kinds
// ─────────────────────────────────────────────────────────────────────────────

pub const FixupKind = enum {
    /// 4-byte relative offset for CALL rel32 (0xE8).  Patch site = start of imm32.
    call_rel32,
    /// 4-byte relative offset for JMP rel32 (0xE9) or Jcc rel32 (0x0F 0x8x).
    jmp_rel32,
    /// 4-byte RIP-relative offset for LEA reg,[RIP+rel32] (used by func_sym).
    lea_rip_rel32,
};

pub const Fixup = struct {
    kind: FixupKind,
    /// Byte offset in the code buffer of the 4-byte field to be patched.
    patch_offset: u32,
    /// Name of the symbol the fixup points to.
    target: []const u8,
};

// ─────────────────────────────────────────────────────────────────────────────
//  Encoder
// ─────────────────────────────────────────────────────────────────────────────

pub const Encoder = struct {
    allocator: std.mem.Allocator,
    /// Raw machine-code bytes; grows as instructions are emitted.
    buf: std.ArrayList(u8),
    /// symbol_name → byte offset in buf at which the symbol starts.
    symbols: std.StringHashMap(u32),
    /// Pending relocations to be patched after all code is emitted.
    fixups: std.ArrayList(Fixup),

    pub fn init(allocator: std.mem.Allocator) Encoder {
        return .{
            .allocator = allocator,
            .buf = std.ArrayList(u8).empty,
            .symbols = std.StringHashMap(u32).init(allocator),
            .fixups = std.ArrayList(Fixup).empty,
        };
    }

    pub fn deinit(self: *Encoder) void {
        self.buf.deinit(self.allocator);
        self.symbols.deinit();
        self.fixups.deinit(self.allocator);
    }

    // ── helpers ───────────────────────────────────────────────────────────────

    pub fn pos(self: *const Encoder) u32 {
        return @as(u32, @intCast(self.buf.items.len));
    }

    fn emit1(self: *Encoder, b: u8) !void {
        try self.buf.append(self.allocator, b);
    }

    fn emit2(self: *Encoder, b0: u8, b1: u8) !void {
        try self.buf.append(self.allocator, b0);
        try self.buf.append(self.allocator, b1);
    }

    fn emit3(self: *Encoder, b0: u8, b1: u8, b2: u8) !void {
        try self.buf.append(self.allocator, b0);
        try self.buf.append(self.allocator, b1);
        try self.buf.append(self.allocator, b2);
    }

    fn emitImm32(self: *Encoder, v: i32) !void {
        const u: u32 = @bitCast(v);
        try self.buf.append(self.allocator, @as(u8, @truncate(u)));
        try self.buf.append(self.allocator, @as(u8, @truncate(u >> 8)));
        try self.buf.append(self.allocator, @as(u8, @truncate(u >> 16)));
        try self.buf.append(self.allocator, @as(u8, @truncate(u >> 24)));
    }

    fn emitImm64(self: *Encoder, v: i64) !void {
        const u: u64 = @bitCast(v);
        try self.buf.append(self.allocator, @as(u8, @truncate(u)));
        try self.buf.append(self.allocator, @as(u8, @truncate(u >> 8)));
        try self.buf.append(self.allocator, @as(u8, @truncate(u >> 16)));
        try self.buf.append(self.allocator, @as(u8, @truncate(u >> 24)));
        try self.buf.append(self.allocator, @as(u8, @truncate(u >> 32)));
        try self.buf.append(self.allocator, @as(u8, @truncate(u >> 40)));
        try self.buf.append(self.allocator, @as(u8, @truncate(u >> 48)));
        try self.buf.append(self.allocator, @as(u8, @truncate(u >> 56)));
    }

    /// Patch a 4-byte little-endian i32 at buf[offset].
    fn patch32(self: *Encoder, offset: u32, v: i32) void {
        const u: u32 = @bitCast(v);
        self.buf.items[offset + 0] = @as(u8, @truncate(u));
        self.buf.items[offset + 1] = @as(u8, @truncate(u >> 8));
        self.buf.items[offset + 2] = @as(u8, @truncate(u >> 16));
        self.buf.items[offset + 3] = @as(u8, @truncate(u >> 24));
    }

    /// REX.W prefix for 64-bit ops.
    /// REX = 0100 WRXB  (W=1, R=reg ext, X=index ext, B=rm/base ext)
    fn rex(w: bool, r: bool, x: bool, b: bool) u8 {
        return x86.encodeRex(w, r, x, b);
    }

    /// ModRM byte: mod=2 bits, reg=3 bits, rm=3 bits.
    fn modrm(mod: u2, r: u3, rm: u3) u8 {
        return x86.encodeModRm(mod, r, rm);
    }

    fn regIdx(r: Reg) u8 {
        return @intFromEnum(r);
    }

    /// Low 3 bits of register encoding.
    fn lo3(r: Reg) u3 {
        return @as(u3, @truncate(regIdx(r)));
    }

    /// True if register index ≥ 8 (needs REX.R or REX.B extension bit).
    fn needsRex(r: Reg) bool {
        return regIdx(r) >= 8;
    }

    /// Displacement encoding: use disp8 if it fits, else disp32.
    fn dispEncoding(disp: i32) struct { mod: u2, bytes: [4]u8, len: u8 } {
        if (disp >= -128 and disp <= 127) {
            return .{ .mod = 0b01, .bytes = .{ @as(u8, @bitCast(@as(i8, @intCast(disp)))), 0, 0, 0 }, .len = 1 };
        }
        const u: u32 = @bitCast(disp);
        return .{ .mod = 0b10, .bytes = .{
            @as(u8, @truncate(u)),
            @as(u8, @truncate(u >> 8)),
            @as(u8, @truncate(u >> 16)),
            @as(u8, @truncate(u >> 24)),
        }, .len = 4 };
    }

    // ── symbol table ──────────────────────────────────────────────────────────

    /// Mark the current position as the start of a named symbol.
    pub fn defineSymbol(self: *Encoder, name: []const u8) !void {
        std.debug.print("[enc] defineSymbol '{s}' @ 0x{x}\n", .{ name, self.pos() });
        try self.symbols.put(name, self.pos());
    }

    /// Record a fixup: emit 4 zero bytes as placeholder and register the reloc.
    pub fn addFixup(self: *Encoder, kind: FixupKind, target: []const u8) !void {
        const offset = self.pos();
        std.debug.print("[enc] addFixup {s} → '{s}' @ 0x{x}\n", .{ @tagName(kind), target, offset });
        try self.fixups.append(self.allocator, .{ .kind = kind, .patch_offset = offset, .target = target });
        try self.emitImm32(0); // placeholder
    }

    /// Resolve all fixups by computing relative offsets and patching the buffer.
    /// Must be called after all code has been emitted.
    pub fn applyFixups(self: *Encoder) !void {
        std.debug.print("[enc] applying {d} fixups\n", .{self.fixups.items.len});
        for (self.fixups.items) |fixup| {
            const target_pos = self.symbols.get(fixup.target) orelse {
                std.debug.print("[enc] ERROR: undefined symbol '{s}'\n", .{fixup.target});
                return error.UndefinedSymbol;
            };
            // rel32 = target - (patch_site + 4)
            const after_fixup: i64 = @as(i64, fixup.patch_offset) + 4;
            const rel32: i32 = @as(i32, @intCast(@as(i64, target_pos) - after_fixup));
            std.debug.print("[enc] patching '{s}': rel32={d} @ 0x{x}\n", .{ fixup.target, rel32, fixup.patch_offset });
            self.patch32(fixup.patch_offset, rel32);
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Instruction emitters
    // ─────────────────────────────────────────────────────────────────────────

    // ── mov ──────────────────────────────────────────────────────────────────

    /// MOV reg, imm64   (REX.W B8+rd id)
    pub fn emitMovRegImm64(self: *Encoder, dst: Reg, imm: i64) !void {
        std.debug.print("[enc] mov {s}, {d}\n", .{ @tagName(dst), imm });
        try self.emit1(rex(true, false, false, needsRex(dst)));
        try self.emit1(0xB8 | @as(u8, lo3(dst)));
        try self.emitImm64(imm);
    }

    /// MOV dst, src   (REX.W 89 /r)
    pub fn emitMovRegReg(self: *Encoder, dst: Reg, src: Reg) !void {
        std.debug.print("[enc] mov {s}, {s}\n", .{ @tagName(dst), @tagName(src) });
        if (dst == src) return; // elide no-op
        try self.emit1(rex(true, needsRex(src), false, needsRex(dst)));
        try self.emit1(0x89);
        try self.emit1(modrm(0b11, lo3(src), lo3(dst)));
    }

    /// MOV dst, [rbp - offset]   load from local stack slot
    pub fn emitMovRegMem(self: *Encoder, dst: Reg, rbp_offset: i32) !void {
        std.debug.print("[enc] mov {s}, [rbp-{d}]\n", .{ @tagName(dst), rbp_offset });
        const d = dispEncoding(-rbp_offset);
        try self.emit1(rex(true, needsRex(dst), false, false));
        try self.emit1(0x8B);
        try self.emit1(modrm(d.mod, lo3(dst), lo3(Reg.rbp)));
        try self.buf.appendSlice(self.allocator, d.bytes[0..d.len]);
    }

    /// MOV [rbp - offset], src   store to local stack slot
    pub fn emitMovMemReg(self: *Encoder, rbp_offset: i32, src: Reg) !void {
        std.debug.print("[enc] mov [rbp-{d}], {s}\n", .{ rbp_offset, @tagName(src) });
        const d = dispEncoding(-rbp_offset);
        try self.emit1(rex(true, needsRex(src), false, false));
        try self.emit1(0x89);
        try self.emit1(modrm(d.mod, lo3(src), lo3(Reg.rbp)));
        try self.buf.appendSlice(self.allocator, d.bytes[0..d.len]);
    }

    // ── lea ──────────────────────────────────────────────────────────────────

    /// LEA dst, [rbp - offset]
    pub fn emitLeaRegMem(self: *Encoder, dst: Reg, rbp_offset: i32) !void {
        std.debug.print("[enc] lea {s}, [rbp-{d}]\n", .{ @tagName(dst), rbp_offset });
        const d = dispEncoding(-rbp_offset);
        try self.emit1(rex(true, needsRex(dst), false, false));
        try self.emit1(0x8D);
        try self.emit1(modrm(d.mod, lo3(dst), lo3(Reg.rbp)));
        try self.buf.appendSlice(self.allocator, d.bytes[0..d.len]);
    }

    /// LEA dst, [RIP + rel32]   (RIP-relative, used for function references)
    /// Emits a fixup for the 4-byte offset.
    pub fn emitLeaRipRel(self: *Encoder, dst: Reg, target: []const u8) !void {
        std.debug.print("[enc] lea {s}, [RIP+rel '{s}']\n", .{ @tagName(dst), target });
        try self.emit1(rex(true, needsRex(dst), false, false));
        try self.emit1(0x8D);
        // ModRM: mod=00 (no disp), reg=dst, rm=101 (RIP-relative)
        try self.emit1(modrm(0b00, lo3(dst), 0b101));
        try self.addFixup(.lea_rip_rel32, target);
    }

    // ── arithmetic ────────────────────────────────────────────────────────────

    /// ADD dst, src   (REX.W 01 /r)
    pub fn emitAdd(self: *Encoder, dst: Reg, src: Reg) !void {
        std.debug.print("[enc] add {s}, {s}\n", .{ @tagName(dst), @tagName(src) });
        try self.emit1(rex(true, needsRex(src), false, needsRex(dst)));
        try self.emit1(0x01);
        try self.emit1(modrm(0b11, lo3(src), lo3(dst)));
    }

    /// SUB dst, src   (REX.W 29 /r)
    pub fn emitSub(self: *Encoder, dst: Reg, src: Reg) !void {
        std.debug.print("[enc] sub {s}, {s}\n", .{ @tagName(dst), @tagName(src) });
        try self.emit1(rex(true, needsRex(src), false, needsRex(dst)));
        try self.emit1(0x29);
        try self.emit1(modrm(0b11, lo3(src), lo3(dst)));
    }

    /// IMUL dst, src   (REX.W 0F AF /r)
    pub fn emitIMul(self: *Encoder, dst: Reg, src: Reg) !void {
        std.debug.print("[enc] imul {s}, {s}\n", .{ @tagName(dst), @tagName(src) });
        try self.emit1(rex(true, needsRex(dst), false, needsRex(src)));
        try self.emit2(0x0F, 0xAF);
        try self.emit1(modrm(0b11, lo3(dst), lo3(src)));
    }

    pub fn emitAnd(self: *Encoder, dst: Reg, src: Reg) !void {
        try self.emit1(rex(true, needsRex(src), false, needsRex(dst)));
        try self.emit1(0x21);
        try self.emit1(modrm(0b11, lo3(src), lo3(dst)));
    }

    pub fn emitOr(self: *Encoder, dst: Reg, src: Reg) !void {
        try self.emit1(rex(true, needsRex(src), false, needsRex(dst)));
        try self.emit1(0x09);
        try self.emit1(modrm(0b11, lo3(src), lo3(dst)));
    }

    pub fn emitXor(self: *Encoder, dst: Reg, src: Reg) !void {
        try self.emit1(rex(true, needsRex(src), false, needsRex(dst)));
        try self.emit1(0x31);
        try self.emit1(modrm(0b11, lo3(src), lo3(dst)));
    }

    /// Variable-count shift. x86_64 reads the low byte of rcx (`cl`).
    pub fn emitShiftCl(self: *Encoder, dst: Reg, left: bool, arithmetic_right: bool) !void {
        const extension: u3 = if (left) 4 else if (arithmetic_right) 7 else 5;
        try self.emit1(rex(true, false, false, needsRex(dst)));
        try self.emit1(0xD3);
        try self.emit1(modrm(0b11, extension, lo3(dst)));
    }

    pub fn emitCqo(self: *Encoder) !void {
        try self.emit2(0x48, 0x99);
    }

    /// DIV/IDIV r/m64. The dividend is in rdx:rax; quotient and remainder are
    /// returned in rax and rdx respectively.
    pub fn emitDivide(self: *Encoder, divisor: Reg, signed: bool) !void {
        try self.emit1(rex(true, false, false, needsRex(divisor)));
        try self.emit1(0xF7);
        try self.emit1(modrm(0b11, if (signed) 7 else 6, lo3(divisor)));
    }

    /// SUB rsp, imm8   (REX.W 83 /5 ib)  — stack allocation
    pub fn emitSubRspImm8(self: *Encoder, imm: u8) !void {
        std.debug.print("[enc] sub rsp, {d}\n", .{imm});
        try self.emit1(rex(true, false, false, false));
        try self.emit2(0x83, 0xEC);
        try self.emit1(imm);
    }

    /// SUB rsp, imm32   (REX.W 81 /5 id)  — larger stack allocation
    pub fn emitSubRspImm32(self: *Encoder, imm: u32) !void {
        std.debug.print("[enc] sub rsp, {d}\n", .{imm});
        try self.emit1(rex(true, false, false, false));
        try self.emit2(0x81, 0xEC);
        try self.emitImm32(@as(i32, @intCast(imm)));
    }

    // ── compare / test ────────────────────────────────────────────────────────

    /// TEST reg, reg   (REX.W 85 /r)
    pub fn emitTest(self: *Encoder, reg: Reg) !void {
        std.debug.print("[enc] test {s}, {s}\n", .{ @tagName(reg), @tagName(reg) });
        try self.emit1(rex(true, needsRex(reg), false, needsRex(reg)));
        try self.emit1(0x85);
        try self.emit1(modrm(0b11, lo3(reg), lo3(reg)));
    }

    /// CMP lhs, rhs   (REX.W 39 /r)
    pub fn emitCmp(self: *Encoder, lhs: Reg, rhs: Reg) !void {
        std.debug.print("[enc] cmp {s}, {s}\n", .{ @tagName(lhs), @tagName(rhs) });
        try self.emit1(rex(true, needsRex(rhs), false, needsRex(lhs)));
        try self.emit1(0x39);
        try self.emit1(modrm(0b11, lo3(rhs), lo3(lhs)));
    }

    /// SETcc dst.low8 after clearing the complete destination register.
    pub fn emitSetcc(self: *Encoder, dst: Reg, condition: Condition) !void {
        std.debug.print("[enc] set{s} {s}b\n", .{ @tagName(condition), @tagName(dst) });
        try self.emitMovRegImm64(dst, 0);
        // A REX prefix is required for spl/bpl/sil/dil and harmless for all GP regs.
        try self.emit1(rex(false, false, false, needsRex(dst)));
        try self.emit2(0x0F, 0x90 + @intFromEnum(condition));
        try self.emit1(modrm(0b11, 0, lo3(dst)));
    }

    // ── control flow ──────────────────────────────────────────────────────────

    /// JMP rel32   (E9 id)
    pub fn emitJmpRel(self: *Encoder, target: []const u8) !void {
        std.debug.print("[enc] jmp '{s}'\n", .{target});
        try self.emit1(0xE9);
        try self.addFixup(.jmp_rel32, target);
    }

    /// JNZ rel32   (0F 85 id)  — jump if not zero (condition true)
    pub fn emitJnzRel(self: *Encoder, target: []const u8) !void {
        std.debug.print("[enc] jnz '{s}'\n", .{target});
        try self.emit2(0x0F, 0x85);
        try self.addFixup(.jmp_rel32, target);
    }

    /// JZ rel32   (0F 84 id)  — jump if zero (condition false)
    pub fn emitJzRel(self: *Encoder, target: []const u8) !void {
        std.debug.print("[enc] jz '{s}'\n", .{target});
        try self.emit2(0x0F, 0x84);
        try self.addFixup(.jmp_rel32, target);
    }

    /// CALL rel32   (E8 id)
    pub fn emitCallRel(self: *Encoder, target: []const u8) !void {
        std.debug.print("[enc] call '{s}'\n", .{target});
        try self.emit1(0xE8);
        try self.addFixup(.call_rel32, target);
    }

    /// CALL r/m64   (FF /2)  — indirect call through register
    pub fn emitCallReg(self: *Encoder, reg: Reg) !void {
        std.debug.print("[enc] call {s}\n", .{@tagName(reg)});
        if (needsRex(reg)) try self.emit1(rex(false, false, false, true));
        try self.emit1(0xFF);
        try self.emit1(modrm(0b11, 2, lo3(reg)));
    }

    /// RET   (C3)
    pub fn emitRet(self: *Encoder) !void {
        std.debug.print("[enc] ret\n", .{});
        try self.emit1(0xC3);
    }

    // ── frame setup ───────────────────────────────────────────────────────────

    /// PUSH rbp   (55)
    pub fn emitPushRbp(self: *Encoder) !void {
        std.debug.print("[enc] push rbp\n", .{});
        try self.emit1(0x55);
    }

    /// POP rbp   (5D)
    pub fn emitPopRbp(self: *Encoder) !void {
        std.debug.print("[enc] pop rbp\n", .{});
        try self.emit1(0x5D);
    }

    /// MOV rbp, rsp   (REX.W 89 E5)
    pub fn emitMovRbpRsp(self: *Encoder) !void {
        std.debug.print("[enc] mov rbp, rsp\n", .{});
        try self.emit1(rex(true, false, false, false));
        try self.emit2(0x89, 0xE5);
    }

    /// MOV rsp, rbp   (REX.W 89 EC)
    pub fn emitMovRspRbp(self: *Encoder) !void {
        std.debug.print("[enc] mov rsp, rbp\n", .{});
        try self.emit1(rex(true, false, false, false));
        try self.emit2(0x89, 0xEC);
    }

    // ── syscall ───────────────────────────────────────────────────────────────

    /// SYSCALL   (0F 05)
    pub fn emitSyscall(self: *Encoder) !void {
        std.debug.print("[enc] syscall\n", .{});
        try self.emit2(0x0F, 0x05);
    }

    pub fn emitTrap(self: *Encoder) !void {
        std.debug.print("[enc] ud2\n", .{});
        try self.emit2(0x0f, 0x0b);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Tests
// ─────────────────────────────────────────────────────────────────────────────

test "MOV rax, imm64" {
    const testing = std.testing;
    var enc = Encoder.init(testing.allocator);
    defer enc.deinit();
    try enc.emitMovRegImm64(.rax, 60);
    // REX.W=0x48, opcode=0xB8+0=0xB8, imm64=60
    try testing.expectEqual(@as(u8, 0x48), enc.buf.items[0]);
    try testing.expectEqual(@as(u8, 0xB8), enc.buf.items[1]);
    try testing.expectEqual(@as(u8, 60), enc.buf.items[2]);
    try testing.expectEqual(@as(usize, 10), enc.buf.items.len);
}

test "MOV r15, imm64 — needs REX.B" {
    const testing = std.testing;
    var enc = Encoder.init(testing.allocator);
    defer enc.deinit();
    try enc.emitMovRegImm64(.r15, 0);
    // REX = 0x49 (REX.W + REX.B), opcode = 0xB8 + 7 = 0xBF
    try testing.expectEqual(@as(u8, 0x49), enc.buf.items[0]);
    try testing.expectEqual(@as(u8, 0xBF), enc.buf.items[1]);
}

test "MOV [rbp-8], rax" {
    const testing = std.testing;
    var enc = Encoder.init(testing.allocator);
    defer enc.deinit();
    try enc.emitMovMemReg(8, .rax);
    // REX.W=0x48, 0x89, ModRM(01, 0, 5)=0x45, disp8=-8=0xF8
    try testing.expectEqual(@as(u8, 0x48), enc.buf.items[0]);
    try testing.expectEqual(@as(u8, 0x89), enc.buf.items[1]);
    try testing.expectEqual(@as(u8, 0x45), enc.buf.items[2]);
    try testing.expectEqual(@as(u8, 0xF8), enc.buf.items[3]);
}

test "ADD rax, rcx" {
    const testing = std.testing;
    var enc = Encoder.init(testing.allocator);
    defer enc.deinit();
    try enc.emitAdd(.rax, .rcx);
    // REX.W=0x48, 0x01, ModRM(11, 1, 0)=0xC8
    try testing.expectEqual(@as(u8, 0x48), enc.buf.items[0]);
    try testing.expectEqual(@as(u8, 0x01), enc.buf.items[1]);
    try testing.expectEqual(@as(u8, 0xC8), enc.buf.items[2]);
}

test "PUSH rbp / POP rbp" {
    const testing = std.testing;
    var enc = Encoder.init(testing.allocator);
    defer enc.deinit();
    try enc.emitPushRbp();
    try enc.emitPopRbp();
    try testing.expectEqual(@as(u8, 0x55), enc.buf.items[0]);
    try testing.expectEqual(@as(u8, 0x5D), enc.buf.items[1]);
}

test "SYSCALL" {
    const testing = std.testing;
    var enc = Encoder.init(testing.allocator);
    defer enc.deinit();
    try enc.emitSyscall();
    try testing.expectEqual(@as(u8, 0x0F), enc.buf.items[0]);
    try testing.expectEqual(@as(u8, 0x05), enc.buf.items[1]);
}

test "symbol define and fixup" {
    const testing = std.testing;
    var enc = Encoder.init(testing.allocator);
    defer enc.deinit();

    // Emit: E8 <rel32>  (call "target")
    try enc.emitCallRel("target"); // at offset 0: E8 00 00 00 00
    // Define "target" at offset 5
    try enc.defineSymbol("target");
    try enc.emitSyscall(); // 0F 05 at offset 5

    try enc.applyFixups();
    // rel32 = 5 - (0 + 1 + 4) = 5 - 5 = 0
    try testing.expectEqual(@as(u8, 0x00), enc.buf.items[1]);
    try testing.expectEqual(@as(u8, 0x00), enc.buf.items[2]);
    try testing.expectEqual(@as(u8, 0x00), enc.buf.items[3]);
    try testing.expectEqual(@as(u8, 0x00), enc.buf.items[4]);
}

test "RET" {
    const testing = std.testing;
    var enc = Encoder.init(testing.allocator);
    defer enc.deinit();
    try enc.emitRet();
    try testing.expectEqual(@as(u8, 0xC3), enc.buf.items[0]);
}
