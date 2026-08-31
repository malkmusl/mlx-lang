const Encoder = @import("../encoder.zig").Encoder;
const Reg = @import("../target.zig").Register;

// ── low-level helpers (pointer-based store/load) ─────────────────────────────

/// MOV [ptr_reg], val_reg   (REX.W 89 ModRM(00, val, ptr))
pub fn emitStoreViaPtr(enc: *Encoder, ptr_r: Reg, val_r: Reg) !void {
    const ptr_idx = @intFromEnum(ptr_r);
    const val_idx = @intFromEnum(val_r);
    const needs_rex_r = val_idx >= 8;
    const needs_rex_b = ptr_idx >= 8;
    const rex_byte: u8 = 0x48 |
        (if (needs_rex_r) @as(u8, 0x04) else 0) |
        (if (needs_rex_b) @as(u8, 0x01) else 0);
    const ptr_low: u8 = @as(u8, @truncate(ptr_idx)) & 7;
    const mod_bits: u8 = if (ptr_low == 5) 0b01 else 0b00;
    const modrm_byte: u8 = mod_bits << 6 | (@as(u8, @truncate(val_idx)) & 7) << 3 | ptr_low;
    try enc.buf.append(enc.allocator, rex_byte);
    try enc.buf.append(enc.allocator, 0x89);
    try enc.buf.append(enc.allocator, modrm_byte);
    if (ptr_low == 4) try enc.buf.append(enc.allocator, 0x24); // [rsp]/[r12] SIB
    if (ptr_low == 5) try enc.buf.append(enc.allocator, 0x00); // [rbp]/[r13]+disp8(0)
}

/// MOV byte [ptr_reg], val_reg.low8 (REX 88 /r)
pub fn emitStoreByteViaPtr(enc: *Encoder, ptr_r: Reg, val_r: Reg) !void {
    const ptr_idx = @intFromEnum(ptr_r);
    const val_idx = @intFromEnum(val_r);
    const rex_byte: u8 = 0x40 |
        (if (val_idx >= 8) @as(u8, 0x04) else 0) |
        (if (ptr_idx >= 8) @as(u8, 0x01) else 0);
    const ptr_low: u8 = @as(u8, @truncate(ptr_idx)) & 7;
    const mod_bits: u8 = if (ptr_low == 5) 0b01 else 0b00;
    const modrm_byte: u8 = mod_bits << 6 | (@as(u8, @truncate(val_idx)) & 7) << 3 | ptr_low;
    try enc.buf.append(enc.allocator, rex_byte);
    try enc.buf.appendSlice(enc.allocator, &.{ 0x88, modrm_byte });
    if (ptr_low == 4) try enc.buf.append(enc.allocator, 0x24);
    if (ptr_low == 5) try enc.buf.append(enc.allocator, 0x00);
}

/// MOV dst_reg, [ptr_reg]   (REX.W 8B ModRM(00, dst, ptr))
pub fn emitLoadViaPtr(enc: *Encoder, dst_r: Reg, ptr_r: Reg) !void {
    const ptr_idx = @intFromEnum(ptr_r);
    const dst_idx = @intFromEnum(dst_r);
    const needs_rex_r = dst_idx >= 8;
    const needs_rex_b = ptr_idx >= 8;
    const rex_byte: u8 = 0x48 |
        (if (needs_rex_r) @as(u8, 0x04) else 0) |
        (if (needs_rex_b) @as(u8, 0x01) else 0);
    const ptr_low: u8 = @as(u8, @truncate(ptr_idx)) & 7;
    const mod_bits: u8 = if (ptr_low == 5) 0b01 else 0b00;
    const modrm_byte: u8 = mod_bits << 6 | (@as(u8, @truncate(dst_idx)) & 7) << 3 | ptr_low;
    try enc.buf.append(enc.allocator, rex_byte);
    try enc.buf.append(enc.allocator, 0x8B);
    try enc.buf.append(enc.allocator, modrm_byte);
    if (ptr_low == 4) try enc.buf.append(enc.allocator, 0x24); // [rsp]/[r12] SIB
    if (ptr_low == 5) try enc.buf.append(enc.allocator, 0x00); // [rbp]/[r13]+disp8(0)
}

