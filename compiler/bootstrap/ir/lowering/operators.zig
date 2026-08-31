const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Tag = @import("../../syntax/token.zig").Token.Tag;
const syntax_operators = @import("../../syntax/parser/operators.zig");
const semantic_operators = @import("../../semantic/expressions/operators.zig");
const Type = @import("../../semantic/type.zig").Type;
const lir = @import("../lir.zig");
const Inst = lir.Inst;
const lvalue = @import("lvalue.zig");

const Core = enum { add, sub, mul, div, rem, bit_and, bit_or, bit_xor, shl, shr };
const Policy = enum { checked, wrapping, saturating };
const CombineOrder = enum { original_first, shifted_first };
const ShiftSpec = struct {
    direction: Core,
    combiner: ?Core = null,
    order: CombineOrder = .original_first,
    wrapping_count: bool = false,
    saturating_result: bool = false,
    combiner_policy: Policy = .checked,
};

pub fn lower(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    const tag = builder.sema.ast_tree.tokens[node.main_token].tag;
    if (syntax_operators.isAssignment(tag)) return lowerAssignment(builder, node_index, tag);
    if (semantic_operators.isLogical(tag)) return lowerLogical(builder, node_index, tag);

    const lhs = try builder.lowerNode(node.data.lhs) orelse return null;
    const rhs = try builder.lowerNode(node.data.rhs) orelse return null;
    const type_id = builder.sema.node_types.get(node_index) orelse return null;
    if (semantic_operators.isComparison(tag)) {
        const result = try emitComparison(builder, tag, lhs, rhs, type_id);
        return result;
    }
    if (shiftSpec(tag)) |spec| {
        const result = try emitShift(builder, spec, lhs, rhs, type_id);
        return result;
    }
    const core = coreForToken(tag) orelse return null;
    const result = try emitPolicy(builder, core, policyForToken(tag), lhs, rhs, type_id);
    return result;
}

fn lowerAssignment(builder: anytype, node_index: Node.Index, assignment: Tag) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    const address = try lvalue.lowerAddress(builder, node.data.lhs) orelse return null;
    const value = try builder.lowerNode(node.data.rhs) orelse return null;
    const target_type = builder.sema.node_types.get(node.data.lhs) orelse return null;
    const stored_value = if (assignment == .equal)
        value
    else blk: {
        const current = try builder.emitInst(.{ .opcode = .load, .type_id = target_type, .data = .{ .load = .{ .ptr = address } } });
        const operator = assignmentOperator(assignment) orelse return null;
        if (shiftSpec(operator)) |spec| break :blk try emitShift(builder, spec, current, value, target_type);
        break :blk try emitPolicy(builder, coreForToken(operator) orelse return null, policyForToken(operator), current, value, target_type);
    };
    _ = try builder.emitInst(.{
        .opcode = .store,
        .type_id = target_type,
        .data = .{ .store = .{ .ptr = address, .val = stored_value } },
    });
    return null;
}

fn lowerLogical(builder: anytype, node_index: Node.Index, tag: Tag) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    const lhs = try builder.lowerNode(node.data.lhs) orelse return null;
    const bool_type = builder.sema.node_types.get(node_index) orelse return null;
    const rhs_block = try builder.newBlock();
    const end_block = try builder.newBlock();
    const result_address = try builder.emitInst(.{
        .opcode = .addr,
        .type_id = bool_type,
        .data = .{ .addr = 0xe000_0000 | node_index },
    });
    const default_value = try builder.emitInst(.{
        .opcode = .const_i,
        .type_id = bool_type,
        .data = .{ .const_i = @intFromBool(tag == .pipe_pipe) },
    });
    _ = try builder.emitInst(.{
        .opcode = .store,
        .type_id = bool_type,
        .data = .{ .store = .{ .ptr = result_address, .val = default_value } },
    });
    _ = try builder.emitInst(.{
        .opcode = .condbr,
        .type_id = bool_type,
        .data = .{ .condbr = if (tag == .ampersand_ampersand)
            .{ .cond = lhs, .true_dest = rhs_block, .false_dest = end_block }
        else
            .{ .cond = lhs, .true_dest = end_block, .false_dest = rhs_block } },
    });

    builder.current_block = rhs_block;
    const rhs = try builder.lowerNode(node.data.rhs) orelse return null;
    _ = try builder.emitInst(.{
        .opcode = .store,
        .type_id = bool_type,
        .data = .{ .store = .{ .ptr = result_address, .val = rhs } },
    });
    _ = try builder.emitInst(.{ .opcode = .br, .type_id = bool_type, .data = .{ .br = .{ .dest = end_block } } });
    builder.current_block = end_block;
    const result = try builder.emitInst(.{ .opcode = .load, .type_id = bool_type, .data = .{ .load = .{ .ptr = result_address } } });
    return result;
}

