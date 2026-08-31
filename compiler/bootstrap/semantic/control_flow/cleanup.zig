const Node = @import("../../syntax/ast.zig").Node;
const Scope = @import("../scope.zig").Scope;
const Type = @import("../type.zig").Type;

pub fn analyze(sema: anytype, node_index: Node.Index, scope: *Scope) !Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    _ = try sema.analyzeNode(node.data.lhs, scope);
    if (node.tag == .errdefer_stmt) {
        const return_type = if (sema.current_return_type) |type_id| sema.type_pool.get(type_id) else null;
        if (return_type == null or return_type.?.data != .error_union) {
            try sema.reportError(4008, .sema, sema.ast_tree.tokens[node.main_token].start, "errdefer requires an error-capable function");
        }
    }
    const void_type = try sema.type_pool.internPrimitive(.void_type);
    try sema.node_types.put(node_index, void_type);
    return void_type;
}
