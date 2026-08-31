const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Type = @import("../../semantic/type.zig").Type;
const Inst = @import("../lir.zig").Inst;

pub fn lower(builder: anytype, node_idx: Node.Index) std.mem.Allocator.Error!?Inst.Index {
    return switch (builder.sema.ast_tree.nodes.get(node_idx).tag) {
        .while_stmt => lowerWhile(builder, node_idx),
        .for_stmt => lowerFor(builder, node_idx),
        .break_stmt => lowerBreak(builder, node_idx),
        .continue_stmt => lowerContinue(builder, node_idx),
        else => unreachable,
    };
}

fn lowerWhile(builder: anytype, node_idx: Node.Index) std.mem.Allocator.Error!?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_idx);
    const extra_start = node.data.lhs;
    const label_token = builder.sema.ast_tree.extra_data[extra_start];
    const cond_node = builder.sema.ast_tree.extra_data[extra_start + 1];
    const body_node = builder.sema.ast_tree.extra_data[extra_start + 2];

    const result_type = builder.sema.node_types.get(node_idx) orelse 0;
    const result_addr = if (builder.hasRuntimeValuePublic(result_type))
        try builder.emitInst(.{
            .opcode = .addr,
            .type_id = result_type,
            .data = .{ .addr = 0xd000_0000 | node_idx },
        })
    else
        null;

    const cond_block = try builder.newBlock();
    const body_block = try builder.newBlock();
    const end_block = try builder.newBlock();
    _ = try builder.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = cond_block } } });

    builder.current_block = cond_block;
    const cond_inst = (try builder.lowerNode(cond_node)) orelse return null;
    _ = try builder.emitInst(.{
        .opcode = .condbr,
        .type_id = 0,
        .data = .{ .condbr = .{ .cond = cond_inst, .true_dest = body_block, .false_dest = end_block } },
    });

    builder.current_block = body_block;
    try builder.loop_stack.append(builder.allocator, .{
        .label_token = label_token,
        .break_dest = end_block,
        .continue_dest = cond_block,
        .result_addr = result_addr,
        .result_type = result_type,
        .cleanup_depth = builder.cleanup_stack.items.len,
    });
    _ = try builder.lowerNode(body_node);
    _ = builder.loop_stack.pop();
    if (!builder.currentBlockTerminatedPublic()) {
        _ = try builder.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = cond_block } } });
    }

    builder.current_block = end_block;
    if (result_addr) |address| {
        return try builder.emitInst(.{ .opcode = .load, .type_id = result_type, .data = .{ .load = .{ .ptr = address } } });
    }
    return null;
}

fn lowerBreak(builder: anytype, node_idx: Node.Index) std.mem.Allocator.Error!?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_idx);
    const target_index = findLoopTargetIndex(builder, node.data.lhs) orelse return null;
    const targets = builder.loop_stack.items[target_index];
    if (node.data.rhs != std.math.maxInt(u32)) {
        const value = try builder.lowerNode(node.data.rhs) orelse return null;
        if (targets.result_addr) |address| {
            _ = try builder.emitInst(.{
                .opcode = .store,
                .type_id = targets.result_type,
                .data = .{ .store = .{ .ptr = address, .val = value } },
            });
        }
    }
    try builder.emitCleanups(targets.cleanup_depth, false);
    _ = try builder.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = targets.break_dest } } });
    return null;
}

fn lowerContinue(builder: anytype, node_idx: Node.Index) std.mem.Allocator.Error!?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_idx);
    const target_index = findLoopTargetIndex(builder, node.data.lhs) orelse return null;
    const targets = builder.loop_stack.items[target_index];
    try builder.emitCleanups(targets.cleanup_depth, false);
    _ = try builder.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = targets.continue_dest } } });
    return null;
}

fn findLoopTargetIndex(builder: anytype, label_token: u32) ?usize {
    if (builder.loop_stack.items.len == 0) return null;
    if (label_token == std.math.maxInt(u32)) return builder.loop_stack.items.len - 1;

    const source = builder.sema.diags.source_manager.getFile(builder.sema.source_id).?.content;
    const wanted_token = builder.sema.ast_tree.tokens[label_token];
    const wanted = source[wanted_token.start..wanted_token.end];
    var index = builder.loop_stack.items.len;
    while (index > 0) {
        index -= 1;
        const target = builder.loop_stack.items[index];
        if (target.label_token == std.math.maxInt(u32)) continue;
        const candidate_token = builder.sema.ast_tree.tokens[target.label_token];
        if (std.mem.eql(u8, wanted, source[candidate_token.start..candidate_token.end])) return index;
    }
    return null;
}