fn emitComparison(builder: anytype, tag: Tag, lhs: Inst.Index, rhs: Inst.Index, type_id: Type.Id) !Inst.Index {
    const lhs_type = builder.sema.type_pool.get(builder.lir.insts.items[lhs].type_id);
    const rhs_type = builder.sema.type_pool.get(builder.lir.insts.items[rhs].type_id);
    const signed = isSignedInteger(lhs_type) or (isComptimeInteger(lhs_type) and isSignedInteger(rhs_type));
    const predicate: lir.CmpPredicate = switch (tag) {
        .equal_equal => .eq,
        .bang_equal => .ne,
        .angle_bracket_left => if (signed) .lt else .ult,
        .angle_bracket_left_equal => if (signed) .le else .ule,
        .angle_bracket_right => if (signed) .gt else .ugt,
        .angle_bracket_right_equal => if (signed) .ge else .uge,
        else => unreachable,
    };
    return builder.emitInst(.{ .opcode = .icmp, .type_id = type_id, .data = .{ .icmp = .{ .predicate = predicate, .lhs = lhs, .rhs = rhs } } });
}

fn emitShift(builder: anytype, spec: ShiftSpec, lhs: Inst.Index, rhs: Inst.Index, type_id: Type.Id) !Inst.Index {
    var count = rhs;
    if (spec.wrapping_count) {
        const width = bitWidth(builder, type_id);
        const width_value = try builder.emitInst(.{ .opcode = .const_i, .type_id = type_id, .data = .{ .const_i = width } });
        count = try emitCore(builder, .rem, rhs, width_value, type_id);
    } else {
        try emitCheckedShiftCount(builder, rhs, type_id);
    }
    const shifted = if (spec.saturating_result)
        try emitPolicy(builder, spec.direction, .saturating, lhs, count, type_id)
    else
        try emitPolicy(builder, spec.direction, .checked, lhs, count, type_id);
    const combiner = spec.combiner orelse return shifted;
    return switch (spec.order) {
        .original_first => emitPolicy(builder, combiner, spec.combiner_policy, lhs, shifted, type_id),
        .shifted_first => emitPolicy(builder, combiner, spec.combiner_policy, shifted, lhs, type_id),
    };
}

fn emitPolicy(builder: anytype, core: Core, policy: Policy, lhs: Inst.Index, rhs: Inst.Index, type_id: Type.Id) !Inst.Index {
    if (core == .div or core == .rem) try emitNonZeroCheck(builder, rhs, type_id);
    const raw = try emitCore(builder, core, lhs, rhs, type_id);
    if (policy == .wrapping) return maskToWidth(builder, raw, type_id);
    const type_value = builder.sema.type_pool.get(type_id);
    const integer = if (type_value.data == .integer) type_value.data.integer else return raw;
    if (integer.is_signed or integer.bits >= 64 or integer.bits == 0) return raw;
    const max_value = (@as(u64, 1) << @intCast(integer.bits)) - 1;
    const max = try builder.emitInst(.{ .opcode = .const_i, .type_id = type_id, .data = .{ .const_i = max_value } });
    const bool_type = try builder.sema.type_pool.internPrimitive(.bool_type);
    const overflow = if (core == .sub)
        try builder.emitInst(.{ .opcode = .icmp, .type_id = bool_type, .data = .{ .icmp = .{ .predicate = .ult, .lhs = lhs, .rhs = rhs } } })
    else if (core == .add or core == .mul or core == .shl)
        try builder.emitInst(.{ .opcode = .icmp, .type_id = bool_type, .data = .{ .icmp = .{ .predicate = .ugt, .lhs = raw, .rhs = max } } })
    else
        return raw;
    if (policy == .checked) {
        try emitTrapIf(builder, overflow);
        return raw;
    }
    return selectSaturated(builder, overflow, raw, if (core == .sub) null else max, type_id);
}

