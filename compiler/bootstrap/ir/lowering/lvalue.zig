const Node = @import("../../syntax/ast.zig").Node;
const Inst = @import("../lir.zig").Inst;

pub fn lowerAddress(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    return switch (node.tag) {
        .identifier => lowerIdentifier(builder, node),
        .field_access => lowerField(builder, node_index),
        .array_access => lowerIndex(builder, node_index),
        .unary_op => if (builder.sema.ast_tree.tokens[node.main_token].tag == .dot_asterisk)
            builder.lowerNode(node.data.lhs)
        else
            null,
        else => null,
    };
}

fn lowerIdentifier(builder: anytype, node: Node) ?Inst.Index {
    const token = builder.sema.ast_tree.tokens[node.main_token];
    const source = builder.sema.diags.source_manager.getFile(builder.sema.source_id).?.content;
    return builder.var_addresses.get(source[token.start..token.end]);
}

fn lowerField(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    const base = try builder.lowerNode(node.data.lhs) orelse return null;
    const base_type = builder.sema.node_types.get(node.data.lhs) orelse return null;
    const token = builder.sema.ast_tree.tokens[node.main_token];
    const source = builder.sema.diags.source_manager.getFile(builder.sema.source_id).?.content;
    const field = builder.sema.type_pool.aggregateField(base_type, source[token.start..token.end]) orelse return null;
    const result = try offsetAddress(builder, base, field.offset, field.type_id);
    return result;
}

fn lowerIndex(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    const base = try builder.lowerNode(node.data.lhs) orelse return null;
    const index = try builder.lowerNode(node.data.rhs) orelse return null;
    const child_type = builder.sema.node_types.get(node_index) orelse return null;
    const stride: i32 = @intCast(@max(builder.sema.type_pool.sizeOf(child_type) catch 1, 1));
    const pointer_type = try builder.sema.type_pool.internPtr(child_type, false);
    const result = try builder.emitInst(.{
        .opcode = .gep,
        .type_id = pointer_type,
        .data = .{ .gep = .{ .base = base, .index = index, .stride = stride } },
    });
    return result;
}

fn offsetAddress(builder: anytype, base: Inst.Index, offset_value: u64, child_type: u32) !Inst.Index {
    const offset_type = try builder.sema.type_pool.internSizeInt(false);
    const offset = try builder.emitInst(.{ .opcode = .const_i, .type_id = offset_type, .data = .{ .const_i = offset_value } });
    const pointer_type = try builder.sema.type_pool.internPtr(child_type, false);
    return builder.emitInst(.{
        .opcode = .gep,
        .type_id = pointer_type,
        .data = .{ .gep = .{ .base = base, .index = offset, .stride = 1 } },
    });
}
