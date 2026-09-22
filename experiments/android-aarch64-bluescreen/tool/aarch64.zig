//! Minimal hand-written AArch64 (A64) instruction encoder.
//!
//! This mirrors the philosophy of `compiler/bootstrap/backend/x86_64/encoder.zig`
//! (part of mlx0's real code generator): no external assembler, no LLVM — every
//! instruction word is built directly from the bit layouts in the Arm
//! Architecture Reference Manual (ARM DDI 0487).
//!
//! Only the instruction forms this experiment's NativeActivity payload needs
//! are implemented. Each encoder function documents the exact bitfield layout
//! it implements so the encoding can be checked by hand against the manual.
//!
//! Every encoding here was cross-checked once, out of band, against `llvm-mc`
//! disassembly during development (see the experiment README) — that tool is
//! not a build- or run-time dependency of this module or of mlx.

const std = @import("std");

/// A general-purpose register number, 0..30, or 31 meaning either the zero
/// register (XZR/WZR) or the stack pointer (SP) depending on instruction
/// class — exactly as the real ISA overloads register 31.
pub const Reg = u5;

pub const sp: Reg = 31;
pub const xzr: Reg = 31;

/// ─────────────────────────────────────────────────────────────────────────
///  MOVZ / MOVK — move (wide immediate)
///
///  31   30-29  28-23    22-21  20-5     4-0
///  sf   opc    100101   hw     imm16    Rd
///
///  opc: 10 = MOVZ, 11 = MOVK, 00 = MOVN
///  hw:  imm16 is logically shifted left by (hw * 16)
/// ─────────────────────────────────────────────────────────────────────────
fn movWide(sf: u1, opc: u2, hw: u2, imm16: u16, rd: Reg) u32 {
    return (@as(u32, sf) << 31) | (@as(u32, opc) << 29) | (@as(u32, 0b100101) << 23) |
        (@as(u32, hw) << 21) | (@as(u32, imm16) << 5) | @as(u32, rd);
}

pub fn movz32(rd: Reg, imm16: u16, hw: u2) u32 {
    return movWide(0, 0b10, hw, imm16, rd);
}
pub fn movz64(rd: Reg, imm16: u16, hw: u2) u32 {
    return movWide(1, 0b10, hw, imm16, rd);
}
pub fn movk32(rd: Reg, imm16: u16, hw: u2) u32 {
    return movWide(0, 0b11, hw, imm16, rd);
}
pub fn movk64(rd: Reg, imm16: u16, hw: u2) u32 {
    return movWide(1, 0b11, hw, imm16, rd);
}

/// Emit the instructions needed to materialize an arbitrary 32-bit immediate
/// into a W register (1 or 2 instructions).
pub fn loadImm32(out: *std.ArrayList(u32), rd: Reg, value: u32) !void {
    const lo: u16 = @truncate(value);
    const hi: u16 = @truncate(value >> 16);
    try out.append(movz32(rd, lo, 0));
    if (hi != 0) try out.append(movk32(rd, hi, 1));
}

/// ─────────────────────────────────────────────────────────────────────────
///  ADRP — form a PC-relative page address
///
///  31  30-29    28-24   23-5     4-0
///  op  immlo    10000   immhi    Rd
///
///  op = 1 for ADRP. The 21-bit signed immediate (immhi:immlo) counts pages
///  (units of 4096 bytes) from the page containing this instruction.
/// ─────────────────────────────────────────────────────────────────────────
pub fn adrp(rd: Reg, pc: u64, target: u64) u32 {
    const pc_page = pc & ~@as(u64, 0xfff);
    const target_page = target & ~@as(u64, 0xfff);
    const delta_pages: i64 = @as(i64, @intCast(target_page)) - @as(i64, @intCast(pc_page));
    const rel_pages: i21 = @intCast(@divExact(delta_pages, 4096));
    const imm21: u21 = @bitCast(rel_pages);
    const immlo: u32 = imm21 & 0b11;
    const immhi: u32 = (imm21 >> 2) & 0x7ffff;
    return (@as(u32, 1) << 31) | (immlo << 29) | (@as(u32, 0b10000) << 24) | (immhi << 5) | @as(u32, rd);
}

/// ─────────────────────────────────────────────────────────────────────────
///  ADD / SUB (immediate)
///
///  31  30  29  28-23    22  21-10   9-5   4-0
///  sf  op  S   100010   sh  imm12   Rn    Rd
///
///  op = 0 ADD, 1 SUB. Rn/Rd = 31 mean SP in this instruction class
///  (unlike most other classes, where 31 means the zero register).
/// ─────────────────────────────────────────────────────────────────────────
fn addSubImm(sf: u1, op: u1, rd: Reg, rn: Reg, imm12: u12) u32 {
    return (@as(u32, sf) << 31) | (@as(u32, op) << 30) | (@as(u32, 0b100010) << 23) |
        (@as(u32, imm12) << 10) | (@as(u32, rn) << 5) | @as(u32, rd);
}
pub fn addImm64(rd: Reg, rn: Reg, imm12: u12) u32 {
    return addSubImm(1, 0, rd, rn, imm12);
}
pub fn subImm64(rd: Reg, rn: Reg, imm12: u12) u32 {
    return addSubImm(1, 1, rd, rn, imm12);
}

