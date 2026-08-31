const Node = @import("../../syntax/ast.zig").Node;
const Inst = @import("../lir.zig").Inst;
const lvalue = @import("lvalue.zig");

pub fn lower(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    const operator = builder.sema.ast_tree.tokens[node.main_token].tag;
    return switch (operator) {
        .keyword_try => lowerTry(builder, node_index),
        .keyword_comptime => builder.lowerNode(node.data.lhs),
        .ampersand => lowerAddressOf(builder, node_index),
        .bang => lowerLogicalNot(builder, node_index),
        .minus => lowerNegate(builder, node_index),
        .tilde => lowerComplement(builder, node_index),
        else => null,
    };
}

fn lowerTry(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    const operand = try builder.lowerNode(node.data.lhs) orelse return null;
    const bool_type = try builder.sema.type_pool.internPrimitive(.bool_type);
    const error_test = try builder.emitInst(.{
        .opcode = .error_test,
        .type_id = bool_type,
        .data = .{ .error_test = operand },
    });
    const error_block = try builder.newBlock();
    const success_block = try builder.newBlock();
    _ = try builder.emitInst(.{
        .opcode = .condbr,
        .type_id = 0,
        .data = .{ .condbr = .{ .cond = error_test, .true_dest = error_block, .false_dest = success_block } },
    });

    builder.current_block = error_block;
    try builder.emitCleanups(0, true);
    _ = try builder.emitInst(.{
        .opcode = .ret_error,
        .type_id = builder.current_return_type.?,
        .data = .{ .ret_error = error_test },
    });

    builder.current_block = success_block;
    const result = try builder.emitInst(.{
        .opcode = .error_payload,
        .type_id = builder.sema.node_types.get(node_index) orelse 0,
        .data = .{ .error_payload = operand },
    });
    if (builder.slice_lengths.get(operand)) |length| try builder.slice_lengths.put(result, length);
    return result;
}

fn lowerAddressOf(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    return lvalue.lowerAddress(builder, node.data.lhs);
}

fn lowerLogicalNot(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    const operand = try builder.lowerNode(node.data.lhs) orelse return null;
    const result_type = builder.sema.node_types.get(node_index) orelse return null;
    const zero = try builder.emitInst(.{ .opcode = .const_i, .type_id = result_type, .data = .{ .const_i = 0 } });
    const result = try builder.emitInst(.{
        .opcode = .icmp,
        .type_id = result_type,
        .data = .{ .icmp = .{ .predicate = .eq, .lhs = operand, .rhs = zero } },
    });
    return result;
}

fn lowerNegate(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    const operand = try builder.lowerNode(node.data.lhs) orelse return null;
    const result_type = builder.sema.node_types.get(node_index) orelse return null;
    const zero = try builder.emitInst(.{ .opcode = .const_i, .type_id = result_type, .data = .{ .const_i = 0 } });
    const result = try builder.emitInst(.{ .opcode = .sub, .type_id = result_type, .data = .{ .sub = .{ .lhs = zero, .rhs = operand } } });
    return result;
}

fn lowerComplement(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    const operand = try builder.lowerNode(node.data.lhs) orelse return null;
    const result_type = builder.sema.node_types.get(node_index) orelse return null;
    const mask = try builder.emitInst(.{ .opcode = .const_i, .type_id = result_type, .data = .{ .const_i = ~@as(u64, 0) } });
    const result = try builder.emitInst(.{ .opcode = .bit_xor, .type_id = result_type, .data = .{ .bit_xor = .{ .lhs = operand, .rhs = mask } } });
    return result;
}
