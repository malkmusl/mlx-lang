const Node = @import("../../syntax/ast.zig").Node;
const Inst = @import("../lir.zig").Inst;

pub fn lowerTupleLiteral(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    const type_id = builder.sema.node_types.get(node_index) orelse return null;
    const size: u32 = @intCast(@max(builder.sema.type_pool.sizeOf(type_id) catch 8, 1));
    const alignment: u32 = @intCast(builder.sema.type_pool.alignOf(type_id) catch 8);
    const base = try builder.emitInst(.{
        .opcode = .alloca,
        .type_id = type_id,
        .data = .{ .alloca = .{
            .id = builder.nextSyntheticLocal(),
            .size = size,
            .alignment = alignment,
        } },
    });
    const fields = builder.sema.type_pool.aggregateFields(type_id) orelse return null;
    const count = builder.sema.ast_tree.extra_data[node.data.lhs];
    const offset_type = try builder.sema.type_pool.internSizeInt(false);
    var index: u32 = 0;
    while (index < count and index < fields.len) : (index += 1) {
        const field = fields[index];
        const offset = try builder.emitInst(.{
            .opcode = .const_i,
            .type_id = offset_type,
            .data = .{ .const_i = field.offset },
        });
        const pointer_type = try builder.sema.type_pool.internPtr(field.type_id, false);
        const address = try builder.emitInst(.{
            .opcode = .gep,
            .type_id = pointer_type,
            .data = .{ .gep = .{ .base = base, .index = offset, .stride = 1 } },
        });
        const value_node = builder.sema.ast_tree.extra_data[node.data.lhs + 1 + index];
        const value = try builder.lowerNode(value_node) orelse return null;
        if (isAggregate(builder, field.type_id)) {
            _ = try builder.emitInst(.{
                .opcode = .memory_copy,
                .type_id = field.type_id,
                .data = .{ .memory_copy = .{
                    .destination = address,
                    .source = value,
                    .size = @intCast(builder.sema.type_pool.sizeOf(field.type_id) catch return null),
                } },
            });
        } else {
            _ = try builder.emitInst(.{
                .opcode = .store,
                .type_id = field.type_id,
                .data = .{ .store = .{ .ptr = address, .val = value } },
            });
        }
        if (isSlice(builder, field.type_id)) {
            const length = builder.slice_lengths.get(value) orelse return null;
            const length_offset = try builder.emitInst(.{
                .opcode = .const_i,
                .type_id = offset_type,
                .data = .{ .const_i = field.offset + 8 },
            });
            const length_type = try builder.sema.type_pool.internSizeInt(false);
            const length_pointer_type = try builder.sema.type_pool.internPtr(length_type, false);
            const length_address = try builder.emitInst(.{
                .opcode = .gep,
                .type_id = length_pointer_type,
                .data = .{ .gep = .{ .base = base, .index = length_offset, .stride = 1 } },
            });
            _ = try builder.emitInst(.{
                .opcode = .store,
                .type_id = length_type,
                .data = .{ .store = .{ .ptr = length_address, .val = length } },
            });
        }
    }
    return base;
}

pub fn lowerLiteral(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    const type_id = builder.sema.node_types.get(node_index) orelse return null;
    const size: u32 = @intCast(@max(builder.sema.type_pool.sizeOf(type_id) catch 8, 1));
    const alignment: u32 = @intCast(builder.sema.type_pool.alignOf(type_id) catch 8);
    const allocation_id = builder.nextSyntheticLocal();
    const base = try builder.emitInst(.{
        .opcode = .alloca,
        .type_id = type_id,
        .data = .{ .alloca = .{ .id = allocation_id, .size = size, .alignment = alignment } },
    });
    const source = builder.sema.diags.source_manager.getFile(builder.sema.source_id).?.content;
    const start = node.data.rhs;
    const count = builder.sema.ast_tree.extra_data[start];
    const offset_type = try builder.sema.type_pool.internSizeInt(false);
    const aggregate = builder.sema.type_pool.get(type_id);
    const aggregate_info = builder.sema.type_pool.aggregateInfo(type_id) orelse return null;
    var index: u32 = 0;
    while (index < count) : (index += 1) {
        const name_token_index = builder.sema.ast_tree.extra_data[start + 1 + index * 2];
        const value_node = builder.sema.ast_tree.extra_data[start + 2 + index * 2];
        const token = builder.sema.ast_tree.tokens[name_token_index];
        const field = builder.sema.type_pool.aggregateField(type_id, source[token.start..token.end]) orelse continue;
        if (aggregate.data == .@"union" and aggregate_info.backing_type != null) {
            const tag_type = aggregate_info.backing_type.?;
            const tag_pointer_type = try builder.sema.type_pool.internPtr(tag_type, false);
            const tag_address = try builder.emitInst(.{
                .opcode = .gep,
                .type_id = tag_pointer_type,
                .data = .{ .gep = .{
                    .base = base,
                    .index = try builder.emitInst(.{ .opcode = .const_i, .type_id = offset_type, .data = .{ .const_i = 0 } }),
                    .stride = 1,
                } },
            });
            const tag_value = try builder.emitInst(.{
                .opcode = .const_i,
                .type_id = tag_type,
                .data = .{ .const_i = field.value orelse 0 },
            });
            _ = try builder.emitInst(.{
                .opcode = .store,
                .type_id = tag_type,
                .data = .{ .store = .{ .ptr = tag_address, .val = tag_value } },
            });
        }
        const offset = try builder.emitInst(.{ .opcode = .const_i, .type_id = offset_type, .data = .{ .const_i = field.offset } });
        const pointer_type = try builder.sema.type_pool.internPtr(field.type_id, false);
        const address = try builder.emitInst(.{
            .opcode = .gep,
            .type_id = pointer_type,
            .data = .{ .gep = .{ .base = base, .index = offset, .stride = 1 } },
        });
        const value = try builder.lowerNode(value_node) orelse return null;
        if (isAggregate(builder, field.type_id)) {
            _ = try builder.emitInst(.{
                .opcode = .memory_copy,
                .type_id = field.type_id,
                .data = .{ .memory_copy = .{
                    .destination = address,
                    .source = value,
                    .size = @intCast(builder.sema.type_pool.sizeOf(field.type_id) catch return null),
                } },
            });
        } else {
            _ = try builder.emitInst(.{ .opcode = .store, .type_id = field.type_id, .data = .{ .store = .{ .ptr = address, .val = value } } });
        }
        if (isSlice(builder, field.type_id)) {
            const length = builder.slice_lengths.get(value) orelse return null;
            const length_offset = try builder.emitInst(.{ .opcode = .const_i, .type_id = offset_type, .data = .{ .const_i = field.offset + 8 } });
            const length_type = try builder.sema.type_pool.internSizeInt(false);
            const length_pointer_type = try builder.sema.type_pool.internPtr(length_type, false);
            const length_address = try builder.emitInst(.{
                .opcode = .gep,
                .type_id = length_pointer_type,
                .data = .{ .gep = .{ .base = base, .index = length_offset, .stride = 1 } },
            });
            _ = try builder.emitInst(.{ .opcode = .store, .type_id = length_type, .data = .{ .store = .{ .ptr = length_address, .val = length } } });
        }
    }
    return base;
}