/// MOVZX dst_reg, byte [ptr_reg] (REX.W 0F B6 /r)
pub fn emitLoadByteViaPtr(enc: *Encoder, dst_r: Reg, ptr_r: Reg) !void {
    const ptr_idx = @intFromEnum(ptr_r);
    const dst_idx = @intFromEnum(dst_r);
    const needs_rex_r = dst_idx >= 8;
    const needs_rex_b = ptr_idx >= 8;
    const rex_byte: u8 = 0x48 |
        (if (needs_rex_r) @as(u8, 0x04) else 0) |
        (if (needs_rex_b) @as(u8, 0x01) else 0);
    const ptr_low: u8 = @as(u8, @truncate(ptr_idx)) & 7;
    const mod_bits: u8 = if (ptr_low == 5) 0b01 else 0b00;
    const modrm_byte: u8 = mod_bits << 6 | (@as(u8, @truncate(dst_idx)) & 7) << 3 | ptr_low;
    try enc.buf.append(enc.allocator, rex_byte);
    try enc.buf.appendSlice(enc.allocator, &.{ 0x0f, 0xb6, modrm_byte });
    if (ptr_low == 4) try enc.buf.append(enc.allocator, 0x24);
    if (ptr_low == 5) try enc.buf.append(enc.allocator, 0x00);
}

/// MOVSX dst_reg, byte [ptr_reg] (REX.W 0F BE /r)
pub fn emitLoadSignedByteViaPtr(enc: *Encoder, dst_r: Reg, ptr_r: Reg) !void {
    const ptr_idx = @intFromEnum(ptr_r);
    const dst_idx = @intFromEnum(dst_r);
    const needs_rex_r = dst_idx >= 8;
    const needs_rex_b = ptr_idx >= 8;
    const rex_byte: u8 = 0x48 |
        (if (needs_rex_r) @as(u8, 0x04) else 0) |
        (if (needs_rex_b) @as(u8, 0x01) else 0);
    const ptr_low: u8 = @as(u8, @truncate(ptr_idx)) & 7;
    const mod_bits: u8 = if (ptr_low == 5) 0b01 else 0b00;
    const modrm_byte: u8 = mod_bits << 6 | (@as(u8, @truncate(dst_idx)) & 7) << 3 | ptr_low;
    try enc.buf.append(enc.allocator, rex_byte);
    try enc.buf.appendSlice(enc.allocator, &.{ 0x0f, 0xbe, modrm_byte });
    if (ptr_low == 4) try enc.buf.append(enc.allocator, 0x24);
    if (ptr_low == 5) try enc.buf.append(enc.allocator, 0x00);
}

/// MOVQ xmm0, src_gp (66 REX.W 0F 6E /r)
pub fn emitMovqXmm0FromGp(enc: *Encoder, src: Reg) !void {
    const source: u8 = @intFromEnum(src);
    try enc.buf.append(enc.allocator, 0x66);
    try enc.buf.append(enc.allocator, 0x48 | (if (source >= 8) @as(u8, 0x01) else 0));
    try enc.buf.appendSlice(enc.allocator, &.{ 0x0f, 0x6e, 0xc0 | (source & 7) });
}

/// MOVQ dst_gp, xmm0 (66 REX.W 0F 7E /r)
pub fn emitMovqGpFromXmm0(enc: *Encoder, destination: Reg) !void {
    const target: u8 = @intFromEnum(destination);
    try enc.buf.append(enc.allocator, 0x66);
    try enc.buf.append(enc.allocator, 0x48 | (if (target >= 8) @as(u8, 0x01) else 0));
    try enc.buf.appendSlice(enc.allocator, &.{ 0x0f, 0x7e, 0xc0 | (target & 7) });
}

/// Copy a compile-time-sized payload from rsi to rdi.
pub fn emitMemoryCopy(enc: *Encoder, size: u32) !void {
    var remaining = size;
    while (remaining >= 8) {
        try emitLoadViaPtr(enc, .rax, .rsi);
        try emitStoreViaPtr(enc, .rdi, .rax);
        remaining -= 8;
        if (remaining > 0) {
            try enc.emitMovRegImm64(.r8, 8);
            try enc.emitAdd(.rsi, .r8);
            try enc.emitAdd(.rdi, .r8);
        }
    }
    while (remaining > 0) : (remaining -= 1) {
        try emitLoadByteViaPtr(enc, .rax, .rsi);
        try emitStoreByteViaPtr(enc, .rdi, .rax);
        if (remaining > 1) {
            try enc.emitMovRegImm64(.r8, 1);
            try enc.emitAdd(.rsi, .r8);
            try enc.emitAdd(.rdi, .r8);
        }
    }
}
