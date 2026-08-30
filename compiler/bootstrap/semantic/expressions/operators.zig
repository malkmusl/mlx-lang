const Node = @import("../../syntax/ast.zig").Node;
const Tag = @import("../../syntax/token.zig").Token.Tag;
const syntax_operators = @import("../../syntax/parser/operators.zig");
const Scope = @import("../scope.zig").Scope;
const Type = @import("../type.zig").Type;

pub fn analyze(sema: anytype, node_index: Node.Index, scope: *Scope) !Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    const operator = sema.ast_tree.tokens[node.main_token].tag;
    if (syntax_operators.isAssignment(operator)) return analyzeAssignment(sema, node_index, scope, operator);

    const lhs = try sema.analyzeNode(node.data.lhs, scope);
    const rhs = try sema.analyzeNode(node.data.rhs, scope);
    const lhs_type = sema.type_pool.get(lhs);
    const rhs_type = sema.type_pool.get(rhs);
    const compatible = sema.type_pool.isCoercible(lhs, rhs) or sema.type_pool.isCoercible(rhs, lhs);

    if (isComparison(operator)) {
        if (!compatible) try invalidOperands(sema, node, "Comparison operands have incompatible types");
        const result = try sema.type_pool.internPrimitive(.bool_type);
        try sema.node_types.put(node_index, result);
        return result;
    }
    if (isLogical(operator)) {
        if (!isBool(lhs_type) or !isBool(rhs_type)) try invalidOperands(sema, node, "Logical operators require bool operands");
        const result = try sema.type_pool.internPrimitive(.bool_type);
        try sema.node_types.put(node_index, result);
        return result;
    }
    if (isShift(operator)) {
        if (!lhs_type.isInteger() or !rhs_type.isInteger()) try invalidOperands(sema, node, "Shift operators require integer operands");
        if (sema.const_values.get(node.data.rhs)) |count| {
            const width = integerWidth(lhs_type);
            if (width != 0 and count >= width and !hasWrappingShiftCount(operator)) {
                try sema.reportError(4012, .sema, sema.ast_tree.tokens[node.main_token].start, "Shift count is outside the operand bit width");
            }
        }
    } else if (isBitwise(operator)) {
        if (!lhs_type.isInteger() or !rhs_type.isInteger() or !compatible) try invalidOperands(sema, node, "Bitwise operators require compatible integer operands");
    } else if (isArithmetic(operator)) {
        const numeric = (lhs_type.isInteger() or lhs_type.isFloat()) and (rhs_type.isInteger() or rhs_type.isFloat());
        if (!numeric or !compatible) try invalidOperands(sema, node, "Arithmetic operators require compatible numeric operands");
    } else {
        try invalidOperands(sema, node, "Unsupported binary operator");
    }

    try sema.node_types.put(node_index, lhs);
    return lhs;
}

fn analyzeAssignment(sema: anytype, node_index: Node.Index, scope: *Scope, operator: Tag) !Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    const target = sema.ast_tree.nodes.get(node.data.lhs);
    const target_type = try sema.analyzeNode(node.data.lhs, scope);
    const value_type = try sema.analyzeNode(node.data.rhs, scope);
    if (target.tag != .identifier) {
        try sema.reportError(4015, .sema, sema.ast_tree.tokens[node.main_token].start, "Assignment target is not addressable storage");
    } else {
        const token = sema.ast_tree.tokens[target.main_token];
        const source = sema.diags.source_manager.getFile(0).?.content;
        if (scope.get(source[token.start..token.end])) |symbol| {
            if (symbol.is_const) try sema.reportError(4015, .sema, token.start, "Cannot assign to const storage");
        }
    }
    if (!sema.type_pool.isCoercible(value_type, target_type)) {
        try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "Assigned expression does not match target type");
    }
    if (operator != .equal) {
        const target_value = sema.type_pool.get(target_type);
        if (!(target_value.isInteger() or target_value.isFloat())) {
            try invalidOperands(sema, node, "Compound assignment requires a numeric or integer destination");
        }
    }
    const result = try sema.type_pool.internPrimitive(.void_type);
    try sema.node_types.put(node_index, result);
    return result;
}

fn invalidOperands(sema: anytype, node: Node, message: []const u8) !void {
    try sema.reportError(4014, .sema, sema.ast_tree.tokens[node.main_token].start, message);
}

fn isBool(value: Type) bool {
    return value.data == .primitive and value.data.primitive == .bool_type;
}

pub fn isComparison(tag: Tag) bool {
    return switch (tag) {
        .equal_equal, .bang_equal, .angle_bracket_left, .angle_bracket_left_equal, .angle_bracket_right, .angle_bracket_right_equal => true,
        else => false,
    };
}

pub fn isLogical(tag: Tag) bool {
    return tag == .ampersand_ampersand or tag == .pipe_pipe;
}

pub fn isBitwise(tag: Tag) bool {
    return tag == .ampersand or tag == .pipe or tag == .caret;
}

pub fn isArithmetic(tag: Tag) bool {
    return switch (tag) {
        .plus, .minus, .asterisk, .slash, .percent,
        .plus_percent, .minus_percent, .asterisk_percent,
        .plus_pipe, .minus_pipe, .asterisk_pipe,
        => true,
        else => false,
    };
}

pub fn isShift(tag: Tag) bool {
    return switch (tag) {
        .shl, .shr, .shl_percent, .shr_percent, .shl_pipe, .shl_percent_pipe,
        .ampersand_shl, .pipe_shl, .caret_shl, .ampersand_shr, .pipe_shr, .caret_shr,
        .shl_ampersand, .shl_caret, .shr_ampersand, .shr_pipe, .shr_caret,
        .plus_shl, .minus_shl, .asterisk_shl,
        .plus_percent_shl, .minus_percent_shl, .asterisk_percent_shl,
        .plus_pipe_shl, .minus_pipe_shl, .asterisk_pipe_shl,
        .plus_shr, .minus_shr, .asterisk_shr,
        .plus_percent_shr, .minus_percent_shr, .asterisk_percent_shr,
        .plus_pipe_shr, .minus_pipe_shr, .asterisk_pipe_shr,
        => true,
        else => false,
    };
}

fn hasWrappingShiftCount(tag: Tag) bool {
    return tag == .shl_percent or tag == .shr_percent or tag == .shl_percent_pipe;
}

fn integerWidth(value: Type) u16 {
    return switch (value.data) {
        .integer => |integer| integer.bits,
        .size_int => 64,
        else => 0,
    };
}