pub fn lowerTag(builder: anytype, value_node: Node.Index) !?Inst.Index {
    const base = try builder.lowerNode(value_node) orelse return null;
    const union_type = builder.sema.node_types.get(value_node) orelse return null;
    const info = builder.sema.type_pool.aggregateInfo(union_type) orelse return null;
    const tag_type = info.backing_type orelse return null;
    const offset_type = try builder.sema.type_pool.internSizeInt(false);
    const zero = try builder.emitInst(.{ .opcode = .const_i, .type_id = offset_type, .data = .{ .const_i = 0 } });
    const pointer_type = try builder.sema.type_pool.internPtr(tag_type, false);
    const address = try builder.emitInst(.{
        .opcode = .gep,
        .type_id = pointer_type,
        .data = .{ .gep = .{ .base = base, .index = zero, .stride = 1 } },
    });
    return try builder.emitInst(.{ .opcode = .load, .type_id = tag_type, .data = .{ .load = .{ .ptr = address } } });
}

pub fn lowerField(builder: anytype, node_index: Node.Index) !?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_index);
    const token = builder.sema.ast_tree.tokens[node.main_token];
    const source = builder.sema.diags.source_manager.getFile(builder.sema.source_id).?.content;
    return lowerFieldNamed(builder, node.data.lhs, source[token.start..token.end]);
}

pub fn lowerFieldNamed(builder: anytype, base_node: Node.Index, name: []const u8) !?Inst.Index {
    const base_type = builder.sema.node_types.get(base_node) orelse return null;
    const field = builder.sema.type_pool.aggregateField(base_type, name) orelse return null;
    return lowerFieldResolved(builder, base_node, field.offset, field.type_id);
}

pub fn lowerFieldResolved(builder: anytype, base_node: Node.Index, field_offset: u64, field_type: u32) !?Inst.Index {
    const base = try builder.lowerNode(base_node) orelse return null;
    const offset_type = try builder.sema.type_pool.internSizeInt(false);
    const offset = try builder.emitInst(.{ .opcode = .const_i, .type_id = offset_type, .data = .{ .const_i = field_offset } });
    const pointer_type = try builder.sema.type_pool.internPtr(field_type, false);
    const address = try builder.emitInst(.{
        .opcode = .gep,
        .type_id = pointer_type,
        .data = .{ .gep = .{ .base = base, .index = offset, .stride = 1 } },
    });
    const result = if (isAggregate(builder, field_type))
        address
    else
        try builder.emitInst(.{ .opcode = .load, .type_id = field_type, .data = .{ .load = .{ .ptr = address } } });
    if (isSlice(builder, field_type)) {
        const length_offset = try builder.emitInst(.{ .opcode = .const_i, .type_id = offset_type, .data = .{ .const_i = field_offset + 8 } });
        const length_type = try builder.sema.type_pool.internSizeInt(false);
        const length_pointer_type = try builder.sema.type_pool.internPtr(length_type, false);
        const length_address = try builder.emitInst(.{
            .opcode = .gep,
            .type_id = length_pointer_type,
            .data = .{ .gep = .{ .base = base, .index = length_offset, .stride = 1 } },
        });
        const length = try builder.emitInst(.{ .opcode = .load, .type_id = length_type, .data = .{ .load = .{ .ptr = length_address } } });
        try builder.slice_lengths.put(result, length);
    }
    return result;
}

fn isSlice(builder: anytype, type_id: u32) bool {
    const value_type = builder.sema.type_pool.get(type_id);
    return value_type.data == .pointer and value_type.data.pointer.size == .Slice;
}

fn isAggregate(builder: anytype, type_id: u32) bool {
    return switch (builder.sema.type_pool.get(type_id).data) {
        .array, .@"struct", .@"union", .tuple => true,
        else => false,
    };
}