/// ─────────────────────────────────────────────────────────────────────────
///  LDR / STR (immediate, unsigned offset)
///
///  31-30  29-27  26  25-24  23-22  21-10   9-5   4-0
///  size   111    V   01     opc    imm12   Rn    Rt
///
///  size: 10 = 32-bit (W), 11 = 64-bit (X). opc: 00 = STR, 01 = LDR.
///  imm12 is an *element count*, scaled by the access size (4 or 8 bytes).
/// ─────────────────────────────────────────────────────────────────────────
fn ldStImm(size: u2, opc: u2, rt: Reg, rn: Reg, byte_offset: u16) u32 {
    const scale: u16 = if (size == 0b11) 8 else 4;
    std.debug.assert(byte_offset % scale == 0);
    const imm12: u12 = @intCast(byte_offset / scale);
    return (@as(u32, size) << 30) | (@as(u32, 0b111) << 27) | (@as(u32, 0b01) << 24) |
        (@as(u32, opc) << 22) | (@as(u32, imm12) << 10) | (@as(u32, rn) << 5) | @as(u32, rt);
}
pub fn ldrW(rt: Reg, rn: Reg, byte_offset: u16) u32 {
    return ldStImm(0b10, 0b01, rt, rn, byte_offset);
}
pub fn ldrX(rt: Reg, rn: Reg, byte_offset: u16) u32 {
    return ldStImm(0b11, 0b01, rt, rn, byte_offset);
}
pub fn strW(rt: Reg, rn: Reg, byte_offset: u16) u32 {
    return ldStImm(0b10, 0b00, rt, rn, byte_offset);
}
pub fn strX(rt: Reg, rn: Reg, byte_offset: u16) u32 {
    return ldStImm(0b11, 0b00, rt, rn, byte_offset);
}

/// ─────────────────────────────────────────────────────────────────────────
///  STR (immediate, post-index) — "STR Wt, [Xn], #simm" — used for the pixel
///  fill loop.
///
///  31-30  29-27  26  25-24  23-22  21  20-12   11-10  9-5   4-0
///  size   111    V   00     opc    0   imm9    01     Rn    Rt
///
///  opc bit1=0(store)/1(load), bit0 here is the high bit of the "signed
///  offset" encoding (00 = STR immediate post/pre-indexed family, opc=00).
/// ─────────────────────────────────────────────────────────────────────────
pub fn strwPostIndex(rt: Reg, rn: Reg, simm9: i9) u32 {
    const imm9: u9 = @bitCast(simm9);
    return (@as(u32, 0b10) << 30) | (@as(u32, 0b111) << 27) | (@as(u32, 0b00) << 24) |
        (@as(u32, 0b00) << 22) | (@as(u32, imm9) << 12) | (@as(u32, 0b01) << 10) |
        (@as(u32, rn) << 5) | @as(u32, rt);
}

/// ─────────────────────────────────────────────────────────────────────────
///  MUL (alias of MADD Xd, Xn, Xm, XZR)
///
///  31  30  29  28-24    23-21  20-16  15  14-10  9-5   4-0
///  sf  0   0   11011    000    Rm     0   Ra     Rn    Rd
/// ─────────────────────────────────────────────────────────────────────────
pub fn mul64(rd: Reg, rn: Reg, rm: Reg) u32 {
    return (@as(u32, 1) << 31) | (@as(u32, 0b11011) << 24) | (@as(u32, rm) << 16) |
        (@as(u32, xzr) << 10) | (@as(u32, rn) << 5) | @as(u32, rd);
}

/// ─────────────────────────────────────────────────────────────────────────
///  CBZ / CBNZ — compare and branch on (non)zero
///
///  31   30-25    24  23-5     4-0
///  sf   011010   op  imm19    Rt
///
///  op = 0 CBZ, 1 CBNZ. imm19 is a signed word (4-byte) offset from this
///  instruction to the target.
/// ─────────────────────────────────────────────────────────────────────────
fn cmpBranch(sf: u1, op: u1, rt: Reg, byte_delta: i32) u32 {
    std.debug.assert(@rem(byte_delta, 4) == 0);
    const word_delta: i32 = @divExact(byte_delta, 4);
    const imm19: u19 = @bitCast(@as(i19, @intCast(word_delta)));
    return (@as(u32, sf) << 31) | (@as(u32, 0b011010) << 25) | (@as(u32, op) << 24) |
        (@as(u32, imm19) << 5) | @as(u32, rt);
}
pub fn cbzX(rt: Reg, byte_delta: i32) u32 {
    return cmpBranch(1, 0, rt, byte_delta);
}
pub fn cbnzX(rt: Reg, byte_delta: i32) u32 {
    return cmpBranch(1, 1, rt, byte_delta);
}
pub fn cbzW(rt: Reg, byte_delta: i32) u32 {
    return cmpBranch(0, 0, rt, byte_delta);
}
pub fn cbnzW(rt: Reg, byte_delta: i32) u32 {
    return cmpBranch(0, 1, rt, byte_delta);
}

