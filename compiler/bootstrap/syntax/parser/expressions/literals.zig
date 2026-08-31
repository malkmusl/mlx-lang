const std = @import("std");
const Node = @import("../../ast.zig").Node;

pub fn parseTuple(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    const dot_token = parser.index;
    parser.consumeToken("Consume tuple literal '.'");
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .l_brace) {
        try parser.reportError(2001, "Expected '{' after '.' for tuple literal");
        return null;
    }
    parser.consumeToken("Consume tuple literal '{'");
    var elements = std.ArrayList(Node.Index).empty;
    defer elements.deinit(parser.allocator);
    while (parser.index < parser.tokens.len and parser.tokens[parser.index].tag != .r_brace) {
        try elements.append(parser.allocator, try parser.parseExpr(0) orelse return null);
        if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .comma) {
            parser.consumeToken("Consume tuple element comma");
        } else break;
    }
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_brace) {
        try parser.reportError(2003, "Expected '}' after tuple literal");
        return null;
    }
    parser.consumeToken("Consume tuple literal '}'");
    return try appendTuple(parser, dot_token, elements.items);
}

pub fn appendTuple(parser: anytype, main_token: u32, elements: []const Node.Index) std.mem.Allocator.Error!Node.Index {
    const extra_start: u32 = @intCast(parser.extra_data.items.len);
    try parser.extra_data.append(parser.allocator, @intCast(elements.len));
    try parser.extra_data.appendSlice(parser.allocator, elements);
    try parser.nodes.append(parser.allocator, .{
        .tag = .tuple_literal,
        .main_token = main_token,
        .data = .{ .lhs = extra_start, .rhs = @intCast(elements.len) },
    });
    return @intCast(parser.nodes.len - 1);
}

pub fn parseArray(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    const bracket_token = parser.index;
    parser.consumeToken("Consume array literal '['");
    var length_node: Node.Index = std.math.maxInt(u32);
    if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .ident and std.mem.eql(u8, tokenText(parser, parser.index), "_")) {
        parser.consumeToken("Consume inferred array length '_'");
    } else {
        length_node = try parser.parseExpr(0) orelse {
            try parser.reportError(2011, "Expected array literal length");
            return null;
        };
    }
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_bracket) {
        try parser.reportError(2012, "Expected ']' after array literal length");
        return null;
    }
    parser.consumeToken("Consume array literal ']'");
    const element_type = try parser.parseTypeExprPublic() orelse {
        try parser.reportError(2005, "Expected array element type");
        return null;
    };
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .l_brace) {
        try parser.reportError(2003, "Expected '{' after array literal type");
        return null;
    }
    parser.consumeToken("Consume array literal '{'");
    var elements = std.ArrayList(Node.Index).empty;
    defer elements.deinit(parser.allocator);
    while (parser.index < parser.tokens.len and parser.tokens[parser.index].tag != .r_brace) {
        try elements.append(parser.allocator, try parser.parseExpr(0) orelse return null);
        if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .comma) {
            parser.consumeToken("Consume array element comma");
        } else break;
    }
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_brace) {
        try parser.reportError(2003, "Expected '}' after array literal");
        return null;
    }
    parser.consumeToken("Consume array literal '}'");
    const extra_start: u32 = @intCast(parser.extra_data.items.len);
    try parser.extra_data.appendSlice(parser.allocator, &.{ length_node, element_type, @intCast(elements.items.len) });
    try parser.extra_data.appendSlice(parser.allocator, elements.items);
    try parser.nodes.append(parser.allocator, .{
        .tag = .array_literal,
        .main_token = bracket_token,
        .data = .{ .lhs = extra_start, .rhs = @intCast(parser.extra_data.items.len) },
    });
    return @intCast(parser.nodes.len - 1);
}

fn tokenText(parser: anytype, token_index: u32) []const u8 {
    const file = parser.diags.source_manager.getFile(parser.source_id).?;
    const token = parser.tokens[token_index];
    return file.content[token.start..token.end];
}
