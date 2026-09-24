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
///  UDF — permanently undefined instruction, always raises SIGILL
///
///   31-16              15-0
///   0000000000000000   imm16
///
/// Used purely as a deliberate, distinctly-located debugging trap (not part
/// of the intended program logic): a `udf #0` at a known, unique address
/// lets a real-device crash tombstone's faulting PC be matched back to
/// exactly which call site failed, when there is no adb/logcat access to
/// find that out any other way.
/// ─────────────────────────────────────────────────────────────────────────
pub fn udf(imm16: u16) u32 {
    return imm16;
}

/// ─────────────────────────────────────────────────────────────────────────
///  B — unconditional branch (PC-relative immediate)
///
///   31-26     25-0
///   000101    imm26
///
/// imm26 is a signed word (4-byte) offset from this instruction to target.
/// Verified against `llvm-mc -triple=aarch64 -filetype=obj` +
/// `llvm-objdump -d`: `b .+8` -> 0x14000002, `b .-8` -> 0x17fffffe,
/// `b .+1048572` -> 0x1403ffff — all match.
/// ─────────────────────────────────────────────────────────────────────────
pub fn b(byte_delta: i32) u32 {
    const word_delta: i32 = @divTrunc(byte_delta, 4);
    const imm26: u32 = if (word_delta >= 0) @intCast(word_delta) else @intCast(word_delta + 67108864);
    return (@as(u32, 0b000101) << 26) | (imm26 & 0x3ffffff);
}

/// ─────────────────────────────────────────────────────────────────────────
///  TBZ / TBNZ — test bit and branch (if zero / if not zero)
///
///   31   30-25    24  23-19   18-5      4-0
///   b5   011011   op  b40     imm14     Rt
///
/// op: 0 = TBZ, 1 = TBNZ. The bit position (0-63) is split as b5 (its top
/// bit, distinguishing the W/X register width) and b40 (its low 5 bits).
/// imm14 is a signed word (4-byte) offset from this instruction to target.
/// Verified the same way as `b` above: `tbnz w0, #31, .+8` -> 0x37f80040,
/// `tbz w0, #31, .+8` -> 0x36f80040, `tbnz w9, #0, .+16` -> 0x37000089,
/// `tbnz x9, #33, .+8` -> 0xb7080049 — all match.
/// ─────────────────────────────────────────────────────────────────────────
fn testBranch(op: u32, rt: Reg, bitPos: u32, byte_delta: i32) u32 {
    const word_delta: i32 = @divTrunc(byte_delta, 4);
    const imm14: u32 = if (word_delta >= 0) @intCast(word_delta) else @intCast(word_delta + 16384);
    const b5: u32 = (bitPos >> 5) & 1;
    const b40: u32 = bitPos & 0x1f;
    return (b5 << 31) | (@as(u32, 0b011011) << 25) | (op << 24) | (b40 << 19) | ((imm14 & 0x3fff) << 5) | @as(u32, rt);
}
pub fn tbz(rt: Reg, bitPos: u32, byte_delta: i32) u32 {
    return testBranch(0, rt, bitPos, byte_delta);
}
pub fn tbnz(rt: Reg, bitPos: u32, byte_delta: i32) u32 {
    return testBranch(1, rt, bitPos, byte_delta);
}

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

// Every encoding below was ground-truthed against `llvm-mc -show-encoding`
// (and, for B.cond, `llvm-mc -filetype=obj` + `llvm-objdump -d` to resolve
// the branch offset); the tests at the end of this file check each one.

/// FCVTZS Wd, Sn -- float32 to int32, rounding toward zero.
pub fn fcvtzsWS(rd: Reg, rn: Reg) u32 {
    return 0x1E380000 | (@as(u32, rn) << 5) | @as(u32, rd);
}

/// SUB (shifted register, no shift).
pub fn subReg32(rd: Reg, rn: Reg, rm: Reg) u32 {
    return 0x4B000000 | (@as(u32, rm) << 16) | (@as(u32, rn) << 5) | @as(u32, rd);
}
pub fn subReg64(rd: Reg, rn: Reg, rm: Reg) u32 {
    return 0xCB000000 | (@as(u32, rm) << 16) | (@as(u32, rn) << 5) | @as(u32, rd);
}

/// ADD (shifted register, LSL #shift).
pub fn addRegLsl64(rd: Reg, rn: Reg, rm: Reg, shift: u6) u32 {
    return 0x8B000000 | (@as(u32, rm) << 16) | (@as(u32, shift) << 10) | (@as(u32, rn) << 5) | @as(u32, rd);
}

/// CMP -- alias of SUBS with Rd = WZR/XZR.
pub fn cmpImm32(rn: Reg, imm12: u12) u32 {
    return 0x7100001F | (@as(u32, imm12) << 10) | (@as(u32, rn) << 5);
}
pub fn cmpReg32(rn: Reg, rm: Reg) u32 {
    return 0x6B00001F | (@as(u32, rm) << 16) | (@as(u32, rn) << 5);
}
pub fn cmpReg64(rn: Reg, rm: Reg) u32 {
    return 0xEB00001F | (@as(u32, rm) << 16) | (@as(u32, rn) << 5);
}

