const std = @import("std");
const Node = @import("../../ast.zig").Node;
const aggregate = @import("aggregate.zig");

const PointerQualifiers = struct {
    flags: u32 = 0,
    alignment_node: ?Node.Index = null,
};

/// Parses the complete annotation-position type grammar.
pub fn parse(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    if (parser.index >= parser.tokens.len) return null;
    const token = parser.tokens[parser.index];
    switch (token.tag) {
        .asterisk => {
            const star_token = parser.index;
            parser.consumeToken("Consume *");
            const qualifiers = try parsePointerQualifiers(parser);
            const child = try parse(parser) orelse return null;
            const encoded = try encodePointerQualifiers(parser, qualifiers, 0);
            try parser.nodes.append(parser.allocator, .{
                .tag = .pointer_type,
                .main_token = star_token,
                .data = .{ .lhs = child, .rhs = encoded },
            });
            return @intCast(parser.nodes.len - 1);
        },
        .l_bracket => return parseBracketType(parser),
        .question => {
            const question_token = parser.index;
            parser.consumeToken("Consume ?");
            const child = try parse(parser) orelse return null;
            try parser.nodes.append(parser.allocator, .{
                .tag = .optional_type,
                .main_token = question_token,
                .data = .{ .lhs = child, .rhs = 0 },
            });
            return @intCast(parser.nodes.len - 1);
        },
        .bang => {
            const bang_token = parser.index;
            parser.consumeToken("Consume !");
            const payload = try parse(parser) orelse return null;
            try parser.nodes.append(parser.allocator, .{
                .tag = .error_union_type,
                .main_token = bang_token,
                .data = .{ .lhs = 0, .rhs = payload },
            });
            return @intCast(parser.nodes.len - 1);
        },
        .l_paren => return parseTupleType(parser),
        .ident, .keyword_anytype, .keyword_type => return parseNamedType(parser),
        .keyword_fn => return parseFunctionType(parser),
        .keyword_struct, .keyword_enum, .keyword_union => return aggregate.parseType(parser),
        .keyword_error => return aggregate.parseErrorSet(parser),
        else => return null,
    }
}

fn parseTupleType(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    const paren_token = parser.index;
    parser.consumeToken("Consume tuple type '('");
    const first = try parse(parser) orelse {
        try parser.reportError(2005, "Expected tuple element type");
        return null;
    };
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .comma) {
        if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_paren) {
            try parser.reportError(2008, "Expected ')' after grouped type");
            return null;
        }
        parser.consumeToken("Consume grouped type ')'");
        return first;
    }

    var elements = std.ArrayList(Node.Index).empty;
    defer elements.deinit(parser.allocator);
    try elements.append(parser.allocator, first);
    while (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .comma) {
        parser.consumeToken("Consume tuple type comma");
        if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .r_paren) break;
        try elements.append(parser.allocator, try parse(parser) orelse {
            try parser.reportError(2005, "Expected tuple element type after ','");
            return null;
        });
    }
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_paren) {
        try parser.reportError(2008, "Expected ')' after tuple type");
        return null;
    }
    parser.consumeToken("Consume tuple type ')'");
    const extra_start: u32 = @intCast(parser.extra_data.items.len);
    try parser.extra_data.append(parser.allocator, @intCast(elements.items.len));
    try parser.extra_data.appendSlice(parser.allocator, elements.items);
    try parser.nodes.append(parser.allocator, .{
        .tag = .tuple_type,
        .main_token = paren_token,
        .data = .{ .lhs = extra_start, .rhs = @intCast(elements.items.len) },
    });
    return @intCast(parser.nodes.len - 1);
}

fn parseBracketType(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    const bracket_token = parser.index;
    parser.consumeToken("Consume [");
    if (parser.index >= parser.tokens.len) return null;

    if (parser.tokens[parser.index].tag == .r_bracket) {
        parser.consumeToken("Consume ]");
        var is_const = false;
        if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .keyword_const) {
            is_const = true;
            parser.consumeToken("Consume const");
        }
        const child = try parse(parser) orelse return null;
        try parser.nodes.append(parser.allocator, .{
            .tag = .slice_type,
            .main_token = bracket_token,
            .data = .{ .lhs = child, .rhs = @intFromBool(is_const) },
        });
        return @intCast(parser.nodes.len - 1);
    }

    if (parser.tokens[parser.index].tag == .asterisk) {
        parser.consumeToken("Consume *");
        if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_bracket) {
            try parser.reportError(2010, "Expected ']' after [*");
            return null;
        }
        parser.consumeToken("Consume ]");
        const qualifiers = try parsePointerQualifiers(parser);
        const child = try parse(parser) orelse return null;
        const encoded = try encodePointerQualifiers(parser, qualifiers, 2);
        try parser.nodes.append(parser.allocator, .{
            .tag = .pointer_type,
            .main_token = bracket_token,
            .data = .{ .lhs = child, .rhs = encoded },
        });
        return @intCast(parser.nodes.len - 1);
    }

    const size_expression = try parser.parseExpr(0) orelse {
        try parser.reportError(2011, "Expected array size expression");
        return null;
    };
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_bracket) {
        try parser.reportError(2012, "Expected ']' after array size");
        return null;
    }
    parser.consumeToken("Consume ]");
    const element_type = try parse(parser) orelse return null;
    const extra_start: u32 = @intCast(parser.extra_data.items.len);
    try parser.extra_data.appendSlice(parser.allocator, &.{ size_expression, element_type });
    try parser.nodes.append(parser.allocator, .{
        .tag = .array_type,
        .main_token = bracket_token,
        .data = .{ .lhs = extra_start, .rhs = 0 },
    });
    return @intCast(parser.nodes.len - 1);
}

