const std = @import("std");
const Node = @import("../ast.zig").Node;

pub fn parseTopLevel(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    var flags: Node.DeclFlags = .{};
    var extern_name_token: u32 = std.math.maxInt(u32);
    while (parser.index < parser.tokens.len) {
        switch (parser.tokens[parser.index].tag) {
            .keyword_pub => {
                flags.public = true;
                parser.consumeToken("Consume pub modifier");
            },
            .keyword_export => {
                flags.exported = true;
                parser.consumeToken("Consume export modifier");
            },
            .keyword_inline => {
                flags.inline_hint = true;
                parser.consumeToken("Consume inline modifier");
            },
            .keyword_noinline => {
                flags.noinline_hint = true;
                parser.consumeToken("Consume noinline modifier");
            },
            .keyword_extern => {
                flags.extern_decl = true;
                parser.consumeToken("Consume extern modifier");
                if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .l_paren) {
                    parser.consumeToken("Consume '(' after extern");
                    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .string) {
                        try parser.reportError(2001, "Expected ABI string in extern modifier");
                        return null;
                    }
                    extern_name_token = parser.index;
                    parser.consumeToken("Consume extern ABI string");
                    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_paren) {
                        try parser.reportError(2003, "Expected ')' after extern ABI string");
                        return null;
                    }
                    parser.consumeToken("Consume ')' after extern ABI string");
                }
            },
            else => break,
        }
    }

    if (parser.index >= parser.tokens.len) {
        try parser.reportError(2001, "Expected declaration after modifier");
        return null;
    }
    const declaration = switch (parser.tokens[parser.index].tag) {
        .keyword_const, .keyword_var => try parseVariable(parser),
        .keyword_fn => try parseFunction(parser),
        else => {
            try parser.reportError(2001, "Expected top-level declaration");
            return null;
        },
    };
    if (declaration) |declaration_index| {
        parser.nodes.items(.decl_flags)[declaration_index] = flags;
        parser.nodes.items(.extern_name_token)[declaration_index] = extern_name_token;
    }
    return declaration;
}

pub fn parseVariable(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    const start_token = parser.index;
    parser.consumeToken("Consume const/var keyword");
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .ident) {
        try parser.reportError(2002, "Expected identifier after const/var");
        return null;
    }
    const identifier_token = parser.index;
    parser.consumeToken("Consume identifier");

    var type_node: Node.Index = 0;
    if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .colon) {
        parser.consumeToken("Consume colon");
        type_node = try parser.parseTypeExprPublic() orelse {
            try parser.reportError(2005, "Expected type expression after ':'");
            return null;
        };
    }
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .equal) {
        try parser.reportError(2003, "Expected '=' in variable declaration");
        return null;
    }
    parser.consumeToken("Consume '='");
    const expression = try parser.parseExpr(0) orelse return null;
    if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .statement_end) parser.index += 1;

    const extra_start: u32 = @intCast(parser.extra_data.items.len);
    try parser.extra_data.appendSlice(parser.allocator, &.{ type_node, expression });
    try parser.nodes.append(parser.allocator, .{
        .tag = if (parser.tokens[start_token].tag == .keyword_const) .const_decl else .var_decl,
        .main_token = start_token,
        .data = .{ .lhs = identifier_token, .rhs = extra_start },
    });
    return @intCast(parser.nodes.len - 1);
}

pub fn parseFunction(parser: anytype) std.mem.Allocator.Error!?Node.Index {
    const start_token = parser.index;
    parser.consumeToken("Consume fn");
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .ident) {
        try parser.reportError(2002, "Expected identifier after fn");
        return null;
    }
    const name_token = parser.index;
    parser.consumeToken("Consume function name");
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .l_paren) {
        try parser.reportError(2004, "Expected '(' for parameters");
        return null;
    }
    parser.consumeToken("Consume parameter '('");

    var parameters = std.ArrayList(Node.Index).empty;
    defer parameters.deinit(parser.allocator);
    while (parser.index < parser.tokens.len and parser.tokens[parser.index].tag != .r_paren) {
        var parameter_flags: Node.DeclFlags = .{};
        if (parser.tokens[parser.index].tag == .keyword_comptime) {
            parameter_flags.comptime_param = true;
            parser.consumeToken("Consume comptime parameter modifier");
        }
        if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .ident) {
            try parser.reportError(2005, "Expected parameter name");
            return null;
        }
        const parameter_name = parser.index;
        parser.consumeToken("Consume parameter name");
        if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .colon) {
            try parser.reportError(2006, "Expected ':' after parameter name");
            return null;
        }
        parser.consumeToken("Consume parameter ':'");
        const parameter_type = try parser.parseTypeExprPublic() orelse return null;
        try parser.nodes.append(parser.allocator, .{
            .tag = .param_decl,
            .main_token = parameter_name,
            .data = .{ .lhs = parameter_name, .rhs = parameter_type },
            .decl_flags = parameter_flags,
        });
        try parameters.append(parser.allocator, @intCast(parser.nodes.len - 1));
        if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .comma) {
            parser.consumeToken("Consume parameter comma");
        } else break;
    }
    if (parser.index >= parser.tokens.len or parser.tokens[parser.index].tag != .r_paren) {
        try parser.reportError(2007, "Expected ')' after parameters");
        return null;
    }
    parser.consumeToken("Consume parameter ')'");
    // `->` is the canonical Zin 1.0 return-type separator. Stage0 still
    // accepts the old adjacent return type while the existing corpus migrates.
    if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .arrow) {
        parser.consumeToken("Consume function return arrow");
    }
    if (parser.index < parser.tokens.len and parser.tokens[parser.index].tag == .l_brace) {
        try parser.reportError(2002, "Expected function return type");
        return null;
    }
    const return_type = try parser.parseTypeExprPublic() orelse {
        try parser.reportError(2002, "Expected function return type");
        return null;
    };

    const extra_start: u32 = @intCast(parser.extra_data.items.len);
    try parser.extra_data.append(parser.allocator, return_type);
    try parser.extra_data.appendSlice(parser.allocator, parameters.items);
    try parser.nodes.append(parser.allocator, .{
        .tag = .fn_proto,
        .main_token = name_token,
        .data = .{ .lhs = extra_start, .rhs = @intCast(parser.extra_data.items.len - extra_start) },
    });
    const prototype: Node.Index = @intCast(parser.nodes.len - 1);
    const body = try parser.parseBlockPublic() orelse return null;
    try parser.nodes.append(parser.allocator, .{
        .tag = .fn_decl,
        .main_token = start_token,
        .data = .{ .lhs = prototype, .rhs = body },
    });
    return @intCast(parser.nodes.len - 1);
}