fn maskToWidth(builder: anytype, value: Inst.Index, type_id: Type.Id) !Inst.Index {
    const type_value = builder.sema.type_pool.get(type_id);
    const width: u16 = if (type_value.data == .integer) type_value.data.integer.bits else return value;
    if (width == 0 or width >= 64) return value;
    const mask_value = (@as(u64, 1) << @intCast(width)) - 1;
    const mask = try builder.emitInst(.{ .opcode = .const_i, .type_id = type_id, .data = .{ .const_i = mask_value } });
    return emitCore(builder, .bit_and, value, mask, type_id);
}

fn selectSaturated(builder: anytype, overflow: Inst.Index, raw: Inst.Index, upper: ?Inst.Index, type_id: Type.Id) !Inst.Index {
    const overflow_block = try builder.newBlock();
    const merge_block = try builder.newBlock();
    const address = try builder.emitInst(.{ .opcode = .addr, .type_id = type_id, .data = .{ .addr = builder.nextSyntheticLocal() } });
    _ = try builder.emitInst(.{ .opcode = .store, .type_id = type_id, .data = .{ .store = .{ .ptr = address, .val = raw } } });
    _ = try builder.emitInst(.{ .opcode = .condbr, .type_id = 0, .data = .{ .condbr = .{ .cond = overflow, .true_dest = overflow_block, .false_dest = merge_block } } });
    builder.current_block = overflow_block;
    const bound = upper orelse try builder.emitInst(.{ .opcode = .const_i, .type_id = type_id, .data = .{ .const_i = 0 } });
    _ = try builder.emitInst(.{ .opcode = .store, .type_id = type_id, .data = .{ .store = .{ .ptr = address, .val = bound } } });
    _ = try builder.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = merge_block } } });
    builder.current_block = merge_block;
    return builder.emitInst(.{ .opcode = .load, .type_id = type_id, .data = .{ .load = .{ .ptr = address } } });
}

fn emitNonZeroCheck(builder: anytype, rhs: Inst.Index, type_id: Type.Id) !void {
    const zero = try builder.emitInst(.{ .opcode = .const_i, .type_id = type_id, .data = .{ .const_i = 0 } });
    const bool_type = try builder.sema.type_pool.internPrimitive(.bool_type);
    const invalid = try builder.emitInst(.{ .opcode = .icmp, .type_id = bool_type, .data = .{ .icmp = .{ .predicate = .eq, .lhs = rhs, .rhs = zero } } });
    try emitTrapIf(builder, invalid);
}

fn emitCheckedShiftCount(builder: anytype, count: Inst.Index, type_id: Type.Id) !void {
    const width = bitWidth(builder, type_id);
    if (width == 0) return;
    const width_value = try builder.emitInst(.{ .opcode = .const_i, .type_id = type_id, .data = .{ .const_i = width } });
    const bool_type = try builder.sema.type_pool.internPrimitive(.bool_type);
    const invalid = try builder.emitInst(.{ .opcode = .icmp, .type_id = bool_type, .data = .{ .icmp = .{ .predicate = .uge, .lhs = count, .rhs = width_value } } });
    try emitTrapIf(builder, invalid);
}

fn emitTrapIf(builder: anytype, condition: Inst.Index) !void {
    const trap_block = try builder.newBlock();
    const continue_block = try builder.newBlock();
    _ = try builder.emitInst(.{ .opcode = .condbr, .type_id = 0, .data = .{ .condbr = .{ .cond = condition, .true_dest = trap_block, .false_dest = continue_block } } });
    builder.current_block = trap_block;
    _ = try builder.emitInst(.{ .opcode = .unreachable_inst, .type_id = 0, .data = .{ .unreachable_inst = {} } });
    builder.current_block = continue_block;
}

