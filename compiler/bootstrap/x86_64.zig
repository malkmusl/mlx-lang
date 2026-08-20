const std = @import("std");

pub const Register = enum(u8) {
    rax = 0,
    rcx = 1,
    rdx = 2,
    rbx = 3,
    rsp = 4,
    rbp = 5,
    rsi = 6,
    rdi = 7,
    r8  = 8,
    r9  = 9,
    r10 = 10,
    r11 = 11,
    r12 = 12,
    r13 = 13,
    r14 = 14,
    r15 = 15,
};

pub const ModRm = packed struct {
    rm: u3,
    reg: u3,
    mod: u2,
};

pub const Rex = packed struct {
    b: u1,
    x: u1,
    r: u1,
    w: u1,
    prefix: u4 = 0b0100,
};

pub fn encodeModRm(mod: u2, reg: u3, rm: u3) u8 {
    return @as(u8, @bitCast(ModRm{ .mod = mod, .reg = reg, .rm = rm }));
}

pub fn encodeRex(w: bool, r: bool, x: bool, b: bool) u8 {
    return @as(u8, @bitCast(Rex{ .w = @intFromBool(w), .r = @intFromBool(r), .x = @intFromBool(x), .b = @intFromBool(b) }));
}
