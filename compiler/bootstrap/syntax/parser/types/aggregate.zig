const std = @import("std");
const Token = @import("../../token.zig").Token;
const Node = @import("../../ast.zig").Node;

const Kind = enum { @"struct", @"enum", @"union" };
const Header = struct {
    backing: Node.Index,
    nonexhaustive: bool = false,
};

pub fn isStart(tag: Token.Tag) bool {
    return tag == .keyword_struct or tag == .keyword_enum or tag == .keyword_union;
}

pub fn parseType(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    const head_token = parser.index;
    const kind = aggregateKind(parser.tokens[parser.index].tag);
    parser.consumeToken("Consume aggregate type head");
    const header = try parseHeader(parser, kind) orelse return null;
    return parseBody(parser, kind, head_token, header);
}

pub fn parseNoCopyArgument(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    const head_token = parser.index;
    const kind = aggregateKind(parser.tokens[parser.index].tag);
    parser.consumeToken("Consume @nocopy aggregate type head");
    const header = try parseHeader(parser, kind) orelse return null;
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_paren) {
        try parser.reportError(2003, "Expected ')' after @nocopy aggregate head");
        return null;
    }
    parser.consumeToken("Consume ')' after @nocopy aggregate head");
    return parseBody(parser, kind, head_token, header);
}

pub fn parseErrorSet(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    const error_token = parser.index;
    parser.consumeToken("Consume error-set head");
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .l_brace) {
        try parser.reportError(2001, "Expected '{' after error");
        return null;
    }
    parser.consumeToken("Consume error-set '{'");
    var members = std.ArrayList(Node.Index).empty;
    defer members.deinit(parser.allocator);
    while (parser.index < parser.tokens.len and parser.tokens[parser.index].tag != .r_brace) {
        while (parser.index < parser.tokens.len and
            (parser.tokens[parser.index].tag == .statement_end or parser.tokens[parser.index].tag == .comma)) parser.index += 1;
        if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag == .r_brace) break;
        if (parser.tokens[parser.index].tag != .ident) {
            try parser.reportError(2001, "Expected error-set member name");
            return null;
        }
        const member_token = parser.index;
        parser.consumeToken("Consume error-set member");
        try parser.nodes.append(parser.allocator, .{
            .tag = .error_member,
            .main_token = member_token,
            .data = .{ .lhs = 0, .rhs = 0 },
        });
        try members.append(parser.allocator, @intCast(parser.nodes.len - 1));
    }
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_brace) {
        try parser.reportError(2003, "Expected '}' after error-set members");
        return null;
    }
    parser.consumeToken("Consume error-set '}'");
    const extra_start: u32 = @intCast(parser.extra_data.items.len);
    try parser.extra_data.appendSlice(parser.allocator, members.items);
    try parser.nodes.append(parser.allocator, .{
        .tag = .error_set_decl,
        .main_token = error_token,
        .data = .{ .lhs = extra_start, .rhs = @intCast(parser.extra_data.items.len) },
    });
    return @intCast(parser.nodes.len - 1);
}

fn aggregateKind(tag: Token.Tag) Kind {
    return switch (tag) {
        .keyword_struct => .@"struct",
        .keyword_enum => .@"enum",
        .keyword_union => .@"union",
        else => unreachable,
    };
}

