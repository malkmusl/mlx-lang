const std = @import("std");
const Token = @import("token.zig").Token;
const Tag = Token.Tag;
const ast = @import("ast.zig");
const Node = ast.Node;
const Ast = ast.Ast;
const DiagnosticEngine = @import("diagnostics.zig").DiagnosticEngine;
const SourceManager = @import("source_manager.zig").SourceManager;

pub const Parser = struct {
    allocator: std.mem.Allocator,
    tokens: []const Token,
    index: u32,
    nodes: std.MultiArrayList(Node),
    extra_data: std.ArrayList(u32),
    diags: *DiagnosticEngine,
    source_id: u32,

    pub fn init(allocator: std.mem.Allocator, tokens: []const Token, diags: *DiagnosticEngine, source_id: u32) Parser {
        return .{
            .allocator = allocator,
            .tokens = tokens,
            .index = 0,
            .nodes = std.MultiArrayList(Node){},
            .extra_data = .empty,
            .diags = diags,
            .source_id = source_id,
        };
    }

    pub fn parse(self: *Parser) !Ast {
        // Source file is a series of top-level declarations
        var root_children = std.ArrayList(Node.Index).empty;
        defer root_children.deinit(self.allocator);

        while (self.index < self.tokens.len and self.tokens[self.index].tag != .eof) {
            if (self.tokens[self.index].tag == .statement_end) {
                self.index += 1;
                continue;
            }

            const decl_node = try self.parseTopLevelDecl();
            if (decl_node != 0 or self.tokens[self.index - 1].tag != .invalid) {
                try root_children.append(self.allocator, decl_node);
            } else {
                // If it failed to parse, recover
                self.recover();
            }
        }

        // Add root node
        const extra_start = @as(u32, @intCast(self.extra_data.items.len));
        try self.extra_data.appendSlice(self.allocator, root_children.items);
        
        try self.nodes.append(self.allocator, .{
            .tag = .root,
            .main_token = 0,
            .data = .{
                .lhs = extra_start,
                .rhs = extra_start + @as(u32, @intCast(root_children.items.len)),
            },
        });
        
        const root_index = @as(Node.Index, @intCast(self.nodes.len - 1));
        _ = root_index; // normally would return or store this if AST doesn't assume last node is root

        return Ast{
            .tokens = try self.allocator.dupe(Token, self.tokens), // Assume AST owns its tokens
            .nodes = self.nodes, // Move
            .extra_data = try self.extra_data.toOwnedSlice(self.allocator),
        };
    }

    fn parseTopLevelDecl(self: *Parser) !Node.Index {
        const token = self.tokens[self.index];
        switch (token.tag) {
            .keyword_const, .keyword_var => {
                return self.parseVarDecl();
            },
            .keyword_fn => {
                return self.parseFnDecl();
            },
            else => {
                // Report error
                try self.reportError(2001, "Expected top-level declaration");
                return 0; // 0 is invalid/dummy node index
            }
        }
    }

    fn parseVarDecl(self: *Parser) !Node.Index {
        const start_tok = self.index;
        self.index += 1; // consume const/var
        
        if (self.tokens[self.index].tag != .ident) {
            try self.reportError(2002, "Expected identifier after const/var");
            return 0;
        }
        self.index += 1;
        
        // type annotation optional for now
        if (self.tokens[self.index].tag == .colon) {
            self.index += 1;
            _ = try self.parseExpr(0); // parse type
        }

        if (self.tokens[self.index].tag != .equal) {
            try self.reportError(2003, "Expected '=' in variable declaration");
            return 0;
        }
        self.index += 1;

        const expr = try self.parseExpr(0);
        
        try self.nodes.append(self.allocator, .{
            .tag = if (self.tokens[start_tok].tag == .keyword_const) .const_decl else .var_decl,
            .main_token = start_tok,
            .data = .{ .lhs = start_tok + 1, .rhs = expr }, // lhs is ident token, rhs is expr node
        });
        
        return @as(u32, @intCast(self.nodes.len - 1));
    }

    fn parseFnDecl(self: *Parser) !Node.Index {
        const start_tok = self.index;
        self.index += 1; // consume fn
        
        if (self.tokens[self.index].tag != .ident) {
            try self.reportError(2002, "Expected identifier after fn");
            return 0;
        }
        self.index += 1;
        
        if (self.tokens[self.index].tag != .l_paren) {
            try self.reportError(2004, "Expected '(' for parameters");
            return 0;
        }
        self.index += 1;
        
        while (self.index < self.tokens.len and self.tokens[self.index].tag != .r_paren) {
            self.index += 1; // dummy parameter parse
        }
        if (self.tokens[self.index].tag == .r_paren) self.index += 1;
        
        // return type
        if (self.tokens[self.index].tag != .l_brace) {
            _ = try self.parseExpr(0);
        }
        
        const body = try self.parseBlock();
        
        try self.nodes.append(self.allocator, .{
            .tag = .fn_decl,
            .main_token = start_tok,
            .data = .{ .lhs = start_tok + 1, .rhs = body },
        });
        
        return @as(u32, @intCast(self.nodes.len - 1));
    }

    fn parseBlock(self: *Parser) !Node.Index {
        const start_tok = self.index;
        if (self.tokens[self.index].tag != .l_brace) {
            try self.reportError(2005, "Expected '{' for block");
            return 0;
        }
        self.index += 1;
        
        var stmts = std.ArrayList(Node.Index).empty;
        defer stmts.deinit(self.allocator);
        
        while (self.index < self.tokens.len and self.tokens[self.index].tag != .r_brace) {
            if (self.tokens[self.index].tag == .statement_end) {
                self.index += 1;
                continue;
            }
            // dummy statement parse
            const expr = try self.parseExpr(0);
            try stmts.append(self.allocator, expr);
        }
        if (self.tokens[self.index].tag == .r_brace) self.index += 1;
        
        const extra_start = @as(u32, @intCast(self.extra_data.items.len));
        try self.extra_data.appendSlice(self.allocator, stmts.items);
        
        try self.nodes.append(self.allocator, .{
            .tag = .block,
            .main_token = start_tok,
            .data = .{ .lhs = extra_start, .rhs = extra_start + @as(u32, @intCast(stmts.items.len)) },
        });
        
        return @as(u32, @intCast(self.nodes.len - 1));
    }

    fn parseExpr(self: *Parser, binding_power: u8) !Node.Index {
        var lhs = try self.parsePrefix();
        
        while (self.index < self.tokens.len) {
            const op_tok = self.tokens[self.index];
            const bp = getBindingPower(op_tok.tag);
            
            if (bp.left < binding_power) break;
            
            self.index += 1;
            const rhs = try self.parseExpr(bp.right);
            
            try self.nodes.append(self.allocator, .{
                .tag = .binary_op,
                .main_token = self.index - 1,
                .data = .{ .lhs = lhs, .rhs = rhs },
            });
            lhs = @as(u32, @intCast(self.nodes.len - 1));
        }
        
        return lhs;
    }

    fn parsePrefix(self: *Parser) !Node.Index {
        const tok = self.tokens[self.index];
        switch (tok.tag) {
            .integer, .ident, .string, .float => {
                const tag: Node.Tag = if (tok.tag == .ident) .identifier else .integer_literal; // simplified
                try self.nodes.append(self.allocator, .{
                    .tag = tag,
                    .main_token = self.index,
                    .data = .{ .lhs = 0, .rhs = 0 },
                });
                self.index += 1;
                return @as(u32, @intCast(self.nodes.len - 1));
            },
            else => {
                try self.reportError(2006, "Expected expression");
                self.index += 1;
                return 0;
            }
        }
    }

    fn recover(self: *Parser) void {
        while (self.index < self.tokens.len) {
            const tag = self.tokens[self.index].tag;
            if (tag == .statement_end or tag == .r_brace or tag == .eof) {
                self.index += 1;
                break;
            }
            if (tag == .keyword_fn or tag == .keyword_const or tag == .keyword_pub) {
                break; // Found start of next decl
            }
            self.index += 1;
        }
    }

    fn reportError(self: *Parser, code: u32, msg: []const u8) !void {
        const tok = if (self.index < self.tokens.len) self.tokens[self.index] else self.tokens[self.tokens.len - 1];
        try self.diags.report(.{
            .code = code,
            .phase = .parser,
            .severity = .@"error",
            .primary_span = .{
                .file_id = self.source_id,
                .start_byte = tok.start,
                .end_byte = tok.end,
            },
            .message = msg,
        });
    }

    const BindingPower = struct { left: u8, right: u8 };
    
    fn getBindingPower(tag: Tag) BindingPower {
        return switch (tag) {
            .asterisk, .slash, .percent => .{ .left = 50, .right = 51 },
            .plus, .minus => .{ .left = 40, .right = 41 },
            .equal_equal, .bang_equal, .angle_bracket_left => .{ .left = 30, .right = 31 },
            .plus_shl, .ampersand_shl => .{ .left = 20, .right = 21 }, // shift combines
            else => .{ .left = 0, .right = 0 },
        };
    }
};

