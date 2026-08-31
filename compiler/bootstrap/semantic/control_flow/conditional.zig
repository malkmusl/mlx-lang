const Node = @import("../../syntax/ast.zig").Node;
const Scope = @import("../scope.zig").Scope;
const Type = @import("../type.zig").Type;

pub fn analyze(sema: anytype, node_index: Node.Index, scope: *Scope) !Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    const start = node.data.lhs;
    const condition_node = sema.ast_tree.extra_data[start];
    const condition_type = try sema.analyzeNode(condition_node, scope);
    if (!isPrimitive(sema.type_pool.get(condition_type), .bool_type)) {
        try sema.reportError(4001, .sema, sema.ast_tree.tokens[sema.ast_tree.nodes.get(condition_node).main_token].start, "if condition must have type bool");
    }

    const has_else = node.data.rhs > start + 2;
    if (sema.const_values.get(condition_node)) |condition| {
        const selected = if (condition != 0)
            sema.ast_tree.extra_data[start + 1]
        else if (has_else)
            sema.ast_tree.extra_data[start + 2]
        else {
            const void_type = try sema.type_pool.internPrimitive(.void_type);
            try sema.node_types.put(node_index, void_type);
            return void_type;
        };
        const selected_type = try sema.analyzeNode(selected, scope);
        if (sema.const_values.get(selected)) |value| try sema.const_values.put(node_index, value);
        if (sema.type_values.get(selected)) |value| try sema.type_values.put(node_index, value);
        try sema.node_types.put(node_index, selected_type);
        return selected_type;
    }

    const then_type = try sema.analyzeNode(sema.ast_tree.extra_data[start + 1], scope);
    var else_type = try sema.type_pool.internPrimitive(.void_type);
    if (has_else) else_type = try sema.analyzeNode(sema.ast_tree.extra_data[start + 2], scope);

    var result_type = try sema.type_pool.internPrimitive(.void_type);
    if (has_else) {
        const then_value = sema.type_pool.get(then_type);
        const else_value = sema.type_pool.get(else_type);
        if (isPrimitive(then_value, .noreturn_type)) {
            result_type = else_type;
        } else if (isPrimitive(else_value, .noreturn_type)) {
            result_type = then_type;
        } else if (sema.type_pool.isCoercible(then_type, else_type)) {
            result_type = else_type;
        } else if (sema.type_pool.isCoercible(else_type, then_type)) {
            result_type = then_type;
        } else {
            try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "if and else branches have incompatible types");
            result_type = then_type;
        }
    }
    try sema.node_types.put(node_index, result_type);
    return result_type;
}

fn isPrimitive(value: Type, primitive: Type.Primitive) bool {
    return value.data == .primitive and value.data.primitive == primitive;
}
