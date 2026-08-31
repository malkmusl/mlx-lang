const Node = @import("../../syntax/ast.zig").Node;
const Tag = @import("../../syntax/token.zig").Token.Tag;
const syntax_operators = @import("../../syntax/parser/operators.zig");
const Scope = @import("../scope.zig").Scope;
const Type = @import("../type.zig").Type;
const lvalue = @import("lvalue.zig");
const integer_semantics = @import("../numbers/integer.zig");

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
        const comparison_type = if (isComptimeNumber(lhs_type) and !isComptimeNumber(rhs_type)) rhs else lhs;
        try foldComparison(sema, node_index, node, operator, comparison_type);
        try sema.node_types.put(node_index, result);
        return result;
    }
    if (isLogical(operator)) {
        if (!isBool(lhs_type) or !isBool(rhs_type)) try invalidOperands(sema, node, "Logical operators require bool operands");
        const result = try sema.type_pool.internPrimitive(.bool_type);
        if (sema.const_values.get(node.data.lhs)) |left| if (sema.const_values.get(node.data.rhs)) |right| {
            try sema.const_values.put(node_index, @intFromBool(if (operator == .ampersand_ampersand) left != 0 and right != 0 else left != 0 or right != 0));
        };
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

    const result_type = if (isComptimeNumber(lhs_type) and !isComptimeNumber(rhs_type)) rhs else lhs;
    try foldInteger(sema, node_index, node, operator, result_type);
    try sema.node_types.put(node_index, result_type);
    return result_type;
}

fn foldComparison(sema: anytype, node_index: Node.Index, node: Node, operator: Tag, comparison_type: Type.Id) !void {
    const left = sema.const_values.get(node.data.lhs) orelse return;
    const right = sema.const_values.get(node.data.rhs) orelse return;
    const value_type = sema.type_pool.get(comparison_type);
    const signed_comparison = integer_semantics.isSigned(value_type) or
        (value_type.data == .primitive and value_type.data.primitive == .comptime_int_type);
    const bits: u16 = switch (value_type.data) {
        .integer => |integer| integer.bits,
        else => 64,
    };
    const signed_left = integer_semantics.signed(left, bits);
    const signed_right = integer_semantics.signed(right, bits);
    const result = switch (operator) {
        .equal_equal => left == right,
        .bang_equal => left != right,
        .angle_bracket_left => if (signed_comparison) signed_left < signed_right else left < right,
        .angle_bracket_left_equal => if (signed_comparison) signed_left <= signed_right else left <= right,
        .angle_bracket_right => if (signed_comparison) signed_left > signed_right else left > right,
        .angle_bracket_right_equal => if (signed_comparison) signed_left >= signed_right else left >= right,
        else => return,
    };
    try sema.const_values.put(node_index, @intFromBool(result));
}

fn foldInteger(sema: anytype, node_index: Node.Index, node: Node, operator: Tag, result_type: Type.Id) !void {
    const left = sema.const_values.get(node.data.lhs) orelse return;
    const right = sema.const_values.get(node.data.rhs) orelse return;
    const result_value = sema.type_pool.get(result_type);
    if (!result_value.isInteger()) return;
    const width = integerWidth(result_value);
    const max = unsignedMax(width);
    var result: u64 = 0;
    switch (operator) {
        .plus, .plus_percent, .plus_pipe => {
            const wide = @as(u128, left) + @as(u128, right);
            if (operator == .plus and width != 0 and wide > max) try sema.reportError(4002, .sema, sema.ast_tree.tokens[node.main_token].start, "Checked integer addition overflows its result type");
            result = if (operator == .plus_pipe and width != 0) @intCast(@min(wide, max)) else @truncate(wide);
        },
        .minus, .minus_percent, .minus_pipe => {
            if (operator == .minus and width != 0 and left < right) try sema.reportError(4002, .sema, sema.ast_tree.tokens[node.main_token].start, "Checked integer subtraction overflows its result type");
            result = if (operator == .minus_pipe and width != 0 and left < right) 0 else left -% right;
        },
        .asterisk, .asterisk_percent, .asterisk_pipe => {
            const wide = @as(u128, left) * @as(u128, right);
            if (operator == .asterisk and width != 0 and wide > max) try sema.reportError(4002, .sema, sema.ast_tree.tokens[node.main_token].start, "Checked integer multiplication overflows its result type");
            result = if (operator == .asterisk_pipe and width != 0) @intCast(@min(wide, max)) else @truncate(wide);
        },
        .slash, .percent => {
            if (right == 0) {
                try sema.reportError(4002, .sema, sema.ast_tree.tokens[node.main_token].start, "Division by zero in a comptime expression");
                return;
            }
            result = if (operator == .slash) left / right else left % right;
        },
        .ampersand => result = left & right,
        .pipe => result = left | right,
        .caret => result = left ^ right,
        .shl, .shl_percent, .shl_pipe, .shl_percent_pipe => {
            if (width == 0) return;
            const count = if (hasWrappingShiftCount(operator)) right % width else right;
            if (count >= 64) return;
            const wide = @as(u128, left) << @intCast(count);
            if ((operator == .shl or operator == .shl_percent) and wide > max) {
                try sema.reportError(4013, .sema, sema.ast_tree.tokens[node.main_token].start, "Checked left shift loses value bits");
            }
            result = if (operator == .shl_pipe or operator == .shl_percent_pipe) @intCast(@min(wide, max)) else @truncate(wide);
        },
        .shr, .shr_percent => {
            if (width == 0) return;
            const count = if (hasWrappingShiftCount(operator)) right % width else right;
            if (count >= 64) return;
            result = left >> @intCast(count);
        },
        else => return,
    }
    if (width != 0 and width < 64 and (isWrappingArithmetic(operator) or operator == .shl_percent or operator == .shr_percent)) result &= @intCast(max);
    try sema.const_values.put(node_index, result);
}

