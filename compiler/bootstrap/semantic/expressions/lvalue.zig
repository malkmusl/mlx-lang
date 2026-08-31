const Node = @import("../../syntax/ast.zig").Node;
const Scope = @import("../scope.zig").Scope;
const Type = @import("../type.zig").Type;

pub fn validateMutable(sema: anytype, node_index: Node.Index, scope: *Scope, operator_start: u32) !Type.Id {
    const type_id = try sema.analyzeNode(node_index, scope);
    if (!isAddressable(sema, node_index)) {
        try sema.reportError(4015, .sema, operator_start, "Assignment target is not addressable storage");
        return type_id;
    }
    if (isConstStorage(sema, node_index, scope)) {
        try sema.reportError(4015, .sema, operator_start, "Cannot assign to const storage");
    }
    return type_id;
}

pub fn isAddressable(sema: anytype, node_index: Node.Index) bool {
    const node = sema.ast_tree.nodes.get(node_index);
    return switch (node.tag) {
        .identifier, .field_access, .array_access => true,
        .unary_op => sema.ast_tree.tokens[node.main_token].tag == .dot_asterisk,
        else => false,
    };
}

fn isConstStorage(sema: anytype, node_index: Node.Index, scope: *Scope) bool {
    const node = sema.ast_tree.nodes.get(node_index);
    return switch (node.tag) {
        .identifier => blk: {
            const token = sema.ast_tree.tokens[node.main_token];
            const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
            break :blk if (scope.get(source[token.start..token.end])) |symbol| symbol.is_const else true;
        },
        .field_access => isConstStorage(sema, node.data.lhs, scope),
        .array_access => blk: {
            const container_id = sema.node_types.get(node.data.lhs) orelse break :blk true;
            const container = sema.type_pool.get(container_id);
            // A const binding containing a mutable pointer does not make the
            // pointee const. Arrays, in contrast, inherit their binding's
            // mutability because the storage is the binding itself.
            if (container.data == .pointer) break :blk container.data.pointer.is_const;
            break :blk isConstStorage(sema, node.data.lhs, scope);
        },
        .unary_op => blk: {
            const pointer_id = sema.node_types.get(node.data.lhs) orelse break :blk true;
            const pointer = sema.type_pool.get(pointer_id);
            break :blk pointer.data != .pointer or pointer.data.pointer.is_const;
        },
        else => true,
    };
}