fn emitCore(builder: anytype, core: Core, lhs: Inst.Index, rhs: Inst.Index, type_id: Type.Id) !Inst.Index {
    return builder.emitInst(switch (core) {
        .add => .{ .opcode = .add, .type_id = type_id, .data = .{ .add = .{ .lhs = lhs, .rhs = rhs } } },
        .sub => .{ .opcode = .sub, .type_id = type_id, .data = .{ .sub = .{ .lhs = lhs, .rhs = rhs } } },
        .mul => .{ .opcode = .mul, .type_id = type_id, .data = .{ .mul = .{ .lhs = lhs, .rhs = rhs } } },
        .div => .{ .opcode = .div, .type_id = type_id, .data = .{ .div = .{ .lhs = lhs, .rhs = rhs } } },
        .rem => .{ .opcode = .rem, .type_id = type_id, .data = .{ .rem = .{ .lhs = lhs, .rhs = rhs } } },
        .bit_and => .{ .opcode = .bit_and, .type_id = type_id, .data = .{ .bit_and = .{ .lhs = lhs, .rhs = rhs } } },
        .bit_or => .{ .opcode = .bit_or, .type_id = type_id, .data = .{ .bit_or = .{ .lhs = lhs, .rhs = rhs } } },
        .bit_xor => .{ .opcode = .bit_xor, .type_id = type_id, .data = .{ .bit_xor = .{ .lhs = lhs, .rhs = rhs } } },
        .shl => .{ .opcode = .shl, .type_id = type_id, .data = .{ .shl = .{ .lhs = lhs, .rhs = rhs } } },
        .shr => .{ .opcode = .shr, .type_id = type_id, .data = .{ .shr = .{ .lhs = lhs, .rhs = rhs } } },
    });
}

fn coreForToken(tag: Tag) ?Core {
    return switch (tag) {
        .plus, .plus_percent, .plus_pipe => .add,
        .minus, .minus_percent, .minus_pipe => .sub,
        .asterisk, .asterisk_percent, .asterisk_pipe => .mul,
        .slash => .div,
        .percent => .rem,
        .ampersand => .bit_and,
        .pipe => .bit_or,
        .caret => .bit_xor,
        else => null,
    };
}

fn policyForToken(tag: Tag) Policy {
    return switch (tag) {
        .plus_percent, .minus_percent, .asterisk_percent => .wrapping,
        .plus_pipe, .minus_pipe, .asterisk_pipe => .saturating,
        else => .checked,
    };
}

fn shiftSpec(tag: Tag) ?ShiftSpec {
    return switch (tag) {
        .shl => .{ .direction = .shl },
        .shr => .{ .direction = .shr },
        .shl_percent => .{ .direction = .shl, .wrapping_count = true },
        .shr_percent => .{ .direction = .shr, .wrapping_count = true },
        .shl_pipe, .shl_percent_pipe => .{ .direction = .shl, .wrapping_count = tag == .shl_percent_pipe, .saturating_result = true },
        .ampersand_shl => .{ .direction = .shl, .combiner = .bit_and },
        .pipe_shl => .{ .direction = .shl, .combiner = .bit_or },
        .caret_shl => .{ .direction = .shl, .combiner = .bit_xor },
        .ampersand_shr => .{ .direction = .shr, .combiner = .bit_and },
        .pipe_shr => .{ .direction = .shr, .combiner = .bit_or },
        .caret_shr => .{ .direction = .shr, .combiner = .bit_xor },
        .shl_ampersand => .{ .direction = .shl, .combiner = .bit_and, .order = .shifted_first },
        .shl_caret => .{ .direction = .shl, .combiner = .bit_xor, .order = .shifted_first },
        .shr_ampersand => .{ .direction = .shr, .combiner = .bit_and, .order = .shifted_first },
        .shr_pipe => .{ .direction = .shr, .combiner = .bit_or, .order = .shifted_first },
        .shr_caret => .{ .direction = .shr, .combiner = .bit_xor, .order = .shifted_first },
        .plus_shl, .plus_percent_shl, .plus_pipe_shl => .{ .direction = .shl, .combiner = .add, .combiner_policy = policyForToken(if (tag == .plus_percent_shl) .plus_percent else if (tag == .plus_pipe_shl) .plus_pipe else .plus) },
        .minus_shl, .minus_percent_shl, .minus_pipe_shl => .{ .direction = .shl, .combiner = .sub, .combiner_policy = policyForToken(if (tag == .minus_percent_shl) .minus_percent else if (tag == .minus_pipe_shl) .minus_pipe else .minus) },
        .asterisk_shl, .asterisk_percent_shl, .asterisk_pipe_shl => .{ .direction = .shl, .combiner = .mul, .combiner_policy = policyForToken(if (tag == .asterisk_percent_shl) .asterisk_percent else if (tag == .asterisk_pipe_shl) .asterisk_pipe else .asterisk) },
        .plus_shr, .plus_percent_shr, .plus_pipe_shr => .{ .direction = .shr, .combiner = .add, .combiner_policy = policyForToken(if (tag == .plus_percent_shr) .plus_percent else if (tag == .plus_pipe_shr) .plus_pipe else .plus) },
        .minus_shr, .minus_percent_shr, .minus_pipe_shr => .{ .direction = .shr, .combiner = .sub, .combiner_policy = policyForToken(if (tag == .minus_percent_shr) .minus_percent else if (tag == .minus_pipe_shr) .minus_pipe else .minus) },
        .asterisk_shr, .asterisk_percent_shr, .asterisk_pipe_shr => .{ .direction = .shr, .combiner = .mul, .combiner_policy = policyForToken(if (tag == .asterisk_percent_shr) .asterisk_percent else if (tag == .asterisk_pipe_shr) .asterisk_pipe else .asterisk) },
        else => null,
    };
}

