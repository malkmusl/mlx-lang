/// zincc ABI — System V AMD64 calling convention implementation
/// Used by zin0 bootstrap for Linux x86_64.
///
/// Rules (from psABI spec):
///   - Integer/pointer args 1-6 → rdi, rsi, rdx, rcx, r8, r9
///   - Float/SSE args 1-8 → xmm0..xmm7
///   - Further args → pushed on stack right-to-left
///   - Return value:
///       scalar ≤ 64-bit     → rax
///       scalar 65-128 bit   → rax (lo) + rdx (hi)
///       float               → xmm0
///   - Caller-saved: rax, rcx, rdx, rsi, rdi, r8-r11, xmm0-xmm15
///   - Callee-saved: rbx, rbp, r12-r15

const std = @import("std");
const Type = @import("type.zig").Type;

// ──────────────────────────────────────────
//  Register name tables
// ──────────────────────────────────────────

/// Integer argument registers in order (System V AMD64)
pub const integer_arg_regs = [6][]const u8{
    "rdi", "rsi", "rdx", "rcx", "r8", "r9",
};

/// Registers preserved across a call (callee-saved)
pub const callee_saved_regs = [5][]const u8{
    "rbx", "r12", "r13", "r14", "r15",
};

/// Registers clobbered by a call (caller-saved, not counting args/return)
pub const caller_saved_regs = [7][]const u8{
    "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9",
    // r10, r11 are also caller-saved but used as scratch
};

/// The primary integer return register
pub const return_reg = "rax";

/// The secondary return register (for 128-bit returns)
pub const return_reg_hi = "rdx";

// ──────────────────────────────────────────
//  Argument classification
// ──────────────────────────────────────────

pub const ArgClass = enum {
    /// Fits in a GP register
    INTEGER,
    /// Fits in an SSE register
    SSE,
    /// Passed on stack
    MEMORY,
    /// No-class (e.g., void)
    NO_CLASS,
    /// Upper half of SSE pair
    SSEUP,
};

/// Classify how a Zin type is passed according to System V AMD64 ABI.
/// For Stage 0 we only need INTEGER and NO_CLASS (void).
pub fn classifyType(ty: Type) ArgClass {
    return switch (ty.data) {
        .primitive => |prim| switch (prim) {
            .void_type, .noreturn_type, .null_type, .undefined_type => .NO_CLASS,
            .bool_type,
            .comptime_int_type,
            .comptime_float_type,
            .type_type,
            .anytype_type,
            .anyopaque_type,
            => .INTEGER,
            // Float types → SSE registers
            .f16_type, .f32_type, .f64_type, .f80_type, .f128_type => .SSE,
        },
        .integer => .INTEGER,
        .size_int => .INTEGER, // usize/isize — pointer-width integer
        .pointer => .INTEGER,
        .function => .INTEGER, // function pointer
        .optional => .INTEGER, // small optional fits in register (stage 0 simplification)
        // Aggregates: simplification — pass as MEMORY for now
        else => .MEMORY,
    };
}

// ──────────────────────────────────────────
//  Call site descriptor
// ──────────────────────────────────────────

pub const CallSite = struct {
    /// How many integer register args
    int_reg_count: u8,
    /// How many stack args
    stack_count: u8,
    /// Total stack bytes needed for stack args (aligned to 8)
    stack_bytes: u32,
    /// Whether caller must align stack to 16 before call
    needs_stack_align: bool,
};

/// Compute a CallSite descriptor given argument count and their classes.
/// This tells the code generator how to set up the call.
pub fn describeCall(arg_classes: []const ArgClass) CallSite {
    var int_regs: u8 = 0;
    var stack: u8 = 0;
    for (arg_classes) |cls| {
        switch (cls) {
            .INTEGER => {
                if (int_regs < 6) {
                    int_regs += 1;
                } else {
                    stack += 1;
                }
            },
            .SSE, .SSEUP => {
                // For now, treat as stack (float support stage 2+)
                stack += 1;
            },
            .MEMORY => {
                stack += 1;
            },
            .NO_CLASS => {},
        }
    }
    const stack_bytes: u32 = @as(u32, stack) * 8;
    // We need 16-byte alignment before the call instruction
    const needs_align = (stack_bytes % 16) != 0;
    return .{
        .int_reg_count = int_regs,
        .stack_count = stack,
        .stack_bytes = stack_bytes,
        .needs_stack_align = needs_align,
    };
}

// ──────────────────────────────────────────
//  Frame layout helpers
// ──────────────────────────────────────────

/// Compute a stack frame size aligned to 16 bytes.
/// `local_bytes` is the number of bytes needed for local variables.
pub fn alignedFrameSize(local_bytes: u32) u32 {
    const aligned = (local_bytes + 15) & ~@as(u32, 15);
    return if (aligned == 0) 16 else aligned;
}

// ──────────────────────────────────────────
//  Tests
// ──────────────────────────────────────────

test "classify integer type" {
    const ty = Type{
        .data = .{ .integer = .{ .is_signed = true, .bits = 64 } },
        .copyability = .copyable,
    };
    try std.testing.expectEqual(ArgClass.INTEGER, classifyType(ty));
}

test "classify void type" {
    const ty = Type{
        .data = .{ .primitive = .void_type },
        .copyability = .copyable,
    };
    try std.testing.expectEqual(ArgClass.NO_CLASS, classifyType(ty));
}

test "describeCall 2 int args" {
    const classes = [_]ArgClass{ .INTEGER, .INTEGER };
    const site = describeCall(&classes);
    try std.testing.expectEqual(@as(u8, 2), site.int_reg_count);
    try std.testing.expectEqual(@as(u8, 0), site.stack_count);
}

test "describeCall 7 int args — 6 reg, 1 stack" {
    const classes = [_]ArgClass{ .INTEGER, .INTEGER, .INTEGER, .INTEGER, .INTEGER, .INTEGER, .INTEGER };
    const site = describeCall(&classes);
    try std.testing.expectEqual(@as(u8, 6), site.int_reg_count);
    try std.testing.expectEqual(@as(u8, 1), site.stack_count);
    try std.testing.expectEqual(@as(u32, 8), site.stack_bytes);
}

test "alignedFrameSize" {
    try std.testing.expectEqual(@as(u32, 16), alignedFrameSize(0));
    try std.testing.expectEqual(@as(u32, 16), alignedFrameSize(8));
    try std.testing.expectEqual(@as(u32, 32), alignedFrameSize(24));
    try std.testing.expectEqual(@as(u32, 256), alignedFrameSize(250));
}
