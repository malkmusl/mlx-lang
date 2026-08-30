const std = @import("std");
const lir = @import("../../../ir/lir.zig");
const Encoder = @import("../encoder.zig").Encoder;
const Reg = @import("../target.zig").Register;

pub fn emitBinary(gen: anytype, enc: *Encoder, instruction_index: lir.Inst.Index) !void {
    const instruction = gen.lir.insts.items[instruction_index];
    const pair = operands(instruction);
    const destination = try gen.allocateOp(instruction_index);
    const lhs_operand = try gen.allocateOp(pair.lhs);
    const rhs_operand = try gen.allocateOp(pair.rhs);

    if (instruction.opcode == .div or instruction.opcode == .rem) {
        const lhs_register = try opToRegBin(enc, lhs_operand, .rax);
        const rhs_register = try opToRegBin(enc, rhs_operand, .rcx);
        try enc.emitMovRegReg(.rax, lhs_register);
        try enc.emitMovRegReg(.rcx, rhs_register);
        const signed = isSigned(gen, instruction.type_id);
        if (signed) try enc.emitCqo() else try enc.emitXor(.rdx, .rdx);
        try enc.emitDivide(.rcx, signed);
        const result_register: Reg = if (instruction.opcode == .rem) .rdx else .rax;
        try storeResult(enc, destination, result_register);
        return;
    }

    const lhs_register = try opToRegBin(enc, lhs_operand, .rax);
    const rhs_register = try opToRegBin(enc, rhs_operand, .rcx);
    if (instruction.opcode == .shl or instruction.opcode == .shr) {
        try enc.emitMovRegReg(.rax, lhs_register);
        try enc.emitMovRegReg(.rcx, rhs_register);
        try enc.emitShiftCl(.rax, instruction.opcode == .shl, instruction.opcode == .shr and isSigned(gen, instruction.type_id));
        try storeResult(enc, destination, .rax);
        return;
    }

    const destination_register: Reg = switch (destination) {
        .reg => |name| nameToReg(name),
        .mem => .rax,
    };
    try enc.emitMovRegReg(destination_register, lhs_register);
    switch (instruction.opcode) {
        .bit_and => try enc.emitAnd(destination_register, rhs_register),
        .bit_or => try enc.emitOr(destination_register, rhs_register),
        .bit_xor => try enc.emitXor(destination_register, rhs_register),
        else => unreachable,
    }
    if (destination == .mem) try enc.emitMovMemReg(destination.mem, .rax);
}

pub fn emitText(gen: anytype, writer: anytype, instruction_index: lir.Inst.Index) !void {
    const instruction = gen.lir.insts.items[instruction_index];
    const pair = operands(instruction);
    const destination = try gen.allocateOp(instruction_index);
    const lhs_operand = try gen.allocateOp(pair.lhs);
    const rhs_operand = try gen.allocateOp(pair.rhs);
    const lhs_register = try opToReg(writer, lhs_operand, "rax");
    const rhs_register = try opToReg(writer, rhs_operand, "rcx");
    const result_register = if (destination == .mem) "rax" else destination.reg;
    try writer.print("  mov {s}, {s}\n", .{ result_register, lhs_register });
    switch (instruction.opcode) {
        .bit_and => try writer.print("  and {s}, {s}\n", .{ result_register, rhs_register }),
        .bit_or => try writer.print("  or {s}, {s}\n", .{ result_register, rhs_register }),
        .bit_xor => try writer.print("  xor {s}, {s}\n", .{ result_register, rhs_register }),
        .shl, .shr => {
            try writer.print("  mov rcx, {s}\n", .{rhs_register});
            try writer.print("  {s} {s}, cl\n", .{ if (instruction.opcode == .shl) "shl" else if (isSigned(gen, instruction.type_id)) "sar" else "shr", result_register });
        },
        .div, .rem => {
            try writer.print("  mov rax, {s}\n  mov rcx, {s}\n", .{ lhs_register, rhs_register });
            if (isSigned(gen, instruction.type_id)) try writer.print("  cqo\n  idiv rcx\n", .{}) else try writer.print("  xor rdx, rdx\n  div rcx\n", .{});
            if (instruction.opcode == .rem) try writer.print("  mov {s}, rdx\n", .{result_register}) else if (!std.mem.eql(u8, result_register, "rax")) try writer.print("  mov {s}, rax\n", .{result_register});
        },
        else => unreachable,
    }
    if (destination == .mem) {
        try writer.print("  mov ", .{});
        try printOp(writer, destination);
        try writer.print(", rax\n", .{});
    }
}

fn operands(instruction: lir.Inst) struct { lhs: lir.Inst.Index, rhs: lir.Inst.Index } {
    return switch (instruction.opcode) {
        .div => .{ .lhs = instruction.data.div.lhs, .rhs = instruction.data.div.rhs },
        .rem => .{ .lhs = instruction.data.rem.lhs, .rhs = instruction.data.rem.rhs },
        .bit_and => .{ .lhs = instruction.data.bit_and.lhs, .rhs = instruction.data.bit_and.rhs },
        .bit_or => .{ .lhs = instruction.data.bit_or.lhs, .rhs = instruction.data.bit_or.rhs },
        .bit_xor => .{ .lhs = instruction.data.bit_xor.lhs, .rhs = instruction.data.bit_xor.rhs },
        .shl => .{ .lhs = instruction.data.shl.lhs, .rhs = instruction.data.shl.rhs },
        .shr => .{ .lhs = instruction.data.shr.lhs, .rhs = instruction.data.shr.rhs },
        else => unreachable,
    };
}

fn isSigned(gen: anytype, type_id: u32) bool {
    return switch (gen.type_pool.get(type_id).data) {
        .integer => |integer| integer.is_signed,
        .size_int => |integer| integer.is_signed,
        else => false,
    };
}

fn storeResult(enc: *Encoder, destination: anytype, source: Reg) !void {
    switch (destination) {
        .reg => |name| try enc.emitMovRegReg(nameToReg(name), source),
        .mem => |slot| try enc.emitMovMemReg(slot, source),
    }
}

fn opToRegBin(enc: *Encoder, operand: anytype, scratch: Reg) !Reg {
    return switch (operand) {
        .reg => |name| nameToReg(name),
        .mem => |slot| blk: {
            try enc.emitMovRegMem(scratch, slot);
            break :blk scratch;
        },
    };
}

fn opToReg(writer: anytype, operand: anytype, scratch: []const u8) ![]const u8 {
    return switch (operand) {
        .reg => |name| name,
        .mem => blk: {
            try writer.print("  mov {s}, qword [rbp - {d}]\n", .{ scratch, operand.mem });
            break :blk scratch;
        },
    };
}

fn printOp(writer: anytype, operand: anytype) !void {
    switch (operand) {
        .reg => |name| try writer.print("{s}", .{name}),
        .mem => |slot| try writer.print("qword [rbp - {d}]", .{slot}),
    }
}

fn nameToReg(name: []const u8) Reg {
    inline for (@typeInfo(Reg).@"enum".fields) |field| {
        if (std.mem.eql(u8, name, field.name)) return @enumFromInt(field.value);
    }
    return .rax;
}
