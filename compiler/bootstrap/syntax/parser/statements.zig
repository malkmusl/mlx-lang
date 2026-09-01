const std = @import("std");
const Node = @import("../ast.zig").Node;
const declarations = @import("declarations.zig");
const control_flow = @import("control_flow.zig");
const literals = @import("expressions/literals.zig");

pub fn parseBlock(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    const start_token = parser.index;
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .l_brace) {
        try parser.reportError(2005, "Expected '{' for block");
        return null;
    }
    parser.consumeToken("Consume block '{'");
    var statements = std.ArrayList(Node.Index).empty;
    defer statements.deinit(parser.allocator);

    // EOF is a synchronization point too. Without this guard a malformed or
    // incomplete block repeatedly asks recovery to advance past EOF, which is
    // impossible and used to pin zin0 in an infinite loop.
    while (parser.index < parser.tokens.len and
        parser.tokens[parser.index].tag != .r_brace and
        parser.tokens[parser.index].tag != .eof)
    {
        if (parser.tokens[parser.index].tag == .statement_end) {
            parser.index += 1;
            continue;
        }
        if (try parseStatement(parser)) |statement| {
            try statements.append(parser.allocator, statement);
        } else {
            parser.recoverPublic();
        }
    }
    if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .r_brace) {
        parser.consumeToken("Consume block '}'");
    } else {
        try parser.reportError(2005, "Expected '}' to close block");
    }

    const extra_start: u32 = @intCast(parser.extra_data.items.len);
    try parser.extra_data.appendSlice(parser.allocator, statements.items);
    try parser.nodes.append(parser.allocator, .{
        .tag = .block,
        .main_token = start_token,
        .data = .{ .lhs = extra_start, .rhs = @intCast(parser.extra_data.items.len) },
    });
    return @intCast(parser.nodes.len - 1);
}

pub fn parseStatement(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    if (parser.index >= parser.tokens.len) return null;
    return switch (parser.tokens[parser.index].tag) {
        .keyword_const, .keyword_var => declarations.parseVariable(parser),
        .keyword_fn => declarations.parseFunction(parser),
        .keyword_unsafe => parseUnsafe(parser),
        .keyword_return => parseReturn(parser),
        .keyword_break => control_flow.parseBreak(parser),
        .keyword_continue => control_flow.parseContinue(parser),
        .keyword_defer, .keyword_errdefer => control_flow.parseCleanup(parser),
        else => parser.parseExpr(0),
    };
}

pub fn parseReturn(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    const return_token = parser.index;
    parser.consumeToken("Consume return");
    const explicit_multi_return = parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .arrow;
    if (explicit_multi_return) parser.consumeToken("Consume multi-return arrow");
    var expression: Node.Index = std.math.maxInt(u32);
    if (parser.index < parser.tokens.len) {
        const next = parser.tokens[parser.index].tag;
        if (next != .statement_end and next != .r_brace and next != .eof) expression = try parser.parseExpr(0) orelse return null;
    }
    if (explicit_multi_return and expression != std.math.maxInt(u32) and parser.nodes.items(.tag)[expression] != .tuple_literal) {
        try parser.reportError(2008, "Expected '(value, value, ...)' after 'return ->'");
        return null;
    }
    if (!explicit_multi_return and expression != std.math.maxInt(u32) and parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .comma) {
        var elements = std.ArrayList(Node.Index).empty;
        defer elements.deinit(parser.allocator);
        try elements.append(parser.allocator, expression);
        while (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .comma) {
            parser.consumeToken("Consume legacy return value comma");
            try elements.append(parser.allocator, try parser.parseExpr(0) orelse return null);
        }
        expression = try literals.appendTuple(parser, return_token, elements.items);
    }
    if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .statement_end) parser.index += 1;
    try parser.nodes.append(parser.allocator, .{
        .tag = .return_stmt,
        .main_token = return_token,
        .data = .{ .lhs = 0, .rhs = expression },
    });
    return @intCast(parser.nodes.len - 1);
}

fn parseUnsafe(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    const unsafe_token = parser.index;
    parser.consumeToken("Consume unsafe");
    const body = try parseBlock(parser) orelse return null;
    try parser.nodes.append(parser.allocator, .{
        .tag = .unsafe_block,
        .main_token = unsafe_token,
        .data = .{ .lhs = body, .rhs = 0 },
    });
    return @intCast(parser.nodes.len - 1);
}