fn parseNamedType(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    const identifier_token = parser.index;
    parser.consumeToken("Consume type identifier");
    try parser.nodes.append(parser.allocator, .{
        .tag = .identifier,
        .main_token = identifier_token,
        .data = .{ .lhs = 0, .rhs = 0 },
    });
    var identifier_node: Node.Index = @intCast(parser.nodes.len - 1);

    while (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .dot) {
        parser.consumeToken("Consume qualified type '.'");
        if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .ident) {
            try parser.reportError(2001, "Expected identifier after '.' in qualified type name");
            return null;
        }
        const field_token = parser.index;
        parser.consumeToken("Consume qualified type field");
        try parser.nodes.append(parser.allocator, .{
            .tag = .field_access,
            .main_token = field_token,
            .data = .{ .lhs = identifier_node, .rhs = field_token },
        });
        identifier_node = @intCast(parser.nodes.len - 1);
    }

    if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .bang) {
        const bang_token = parser.index;
        parser.consumeToken("Consume ! in E!T");
        const payload = try parse(parser) orelse return null;
        try parser.nodes.append(parser.allocator, .{
            .tag = .error_union_type,
            .main_token = bang_token,
            .data = .{ .lhs = identifier_node, .rhs = payload },
        });
        return @intCast(parser.nodes.len - 1);
    }
    return identifier_node;
}

fn parseFunctionType(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    const function_token = parser.index;
    parser.consumeToken("Consume fn keyword in type");
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .l_paren) {
        try parser.reportError(2004, "Expected '(' in function type");
        return null;
    }
    parser.consumeToken("Consume function type '('");
    var parameters = std.ArrayList(Node.Index).empty;
    defer parameters.deinit(parser.allocator);
    while (parser.index < parser.tokens.len and parser.tokens[parser.index].tag != .r_paren) {
        // Named parameters are accepted for readability but names are not part
        // of function type identity: fn(context: usize, len: usize) -> T.
        if (parser.index + 1 < parser.tokens.len and parser.tokens[parser.index].tag == .ident and parser.tokens[parser.index + 1].tag == .colon) {
            parser.consumeToken("Consume optional function type parameter name");
            parser.consumeToken("Consume function type parameter ':'");
        }
        const parameter = try parse(parser) orelse {
            try parser.reportError(2005, "Expected parameter type in function type");
            return null;
        };
        try parameters.append(parser.allocator, parameter);
        if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .comma) {
            parser.consumeToken("Consume function type parameter comma");
        } else break;
    }
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_paren) {
        try parser.reportError(2007, "Expected ')' after function type parameters");
        return null;
    }
    parser.consumeToken("Consume function type ')'");
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .arrow) {
        try parser.reportError(2002, "Expected '->' before function type return type");
        return null;
    }
    parser.consumeToken("Consume function type return arrow");
    const return_type = try parse(parser) orelse return null;
    const extra_start: u32 = @intCast(parser.extra_data.items.len);
    try parser.extra_data.append(parser.allocator, return_type);
    try parser.extra_data.append(parser.allocator, @intCast(parameters.items.len));
    try parser.extra_data.appendSlice(parser.allocator, parameters.items);
    try parser.nodes.append(parser.allocator, .{
        .tag = .fn_type,
        .main_token = function_token,
        .data = .{ .lhs = extra_start, .rhs = @intCast(parameters.items.len) },
    });
    return @intCast(parser.nodes.len - 1);
}

fn parsePointerQualifiers(parser: anytype) std.mem.Allocator.Error!PointerQualifiers {
    var result: PointerQualifiers = .{};
    while (parser.index < parser.tokens.len) {
        switch (parser.tokens[parser.index].tag) {
            .keyword_const => {
                result.flags |= 1;
                parser.consumeToken("Consume const pointer qualifier");
            },
            .keyword_volatile => {
                result.flags |= 8;
                parser.consumeToken("Consume volatile pointer qualifier");
            },
            .keyword_align => {
                parser.consumeToken("Consume align pointer qualifier");
                if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .l_paren) {
                    try parser.reportError(2001, "Expected '(' after align");
                    break;
                }
                parser.consumeToken("Consume align '('");
                result.alignment_node = try parser.parseExpr(0);
                if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_paren) {
                    try parser.reportError(2003, "Expected ')' after pointer alignment");
                    break;
                }
                parser.consumeToken("Consume align ')'");
            },
            else => break,
        }
    }
    return result;
}

fn encodePointerQualifiers(parser: anytype, qualifiers: PointerQualifiers, extra_flags: u32) std.mem.Allocator.Error!u32 {
    const flags = qualifiers.flags | extra_flags;
    const alignment_node = qualifiers.alignment_node orelse return flags;
    const qualifier_start: u32 = @intCast(parser.extra_data.items.len);
    try parser.extra_data.appendSlice(parser.allocator, &.{ flags, alignment_node });
    return 0x8000_0000 | qualifier_start;
}