/// ─────────────────────────────────────────────────────────────────────────
///  Unconditional branch (register) — BR / BLR / RET
///
///  Fixed hex bases (well-known, stable A64 encodings):
///    BR  Xn = 0xD61F0000 | (Rn << 5)
///    BLR Xn = 0xD63F0000 | (Rn << 5)
///    RET Xn = 0xD65F0000 | (Rn << 5)   (Rn defaults to X30/LR)
/// ─────────────────────────────────────────────────────────────────────────
pub fn br(rn: Reg) u32 {
    return 0xD61F0000 | (@as(u32, rn) << 5);
}
pub fn blr(rn: Reg) u32 {
    return 0xD63F0000 | (@as(u32, rn) << 5);
}
pub fn ret(rn: Reg) u32 {
    return 0xD65F0000 | (@as(u32, rn) << 5);
}
pub const lr: Reg = 30;

/// ─────────────────────────────────────────────────────────────────────────
///  MOV (register) — alias of ORR Xd, XZR, Xm  (Logical, shifted register)
///
///  31  30-29  28-24    23-22  21  20-16  15-10  9-5   4-0
///  sf  opc    01010    shift  N   Rm     imm6   Rn    Rd
///
///  opc = 01 (ORR), shift = 00, N = 0, Rn = 31 (XZR here, not SP).
/// ─────────────────────────────────────────────────────────────────────────
pub fn movReg64(rd: Reg, rm: Reg) u32 {
    return (@as(u32, 1) << 31) | (@as(u32, 0b01) << 29) | (@as(u32, 0b01010) << 24) |
        (@as(u32, rm) << 16) | (@as(u32, xzr) << 5) | @as(u32, rd);
}

// ───────────────────────────────────────────────────────────────────────────
//  Self-tests: every encoder above checked against known-good, widely
//  documented A64 hex encodings (the same constants any AArch64 disassembler
//  would print). `zig test aarch64.zig` runs these with no external tools.
// ───────────────────────────────────────────────────────────────────────────

test "movz/movk known encodings" {
    try std.testing.expectEqual(@as(u32, 0xD2824680), movz64(0, 0x1234, 0));
    try std.testing.expectEqual(@as(u32, 0xF2A00020), movk64(0, 1, 1));
}

test "add/sub immediate known encodings" {
    try std.testing.expectEqual(@as(u32, 0x91000000), addImm64(0, 0, 0));
    try std.testing.expectEqual(@as(u32, 0xD10043FF), subImm64(sp, sp, 16));
}

test "adrp known encodings (cross-checked against llvm-mc)" {
    // adrp x0, #0
    try std.testing.expectEqual(@as(u32, 0x90000000), adrp(0, 0x1000, 0x1000));
    // adrp x0, #4096  (one page forward: encoded in the *immlo* field, not
    // immhi, since the 21-bit page-count's low 2 bits sit at bits 30-29)
    try std.testing.expectEqual(@as(u32, 0xB0000000), adrp(0, 0x1000, 0x2000));
    // adrp x9, #-4096  (one page back)
    try std.testing.expectEqual(@as(u32, 0xF0FFFFE9), adrp(9, 0x1000, 0x0000));
}

test "ldr/str immediate known encodings" {
    // ldr x0, [x1]
    try std.testing.expectEqual(@as(u32, 0xF9400020), ldrX(0, 1, 0));
    // str x0, [x1, #8]
    try std.testing.expectEqual(@as(u32, 0xF9000420), strX(0, 1, 8));
    // ldr w0, [x1, #4]
    try std.testing.expectEqual(@as(u32, 0xB9400420), ldrW(0, 1, 4));
}

test "mul known encoding" {
    // mul x0, x1, x2
    try std.testing.expectEqual(@as(u32, 0x9B027C20), mul64(0, 1, 2));
}

test "cbz/cbnz known encodings" {
    // cbnz x9, #-8  (loop back two instructions)
    try std.testing.expectEqual(@as(u32, 0xB5FFFFC9), cbnzX(9, -8));
    // cbz x0, #8
    try std.testing.expectEqual(@as(u32, 0xB4000040), cbzX(0, 8));
}

test "branch register known encodings" {
    try std.testing.expectEqual(@as(u32, 0xD65F03C0), ret(lr));
    try std.testing.expectEqual(@as(u32, 0xD63F0120), blr(9));
}

test "mov register known encoding" {
    // mov x0, x1
    try std.testing.expectEqual(@as(u32, 0xAA0103E0), movReg64(0, 1));
}

test "strw post-index known encoding" {
    // str w9, [x8], #4
    try std.testing.expectEqual(@as(u32, 0xB8004509), strwPostIndex(9, 8, 4));
}
