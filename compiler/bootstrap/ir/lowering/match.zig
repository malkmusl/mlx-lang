const Node = @import("../../syntax/ast.zig").Node;
const Inst = @import("../lir.zig").Inst;
const aggregate = @import("aggregate.zig");

pub fn lower(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    const start = node.data.lhs;
    const subject_node = builder.sema.ast_tree.extra_data[start];
    const subject_type = builder.sema.node_types.get(subject_node) orelse return null;
    const subject_data = builder.sema.type_pool.get(subject_type).data;
    const subject = if (subject_data == .@"union")
        try aggregate.lowerTag(builder, subject_node) orelse return null
    else
        try builder.lowerNode(subject_node) orelse return null;
    const arm_count = builder.sema.ast_tree.extra_data[start + 1];
    const result_type = builder.sema.node_types.get(node_index) orelse 0;
    const merge_block = try builder.newBlock();
    const result_address = if (builder.hasRuntimeValuePublic(result_type))
        try builder.emitInst(.{ .opcode = .addr, .type_id = result_type, .data = .{ .addr = 0xb800_0000 | node_index } })
    else
        null;

    var arm: u32 = 0;
    while (arm < arm_count) : (arm += 1) {
        const arm_start = start + 2 + arm * 4;
        const kind = builder.sema.ast_tree.extra_data[arm_start];
        const first = builder.sema.ast_tree.extra_data[arm_start + 1];
        const second = builder.sema.ast_tree.extra_data[arm_start + 2];
        const body_node = builder.sema.ast_tree.extra_data[arm_start + 3];
        const body_block = try builder.newBlock();
        const next_block = if (arm + 1 < arm_count) try builder.newBlock() else merge_block;

        if (kind == 0) {
            _ = try builder.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = body_block } } });
        } else {
            const bool_type = try builder.sema.type_pool.internPrimitive(.bool_type);
            const first_value = try builder.lowerNode(first) orelse return null;
            var condition = try builder.emitInst(.{
                .opcode = .icmp,
                .type_id = bool_type,
                .data = .{ .icmp = .{ .predicate = if (kind == 2) .ge else .eq, .lhs = subject, .rhs = first_value } },
            });
            if (kind == 2) {
                const second_value = try builder.lowerNode(second) orelse return null;
                const upper = try builder.emitInst(.{
                    .opcode = .icmp,
                    .type_id = bool_type,
                    .data = .{ .icmp = .{ .predicate = .lt, .lhs = subject, .rhs = second_value } },
                });
                condition = try builder.emitInst(.{ .opcode = .bit_and, .type_id = bool_type, .data = .{ .bit_and = .{ .lhs = condition, .rhs = upper } } });
            }
            _ = try builder.emitInst(.{
                .opcode = .condbr,
                .type_id = 0,
                .data = .{ .condbr = .{ .cond = condition, .true_dest = body_block, .false_dest = next_block } },
            });
        }

        builder.current_block = body_block;
        const body_value = try builder.lowerNode(body_node);
        if (!builder.currentBlockTerminatedPublic()) {
            if (result_address) |address| if (body_value) |value| {
                _ = try builder.emitInst(.{ .opcode = .store, .type_id = result_type, .data = .{ .store = .{ .ptr = address, .val = value } } });
            };
            _ = try builder.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = merge_block } } });
        }
        builder.current_block = next_block;
        if (kind == 0) break;
    }
    builder.current_block = merge_block;
    if (result_address) |address| {
        const result = try builder.emitInst(.{ .opcode = .load, .type_id = result_type, .data = .{ .load = .{ .ptr = address } } });
        return result;
    }
    return null;
}
