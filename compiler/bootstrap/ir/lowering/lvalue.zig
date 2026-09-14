const Node = @import("../../syntax/ast.zig").Node;
const lir = @import("../lir.zig");
const Inst = lir.Inst;

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
    if (builder.runtime_safety) try emitBoundsCheck(builder, node.data.lhs, base, index);
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

fn emitBoundsCheck(builder: anytype, container_node: Node.Index, base: Inst.Index, index: Inst.Index) !void {
    const container_type_id = builder.sema.node_types.get(container_node) orelse return;
    const container_type = builder.sema.type_pool.get(container_type_id);
    const index_type = try builder.sema.type_pool.internSizeInt(false);
    const length = switch (container_type.data) {
        .array => |array| try builder.emitInst(.{ .opcode = .const_i, .type_id = index_type, .data = .{ .const_i = array.len } }),
        .pointer => |pointer| if (pointer.size == .Slice) builder.slice_lengths.get(base) orelse return else return,
        else => return,
    };
    const bool_type = try builder.sema.type_pool.internPrimitive(.bool_type);
    const invalid = try builder.emitInst(.{
        .opcode = .icmp,
        .type_id = bool_type,
        .data = .{ .icmp = .{ .predicate = .uge, .lhs = index, .rhs = length } },
    });
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