test "parser: empty file" {
    const allocator = std.testing.allocator;
    var diags = DiagnosticEngine.init(allocator, undefined); // test doesn't actually render
    defer diags.deinit();
    const tokens = [_]Token{ .{ .tag = .eof, .start = 0, .end = 0 } };
    var parser = Parser.init(allocator, &tokens, &diags, 0);
    var ast_tree = try parser.parse();
    defer ast_tree.deinit(allocator);

    try std.testing.expectEqual(ast_tree.nodes.items(.tag)[0], .root);
}

test "parser: basic var decl" {
    const allocator = std.testing.allocator;
    var diags = DiagnosticEngine.init(allocator, undefined);
    defer diags.deinit();

    // var a = 1
    const tokens = [_]Token{
        .{ .tag = .keyword_var, .start = 0, .end = 3 },
        .{ .tag = .ident, .start = 4, .end = 5 },
        .{ .tag = .equal, .start = 6, .end = 7 },
        .{ .tag = .integer, .start = 8, .end = 9 },
        .{ .tag = .eof, .start = 9, .end = 9 },
    };

    var parser = Parser.init(allocator, &tokens, &diags, 0);
    var ast_tree = try parser.parse();
    defer ast_tree.deinit(allocator);

    const tags = ast_tree.nodes.items(.tag);
    try std.testing.expectEqual(tags.len, 3); // integer, var_decl, root
    try std.testing.expectEqual(tags[0], .integer_literal);
    try std.testing.expectEqual(tags[1], .var_decl);
    try std.testing.expectEqual(tags[2], .root);
}

