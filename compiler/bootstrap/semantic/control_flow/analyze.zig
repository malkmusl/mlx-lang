const std = @import("std");
const ast = @import("../../syntax/ast.zig");
const Node = ast.Node;
const Type = @import("../type.zig").Type;
const Scope = @import("../scope.zig").Scope;
const flow_analysis = @import("analysis.zig");

pub fn analyze(sema: anytype, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
    const node = sema.ast_tree.nodes.get(node_idx);
    return switch (node.tag) {
        .unsafe_block => analyzeUnsafeBlock(sema, node_idx, scope),
        .block => analyzeBlock(sema, node_idx, scope),
        .while_stmt => analyzeWhile(sema, node_idx, scope),
        .for_stmt => analyzeFor(sema, node_idx, scope),
        .break_stmt, .continue_stmt => analyzeLoopControl(sema, node_idx, scope),
        else => unreachable,
    };
}

fn analyzeUnsafeBlock(sema: anytype, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
    const node = sema.ast_tree.nodes.get(node_idx);
    sema.unsafe_depth += 1;
    defer sema.unsafe_depth -= 1;
    const ty = try sema.analyzeNode(node.data.lhs, scope);
    try sema.node_types.put(node_idx, ty);
    return ty;
}

fn analyzeBlock(sema: anytype, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
    const node = sema.ast_tree.nodes.get(node_idx);
    var child_scope = Scope.init(sema.allocator, scope);
    defer child_scope.deinit();

    var last_type = try sema.type_pool.internPrimitive(.void_type);
    var i: u32 = node.data.lhs;
    while (i < node.data.rhs) : (i += 1) {
        const child_idx = sema.ast_tree.extra_data[i];
        last_type = try sema.analyzeNode(child_idx, &child_scope);
        if (sema.type_pool.get(last_type).data == .error_union and flow_analysis.isDiscardedExpression(sema, child_idx)) {
            const child = sema.ast_tree.nodes.get(child_idx);
            try sema.reportError(4008, .sema, sema.ast_tree.tokens[child.main_token].start, "Error-union value must be handled explicitly");
        }
    }

    try sema.node_types.put(node_idx, last_type);
    return last_type;
}

fn analyzeWhile(sema: anytype, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
    const node = sema.ast_tree.nodes.get(node_idx);
    const extra_start = node.data.lhs;
    const label_token = sema.ast_tree.extra_data[extra_start];
    const cond = sema.ast_tree.extra_data[extra_start + 1];
    const body = sema.ast_tree.extra_data[extra_start + 2];

    const condition_type = try sema.analyzeNode(cond, scope);
    if (!isPrimitive(sema.type_pool.get(condition_type), .bool_type)) {
        try sema.reportError(4001, .sema, sema.ast_tree.tokens[sema.ast_tree.nodes.get(cond).main_token].start, "while condition must have type bool");
    }
    try sema.loop_stack.append(sema.allocator, .{ .label_token = label_token });
    _ = try sema.analyzeNode(body, scope);
    const loop = sema.loop_stack.pop().?;

    const void_type = try sema.type_pool.internPrimitive(.void_type);
    var result_type = loop.break_type orelse void_type;
    if (!isPrimitive(sema.type_pool.get(result_type), .void_type) and sema.const_values.get(cond) != 1) {
        try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "A value-producing while loop must have a statically true condition");
        result_type = void_type;
    }
    try sema.node_types.put(node_idx, result_type);
    return result_type;
}