fn analyzeAssignment(sema: anytype, node_index: Node.Index, scope: *Scope, operator: Tag) !Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    const target = sema.ast_tree.nodes.get(node.data.lhs);
    const target_type = try lvalue.validateMutable(sema, node.data.lhs, scope, sema.ast_tree.tokens[node.main_token].start);
    const value_type = try sema.analyzeNode(node.data.rhs, scope);
    if (!sema.type_pool.isCoercible(value_type, target_type)) {
        try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "Assigned expression does not match target type");
    }
    if (operator != .equal) {
        const target_value = sema.type_pool.get(target_type);
        if (!(target_value.isInteger() or target_value.isFloat())) {
            try invalidOperands(sema, node, "Compound assignment requires a numeric or integer destination");
        }
    }
    if (operator == .equal and target.tag == .identifier) {
        const token = sema.ast_tree.tokens[target.main_token];
        const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
        if (scope.get(source[token.start..token.end])) |symbol| try sema.local_states.put(symbol.decl_node, .initialized);
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

fn isComptimeNumber(value: Type) bool {
    return value.data == .primitive and (value.data.primitive == .comptime_int_type or value.data.primitive == .comptime_float_type);
}

fn isWrappingArithmetic(tag: Tag) bool {
    return tag == .plus_percent or tag == .minus_percent or tag == .asterisk_percent;
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
        .plus,
        .minus,
        .asterisk,
        .slash,
        .percent,
        .plus_percent,
        .minus_percent,
        .asterisk_percent,
        .plus_pipe,
        .minus_pipe,
        .asterisk_pipe,
        => true,
        else => false,
    };
}

pub fn isShift(tag: Tag) bool {
    return switch (tag) {
        .shl,
        .shr,
        .shl_percent,
        .shr_percent,
        .shl_pipe,
        .shl_percent_pipe,
        .ampersand_shl,
        .pipe_shl,
        .caret_shl,
        .ampersand_shr,
        .pipe_shr,
        .caret_shr,
        .shl_ampersand,
        .shl_caret,
        .shr_ampersand,
        .shr_pipe,
        .shr_caret,
        .plus_shl,
        .minus_shl,
        .asterisk_shl,
        .plus_percent_shl,
        .minus_percent_shl,
        .asterisk_percent_shl,
        .plus_pipe_shl,
        .minus_pipe_shl,
        .asterisk_pipe_shl,
        .plus_shr,
        .minus_shr,
        .asterisk_shr,
        .plus_percent_shr,
        .minus_percent_shr,
        .asterisk_percent_shr,
        .plus_pipe_shr,
        .minus_pipe_shr,
        .asterisk_pipe_shr,
        => true,
        else => false,
    };
}

fn hasWrappingShiftCount(tag: Tag) bool {
    return tag == .shl_percent or tag == .shr_percent or tag == .shl_percent_pipe;
}

fn unsignedMax(width: u16) u128 {
    if (width == 0 or width >= 64) return @as(u128, @import("std").math.maxInt(u64));
    return (@as(u128, 1) << @intCast(width)) - 1;
}

fn integerWidth(value: Type) u16 {
    return switch (value.data) {
        .integer => |integer| integer.bits,
        .size_int => 64,
        else => 0,
    };
}
