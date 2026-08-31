const std = @import("std");
const Node = @import("../../ast.zig").Node;
const prefix = @import("../prefix.zig");
const control_flow = @import("../control_flow.zig");
const statements = @import("../statements.zig");
const aggregate = @import("../types/aggregate.zig");
const literals = @import("literals.zig");

pub fn parse(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    if (try prefix.parse(parser)) |expression| return expression;
    if (parser.index >= parser.tokens.len) return null;
    const token = parser.tokens[parser.index];
    return switch (token.tag) {
        .string => appendLiteral(parser, .string_literal, "Consume string literal"),
        .integer => appendLiteral(parser, .integer_literal, "Consume integer literal"),
        .float => appendLiteral(parser, .float_literal, "Consume float literal"),
        .keyword_true, .keyword_false => appendLiteral(parser, .bool_literal, "Consume boolean literal"),
        .keyword_null => appendLiteral(parser, .null_literal, "Consume null literal"),
        .keyword_undefined => appendLiteral(parser, .undefined_literal, "Consume undefined literal"),
        .ident => parseIdentifier(parser),
        .dot => literals.parseTuple(parser),
        .l_bracket => literals.parseArray(parser),
        .at => parseBuiltin(parser),
        .keyword_if => control_flow.parseIf(parser),
        .keyword_while => control_flow.parseWhile(parser, std.math.maxInt(u32)),
        .keyword_for => control_flow.parseFor(parser, std.math.maxInt(u32)),
        .keyword_match => control_flow.parseMatch(parser),
        .keyword_return => statements.parseReturn(parser),
        .l_brace => statements.parseBlock(parser),
        .keyword_struct, .keyword_enum, .keyword_union => aggregate.parseType(parser),
        .keyword_error => aggregate.parseErrorSet(parser),
        .keyword_fn => parser.parseTypeExprPublic(),
        .l_paren => parseParenthesized(parser),
        else => {
            try parser.reportError(2007, "Unexpected token in expression");
            return null;
        },
    };
}

fn appendLiteral(parser: anytype, tag: Node.Tag, message: []const u8) std.mem.Allocator.Error!?Node.Index {
    const token = parser.index;
    try parser.nodes.append(parser.allocator, .{
        .tag = tag,
        .main_token = token,
        .data = .{ .lhs = 0, .rhs = 0 },
    });
    parser.consumeToken(message);
    return @intCast(parser.nodes.len - 1);
}

fn parseIdentifier(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    if (parser.index + 2 < parser.tokens.len and
        parser.tokens[parser.index + 1].tag == .colon and
        (parser.tokens[parser.index + 2].tag == .keyword_while or parser.tokens[parser.index + 2].tag == .keyword_for))
    {
        const label_token = parser.index;
        parser.consumeToken("Consume loop label");
        parser.consumeToken("Consume loop label ':'");
        return switch (parser.tokens[parser.index].tag) {
            .keyword_while => control_flow.parseWhile(parser, label_token),
            .keyword_for => control_flow.parseFor(parser, label_token),
            else => unreachable,
        };
    }
    return appendLiteral(parser, .identifier, "Consume identifier");
}

fn parseBuiltin(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    const start_token = parser.index;
    parser.consumeToken("Consume @");
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .ident) {
        try parser.reportError(2001, "Expected builtin identifier after '@'");
        return null;
    }
    const builtin_token = parser.index;
    parser.consumeToken("Consume builtin name");
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .l_paren) {
        try parser.reportError(2001, "Expected '(' after builtin name");
        return null;
    }
    parser.consumeToken("Consume '('");

    var arguments = std.ArrayList(Node.Index).empty;
    defer arguments.deinit(parser.allocator);
    var closing_consumed = false;
    if (std.mem.eql(u8, tokenText(parser, builtin_token), "nocopy") and parser.index < parser.tokens.len and aggregate.isStart(parser.tokens[parser.index].tag)) {
        try arguments.append(parser.allocator, try aggregate.parseNoCopyArgument(parser) orelse return null);
        closing_consumed = true;
    } else {
        while (parser.index < parser.tokens.len and parser.tokens[parser.index].tag != .r_paren) {
            try arguments.append(parser.allocator, try parseBuiltinArgument(parser) orelse return null);
            if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .comma) {
                parser.consumeToken("Consume builtin argument comma");
            } else break;
        }
    }
    if (!closing_consumed) {
        if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_paren) {
            try parser.reportError(2003, "Expected ')' after builtin arguments");
            return null;
        }
        parser.consumeToken("Consume ')'");
    }

    const extra_start: u32 = @intCast(parser.extra_data.items.len);
    try parser.extra_data.append(parser.allocator, @intCast(arguments.items.len));
    try parser.extra_data.appendSlice(parser.allocator, arguments.items);
    try parser.nodes.append(parser.allocator, .{
        .tag = .builtin_call,
        .main_token = start_token,
        .data = .{ .lhs = builtin_token, .rhs = extra_start },
    });
    return @intCast(parser.nodes.len - 1);
}

fn parseBuiltinArgument(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    return switch (parser.tokens[parser.index].tag) {
        .asterisk, .l_bracket, .question, .bang, .keyword_fn, .keyword_struct, .keyword_enum, .keyword_union, .keyword_error => parser.parseTypeExprPublic(),
        else => parser.parseExpr(0),
    };
}

fn parseParenthesized(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    const paren_token = parser.index;
    parser.consumeToken("Consume '('");
    const expression = try parser.parseExpr(0) orelse return null;
    if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .comma) {
        var elements = std.ArrayList(Node.Index).empty;
        defer elements.deinit(parser.allocator);
        try elements.append(parser.allocator, expression);
        while (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .comma) {
            parser.consumeToken("Consume tuple expression comma");
            if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .r_paren) break;
            try elements.append(parser.allocator, try parser.parseExpr(0) orelse return null);
        }
        if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_paren) {
            try parser.reportError(2008, "Expected ')' after tuple expression");
            return null;
        }
        parser.consumeToken("Consume tuple expression ')'");
        return try literals.appendTuple(parser, paren_token, elements.items);
    }
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_paren) {
        try parser.reportError(2008, "Expected ')'");
        return null;
    }
    parser.consumeToken("Consume ')'");
    return expression;
}

fn tokenText(parser: anytype, token_index: u32) []const u8 {
    const file = parser.diags.source_manager.getFile(parser.source_id).?;
    const token = parser.tokens[token_index];
    return file.content[token.start..token.end];
}