fn analyzeFor(sema: anytype, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
    const node = sema.ast_tree.nodes.get(node_idx);
    const extra_start = node.data.lhs;
    const label_token = sema.ast_tree.extra_data[extra_start];
    const capture_flags = sema.ast_tree.extra_data[extra_start + 1];
    const item_token = sema.ast_tree.extra_data[extra_start + 2];
    const index_token = sema.ast_tree.extra_data[extra_start + 3];
    const iterable_node = sema.ast_tree.extra_data[extra_start + 4];
    const body_node = sema.ast_tree.extra_data[extra_start + 5];
    const iterable = sema.ast_tree.nodes.get(iterable_node);
    var item_type: Type.Id = undefined;
    if (iterable.tag == .range) {
        const start_type = try sema.analyzeNode(iterable.data.lhs, scope);
        const end_type = try sema.analyzeNode(iterable.data.rhs, scope);
        const start_value = sema.type_pool.get(start_type);
        const end_value = sema.type_pool.get(end_type);
        if (!start_value.isInteger() or !end_value.isInteger() or
            (!sema.type_pool.isCoercible(start_type, end_type) and !sema.type_pool.isCoercible(end_type, start_type)))
        {
            try sema.reportError(4001, .sema, sema.ast_tree.tokens[iterable.main_token].start, "for range bounds must have compatible integer types");
        }
        try sema.node_types.put(iterable_node, start_type);
        item_type = start_type;
        if ((capture_flags & 1) != 0) {
            try sema.reportError(4001, .sema, sema.ast_tree.tokens[item_token].start, "A range item has no address and cannot use pointer capture");
        }
    } else {
        const iterable_type_id = try sema.analyzeNode(iterable_node, scope);
        const iterable_type = sema.type_pool.get(iterable_type_id);
        const child_type: Type.Id = switch (iterable_type.data) {
            .array => |array| array.child_type,
            .pointer => |pointer| if (pointer.size == .Slice)
                pointer.child_type
            else blk: {
                try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "for iterable must be an array, slice, or range");
                break :blk try sema.type_pool.internPrimitive(.void_type);
            },
            else => blk: {
                try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "for iterable must be an array, slice, or range");
                break :blk try sema.type_pool.internPrimitive(.void_type);
            },
        };
        if ((capture_flags & 1) != 0) {
            var is_const_storage = switch (iterable_type.data) {
                .pointer => |pointer| pointer.is_const,
                else => false,
            };
            if (iterable.tag == .identifier) {
                const iterable_token = sema.ast_tree.tokens[iterable.main_token];
                const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
                if (scope.get(source[iterable_token.start..iterable_token.end])) |symbol| {
                    is_const_storage = is_const_storage or symbol.is_const;
                }
            }
            if (is_const_storage) {
                try sema.reportError(4001, .sema, sema.ast_tree.tokens[item_token].start, "Pointer capture requires a mutable iterable");
            }
            item_type = try sema.type_pool.internPtr(child_type, false);
        } else {
            item_type = child_type;
        }
    }

    var loop_scope = Scope.init(sema.allocator, scope);
    defer loop_scope.deinit();
    const src = sema.diags.source_manager.getFile(sema.source_id).?.content;
    const item_tok = sema.ast_tree.tokens[item_token];
    try loop_scope.put(src[item_tok.start..item_tok.end], .{
        .name = src[item_tok.start..item_tok.end],
        .decl_node = node_idx,
        .type_id = item_type,
        .is_const = true,
    });
    if (index_token != std.math.maxInt(u32)) {
        const index_tok = sema.ast_tree.tokens[index_token];
        const index_type = try sema.type_pool.internSizeInt(false);
        try loop_scope.put(src[index_tok.start..index_tok.end], .{
            .name = src[index_tok.start..index_tok.end],
            .decl_node = node_idx,
            .type_id = index_type,
            .is_const = true,
        });
    }

    try sema.loop_stack.append(sema.allocator, .{ .label_token = label_token });
    _ = try sema.analyzeNode(body_node, &loop_scope);
    const loop = sema.loop_stack.pop().?;

    const void_type = try sema.type_pool.internPrimitive(.void_type);
    if (loop.break_type) |break_type| {
        if (!isPrimitive(sema.type_pool.get(break_type), .void_type)) {
            try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "A finite for loop cannot produce a value because it may end without break");
        }
    }
    try sema.node_types.put(node_idx, void_type);
    return void_type;
}

fn analyzeLoopControl(sema: anytype, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
    const node = sema.ast_tree.nodes.get(node_idx);
    if (sema.loop_stack.items.len == 0) {
        try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "Loop control statement used outside a loop");
    } else {
        const target_index = flow_analysis.findLoopTarget(sema, node.data.lhs) orelse {
            try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "Unknown loop label");
            const noreturn_type = try sema.type_pool.internPrimitive(.noreturn_type);
            try sema.node_types.put(node_idx, noreturn_type);
            return noreturn_type;
        };
        if (node.tag == .break_stmt) {
            const break_type = if (node.data.rhs == std.math.maxInt(u32))
                try sema.type_pool.internPrimitive(.void_type)
            else
                try sema.analyzeNode(node.data.rhs, scope);
            if (sema.loop_stack.items[target_index].break_type) |previous_type| {
                if (sema.type_pool.isCoercible(break_type, previous_type)) {
                    // Keep the wider/established result type.
                } else if (sema.type_pool.isCoercible(previous_type, break_type)) {
                    sema.loop_stack.items[target_index].break_type = break_type;
                } else {
                    try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "Break values for the same loop have incompatible types");
                }
            } else {
                sema.loop_stack.items[target_index].break_type = break_type;
            }
        }
    }
    const noreturn_type = try sema.type_pool.internPrimitive(.noreturn_type);
    try sema.node_types.put(node_idx, noreturn_type);
    return noreturn_type;
}

fn isPrimitive(ty: Type, primitive: Type.Primitive) bool {
    return switch (ty.data) {
        .primitive => |value| value == primitive,
        else => false,
    };
}
