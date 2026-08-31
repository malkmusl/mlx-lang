const Node = @import("../ast.zig").Node;

pub fn parseCleanup(parser: anytype) !?Node.Index {
    const keyword_token = parser.index;
    const is_error_only = parser.tokens[keyword_token].tag == .keyword_errdefer;
    parser.consumeToken(if (is_error_only) "Consume errdefer" else "Consume defer");
    const action = if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .l_brace)
        try parser.parseBlockPublic()
    else
        try parser.parseExpr(0);
    if (action == null) {
        try parser.reportError(2002, "Expected expression or block after cleanup keyword");
        return null;
    }
    try parser.nodes.append(parser.allocator, .{
        .tag = if (is_error_only) .errdefer_stmt else .defer_stmt,
        .main_token = keyword_token,
        .data = .{ .lhs = action.?, .rhs = 0 },
    });
    return @intCast(parser.nodes.len - 1);
}

/// Match layout: [subject, arm_count, (kind, first, second, body)...].
/// kind 0 = else, 1 = value, 2 = half-open range.
pub fn parseMatch(parser: anytype) !?Node.Index {
    const match_token = parser.index;
    parser.consumeToken("Consume match");
    const subject = try parser.parseExpr(0) orelse return null;
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .l_brace) {
        try parser.reportError(2003, "Expected '{' after match subject");
        return null;
    }
    parser.consumeToken("Consume match '{'");

    var arms = @import("std").ArrayList(u32).empty;
    defer arms.deinit(parser.allocator);
    var arm_count: u32 = 0;
    while (parser.index < parser.tokens.len and parser.tokens[parser.index].tag != .r_brace) {
        while (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .statement_end) parser.index += 1;
        if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag == .r_brace) break;

        var kind: u32 = 1;
        var first: u32 = @import("std").math.maxInt(u32);
        var second: u32 = @import("std").math.maxInt(u32);
        if (parser.tokens[parser.index].tag == .keyword_else) {
            kind = 0;
            parser.consumeToken("Consume match else pattern");
        } else if (parser.tokens[parser.index].tag == .dot) {
            kind = 3;
            const dot_token = parser.index;
            parser.consumeToken("Consume enum pattern '.'");
            if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .ident) {
                try parser.reportError(2001, "Expected enum member after '.'");
                return null;
            }
            const member_token = parser.index;
            parser.consumeToken("Consume enum pattern member");
            try parser.nodes.append(parser.allocator, .{
                .tag = .enum_literal,
                .main_token = member_token,
                .data = .{ .lhs = dot_token, .rhs = 0 },
            });
            first = @intCast(parser.nodes.len - 1);
        } else {
            first = try parser.parseExpr(0) orelse return null;
            if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .dot_dot) {
                kind = 2;
                parser.consumeToken("Consume match range '..'");
                second = try parser.parseExpr(0) orelse return null;
            }
        }
        if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .arrow) {
            try parser.reportError(2001, "Expected '=>' after match pattern");
            return null;
        }
        parser.consumeToken("Consume match '=>'");
        const body = if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .l_brace)
            try parser.parseBlockPublic()
        else
            try parser.parseExpr(0);
        if (body == null) return null;
        try arms.appendSlice(parser.allocator, &.{ kind, first, second, body.? });
        arm_count += 1;
        if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .comma) parser.index += 1;
    }
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_brace) {
        try parser.reportError(2003, "Expected '}' after match arms");
        return null;
    }
    parser.consumeToken("Consume match '}'");

    const extra_start: u32 = @intCast(parser.extra_data.items.len);
    try parser.extra_data.append(parser.allocator, subject);
    try parser.extra_data.append(parser.allocator, arm_count);
    try parser.extra_data.appendSlice(parser.allocator, arms.items);
    try parser.nodes.append(parser.allocator, .{
        .tag = .match_stmt,
        .main_token = match_token,
        .data = .{ .lhs = extra_start, .rhs = @intCast(parser.extra_data.items.len) },
    });
    return @intCast(parser.nodes.len - 1);
}
