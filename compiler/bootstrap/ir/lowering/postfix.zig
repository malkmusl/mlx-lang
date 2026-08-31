const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Type = @import("../../semantic/type.zig").Type;
const Inst = @import("../lir.zig").Inst;
const lvalue = @import("lvalue.zig");

pub fn lower(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    return switch (node.tag) {
        .array_access => lowerIndex(builder, node_index),
        .slice => lowerSlice(builder, node_index),
        else => unreachable,
    };
}

pub fn lowerUnarySuffix(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    const operator = builder.sema.ast_tree.tokens[node.main_token].tag;
    if (operator == .dot_question) return lowerOptionalUnwrap(builder, node_index);
    if (operator != .dot_asterisk) return null;
    const pointer = try builder.lowerNode(node.data.lhs) orelse return null;
    const result = try builder.emitInst(.{
        .opcode = .load,
        .type_id = builder.sema.node_types.get(node_index) orelse return null,
        .data = .{ .load = .{ .ptr = pointer } },
    });
    return result;
}

fn lowerOptionalUnwrap(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    const operand = try builder.lowerNode(node.data.lhs) orelse return null;
    const optional_type_id = builder.sema.node_types.get(node.data.lhs) orelse return null;
    const optional_type = builder.sema.type_pool.get(optional_type_id);
    const child = builder.sema.type_pool.get(optional_type.data.optional.child_type);

    // The spec fixes a null niche for optional pointers. Other optional
    // representations are intentionally not guessed here because their layout
    // remains unspecified in the RC documents.
    if (child.data == .pointer) {
        const zero = try builder.emitInst(.{ .opcode = .const_i, .type_id = optional_type_id, .data = .{ .const_i = 0 } });
        const bool_type = try builder.sema.type_pool.internPrimitive(.bool_type);
        const present = try builder.emitInst(.{
            .opcode = .icmp,
            .type_id = bool_type,
            .data = .{ .icmp = .{ .predicate = .ne, .lhs = operand, .rhs = zero } },
        });
        const success = try builder.newBlock();
        const null_trap = try builder.newBlock();
        _ = try builder.emitInst(.{
            .opcode = .condbr,
            .type_id = 0,
            .data = .{ .condbr = .{ .cond = present, .true_dest = success, .false_dest = null_trap } },
        });
        builder.current_block = null_trap;
        _ = try builder.emitInst(.{ .opcode = .unreachable_inst, .type_id = 0, .data = .{ .unreachable_inst = {} } });
        builder.current_block = success;
    }
    return operand;
}

fn lowerIndex(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const child_type = builder.sema.node_types.get(node_index) orelse return null;
    const address = try lvalue.lowerAddress(builder, node_index) orelse return null;
    const result = try builder.emitInst(.{
        .opcode = .load,
        .type_id = child_type,
        .data = .{ .load = .{ .ptr = address } },
    });
    return result;
}

fn lowerSlice(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    const extra_start = node.data.lhs;
    const container_node = builder.sema.ast_tree.extra_data[extra_start];
    const lower_node = builder.sema.ast_tree.extra_data[extra_start + 1];
    const upper_node = builder.sema.ast_tree.extra_data[extra_start + 2];
    const base = try builder.lowerNode(container_node) orelse return null;
    const index_type = try builder.sema.type_pool.internSizeInt(false);
    const lower_bound = if (lower_node == std.math.maxInt(u32))
        try builder.emitInst(.{ .opcode = .const_i, .type_id = index_type, .data = .{ .const_i = 0 } })
    else
        try builder.lowerNode(lower_node) orelse return null;
    const original_length = builder.slice_lengths.get(base);
    const upper = if (upper_node == std.math.maxInt(u32))
        original_length orelse return null
    else
        try builder.lowerNode(upper_node) orelse return null;

    const result_type_id = builder.sema.node_types.get(node_index) orelse return null;
    const result_type = builder.sema.type_pool.get(result_type_id);
    const child_type: Type.Id = result_type.data.pointer.child_type;
    const stride: i32 = @intCast(@max(builder.sema.type_pool.sizeOf(child_type) catch 1, 1));
    const pointer_type = try builder.sema.type_pool.internPtr(child_type, result_type.data.pointer.is_const);
    const result = try builder.emitInst(.{
        .opcode = .gep,
        .type_id = pointer_type,
        .data = .{ .gep = .{ .base = base, .index = lower_bound, .stride = stride } },
    });
    const length = try builder.emitInst(.{
        .opcode = .sub,
        .type_id = index_type,
        .data = .{ .sub = .{ .lhs = upper, .rhs = lower_bound } },
    });
    try builder.slice_lengths.put(result, length);
    return result;
}