fn assignmentOperator(tag: Tag) ?Tag {
    return switch (tag) {
        .plus_equal => .plus,
        .minus_equal => .minus,
        .asterisk_equal => .asterisk,
        .slash_equal => .slash,
        .percent_equal => .percent,
        .plus_percent_equal => .plus_percent,
        .minus_percent_equal => .minus_percent,
        .asterisk_percent_equal => .asterisk_percent,
        .plus_pipe_equal => .plus_pipe,
        .minus_pipe_equal => .minus_pipe,
        .asterisk_pipe_equal => .asterisk_pipe,
        .ampersand_equal => .ampersand,
        .pipe_equal => .pipe,
        .caret_equal => .caret,
        .shl_equal => .shl,
        .shr_equal => .shr,
        .shl_percent_equal => .shl_percent,
        .shr_percent_equal => .shr_percent,
        .shl_pipe_equal => .shl_pipe,
        .shl_percent_pipe_equal => .shl_percent_pipe,
        .ampersand_shl_equal => .ampersand_shl,
        .pipe_shl_equal => .pipe_shl,
        .caret_shl_equal => .caret_shl,
        .ampersand_shr_equal => .ampersand_shr,
        .pipe_shr_equal => .pipe_shr,
        .caret_shr_equal => .caret_shr,
        .shl_ampersand_equal => .shl_ampersand,
        .shl_caret_equal => .shl_caret,
        .shr_ampersand_equal => .shr_ampersand,
        .shr_pipe_equal => .shr_pipe,
        .shr_caret_equal => .shr_caret,
        .plus_shl_equal => .plus_shl,
        .minus_shl_equal => .minus_shl,
        .asterisk_shl_equal => .asterisk_shl,
        .plus_percent_shl_equal => .plus_percent_shl,
        .minus_percent_shl_equal => .minus_percent_shl,
        .asterisk_percent_shl_equal => .asterisk_percent_shl,
        .plus_pipe_shl_equal => .plus_pipe_shl,
        .minus_pipe_shl_equal => .minus_pipe_shl,
        .asterisk_pipe_shl_equal => .asterisk_pipe_shl,
        .plus_shr_equal => .plus_shr,
        .minus_shr_equal => .minus_shr,
        .asterisk_shr_equal => .asterisk_shr,
        .plus_percent_shr_equal => .plus_percent_shr,
        .minus_percent_shr_equal => .minus_percent_shr,
        .asterisk_percent_shr_equal => .asterisk_percent_shr,
        .plus_pipe_shr_equal => .plus_pipe_shr,
        .minus_pipe_shr_equal => .minus_pipe_shr,
        .asterisk_pipe_shr_equal => .asterisk_pipe_shr,
        else => null,
    };
}

fn bitWidth(builder: anytype, type_id: Type.Id) u64 {
    return switch (builder.sema.type_pool.get(type_id).data) {
        .integer => |integer| integer.bits,
        .size_int, .pointer => 64,
        else => 64,
    };
}

fn isSignedInteger(value: Type) bool {
    return switch (value.data) {
        .integer => |integer| integer.is_signed,
        .size_int => |integer| integer.is_signed,
        else => false,
    };
}

fn isComptimeInteger(value: Type) bool {
    return value.data == .primitive and value.data.primitive == .comptime_int_type;
}