test "parser: pratt expression precedence" {
    const allocator = std.testing.allocator;
    var diags = DiagnosticEngine.init(allocator, undefined);
    defer diags.deinit();

    // 1 + 2 * 3
    const tokens = [_]Token{
        .{ .tag = .keyword_const, .start = 0, .end = 5 },
        .{ .tag = .ident, .start = 6, .end = 7 },
        .{ .tag = .equal, .start = 8, .end = 9 },
        .{ .tag = .integer, .start = 10, .end = 11 }, // 1
        .{ .tag = .plus, .start = 12, .end = 13 },    // +
        .{ .tag = .integer, .start = 14, .end = 15 }, // 2
        .{ .tag = .asterisk, .start = 16, .end = 17 },// *
        .{ .tag = .integer, .start = 18, .end = 19 }, // 3
        .{ .tag = .eof, .start = 19, .end = 19 },
    };

    var parser = Parser.init(allocator, &tokens, &diags, 0);
    var ast_tree = try parser.parse();
    defer ast_tree.deinit(allocator);

    // Nodes:
    // 0: int (1)
    // 1: int (2)
    // 2: int (3)
    // 3: binary_op (2 * 3) -> lhs 1, rhs 2
    // 4: binary_op (1 + (2 * 3)) -> lhs 0, rhs 3
    // 5: const_decl -> lhs ident, rhs 4
    // 6: root
    
    const tags = ast_tree.nodes.items(.tag);
    const data = ast_tree.nodes.items(.data);
    try std.testing.expectEqual(tags[3], .binary_op);
    try std.testing.expectEqual(data[3].lhs, 1);
    try std.testing.expectEqual(data[3].rhs, 2);

    try std.testing.expectEqual(tags[4], .binary_op);
    try std.testing.expectEqual(data[4].lhs, 0);
    try std.testing.expectEqual(data[4].rhs, 3);
}

test "parser: error recovery" {
    const allocator = std.testing.allocator;
    var diags = DiagnosticEngine.init(allocator, undefined);
    defer diags.deinit();

    // const = 1; var b = 2
    const tokens = [_]Token{
        .{ .tag = .keyword_const, .start = 0, .end = 5 },
        .{ .tag = .equal, .start = 6, .end = 7 }, // Error: expected ident
        .{ .tag = .integer, .start = 8, .end = 9 },
        .{ .tag = .statement_end, .start = 9, .end = 10 }, // Recovery point
        .{ .tag = .keyword_var, .start = 11, .end = 14 },
        .{ .tag = .ident, .start = 15, .end = 16 }, // b
        .{ .tag = .equal, .start = 17, .end = 18 },
        .{ .tag = .integer, .start = 19, .end = 20 },
        .{ .tag = .eof, .start = 20, .end = 20 },
    };

    var parser = Parser.init(allocator, &tokens, &diags, 0);
    var ast_tree = try parser.parse();
    defer ast_tree.deinit(allocator);

    try std.testing.expectEqual(diags.error_count, 1);
    
    // The second var decl should be parsed correctly despite the first failing
    const tags = ast_tree.nodes.items(.tag);
    
    // Nodes:
    // 0: int (2)
    // 1: var_decl (b)
    // 2: root
    try std.testing.expectEqual(tags[1], .var_decl);
}
