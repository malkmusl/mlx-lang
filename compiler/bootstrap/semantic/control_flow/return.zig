const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Type = @import("../type.zig").Type;
const Scope = @import("../scope.zig").Scope;
const integer_semantics = @import("../numbers/integer.zig");

pub fn analyze(sema: anytype, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
    const node = sema.ast_tree.nodes.get(node_idx);
    const expected_type = sema.current_return_type orelse {
        try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "return used outside a function");
        return sema.putBuiltinResult(node_idx, try sema.type_pool.internPrimitive(.noreturn_type));
    };
    const expected = sema.type_pool.get(expected_type);
    if (node.data.rhs == std.math.maxInt(u32)) {
        const accepts_empty_return = isPrimitive(expected, .void_type) or
            (expected.data == .error_union and isPrimitive(sema.type_pool.get(expected.data.error_union.payload), .void_type));
        if (!accepts_empty_return) {
            try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "Return value required by function return type");
        }
    } else {
        const value_target = if (expected.data == .error_union) expected.data.error_union.payload else expected_type;
        const actual_type = (try analyzeTupleReturn(sema, node.data.rhs, value_target, scope)) orelse try sema.analyzeNode(node.data.rhs, scope);
        const return_matches = if (expected.data == .error_union)
            sema.type_pool.isCoercible(actual_type, expected_type)
        else
            !isPrimitive(expected, .void_type) and sema.type_pool.isCoercible(actual_type, expected_type);
        if (!return_matches) {
            try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "Returned expression does not match function return type");
        }
        if (sema.const_values.get(node.data.rhs)) |value| {
            const target_id = if (expected.data == .error_union) expected.data.error_union.payload else expected_type;
            const target = sema.type_pool.get(target_id);
            if (target.isInteger() and !integer_semantics.valueFits(target, value)) {
                try sema.reportError(4002, .sema, sema.ast_tree.tokens[node.main_token].start, "Returned comptime integer does not fit the return type");
            }
        }
    }
    const noreturn_type = try sema.type_pool.internPrimitive(.noreturn_type);
    try sema.node_types.put(node_idx, noreturn_type);
    return noreturn_type;
}

fn analyzeTupleReturn(sema: anytype, expression_node: Node.Index, target_type: Type.Id, scope: *Scope) std.mem.Allocator.Error!?Type.Id {
    const expression = sema.ast_tree.nodes.get(expression_node);
    if (expression.tag != .tuple_literal or sema.type_pool.get(target_type).data != .tuple) return null;

    const expected_fields = sema.type_pool.aggregateFields(target_type) orelse return null;
    const count = sema.ast_tree.extra_data[expression.data.lhs];
    if (count != expected_fields.len) {
        try sema.reportError(4001, .sema, sema.ast_tree.tokens[expression.main_token].start, "Returned tuple has the wrong number of values");
    }
    var index: u32 = 0;
    while (index < count and index < expected_fields.len) : (index += 1) {
        const value_node = sema.ast_tree.extra_data[expression.data.lhs + 1 + index];
        const actual = try sema.analyzeNode(value_node, scope);
        const target = expected_fields[index].type_id;
        if (!sema.type_pool.isCoercible(actual, target)) {
            try sema.reportError(4001, .sema, sema.ast_tree.tokens[sema.ast_tree.nodes.get(value_node).main_token].start, "Returned tuple value does not match its declared type");
        }
        if (sema.const_values.get(value_node)) |value| {
            const target_value_type = sema.type_pool.get(target);
            if (target_value_type.isInteger() and !integer_semantics.valueFits(target_value_type, value)) {
                try sema.reportError(4002, .sema, sema.ast_tree.tokens[sema.ast_tree.nodes.get(value_node).main_token].start, "Returned tuple integer does not fit its declared type");
            }
        }
    }
    try sema.node_types.put(expression_node, target_type);
    return target_type;
}

fn isPrimitive(ty: Type, primitive: Type.Primitive) bool {
    return ty.data == .primitive and ty.data.primitive == primitive;
}