fn lowerFor(builder: anytype, node_idx: Node.Index) std.mem.Allocator.Error!?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_idx);
    const extra_start = node.data.lhs;
    const label_token = builder.sema.ast_tree.extra_data[extra_start];
    const capture_flags = builder.sema.ast_tree.extra_data[extra_start + 1];
    const item_token = builder.sema.ast_tree.extra_data[extra_start + 2];
    const index_token = builder.sema.ast_tree.extra_data[extra_start + 3];
    const range_node_idx = builder.sema.ast_tree.extra_data[extra_start + 4];
    const body_node = builder.sema.ast_tree.extra_data[extra_start + 5];
    const range_node = builder.sema.ast_tree.nodes.get(range_node_idx);

    if (range_node.tag != .range) {
        return lowerIterableFor(builder, label_token, capture_flags, item_token, index_token, range_node_idx, body_node);
    }

    const start_inst = try builder.lowerNode(range_node.data.lhs) orelse return null;
    const end_inst = try builder.lowerNode(range_node.data.rhs) orelse return null;
    const item_type = builder.sema.node_types.get(range_node.data.lhs) orelse 0;
    const item_addr = try builder.emitInst(.{ .opcode = .addr, .type_id = item_type, .data = .{ .addr = item_token } });
    _ = try builder.emitInst(.{ .opcode = .store, .type_id = item_type, .data = .{ .store = .{ .ptr = item_addr, .val = start_inst } } });

    const source = builder.sema.diags.source_manager.getFile(builder.sema.source_id).?.content;
    const item_source_token = builder.sema.ast_tree.tokens[item_token];
    const item_name = source[item_source_token.start..item_source_token.end];
    const previous_item = builder.var_addresses.get(item_name);
    try builder.var_addresses.put(item_name, item_addr);
    defer if (previous_item) |address| {
        builder.var_addresses.put(item_name, address) catch {};
    } else {
        _ = builder.var_addresses.remove(item_name);
    };

    var index_addr: ?Inst.Index = null;
    var index_name: ?[]const u8 = null;
    var previous_index: ?u32 = null;
    if (index_token != std.math.maxInt(u32)) {
        const index_type = try builder.sema.type_pool.internSizeInt(false);
        const zero = try builder.emitInst(.{ .opcode = .const_i, .type_id = index_type, .data = .{ .const_i = 0 } });
        index_addr = try builder.emitInst(.{ .opcode = .addr, .type_id = index_type, .data = .{ .addr = index_token } });
        _ = try builder.emitInst(.{ .opcode = .store, .type_id = index_type, .data = .{ .store = .{ .ptr = index_addr.?, .val = zero } } });
        const index_source_token = builder.sema.ast_tree.tokens[index_token];
        index_name = source[index_source_token.start..index_source_token.end];
        previous_index = builder.var_addresses.get(index_name.?);
        try builder.var_addresses.put(index_name.?, index_addr.?);
    }
    defer if (index_name) |name| {
        if (previous_index) |address| {
            builder.var_addresses.put(name, address) catch {};
        } else {
            _ = builder.var_addresses.remove(name);
        }
    };

    const cond_block = try builder.newBlock();
    const body_block = try builder.newBlock();
    const increment_block = try builder.newBlock();
    const end_block = try builder.newBlock();
    _ = try builder.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = cond_block } } });

    builder.current_block = cond_block;
    const current_item = try builder.emitInst(.{ .opcode = .load, .type_id = item_type, .data = .{ .load = .{ .ptr = item_addr } } });
    const bool_type = try builder.sema.type_pool.internPrimitive(.bool_type);
    const condition = try builder.emitInst(.{ .opcode = .icmp, .type_id = bool_type, .data = .{ .icmp = .{ .predicate = .lt, .lhs = current_item, .rhs = end_inst } } });
    _ = try builder.emitInst(.{ .opcode = .condbr, .type_id = 0, .data = .{ .condbr = .{ .cond = condition, .true_dest = body_block, .false_dest = end_block } } });

    builder.current_block = body_block;
    try builder.loop_stack.append(builder.allocator, .{
        .label_token = label_token,
        .break_dest = end_block,
        .continue_dest = increment_block,
        .cleanup_depth = builder.cleanup_stack.items.len,
    });
    _ = try builder.lowerNode(body_node);
    _ = builder.loop_stack.pop();
    if (!builder.currentBlockTerminatedPublic()) _ = try builder.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = increment_block } } });

    builder.current_block = increment_block;
    const item_value = try builder.emitInst(.{ .opcode = .load, .type_id = item_type, .data = .{ .load = .{ .ptr = item_addr } } });
    const one = try builder.emitInst(.{ .opcode = .const_i, .type_id = item_type, .data = .{ .const_i = 1 } });
    const next_item = try builder.emitInst(.{ .opcode = .add, .type_id = item_type, .data = .{ .add = .{ .lhs = item_value, .rhs = one } } });
    _ = try builder.emitInst(.{ .opcode = .store, .type_id = item_type, .data = .{ .store = .{ .ptr = item_addr, .val = next_item } } });
    if (index_addr) |address| {
        const index_type = try builder.sema.type_pool.internSizeInt(false);
        const index_value = try builder.emitInst(.{ .opcode = .load, .type_id = index_type, .data = .{ .load = .{ .ptr = address } } });
        const index_one = try builder.emitInst(.{ .opcode = .const_i, .type_id = index_type, .data = .{ .const_i = 1 } });
        const next_index = try builder.emitInst(.{ .opcode = .add, .type_id = index_type, .data = .{ .add = .{ .lhs = index_value, .rhs = index_one } } });
        _ = try builder.emitInst(.{ .opcode = .store, .type_id = index_type, .data = .{ .store = .{ .ptr = address, .val = next_index } } });
    }
    _ = try builder.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = cond_block } } });

    builder.current_block = end_block;
    return null;
}

