const Node = @import("../ast.zig").Node;
const std = @import("std");

pub fn parseIf(parser: anytype) !?Node.Index {
    const start_token = parser.index;
    parser.consumeToken("Consume if");
    const condition = try parser.parseExpr(0) orelse return null;
    const then_branch = try parser.parseExpr(0) orelse return null;

    var else_branch: ?Node.Index = null;
    if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .keyword_else) {
        parser.consumeToken("Consume else");
        else_branch = try parser.parseExpr(0) orelse return null;
    }

    const extra_start: u32 = @intCast(parser.extra_data.items.len);
    try parser.extra_data.append(parser.allocator, condition);
    try parser.extra_data.append(parser.allocator, then_branch);
    if (else_branch) |branch| try parser.extra_data.append(parser.allocator, branch);
    try parser.nodes.append(parser.allocator, .{
        .tag = .if_stmt,
        .main_token = start_token,
        .data = .{ .lhs = extra_start, .rhs = @intCast(parser.extra_data.items.len) },
    });
    return @intCast(parser.nodes.len - 1);
}

pub fn parseWhile(parser: anytype, label_token: u32) !?Node.Index {
    const start_token = parser.index;
    parser.consumeToken("Consume while");
    const condition = try parser.parseExpr(0) orelse return null;
    const body = try parser.parseExpr(0) orelse return null;
    const extra_start: u32 = @intCast(parser.extra_data.items.len);
    try parser.extra_data.appendSlice(parser.allocator, &.{ label_token, condition, body });
    try parser.nodes.append(parser.allocator, .{
        .tag = .while_stmt,
        .main_token = start_token,
        .data = .{ .lhs = extra_start, .rhs = extra_start + 3 },
    });
    return @intCast(parser.nodes.len - 1);
}

pub fn parseFor(parser: anytype, label_token: u32) !?Node.Index {
    const start_token = parser.index;
    parser.consumeToken("Consume for");

    var capture_flags: u32 = 0;
    if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .asterisk) {
        capture_flags |= 1;
        parser.consumeToken("Consume for-loop pointer capture '*'");
    }
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .ident) {
        try parser.reportError(2002, "Expected for-loop item capture");
        return null;
    }
    const item_token = parser.index;
    parser.consumeToken("Consume for-loop item capture");

    var index_token: u32 = std.math.maxInt(u32);
    if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .comma) {
        parser.consumeToken("Consume for-loop capture comma");
        if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .ident) {
            try parser.reportError(2002, "Expected for-loop index capture");
            return null;
        }
        index_token = parser.index;
        parser.consumeToken("Consume for-loop index capture");
    }

    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .keyword_in) {
        try parser.reportError(2001, "Expected 'in' after for-loop capture");
        return null;
    }
    parser.consumeToken("Consume in");

    const range_start = try parser.parseExpr(0) orelse return null;
    var iterable = range_start;
    if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .dot_dot) {
        const range_token = parser.index;
        parser.consumeToken("Consume range '..'");
        const range_end = try parser.parseExpr(0) orelse return null;
        try parser.nodes.append(parser.allocator, .{
            .tag = .range,
            .main_token = range_token,
            .data = .{ .lhs = range_start, .rhs = range_end },
        });
        iterable = @intCast(parser.nodes.len - 1);
    }

    const body = try parser.parseBlockPublic() orelse return null;
    const extra_start: u32 = @intCast(parser.extra_data.items.len);
    try parser.extra_data.appendSlice(parser.allocator, &.{ label_token, capture_flags, item_token, index_token, iterable, body });
    try parser.nodes.append(parser.allocator, .{
        .tag = .for_stmt,
        .main_token = start_token,
        .data = .{ .lhs = extra_start, .rhs = extra_start + 6 },
    });
    return @intCast(parser.nodes.len - 1);
}

pub fn parseBreak(parser: anytype) !?Node.Index {
    const break_token = parser.index;
    parser.consumeToken("Consume break");
    var label_token: u32 = std.math.maxInt(u32);
    if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .colon) {
        parser.consumeToken("Consume break label ':'");
        if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .ident) {
            try parser.reportError(2002, "Expected break label");
            return null;
        }
        label_token = parser.index;
        parser.consumeToken("Consume break label");
    }

    var value_node: Node.Index = std.math.maxInt(u32);
    if (parser.index < parser.tokens.len) {
        const next = parser.tokens[parser.index].tag;
        if (next != .statement_end and next != .r_brace and next != .eof) value_node = try parser.parseExpr(0) orelse return null;
    }
    if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .statement_end) parser.index += 1;
    try parser.nodes.append(parser.allocator, .{
        .tag = .break_stmt,
        .main_token = break_token,
        .data = .{ .lhs = label_token, .rhs = value_node },
    });
    return @intCast(parser.nodes.len - 1);
}

pub fn parseContinue(parser: anytype) !?Node.Index {
    const continue_token = parser.index;
    parser.consumeToken("Consume continue");
    var label_token: u32 = std.math.maxInt(u32);
    if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .colon) {
        parser.consumeToken("Consume continue label ':'");
        if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .ident) {
            try parser.reportError(2002, "Expected continue label");
            return null;
        }
        label_token = parser.index;
        parser.consumeToken("Consume continue label");
    }
    if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .statement_end) parser.index += 1;
    try parser.nodes.append(parser.allocator, .{
        .tag = .continue_stmt,
        .main_token = continue_token,
        .data = .{ .lhs = label_token, .rhs = std.math.maxInt(u32) },
    });
    return @intCast(parser.nodes.len - 1);
}

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

    var arms = std.ArrayList(u32).empty;
    defer arms.deinit(parser.allocator);
    var arm_count: u32 = 0;
    while (parser.index < parser.tokens.len and parser.tokens[parser.index].tag != .r_brace) {
        while (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .statement_end) parser.index += 1;
        if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag == .r_brace) break;

        var kind: u32 = 1;
        var first: u32 = std.math.maxInt(u32);
        var second: u32 = std.math.maxInt(u32);
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
