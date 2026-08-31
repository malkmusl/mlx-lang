const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Scope = @import("../scope.zig").Scope;
const Type = @import("../type.zig").Type;
const optional = @import("optional.zig");

pub fn analyze(sema: anytype, node_index: Node.Index, scope: *Scope) !Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    return switch (node.tag) {
        .array_access => analyzeIndex(sema, node_index, scope),
        .slice => analyzeSlice(sema, node_index, scope),
        else => unreachable,
    };
}

pub fn analyzeUnarySuffix(sema: anytype, node_index: Node.Index, scope: *Scope) !?Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    const operator = sema.ast_tree.tokens[node.main_token].tag;
    if (operator != .dot_asterisk and operator != .dot_question) return null;

    if (operator == .dot_question) return try optional.analyzeUnwrap(sema, node_index, scope);
    const operand_type_id = try sema.analyzeNode(node.data.lhs, scope);
    const operand_type = sema.type_pool.get(operand_type_id);
    const result_type = switch (operator) {
        .dot_asterisk => if (operand_type.data == .pointer)
            operand_type.data.pointer.child_type
        else blk: {
            try sema.reportError(7001, .sema, sema.ast_tree.tokens[node.main_token].start, "Pointer dereference requires a pointer operand");
            break :blk operand_type_id;
        },
        .dot_question => unreachable,
        else => unreachable,
    };
    try sema.node_types.put(node_index, result_type);
    return result_type;
}

fn analyzeIndex(sema: anytype, node_index: Node.Index, scope: *Scope) !Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    const container_id = try sema.analyzeNode(node.data.lhs, scope);
    const index_id = try sema.analyzeNode(node.data.rhs, scope);
    if (!sema.type_pool.get(index_id).isInteger()) {
        try sema.reportError(4014, .sema, sema.ast_tree.tokens[node.main_token].start, "Index expression must be an integer");
    }
    const result_type = elementType(sema, container_id) orelse {
        try sema.reportError(4014, .sema, sema.ast_tree.tokens[node.main_token].start, "Indexing requires an array, slice, or pointer");
        try sema.node_types.put(node_index, container_id);
        return container_id;
    };
    try sema.node_types.put(node_index, result_type);
    return result_type;
}

fn analyzeSlice(sema: anytype, node_index: Node.Index, scope: *Scope) !Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    const start = node.data.lhs;
    const container_node = sema.ast_tree.extra_data[start];
    const lower_node = sema.ast_tree.extra_data[start + 1];
    const upper_node = sema.ast_tree.extra_data[start + 2];
    const container_id = try sema.analyzeNode(container_node, scope);
    const child = elementType(sema, container_id) orelse {
        try sema.reportError(4014, .sema, sema.ast_tree.tokens[node.main_token].start, "Slicing requires an array, slice, or many-item pointer");
        try sema.node_types.put(node_index, container_id);
        return container_id;
    };
    if (lower_node != std.math.maxInt(u32)) try requireInteger(sema, lower_node, scope);
    if (upper_node != std.math.maxInt(u32)) try requireInteger(sema, upper_node, scope);

    const container = sema.type_pool.get(container_id);
    const is_const = container.data == .pointer and container.data.pointer.is_const;
    const result_type = try sema.type_pool.internSlice(child, is_const);
    try sema.node_types.put(node_index, result_type);
    return result_type;
}

fn requireInteger(sema: anytype, node_index: Node.Index, scope: *Scope) !void {
    const type_id = try sema.analyzeNode(node_index, scope);
    if (!sema.type_pool.get(type_id).isInteger()) {
        const token = sema.ast_tree.tokens[sema.ast_tree.nodes.get(node_index).main_token];
        try sema.reportError(4014, .sema, token.start, "Slice bound must be an integer");
    }
}

fn elementType(sema: anytype, container_id: Type.Id) ?Type.Id {
    return switch (sema.type_pool.get(container_id).data) {
        .array => |array| array.child_type,
        .pointer => |pointer| pointer.child_type,
        else => null,
    };
}
