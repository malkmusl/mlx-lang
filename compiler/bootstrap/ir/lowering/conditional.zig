const Node = @import("../../syntax/ast.zig").Node;
const Inst = @import("../lir.zig").Inst;

pub fn lower(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    const start = node.data.lhs;
    const condition_node = builder.sema.ast_tree.extra_data[start];
    const has_else = node.data.rhs > start + 2;
    if (builder.sema.const_values.get(condition_node)) |condition| {
        if (condition != 0) return builder.lowerNode(builder.sema.ast_tree.extra_data[start + 1]);
        if (has_else) return builder.lowerNode(builder.sema.ast_tree.extra_data[start + 2]);
        return null;
    }

    const condition = try builder.lowerNode(condition_node) orelse return null;
    const then_block = try builder.newBlock();
    const else_block = try builder.newBlock();
    const merge_block = try builder.newBlock();
    const result_type = builder.sema.node_types.get(node_index) orelse 0;
    const result_address = if (has_else and builder.hasRuntimeValuePublic(result_type))
        try builder.emitInst(.{ .opcode = .addr, .type_id = result_type, .data = .{ .addr = 0xc000_0000 | node_index } })
    else
        null;
    _ = try builder.emitInst(.{
        .opcode = .condbr,
        .type_id = 0,
        .data = .{ .condbr = .{ .cond = condition, .true_dest = then_block, .false_dest = if (has_else) else_block else merge_block } },
    });

    builder.current_block = then_block;
    const then_value = try builder.lowerNode(builder.sema.ast_tree.extra_data[start + 1]);
    if (!builder.currentBlockTerminatedPublic()) {
        if (result_address) |address| if (then_value) |value| {
            _ = try builder.emitInst(.{ .opcode = .store, .type_id = result_type, .data = .{ .store = .{ .ptr = address, .val = value } } });
        };
        _ = try builder.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = merge_block } } });
    }

    if (has_else) {
        builder.current_block = else_block;
        const else_value = try builder.lowerNode(builder.sema.ast_tree.extra_data[start + 2]);
        if (!builder.currentBlockTerminatedPublic()) {
            if (result_address) |address| if (else_value) |value| {
                _ = try builder.emitInst(.{ .opcode = .store, .type_id = result_type, .data = .{ .store = .{ .ptr = address, .val = value } } });
            };
            _ = try builder.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = merge_block } } });
        }
    }

    builder.current_block = merge_block;
    if (result_address) |address| return try builder.emitInst(.{ .opcode = .load, .type_id = result_type, .data = .{ .load = .{ .ptr = address } } });
    return null;
}
