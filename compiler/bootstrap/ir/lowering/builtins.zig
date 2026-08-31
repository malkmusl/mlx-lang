const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Inst = @import("../lir.zig").Inst;
const builtin = @import("../../semantic/builtin.zig");
const aggregate_lowering = @import("aggregate.zig");

pub fn lower(builder: anytype, node_idx: Node.Index) std.mem.Allocator.Error!?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_idx);
    if (builder.sema.dynamic_fields.get(node_idx)) |field| {
        return aggregate_lowering.lowerFieldNamed(builder, field.base_node, field.name);
    }
    if (builder.sema.const_values.get(node_idx)) |value| {
        return try builder.emitInst(.{
            .opcode = .const_i,
            .type_id = builder.sema.node_types.get(node_idx) orelse 0,
            .data = .{ .const_i = value },
        });
    }

    const name_token = builder.sema.ast_tree.tokens[node.data.lhs];
    const source = builder.sema.diags.source_manager.getFile(builder.sema.source_id).?.content;
    const kind = builtin.lookup(source[name_token.start..name_token.end]) orelse return null;
    const extra_start = node.data.rhs;
    const argument_count = builder.sema.ast_tree.extra_data[extra_start];
    const value_argument: ?u32 = switch (kind) {
        .move, .discardError, .intFromPtr, .intFromEnum => 0,
        .intCast,
        .floatCast,
        .floatFromInt,
        .intFromFloat,
        .ptrCast,
        .alignCast,
        .bitCast,
        .ptrFromInt,
        .enumFromInt,
        => 1,
        else => null,
    };
    if (kind == .tagOf and argument_count == 1) {
        const value_node = builder.sema.ast_tree.extra_data[extra_start + 1];
        const value_type = builder.sema.node_types.get(value_node) orelse return null;
        return if (builder.sema.type_pool.get(value_type).data == .@"union")
            aggregate_lowering.lowerTag(builder, value_node)
        else
            builder.lowerNode(value_node);
    }
    if (value_argument) |argument_index| {
        if (argument_index < argument_count) {
            return builder.lowerNode(builder.sema.ast_tree.extra_data[extra_start + 1 + argument_index]);
        }
    }
    return null;
}