fn parseHeader(parser: anytype, kind: Kind) std.mem.Allocator.Error!?Header {
    if (kind == .@"struct") return .{ .backing = std.math.maxInt(u32) };
    if (kind == .@"union" and parser.tokens[parser.index].tag != .l_paren) return .{ .backing = std.math.maxInt(u32) };
    if (parser.tokens[parser.index].tag != .l_paren) {
        try parser.reportError(2001, "Enum type requires a backing integer type");
        return null;
    }
    parser.consumeToken("Consume aggregate tag '('");

    var backing: Node.Index = undefined;
    if (kind == .@"union" and parser.tokens[parser.index].tag == .keyword_enum) {
        parser.consumeToken("Consume enum union tag");
        if (parser.tokens[parser.index].tag != .l_paren) {
            try parser.reportError(2001, "Expected '(' after enum union tag");
            return null;
        }
        parser.consumeToken("Consume enum union tag '('");
        backing = try parser.parseTypeExprPublic() orelse return null;
        if (parser.tokens[parser.index].tag != .r_paren) {
            try parser.reportError(2003, "Expected ')' after enum backing type");
            return null;
        }
        parser.consumeToken("Consume enum union tag ')'");
    } else {
        backing = try parser.parseTypeExprPublic() orelse return null;
    }

    var nonexhaustive = false;
    if (kind == .@"enum" and parser.tokens[parser.index].tag == .comma) {
        parser.consumeToken("Consume enum option comma");
        if (parser.tokens[parser.index].tag != .keyword_nonexhaustive) {
            try parser.reportError(2001, "Expected nonexhaustive enum option");
            return null;
        }
        parser.consumeToken("Consume nonexhaustive enum option");
        nonexhaustive = true;
    }
    if (parser.tokens[parser.index].tag != .r_paren) {
        try parser.reportError(2003, "Expected ')' after aggregate tag");
        return null;
    }
    parser.consumeToken("Consume aggregate tag ')'");
    return .{ .backing = backing, .nonexhaustive = nonexhaustive };
}

fn parseBody(parser: anytype, kind: Kind, head_token: u32, header: Header) std.mem.Allocator.Error!?Node.Index {
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .l_brace) {
        try parser.reportError(2001, "Expected '{' after aggregate type head");
        return null;
    }
    parser.consumeToken("Consume aggregate '{'");

    var members = std.ArrayList(Node.Index).empty;
    defer members.deinit(parser.allocator);
    while (parser.index < parser.tokens.len and parser.tokens[parser.index].tag != .r_brace) {
        while (parser.index < parser.tokens.len and
            (parser.tokens[parser.index].tag == .statement_end or parser.tokens[parser.index].tag == .comma)) parser.index += 1;
        if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag == .r_brace) break;

        var is_public = false;
        if (kind == .@"struct" and parser.tokens[parser.index].tag == .keyword_pub) {
            is_public = true;
            parser.consumeToken("Consume public field modifier");
        }
        if (parser.tokens[parser.index].tag != .ident) {
            try parser.reportError(2001, "Expected aggregate member name");
            return null;
        }
        const name_token = parser.index;
        parser.consumeToken("Consume aggregate member name");

        var type_node: Node.Index = std.math.maxInt(u32);
        if (kind != .@"enum" and parser.tokens[parser.index].tag == .colon) {
            parser.consumeToken("Consume member ':'");
            type_node = try parser.parseTypeExprPublic() orelse return null;
        } else if (kind == .@"struct") {
            try parser.reportError(2001, "Struct field requires a type");
            return null;
        }

        var value_node: Node.Index = std.math.maxInt(u32);
        if (parser.tokens[parser.index].tag == .equal) {
            parser.consumeToken("Consume member '='");
            value_node = try parser.parseExpr(0) orelse return null;
        }
        const member_tag: Node.Tag = switch (kind) {
            .@"struct" => .field_decl,
            .@"enum" => .enum_member,
            .@"union" => .union_member,
        };
        try parser.nodes.append(parser.allocator, .{
            .tag = member_tag,
            .main_token = name_token,
            .data = .{ .lhs = type_node, .rhs = value_node },
            .decl_flags = .{ .public = is_public },
        });
        try members.append(parser.allocator, @intCast(parser.nodes.len - 1));
    }

    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_brace) {
        try parser.reportError(2003, "Expected '}' after aggregate members");
        return null;
    }
    parser.consumeToken("Consume aggregate '}'");
    const extra_start: u32 = @intCast(parser.extra_data.items.len);
    try parser.extra_data.append(parser.allocator, header.backing);
    try parser.extra_data.appendSlice(parser.allocator, members.items);
    try parser.nodes.append(parser.allocator, .{
        .tag = switch (kind) {
            .@"struct" => .struct_decl,
            .@"enum" => .enum_decl,
            .@"union" => .union_decl,
        },
        .main_token = head_token,
        .data = .{ .lhs = extra_start, .rhs = @intCast(parser.extra_data.items.len) },
        .decl_flags = .{ .aggregate_nonexhaustive = header.nonexhaustive },
    });
    return @intCast(parser.nodes.len - 1);
}
