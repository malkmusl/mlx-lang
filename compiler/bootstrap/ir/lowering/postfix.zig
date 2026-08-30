const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Type = @import("../../semantic/type.zig").Type;
const Inst = @import("../lir.zig").Inst;

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
    if (operator == .dot_question) return builder.lowerNode(node.data.lhs);
    if (operator != .dot_asterisk) return null;
    const pointer = try builder.lowerNode(node.data.lhs) orelse return null;
    const result = try builder.emitInst(.{
        .opcode = .load,
        .type_id = builder.sema.node_types.get(node_index) orelse return null,
        .data = .{ .load = .{ .ptr = pointer } },
    });
    return result;
}

fn lowerIndex(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    const base = try builder.lowerNode(node.data.lhs) orelse return null;
    const index = try builder.lowerNode(node.data.rhs) orelse return null;
    const child_type = builder.sema.node_types.get(node_index) orelse return null;
    const stride: i32 = @intCast(@max(builder.sema.type_pool.sizeOf(child_type) catch 1, 1));
    const pointer_type = try builder.sema.type_pool.internPtr(child_type, false);
    const address = try builder.emitInst(.{
        .opcode = .gep,
        .type_id = pointer_type,
        .data = .{ .gep = .{ .base = base, .index = index, .stride = stride } },
    });
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