/// B.cond -- `01010100 imm19 0 cond`, imm19 a signed word offset.
pub const Cond = enum(u4) { eq = 0, ne = 1, mi = 4, hi = 8, ge = 10, lt = 11, gt = 12 };
pub fn bCond(cond: Cond, byte_delta: i32) u32 {
    const word_delta: i32 = @divTrunc(byte_delta, 4);
    const imm19: u32 = if (word_delta >= 0) @intCast(word_delta) else @intCast(word_delta + 524288);
    return 0x54000000 | ((imm19 & 0x7ffff) << 5) | @as(u32, @intFromEnum(cond));
}

/// CNEG Wd, Wn, cond -- alias of CSNEG Wd, Wn, Wn, invert(cond). Inverting
/// a condition code flips its low bit.
pub fn cnegW(rd: Reg, rn: Reg, cond: Cond) u32 {
    const inv: u32 = @as(u32, @intFromEnum(cond)) ^ 1;
    return 0x5A800400 | (@as(u32, rn) << 16) | (inv << 12) | (@as(u32, rn) << 5) | @as(u32, rd);
}

/// UXTB Wd, Wn -- alias of UBFM Wd, Wn, #0, #7. Also clears Xd's upper 32
/// bits, as every W-register write does.
pub fn uxtbW(rd: Reg, rn: Reg) u32 {
    return 0x53001C00 | (@as(u32, rn) << 5) | @as(u32, rd);
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

test "b (unconditional branch) known encodings, cross-checked against llvm-mc" {
    try std.testing.expectEqual(@as(u32, 0x14000002), b(8));
    try std.testing.expectEqual(@as(u32, 0x17FFFFFE), b(-8));
    try std.testing.expectEqual(@as(u32, 0x1403FFFF), b(1048572));
}

test "tbz/tbnz known encodings, cross-checked against llvm-mc" {
    try std.testing.expectEqual(@as(u32, 0x37F80040), tbnz(0, 31, 8));
    try std.testing.expectEqual(@as(u32, 0x36F80040), tbz(0, 31, 8));
    try std.testing.expectEqual(@as(u32, 0x37000089), tbnz(9, 0, 16));
    try std.testing.expectEqual(@as(u32, 0xB7080049), tbnz(9, 33, 8));
}

test "gesture-code encodings, cross-checked against llvm-mc" {
    try std.testing.expectEqual(@as(u32, 0x1E380000), fcvtzsWS(0, 0));
    try std.testing.expectEqual(@as(u32, 0x1E380029), fcvtzsWS(9, 1));
    try std.testing.expectEqual(@as(u32, 0x4B0B014A), subReg32(10, 10, 11));
    try std.testing.expectEqual(@as(u32, 0x4B050083), subReg32(3, 4, 5));
    try std.testing.expectEqual(@as(u32, 0xCB090000), subReg64(0, 0, 9));
    try std.testing.expectEqual(@as(u32, 0x7100013F), cmpImm32(9, 0));
    try std.testing.expectEqual(@as(u32, 0x7100153F), cmpImm32(9, 5));
    try std.testing.expectEqual(@as(u32, 0x7100615F), cmpImm32(10, 24));
    try std.testing.expectEqual(@as(u32, 0x6B0D019F), cmpReg32(12, 13));
    try std.testing.expectEqual(@as(u32, 0xEB0A001F), cmpReg64(0, 10));
    try std.testing.expectEqual(@as(u32, 0x54000040), bCond(.eq, 8));
    try std.testing.expectEqual(@as(u32, 0x54FFFFC1), bCond(.ne, -8));
    try std.testing.expectEqual(@as(u32, 0x54000088), bCond(.hi, 16));
    try std.testing.expectEqual(@as(u32, 0x5400006B), bCond(.lt, 12));
    try std.testing.expectEqual(@as(u32, 0x540000AC), bCond(.gt, 20));
    try std.testing.expectEqual(@as(u32, 0x54FFFFEA), bCond(.ge, -4));
    try std.testing.expectEqual(@as(u32, 0x547FFFEB), bCond(.lt, 1048572));
    try std.testing.expectEqual(@as(u32, 0x5A8A554A), cnegW(10, 10, .mi));
    try std.testing.expectEqual(@as(u32, 0x5A8D55AC), cnegW(12, 13, .mi));
    try std.testing.expectEqual(@as(u32, 0x53001C00), uxtbW(0, 0));
    try std.testing.expectEqual(@as(u32, 0x53001C0A), uxtbW(10, 0));
    try std.testing.expectEqual(@as(u32, 0x8B0A0929), addRegLsl64(9, 9, 10, 2));
}
