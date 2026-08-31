const Node = @import("../../syntax/ast.zig").Node;
const Scope = @import("../scope.zig").Scope;
const Type = @import("../type.zig").Type;

pub fn analyze(sema: anytype, node_index: Node.Index, scope: *Scope) !?Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    const operator = sema.ast_tree.tokens[node.main_token].tag;
    return switch (operator) {
        .keyword_try => try analyzeTry(sema, node_index, scope),
        .keyword_comptime => try analyzeComptime(sema, node_index, scope),
        .ampersand => try analyzeAddressOf(sema, node_index, scope),
        .bang => try analyzeLogicalNot(sema, node_index, scope),
        .minus => try analyzeNumeric(sema, node_index, scope, true),
        .tilde => try analyzeNumeric(sema, node_index, scope, false),
        else => null,
    };
}

fn analyzeTry(sema: anytype, node_index: Node.Index, scope: *Scope) !Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    const operand_type = try sema.analyzeNode(node.data.lhs, scope);
    const operand = sema.type_pool.get(operand_type);
    if (operand.data != .error_union) {
        try sema.reportError(4014, .sema, sema.ast_tree.tokens[node.main_token].start, "try requires an error-union operand");
        try sema.node_types.put(node_index, operand_type);
        return operand_type;
    }
    const return_type = if (sema.current_return_type) |type_id| sema.type_pool.get(type_id) else null;
    if (return_type == null or return_type.?.data != .error_union) {
        try sema.reportError(4008, .sema, sema.ast_tree.tokens[node.main_token].start, "try can only propagate from an error-capable function");
    }
    const payload = operand.data.error_union.payload;
    try sema.node_types.put(node_index, payload);
    return payload;
}

fn analyzeComptime(sema: anytype, node_index: Node.Index, scope: *Scope) !Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    const operand_type = try sema.analyzeNode(node.data.lhs, scope);
    if (sema.const_values.get(node.data.lhs)) |value| {
        try sema.const_values.put(node_index, value);
    } else if (sema.type_values.get(node.data.lhs)) |value| {
        try sema.type_values.put(node_index, value);
    } else {
        try sema.reportError(5001, .@"comptime", sema.ast_tree.tokens[node.main_token].start, "comptime expression depends on a runtime value");
    }
    try sema.node_types.put(node_index, operand_type);
    return operand_type;
}

fn analyzeAddressOf(sema: anytype, node_index: Node.Index, scope: *Scope) !Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    const operand_node = sema.ast_tree.nodes.get(node.data.lhs);
    const operand_type = try sema.analyzeNode(node.data.lhs, scope);
    var is_const = false;
    if (operand_node.tag != .identifier) {
        try sema.reportError(7001, .sema, sema.ast_tree.tokens[node.main_token].start, "Address-of requires addressable storage");
    } else {
        const token = sema.ast_tree.tokens[operand_node.main_token];
        const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
        if (scope.get(source[token.start..token.end])) |symbol| is_const = symbol.is_const;
    }
    const result_type = try sema.type_pool.internPtr(operand_type, is_const);
    try sema.node_types.put(node_index, result_type);
    return result_type;
}

fn analyzeLogicalNot(sema: anytype, node_index: Node.Index, scope: *Scope) !Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    const operand_type = try sema.analyzeNode(node.data.lhs, scope);
    const operand = sema.type_pool.get(operand_type);
    if (operand.data != .primitive or operand.data.primitive != .bool_type) {
        try sema.reportError(4014, .sema, sema.ast_tree.tokens[node.main_token].start, "Logical not requires a bool operand");
    }
    const result_type = try sema.type_pool.internPrimitive(.bool_type);
    if (sema.const_values.get(node.data.lhs)) |value| try sema.const_values.put(node_index, @intFromBool(value == 0));
    try sema.node_types.put(node_index, result_type);
    return result_type;
}

fn analyzeNumeric(sema: anytype, node_index: Node.Index, scope: *Scope, is_negate: bool) !Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    const operand_type = try sema.analyzeNode(node.data.lhs, scope);
    const operand = sema.type_pool.get(operand_type);
    if ((is_negate and !(operand.isInteger() or operand.isFloat())) or (!is_negate and !operand.isInteger())) {
        try sema.reportError(4014, .sema, sema.ast_tree.tokens[node.main_token].start, if (is_negate)
            "Numeric negation requires an integer or float operand"
        else
            "Bitwise complement requires an integer operand");
    }
    if (sema.const_values.get(node.data.lhs)) |value| {
        try sema.const_values.put(node_index, if (is_negate) 0 -% value else ~value);
    }
    try sema.node_types.put(node_index, operand_type);
    return operand_type;
}