fn lowerIterableFor(
    builder: anytype,
    label_token: u32,
    capture_flags: u32,
    item_token: u32,
    index_token: u32,
    iterable_node: Node.Index,
    body_node: Node.Index,
) std.mem.Allocator.Error!?Inst.Index {
    const iterable_type_id = builder.sema.node_types.get(iterable_node) orelse return null;
    const iterable_type = builder.sema.type_pool.get(iterable_type_id);
    const child_type: Type.Id = switch (iterable_type.data) {
        .array => |array| array.child_type,
        .pointer => |pointer| pointer.child_type,
        else => return null,
    };
    const base = try builder.lowerNode(iterable_node) orelse return null;
    const index_type = try builder.sema.type_pool.internSizeInt(false);
    const length = switch (iterable_type.data) {
        .array => |array| try builder.emitInst(.{ .opcode = .const_i, .type_id = index_type, .data = .{ .const_i = array.len } }),
        .pointer => builder.slice_lengths.get(base) orelse
            try builder.emitInst(.{ .opcode = .const_i, .type_id = index_type, .data = .{ .const_i = 0 } }),
        else => unreachable,
    };
    const stride: i32 = switch (iterable_type.data) {
        .array => @intCast(@max(builder.sema.type_pool.sizeOf(child_type) catch 8, 1)),
        .pointer => @intCast(@max(builder.sema.type_pool.sizeOf(child_type) catch 8, 1)),
        else => unreachable,
    };

    const zero = try builder.emitInst(.{ .opcode = .const_i, .type_id = index_type, .data = .{ .const_i = 0 } });
    const counter_id = builder.synthetic_local_counter;
    builder.synthetic_local_counter -= 1;
    const counter_addr = try builder.emitInst(.{ .opcode = .addr, .type_id = index_type, .data = .{ .addr = counter_id } });
    _ = try builder.emitInst(.{ .opcode = .store, .type_id = index_type, .data = .{ .store = .{ .ptr = counter_addr, .val = zero } } });

    const item_type = if ((capture_flags & 1) != 0)
        try builder.sema.type_pool.internPtr(child_type, false)
    else
        child_type;
    const item_addr = try builder.emitInst(.{ .opcode = .addr, .type_id = item_type, .data = .{ .addr = item_token } });

    const source = builder.sema.diags.source_manager.getFile(builder.sema.source_id).?.content;
    const item_source_token = builder.sema.ast_tree.tokens[item_token];
    const item_name = source[item_source_token.start..item_source_token.end];
    const previous_item = builder.var_addresses.get(item_name);
    try builder.var_addresses.put(item_name, item_addr);
    defer if (previous_item) |address| {
        builder.var_addresses.put(item_name, address) catch {};
    } else {
        _ = builder.var_addresses.remove(item_name);
    };

    var index_name: ?[]const u8 = null;
    var previous_index: ?Inst.Index = null;
    if (index_token != std.math.maxInt(u32)) {
        const index_source_token = builder.sema.ast_tree.tokens[index_token];
        index_name = source[index_source_token.start..index_source_token.end];
        previous_index = builder.var_addresses.get(index_name.?);
        try builder.var_addresses.put(index_name.?, counter_addr);
    }
    defer if (index_name) |name| {
        if (previous_index) |address| {
            builder.var_addresses.put(name, address) catch {};
        } else {
            _ = builder.var_addresses.remove(name);
        }
    };

    const cond_block = try builder.newBlock();
    const body_block = try builder.newBlock();
    const increment_block = try builder.newBlock();
    const end_block = try builder.newBlock();
    _ = try builder.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = cond_block } } });

    builder.current_block = cond_block;
    const current_index = try builder.emitInst(.{ .opcode = .load, .type_id = index_type, .data = .{ .load = .{ .ptr = counter_addr } } });
    const bool_type = try builder.sema.type_pool.internPrimitive(.bool_type);
    const condition = try builder.emitInst(.{ .opcode = .icmp, .type_id = bool_type, .data = .{ .icmp = .{ .predicate = .lt, .lhs = current_index, .rhs = length } } });
    _ = try builder.emitInst(.{ .opcode = .condbr, .type_id = 0, .data = .{ .condbr = .{ .cond = condition, .true_dest = body_block, .false_dest = end_block } } });

    builder.current_block = body_block;
    const body_index = try builder.emitInst(.{ .opcode = .load, .type_id = index_type, .data = .{ .load = .{ .ptr = counter_addr } } });
    const element_pointer_type = try builder.sema.type_pool.internPtr(child_type, false);
    const element_address = try builder.emitInst(.{
        .opcode = .gep,
        .type_id = element_pointer_type,
        .data = .{ .gep = .{ .base = base, .index = body_index, .stride = stride } },
    });
    const captured_value = if ((capture_flags & 1) != 0)
        element_address
    else
        try builder.emitInst(.{ .opcode = .load, .type_id = child_type, .data = .{ .load = .{ .ptr = element_address } } });
    _ = try builder.emitInst(.{ .opcode = .store, .type_id = item_type, .data = .{ .store = .{ .ptr = item_addr, .val = captured_value } } });

    try builder.loop_stack.append(builder.allocator, .{
        .label_token = label_token,
        .break_dest = end_block,
        .continue_dest = increment_block,
        .cleanup_depth = builder.cleanup_stack.items.len,
    });
    _ = try builder.lowerNode(body_node);
    _ = builder.loop_stack.pop();
    if (!builder.currentBlockTerminatedPublic()) _ = try builder.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = increment_block } } });

    builder.current_block = increment_block;
    const old_index = try builder.emitInst(.{ .opcode = .load, .type_id = index_type, .data = .{ .load = .{ .ptr = counter_addr } } });
    const one = try builder.emitInst(.{ .opcode = .const_i, .type_id = index_type, .data = .{ .const_i = 1 } });
    const next_index = try builder.emitInst(.{ .opcode = .add, .type_id = index_type, .data = .{ .add = .{ .lhs = old_index, .rhs = one } } });
    _ = try builder.emitInst(.{ .opcode = .store, .type_id = index_type, .data = .{ .store = .{ .ptr = counter_addr, .val = next_index } } });
    _ = try builder.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = cond_block } } });

    builder.current_block = end_block;
    return null;
}
