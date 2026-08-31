const std = @import("std");
const Node = @import("../ast.zig").Node;

/// Parses one postfix suffix. Returning `null` means that the next token is not
/// a postfix operator and the Pratt parser should continue with infix parsing.
pub fn parse(parser: anytype, lhs: Node.Index, binding_power: u8) !?Node.Index {
    if (100 < binding_power or parser.index >= parser.tokens.len) return null;
    return switch (parser.tokens[parser.index].tag) {
        .l_paren => parseCall(parser, lhs),
        .dot => if (parser.index + 1 < parser.tokens.len and parser.tokens[parser.index + 1].tag == .l_brace)
            parseAggregateLiteral(parser, lhs)
        else
            parseField(parser, lhs),
        .l_bracket => parseIndexOrSlice(parser, lhs),
        .dot_asterisk, .dot_question => parseUnarySuffix(parser, lhs),
        else => null,
    };
}

/// Typed aggregate literal: `Type.{ .field = value, ... }`.
fn parseAggregateLiteral(parser: anytype, type_node: Node.Index) !?Node.Index {
    const dot_token = parser.index;
    parser.consumeToken("Consume aggregate literal '.'");
    parser.consumeToken("Consume aggregate literal '{'");
    var fields = std.ArrayList(u32).empty;
    defer fields.deinit(parser.allocator);
    var count: u32 = 0;
    while (parser.index < parser.tokens.len and parser.tokens[parser.index].tag != .r_brace) {
        while (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .statement_end) parser.index += 1;
        if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .r_brace) break;
        if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .dot) {
            try parser.reportError(2001, "Expected '.field' in aggregate literal");
            return null;
        }
        parser.consumeToken("Consume aggregate field '.'");
        if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .ident) {
            try parser.reportError(2001, "Expected aggregate field name");
            return null;
        }
        const name_token = parser.index;
        parser.consumeToken("Consume aggregate field name");
        if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .equal) {
            try parser.reportError(2001, "Expected '=' in aggregate field initializer");
            return null;
        }
        parser.consumeToken("Consume aggregate field '='");
        const value = try parser.parseExpr(0) orelse return null;
        try fields.appendSlice(parser.allocator, &.{ name_token, value });
        count += 1;
        if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .comma) parser.index += 1;
    }
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_brace) {
        try parser.reportError(2003, "Expected '}' after aggregate literal");
        return null;
    }
    parser.consumeToken("Consume aggregate literal '}'");
    const extra_start: u32 = @intCast(parser.extra_data.items.len);
    try parser.extra_data.append(parser.allocator, count);
    try parser.extra_data.appendSlice(parser.allocator, fields.items);
    try parser.nodes.append(parser.allocator, .{
        .tag = .aggregate_literal,
        .main_token = dot_token,
        .data = .{ .lhs = type_node, .rhs = extra_start },
    });
    return @intCast(parser.nodes.len - 1);
}

fn parseCall(parser: anytype, lhs: Node.Index) !?Node.Index {
    const call_token = parser.index;
    parser.consumeToken("Consume call '('");

    var arguments = std.ArrayList(Node.Index).empty;
    defer arguments.deinit(parser.allocator);
    while (parser.index < parser.tokens.len and parser.tokens[parser.index].tag != .r_paren) {
        const argument = try parser.parseExpr(0) orelse return null;
        try arguments.append(parser.allocator, argument);
        if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .comma) {
            parser.consumeToken("Consume call argument comma");
        } else break;
    }
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_paren) {
        try parser.reportError(2008, "Expected ')' after function arguments");
        return null;
    }
    parser.consumeToken("Consume call ')'");

    const extra_start: u32 = @intCast(parser.extra_data.items.len);
    try parser.extra_data.append(parser.allocator, @intCast(arguments.items.len));
    try parser.extra_data.appendSlice(parser.allocator, arguments.items);
    try parser.nodes.append(parser.allocator, .{
        .tag = .call,
        .main_token = call_token,
        .data = .{ .lhs = lhs, .rhs = extra_start },
    });
    return @intCast(parser.nodes.len - 1);
}

fn parseField(parser: anytype, lhs: Node.Index) !?Node.Index {
    parser.consumeToken("Consume field access '.'");
    if (parser.index >= parser.tokens.len or (parser.tokens[parser.index].tag != .ident and parser.tokens[parser.index].tag != .integer)) {
        try parser.reportError(2001, "Expected field name after '.'");
        return null;
    }
    const field_token = parser.index;
    parser.consumeToken("Consume field name");
    try parser.nodes.append(parser.allocator, .{
        .tag = .field_access,
        .main_token = field_token,
        .data = .{ .lhs = lhs, .rhs = field_token },
    });
    return @intCast(parser.nodes.len - 1);
}

fn parseIndexOrSlice(parser: anytype, lhs: Node.Index) !?Node.Index {
    const bracket_token = parser.index;
    parser.consumeToken("Consume postfix '['");

    var first: Node.Index = std.math.maxInt(u32);
    if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag != .dot_dot) {
        first = try parser.parseExpr(0) orelse return null;
    }

    if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .dot_dot) {
        parser.consumeToken("Consume slice '..'");
        var end: Node.Index = std.math.maxInt(u32);
        if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag != .r_bracket) {
            end = try parser.parseExpr(0) orelse return null;
        }
        if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_bracket) {
            try parser.reportError(2012, "Expected ']' after slice expression");
            return null;
        }
        parser.consumeToken("Consume slice ']'");
        const extra_start: u32 = @intCast(parser.extra_data.items.len);
        try parser.extra_data.appendSlice(parser.allocator, &.{ lhs, first, end });
        try parser.nodes.append(parser.allocator, .{
            .tag = .slice,
            .main_token = bracket_token,
            .data = .{ .lhs = extra_start, .rhs = extra_start + 3 },
        });
        return @intCast(parser.nodes.len - 1);
    }

    if (first == std.math.maxInt(u32)) {
        try parser.reportError(2007, "Expected index or slice range after '['");
        return null;
    }
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_bracket) {
        try parser.reportError(2012, "Expected ']' after index expression");
        return null;
    }
    parser.consumeToken("Consume index ']'");
    try parser.nodes.append(parser.allocator, .{
        .tag = .array_access,
        .main_token = bracket_token,
        .data = .{ .lhs = lhs, .rhs = first },
    });
    return @intCast(parser.nodes.len - 1);
}

fn parseUnarySuffix(parser: anytype, lhs: Node.Index) !?Node.Index {
    const operator_token = parser.index;
    parser.consumeToken("Consume postfix unary operator");
    try parser.nodes.append(parser.allocator, .{
        .tag = .unary_op,
        .main_token = operator_token,
        .data = .{ .lhs = lhs, .rhs = 0 },
    });
    return @intCast(parser.nodes.len - 1);
}
