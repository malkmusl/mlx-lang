const std = @import("std");
const Token = @import("token.zig").Token;
const Tag = Token.Tag;
const ast = @import("ast.zig");
const Node = ast.Node;
const Ast = ast.Ast;
const DiagnosticEngine = @import("../source/diagnostics.zig").DiagnosticEngine;
const SourceManager = @import("../source/source_manager.zig").SourceManager;
const postfix_parser = @import("parser/postfix.zig");
const operators = @import("parser/operators.zig");
const type_parser = @import("parser/types/type_expression.zig");
const declaration_parser = @import("parser/declarations.zig");
const statement_parser = @import("parser/statements.zig");
const primary_parser = @import("parser/expressions/primary.zig");

pub const Parser = struct {
    allocator: std.mem.Allocator,
    tokens: []const Token,
    index: u32,
    nodes: std.MultiArrayList(Node),
    extra_data: std.ArrayList(u32),
    diags: *DiagnosticEngine,
    source_id: u32,
    trace_level: u32,

    const trace_enabled = false;

    pub fn init(allocator: std.mem.Allocator, tokens: []const Token, diags: *DiagnosticEngine, source_id: u32) Parser {
        return .{
            .allocator = allocator,
            .tokens = tokens,
            .index = 0,
            .nodes = std.MultiArrayList(Node){},
            .extra_data = .empty,
            .diags = diags,
            .source_id = source_id,
            .trace_level = 0,
        };
    }

    fn printIndent(self: *Parser) void {
        if (!trace_enabled) return;
        var i: u32 = 0;
        while (i < self.trace_level) : (i += 1) {
            std.debug.print("  ", .{});
        }
    }

    fn traceRuleEnter(self: *Parser, name: []const u8) void {
        if (!trace_enabled) return;
        self.printIndent();
        std.debug.print("-> ENTER: {s}\n", .{name});
        self.trace_level += 1;
    }

    fn traceRuleExit(self: *Parser, name: []const u8) void {
        if (!trace_enabled) return;
        if (self.trace_level > 0) self.trace_level -= 1;
        self.printIndent();
        std.debug.print("<- EXIT: {s}\n", .{name});
    }

    fn traceToken(self: *Parser, msg: []const u8, tok: Token) void {
        if (!trace_enabled) return;
        if (self.diags.source_manager.getFile(self.source_id)) |file| {
            const snippet = file.content[tok.start..@min(tok.end, file.content.len)];
            self.printIndent();
            std.debug.print("   TOKEN: {s} | Tag: {s} | Text: '{s}'\n", .{ msg, @tagName(tok.tag), snippet });
        } else {
            self.printIndent();
            std.debug.print("   TOKEN: {s} | Tag: {s} | Span: {d}..{d}\n", .{ msg, @tagName(tok.tag), tok.start, tok.end });
        }
    }

    pub fn consumeToken(self: *Parser, msg: []const u8) void {
        const tok = self.tokens[self.index];
        self.traceToken(msg, tok);
        self.index += 1;
    }

    pub fn parse(self: *Parser) !Ast {
        self.traceRuleEnter("parse");
        defer self.traceRuleExit("parse");
        // Source file is a series of top-level declarations
        var root_children = std.ArrayList(Node.Index).empty;
        defer root_children.deinit(self.allocator);

        while (self.index < self.tokens.len and self.tokens[self.index].tag != .eof) {
            if (self.tokens[self.index].tag == .statement_end) {
                self.index += 1;
                continue;
            }

            const decl_node_opt = try declaration_parser.parseTopLevel(self);
            if (decl_node_opt) |decl_node| {
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

    fn parseTypeExpr(self: *Parser) std.mem.Allocator.Error!?Node.Index {
        return type_parser.parse(self);
    }

    pub fn parseTypeExprPublic(self: *Parser) std.mem.Allocator.Error!?Node.Index {
        return type_parser.parse(self);
    }

    pub fn parseBlockPublic(self: *Parser) std.mem.Allocator.Error!?Node.Index {
        return statement_parser.parseBlock(self);
    }

    pub fn parseExpr(self: *Parser, binding_power: u8) std.mem.Allocator.Error!?Node.Index {
        self.traceRuleEnter("parseExpr");
        defer self.traceRuleExit("parseExpr");
        var lhs = try self.parsePrefix();
        if (lhs == null) return null;

        while (self.index < self.tokens.len) {
            const op_tok = self.tokens[self.index];

            if (try postfix_parser.parse(self, lhs.?, binding_power)) |postfix| {
                lhs = postfix;
                continue;
            }

            const bp = operators.bindingPower(op_tok.tag);

            if (bp.left == 0) break;
            if (bp.left < binding_power) break;

            const op_tok_idx = self.index;
            self.index += 1;
            const rhs = try self.parseExpr(bp.right);
            if (rhs == null) return null;

            try self.nodes.append(self.allocator, .{
                .tag = .binary_op,
                .main_token = op_tok_idx,
                .data = .{ .lhs = lhs.?, .rhs = rhs.? },
            });
            lhs = @as(u32, @intCast(self.nodes.len - 1));
        }

        return lhs;
    }

    fn parsePrefix(self: *Parser) std.mem.Allocator.Error!?Node.Index {
        return primary_parser.parse(self);
    }

    fn recover(self: *Parser) void {
        const start_index = self.index;
        while (self.index < self.tokens.len) {
            const tag = self.tokens[self.index].tag;
            if (tag == .statement_end or tag == .r_brace or tag == .eof) {
                if (tag != .eof) self.index += 1;
                break;
            }
            if (isDeclarationStart(tag)) {
                if (self.index == start_index) self.index += 1;
                break;
            }
            self.index += 1;
        }
    }

    pub fn recoverPublic(self: *Parser) void {
        self.recover();
    }

    fn isDeclarationStart(tag: Tag) bool {
        return switch (tag) {
            .keyword_pub,
            .keyword_export,
            .keyword_inline,
            .keyword_noinline,
            .keyword_extern,
            .keyword_const,
            .keyword_var,
            .keyword_fn,
            .keyword_test,
            => true,
            else => false,
        };
    }

    pub fn reportError(self: *Parser, code: u32, msg: []const u8) !void {
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
};

test "parser: empty file" {
    const allocator = std.testing.allocator;
    var sm = @import("../source/source_manager.zig").SourceManager.init(allocator);
    defer sm.deinit();
    _ = try sm.addFile("<test>", "");
    var diags = DiagnosticEngine.init(allocator, &sm);
    defer diags.deinit();
    const tokens = [_]Token{.{ .tag = .eof, .start = 0, .end = 0 }};
    var parser = Parser.init(allocator, &tokens, &diags, 0);
    var ast_tree = try parser.parse();
    defer ast_tree.deinit(allocator);

    try std.testing.expectEqual(ast_tree.nodes.items(.tag)[0], .root);
}

test "parser: basic var decl" {
    const allocator = std.testing.allocator;
    var sm = @import("../source/source_manager.zig").SourceManager.init(allocator);
    defer sm.deinit();
    _ = try sm.addFile("<test>", "var a = 1");
    var diags = DiagnosticEngine.init(allocator, &sm);
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

test "parser: incomplete block reaches EOF instead of looping" {
    const allocator = std.testing.allocator;
    var sm = @import("../source/source_manager.zig").SourceManager.init(allocator);
    defer sm.deinit();
    _ = try sm.addFile("<test>", "fn main() void { return");
    var diags = DiagnosticEngine.init(allocator, &sm);
    defer diags.deinit();

    const tokens = [_]Token{
        .{ .tag = .keyword_fn, .start = 0, .end = 2 },
        .{ .tag = .ident, .start = 3, .end = 7 },
        .{ .tag = .l_paren, .start = 7, .end = 8 },
        .{ .tag = .r_paren, .start = 8, .end = 9 },
        .{ .tag = .ident, .start = 10, .end = 14 },
        .{ .tag = .l_brace, .start = 15, .end = 16 },
        .{ .tag = .keyword_return, .start = 17, .end = 23 },
        .{ .tag = .eof, .start = 23, .end = 23 },
    };

    var parser = Parser.init(allocator, &tokens, &diags, 0);
    var ast_tree = try parser.parse();
    defer ast_tree.deinit(allocator);
    try std.testing.expect(parser.index < tokens.len);
    try std.testing.expectEqual(Token.Tag.eof, tokens[parser.index].tag);
    try std.testing.expect(diags.error_count > 0);
}

test "parser: pratt expression precedence" {
    const allocator = std.testing.allocator;
    var sm = @import("../source/source_manager.zig").SourceManager.init(allocator);
    defer sm.deinit();
    _ = try sm.addFile("<test>", "const x = 1 + 2 * 3");
    var diags = DiagnosticEngine.init(allocator, &sm);
    defer diags.deinit();

    // 1 + 2 * 3
    const tokens = [_]Token{
        .{ .tag = .keyword_const, .start = 0, .end = 5 },
        .{ .tag = .ident, .start = 6, .end = 7 },
        .{ .tag = .equal, .start = 8, .end = 9 },
        .{ .tag = .integer, .start = 10, .end = 11 }, // 1
        .{ .tag = .plus, .start = 12, .end = 13 }, // +
        .{ .tag = .integer, .start = 14, .end = 15 }, // 2
        .{ .tag = .asterisk, .start = 16, .end = 17 }, // *
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
    var sm = @import("../source/source_manager.zig").SourceManager.init(allocator);
    defer sm.deinit();
    _ = try sm.addFile("<test>", "const = 1\nvar b = 2");
    var diags = DiagnosticEngine.init(allocator, &sm);
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

test "parser: declaration modifiers are preserved" {
    const allocator = std.testing.allocator;
    const source = "pub extern(\"syscall\") fn raw() void {}";

    var sm = @import("../source/source_manager.zig").SourceManager.init(allocator);
    defer sm.deinit();
    _ = try sm.addFile("<test>", source);

    var diags = DiagnosticEngine.init(allocator, &sm);
    defer diags.deinit();

    const tokens = [_]Token{
        .{ .tag = .keyword_pub, .start = 0, .end = 3 },
        .{ .tag = .keyword_extern, .start = 4, .end = 10 },
        .{ .tag = .l_paren, .start = 10, .end = 11 },
        .{ .tag = .string, .start = 11, .end = 20 },
        .{ .tag = .r_paren, .start = 20, .end = 21 },
        .{ .tag = .keyword_fn, .start = 22, .end = 24 },
        .{ .tag = .ident, .start = 25, .end = 28 },
        .{ .tag = .l_paren, .start = 28, .end = 29 },
        .{ .tag = .r_paren, .start = 29, .end = 30 },
        .{ .tag = .ident, .start = 31, .end = 35 },
        .{ .tag = .l_brace, .start = 36, .end = 37 },
        .{ .tag = .r_brace, .start = 37, .end = 38 },
        .{ .tag = .eof, .start = 38, .end = 38 },
    };

    var parser = Parser.init(allocator, &tokens, &diags, 0);
    var ast_tree = try parser.parse();
    defer ast_tree.deinit(allocator);

    try std.testing.expectEqual(@as(u32, 0), diags.error_count);
    const tags = ast_tree.nodes.items(.tag);
    const fn_decl_idx = std.mem.indexOfScalar(Node.Tag, tags, .fn_decl).?;
    const fn_decl = ast_tree.nodes.get(fn_decl_idx);
    try std.testing.expect(fn_decl.decl_flags.public);
    try std.testing.expect(fn_decl.decl_flags.extern_decl);
    try std.testing.expectEqual(@as(u32, 3), fn_decl.extern_name_token);
}

test "parser: recovery always makes progress at declaration modifiers" {
    const allocator = std.testing.allocator;
    const source = "pub nope";

    var sm = @import("../source/source_manager.zig").SourceManager.init(allocator);
    defer sm.deinit();
    _ = try sm.addFile("<test>", source);

    var diags = DiagnosticEngine.init(allocator, &sm);
    defer diags.deinit();

    const tokens = [_]Token{
        .{ .tag = .keyword_pub, .start = 0, .end = 3 },
        .{ .tag = .ident, .start = 4, .end = 8 },
        .{ .tag = .eof, .start = 8, .end = 8 },
    };

    var parser = Parser.init(allocator, &tokens, &diags, 0);
    var ast_tree = try parser.parse();
    defer ast_tree.deinit(allocator);

    try std.testing.expectEqual(@as(u32, 1), diags.error_count);
    try std.testing.expectEqual(@as(u32, tokens.len - 1), parser.index);
}

test "parser: bootstrap hello syntax" {
    const allocator = std.testing.allocator;
    const source: [:0]const u8 =
        \\const std = @import("std")
        \\pub fn main() !void {
        \\    try std.io.stdout.print("Hello from Zin\\n", .{})
        \\}
    ;

    var sm = @import("../source/source_manager.zig").SourceManager.init(allocator);
    defer sm.deinit();
    _ = try sm.addFile("<test>", source);

    var diags = DiagnosticEngine.init(allocator, &sm);
    defer diags.deinit();

    var lexer = @import("lexer.zig").Lexer.init(source);
    var tokens = std.ArrayList(Token).empty;
    defer tokens.deinit(allocator);
    while (true) {
        const token = lexer.next();
        try tokens.append(allocator, token);
        if (token.tag == .eof) break;
    }

    var parser = Parser.init(allocator, tokens.items, &diags, 0);
    var ast_tree = try parser.parse();
    defer ast_tree.deinit(allocator);

    try std.testing.expectEqual(@as(u32, 0), diags.error_count);
    const tags = ast_tree.nodes.items(.tag);
    try std.testing.expect(std.mem.indexOfScalar(Node.Tag, tags, .error_union_type) != null);
    try std.testing.expect(std.mem.indexOfScalar(Node.Tag, tags, .field_access) != null);
    try std.testing.expect(std.mem.indexOfScalar(Node.Tag, tags, .unary_op) != null);
    try std.testing.expect(std.mem.indexOfScalar(Node.Tag, tags, .tuple_literal) != null);
}

test "parser: canonical function and multi-return syntax" {
    const allocator = std.testing.allocator;
    const source: [:0]const u8 =
        \\fn connect() -> (u8, u8) {
        \\    return -> (7, 6)
        \\}
    ;

    var sm = @import("../source/source_manager.zig").SourceManager.init(allocator);
    defer sm.deinit();
    _ = try sm.addFile("<test>", source);
    var diags = DiagnosticEngine.init(allocator, &sm);
    defer diags.deinit();

    var lexer = @import("lexer.zig").Lexer.init(source);
    var tokens = std.ArrayList(Token).empty;
    defer tokens.deinit(allocator);
    while (true) {
        const token = lexer.next();
        try tokens.append(allocator, token);
        if (token.tag == .eof) break;
    }

    var parser = Parser.init(allocator, tokens.items, &diags, 0);
    var ast_tree = try parser.parse();
    defer ast_tree.deinit(allocator);

    try std.testing.expectEqual(@as(u32, 0), diags.error_count);
    const tags = ast_tree.nodes.items(.tag);
    try std.testing.expect(std.mem.indexOfScalar(Node.Tag, tags, .tuple_type) != null);
    try std.testing.expect(std.mem.indexOfScalar(Node.Tag, tags, .tuple_literal) != null);
    try std.testing.expect(std.mem.indexOfScalar(Node.Tag, tags, .return_stmt) != null);
}
