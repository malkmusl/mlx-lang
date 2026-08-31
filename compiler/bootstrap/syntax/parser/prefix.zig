const Node = @import("../ast.zig").Node;

/// Parses the closed prefix-operator family from grammar.ebnf. Returning null
/// means the next token starts a primary expression instead.
pub fn parse(parser: anytype) !?Node.Index {
    if (parser.index >= parser.tokens.len) return null;
    return switch (parser.tokens[parser.index].tag) {
        .bang, .minus, .tilde, .ampersand, .keyword_try, .keyword_comptime => parseUnary(parser),
        else => null,
    };
}

fn parseUnary(parser: anytype) !?Node.Index {
    const operator_token = parser.index;
    parser.consumeToken("Consume prefix unary operator");
    const operand = try parser.parseExpr(90) orelse return null;
    try parser.nodes.append(parser.allocator, .{
        .tag = .unary_op,
        .main_token = operator_token,
        .data = .{ .lhs = operand, .rhs = 0 },
    });
    return @intCast(parser.nodes.len - 1);
}
