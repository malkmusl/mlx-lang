const Node = @import("../../syntax/ast.zig").Node;
const Scope = @import("../scope.zig").Scope;
const Type = @import("../type.zig").Type;

pub fn analyzeLiteral(sema: anytype, node_index: Node.Index) !Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    const primitive: Type.Primitive = switch (node.tag) {
        .null_literal => .null_type,
        .undefined_literal => .undefined_type,
        else => unreachable,
    };
    const result = try sema.type_pool.internPrimitive(primitive);
    if (node.tag == .null_literal) try sema.const_values.put(node_index, 0);
    try sema.node_types.put(node_index, result);
    return result;
}

pub fn analyzeUnwrap(sema: anytype, node_index: Node.Index, scope: *Scope) !Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    const operand_type_id = try sema.analyzeNode(node.data.lhs, scope);
    const operand_type = sema.type_pool.get(operand_type_id);
    if (operand_type.data != .optional) {
        try sema.reportError(4014, .sema, sema.ast_tree.tokens[node.main_token].start, "Optional unwrap requires an optional operand");
        try sema.node_types.put(node_index, operand_type_id);
        return operand_type_id;
    }
    const result = operand_type.data.optional.child_type;
    try sema.node_types.put(node_index, result);
    return result;
}
