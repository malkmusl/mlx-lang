const Type = @import("../type.zig").Type;

/// Constants are currently carried in a 64-bit two's-complement envelope.
/// This helper accepts either a non-negative magnitude or a sign-extended
/// negative value when validating concrete signed widths up to 64 bits.
pub fn valueFits(target: Type, value: u64) bool {
    return switch (target.data) {
        .integer => |integer| if (integer.is_signed)
            signedValueFits(integer.bits, value)
        else
            integer.bits >= 64 or value <= ((@as(u64, 1) << @intCast(integer.bits)) - 1),
        .size_int => |integer| if (integer.is_signed) true else true,
        else => false,
    };
}

pub fn isSigned(target: Type) bool {
    return switch (target.data) {
        .integer => |integer| integer.is_signed,
        .size_int => |integer| integer.is_signed,
        else => false,
    };
}

pub fn signed(value: u64, bits: u16) i64 {
    if (bits == 0 or bits >= 64) return @bitCast(value);
    const shift: u6 = @intCast(64 - bits);
    return @as(i64, @bitCast(value << shift)) >> shift;
}

fn signedValueFits(bits: u16, value: u64) bool {
    if (bits == 0) return false;
    if (bits >= 64) return true;
    const positive_max = (@as(u64, 1) << @intCast(bits - 1)) - 1;
    if (value <= positive_max) return true;
    const negative_min: i64 = -(@as(i64, 1) << @intCast(bits - 1));
    return @as(i64, @bitCast(value)) >= negative_min and @as(i64, @bitCast(value)) < 0;
}
