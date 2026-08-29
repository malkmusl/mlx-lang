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
    trace_level: u32,

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
        var i: u32 = 0;
        while (i < self.trace_level) : (i += 1) {
            std.debug.print("  ", .{});
        }
    }

    fn traceRuleEnter(self: *Parser, name: []const u8) void {
        self.printIndent();
        std.debug.print("-> ENTER: {s}\n", .{name});
        self.trace_level += 1;
    }

    fn traceRuleExit(self: *Parser, name: []const u8) void {
        if (self.trace_level > 0) self.trace_level -= 1;
        self.printIndent();
        std.debug.print("<- EXIT: {s}\n", .{name});
    }

    fn traceToken(self: *Parser, msg: []const u8, tok: Token) void {
        if (self.diags.source_manager.getFile(self.source_id)) |file| {
            const snippet = file.content[tok.start..@min(tok.end, file.content.len)];
            self.printIndent();
            std.debug.print("   TOKEN: {s} | Tag: {s} | Text: '{s}'\n", .{ msg, @tagName(tok.tag), snippet });
        } else {
            self.printIndent();
            std.debug.print("   TOKEN: {s} | Tag: {s} | Span: {d}..{d}\n", .{ msg, @tagName(tok.tag), tok.start, tok.end });
        }
    }

    fn consumeToken(self: *Parser, msg: []const u8) void {
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

            const decl_node_opt = try self.parseTopLevelDecl();
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

    fn parseTopLevelDecl(self: *Parser) std.mem.Allocator.Error!?Node.Index {
        self.traceRuleEnter("parseTopLevelDecl");
        defer self.traceRuleExit("parseTopLevelDecl");

        var flags: Node.DeclFlags = .{};
        var extern_name_token: u32 = std.math.maxInt(u32);

        while (self.index < self.tokens.len) {
            switch (self.tokens[self.index].tag) {
                .keyword_pub => {
                    flags.public = true;
                    self.consumeToken("Consume pub modifier");
                },
                .keyword_export => {
                    flags.exported = true;
                    self.consumeToken("Consume export modifier");
                },
                .keyword_inline => {
                    flags.inline_hint = true;
                    self.consumeToken("Consume inline modifier");
                },
                .keyword_noinline => {
                    flags.noinline_hint = true;
                    self.consumeToken("Consume noinline modifier");
                },
                .keyword_extern => {
                    flags.extern_decl = true;
                    self.consumeToken("Consume extern modifier");
                    if (self.index < self.tokens.len and self.tokens[self.index].tag == .l_paren) {
                        self.consumeToken("Consume '(' after extern");
                        if (self.index >= self.tokens.len or self.tokens[self.index].tag != .string) {
                            try self.reportError(2001, "Expected ABI string in extern modifier");
                            return null;
                        }
                        extern_name_token = self.index;
                        self.consumeToken("Consume extern ABI string");
                        if (self.index >= self.tokens.len or self.tokens[self.index].tag != .r_paren) {
                            try self.reportError(2003, "Expected ')' after extern ABI string");
                            return null;
                        }
                        self.consumeToken("Consume ')' after extern ABI string");
                    }
                },
                else => break,
            }
        }

        if (self.index >= self.tokens.len) {
            try self.reportError(2001, "Expected declaration after modifier");
            return null;
        }

        const token = self.tokens[self.index];
        const decl = switch (token.tag) {
            .keyword_const, .keyword_var => try self.parseVarDecl(),
            .keyword_fn => try self.parseFnDecl(),
            else => {
                try self.reportError(2001, "Expected top-level declaration");
                return null;
            },
        };

        if (decl) |decl_idx| {
            self.nodes.items(.decl_flags)[decl_idx] = flags;
            self.nodes.items(.extern_name_token)[decl_idx] = extern_name_token;
        }
        return decl;
    }

    fn parseVarDecl(self: *Parser) std.mem.Allocator.Error!?Node.Index {
        self.traceRuleEnter("parseVarDecl");
        defer self.traceRuleExit("parseVarDecl");
        const start_tok = self.index;
        self.consumeToken("Consume const/var keyword");

        if (self.tokens[self.index].tag != .ident) {
            try self.reportError(2002, "Expected identifier after const/var");
            return null;
        }
        const ident_tok = self.index;
        self.consumeToken("Consume identifier");

        // Optional type annotation: `: TypeExpr`
        var type_node: Node.Index = 0; // 0 = inferred
        if (self.tokens[self.index].tag == .colon) {
            self.consumeToken("Consume colon");
            const ty = try self.parseTypeExpr();
            if (ty == null) {
                try self.reportError(2005, "Expected type expression after ':'");
                return null;
            }
            type_node = ty.?;
        }

        if (self.tokens[self.index].tag != .equal) {
            try self.reportError(2003, "Expected '=' in variable declaration");
            return null;
        }
        self.consumeToken("Consume '='");

        const expr = try self.parseExpr(0);
        if (expr == null) return null;

        // Skip optional statement_end (newline-based)
        if (self.index < self.tokens.len and self.tokens[self.index].tag == .statement_end) {
            self.index += 1;
        }

        // Store type_node and init_expr in extra_data
        // Layout: extra_data[extra_start + 0] = type_node, extra_data[extra_start + 1] = init_expr
        const extra_start = @as(u32, @intCast(self.extra_data.items.len));
        try self.extra_data.append(self.allocator, type_node);
        try self.extra_data.append(self.allocator, expr.?);

        try self.nodes.append(self.allocator, .{
            .tag = if (self.tokens[start_tok].tag == .keyword_const) .const_decl else .var_decl,
            .main_token = start_tok,
            .data = .{ .lhs = ident_tok, .rhs = extra_start },
        });

        return @as(u32, @intCast(self.nodes.len - 1));
    }

    /// Parse a type expression in annotation position.
    /// Handles: `*T`, `*const T`, `[*]T`, `[]T`, `?T`, `[N]T`, `E!T`, `!T`, identifiers.
    fn parseTypeExpr(self: *Parser) std.mem.Allocator.Error!?Node.Index {
        self.traceRuleEnter("parseTypeExpr");
        defer self.traceRuleExit("parseTypeExpr");

        const tok = self.tokens[self.index];

        switch (tok.tag) {
            // `*T` or `*const T`
            .asterisk => {
                const star_tok = self.index;
                self.consumeToken("Consume *");
                var is_const = false;
                if (self.tokens[self.index].tag == .keyword_const) {
                    is_const = true;
                    self.consumeToken("Consume const");
                }
                const child = try self.parseTypeExpr() orelse return null;
                // flags: bit0 = is_const
                const flags: u32 = if (is_const) 1 else 0;
                try self.nodes.append(self.allocator, .{
                    .tag = .pointer_type,
                    .main_token = star_tok,
                    .data = .{ .lhs = child, .rhs = flags },
                });
                return @as(u32, @intCast(self.nodes.len - 1));
            },

            // `[N]T` or `[]T` or `[*]T`
            .l_bracket => {
                const lb_tok = self.index;
                self.consumeToken("Consume [");

                // `[]T` — slice
                if (self.tokens[self.index].tag == .r_bracket) {
                    self.consumeToken("Consume ]");
                    var is_const = false;
                    if (self.tokens[self.index].tag == .keyword_const) {
                        is_const = true;
                        self.consumeToken("Consume const");
                    }
                    const child = try self.parseTypeExpr() orelse return null;
                    const flags: u32 = if (is_const) 1 else 0;
                    try self.nodes.append(self.allocator, .{
                        .tag = .slice_type,
                        .main_token = lb_tok,
                        .data = .{ .lhs = child, .rhs = flags },
                    });
                    return @as(u32, @intCast(self.nodes.len - 1));
                }

                // `[*]T` — many-item pointer
                if (self.tokens[self.index].tag == .asterisk) {
                    self.consumeToken("Consume *");
                    if (self.tokens[self.index].tag != .r_bracket) {
                        try self.reportError(2010, "Expected ']' after [*");
                        return null;
                    }
                    self.consumeToken("Consume ]");
                    var is_const = false;
                    if (self.tokens[self.index].tag == .keyword_const) {
                        is_const = true;
                        self.consumeToken("Consume const");
                    }
                    const child = try self.parseTypeExpr() orelse return null;
                    // flags: bit0 = is_const, bit1 = is_many
                    const flags: u32 = (if (is_const) @as(u32, 1) else 0) | 2;
                    try self.nodes.append(self.allocator, .{
                        .tag = .pointer_type,
                        .main_token = lb_tok,
                        .data = .{ .lhs = child, .rhs = flags },
                    });
                    return @as(u32, @intCast(self.nodes.len - 1));
                }

                // `[N]T` — fixed-size array
                const size_expr = try self.parseExpr(0) orelse {
                    try self.reportError(2011, "Expected array size expression");
                    return null;
                };
                if (self.tokens[self.index].tag != .r_bracket) {
                    try self.reportError(2012, "Expected ']' after array size");
                    return null;
                }
                self.consumeToken("Consume ]");
                const elem_type = try self.parseTypeExpr() orelse return null;
                const extra_start = @as(u32, @intCast(self.extra_data.items.len));
                try self.extra_data.append(self.allocator, size_expr);
                try self.extra_data.append(self.allocator, elem_type);
                try self.nodes.append(self.allocator, .{
                    .tag = .array_type,
                    .main_token = lb_tok,
                    .data = .{ .lhs = extra_start, .rhs = 0 },
                });
                return @as(u32, @intCast(self.nodes.len - 1));
            },

            // `?T` — optional
            .question => {
                const q_tok = self.index;
                self.consumeToken("Consume ?");
                const child = try self.parseTypeExpr() orelse return null;
                try self.nodes.append(self.allocator, .{
                    .tag = .optional_type,
                    .main_token = q_tok,
                    .data = .{ .lhs = child, .rhs = 0 },
                });
                return @as(u32, @intCast(self.nodes.len - 1));
            },

            // `!T` — inferred error union
            .bang => {
                const bang_tok = self.index;
                self.consumeToken("Consume !");
                const payload = try self.parseTypeExpr() orelse return null;
                try self.nodes.append(self.allocator, .{
                    .tag = .error_union_type,
                    .main_token = bang_tok,
                    .data = .{ .lhs = 0, .rhs = payload }, // lhs=0 = inferred error set
                });
                return @as(u32, @intCast(self.nodes.len - 1));
            },

            // Plain identifier — a named type like `i32`, `bool`, `MyStruct`, etc.
            // Also handles `E!T` where `E` is the error set identifier.
            .ident => {
                const ident_tok = self.index;
                self.consumeToken("Consume type identifier");
                try self.nodes.append(self.allocator, .{
                    .tag = .identifier,
                    .main_token = ident_tok,
                    .data = .{ .lhs = 0, .rhs = 0 },
                });
                const ident_node = @as(u32, @intCast(self.nodes.len - 1));

                // Check for `E!T` — error union with explicit error set
                if (self.tokens[self.index].tag == .bang) {
                    const bang_tok = self.index;
                    self.consumeToken("Consume ! in E!T");
                    const payload = try self.parseTypeExpr() orelse return null;
                    try self.nodes.append(self.allocator, .{
                        .tag = .error_union_type,
                        .main_token = bang_tok,
                        .data = .{ .lhs = ident_node, .rhs = payload },
                    });
                    return @as(u32, @intCast(self.nodes.len - 1));
                }

                return ident_node;
            },

            // `fn(...) R` — inline function type
            .keyword_fn => {
                // Delegate to parseFnType (minimal, returns identifier-level node for now)
                const fn_tok = self.index;
                self.consumeToken("Consume fn keyword in type");
                // Skip parameter list
                if (self.tokens[self.index].tag == .l_paren) {
                    self.consumeToken("Consume (");
                    var depth: u32 = 1;
                    while (self.index < self.tokens.len and depth > 0) {
                        if (self.tokens[self.index].tag == .l_paren) depth += 1;
                        if (self.tokens[self.index].tag == .r_paren) depth -= 1;
                        self.consumeToken("Skip fn type param");
                    }
                }
                // Return type
                const ret = try self.parseTypeExpr() orelse return null;
                // Represent as a pointer_type node with flags=0xFF as placeholder
                try self.nodes.append(self.allocator, .{
                    .tag = .pointer_type, // placeholder — stage 2 will add fn_type node
                    .main_token = fn_tok,
                    .data = .{ .lhs = ret, .rhs = 0xFF },
                });
                return @as(u32, @intCast(self.nodes.len - 1));
            },

            else => {
                // Not a type expression — let caller handle the error
                return null;
            },
        }
    }

    fn parseFnDecl(self: *Parser) std.mem.Allocator.Error!?Node.Index {
        self.traceRuleEnter("parseFnDecl");
        defer self.traceRuleExit("parseFnDecl");
        const start_tok = self.index;
        self.index += 1; // consume fn

        if (self.tokens[self.index].tag != .ident) {
            try self.reportError(2002, "Expected identifier after fn");
            return null;
        }
        const name_tok = self.index;
        self.index += 1;

        if (self.tokens[self.index].tag != .l_paren) {
            try self.reportError(2004, "Expected '(' for parameters");
            return null;
        }
        self.index += 1;

        var param_nodes = std.ArrayList(Node.Index).empty;
        defer param_nodes.deinit(self.allocator);

        while (self.index < self.tokens.len and self.tokens[self.index].tag != .r_paren) {
            const param_name_tok = self.index;
            if (self.tokens[self.index].tag != .ident) {
                try self.reportError(2005, "Expected parameter name");
                return null;
            }
            self.index += 1;

            if (self.tokens[self.index].tag != .colon) {
                try self.reportError(2006, "Expected ':' after parameter name");
                return null;
            }
            self.index += 1;

            const type_expr = try self.parseExpr(0);
            if (type_expr == null) return null;

            try self.nodes.append(self.allocator, .{
                .tag = .param_decl,
                .main_token = param_name_tok,
                .data = .{ .lhs = param_name_tok, .rhs = type_expr.? },
            });
            try param_nodes.append(self.allocator, @as(u32, @intCast(self.nodes.len - 1)));

            if (self.tokens[self.index].tag == .comma) {
                self.index += 1;
            } else {
                break;
            }
        }
        if (self.tokens[self.index].tag == .r_paren) {
            self.index += 1;
        } else {
            try self.reportError(2007, "Expected ')' after parameters");
            return null;
        }

        // return type
        var ret_type: Node.Index = std.math.maxInt(u32);
        if (self.tokens[self.index].tag != .l_brace) {
            const rt = try self.parseExpr(0);
            if (rt) |r| {
                ret_type = r;
            }
        }

        const extra_start = @as(u32, @intCast(self.extra_data.items.len));
        try self.extra_data.append(self.allocator, ret_type);
        try self.extra_data.appendSlice(self.allocator, param_nodes.items);
        const extra_len = @as(u32, @intCast(self.extra_data.items.len - extra_start));

        try self.nodes.append(self.allocator, .{
            .tag = .fn_proto,
            .main_token = name_tok,
            .data = .{ .lhs = extra_start, .rhs = extra_len },
        });
        const proto_idx = @as(u32, @intCast(self.nodes.len - 1));

        const body = try self.parseBlock();
        if (body == null) return null;

        try self.nodes.append(self.allocator, .{
            .tag = .fn_decl,
            .main_token = start_tok,
            .data = .{ .lhs = proto_idx, .rhs = body.? },
        });

        return @as(u32, @intCast(self.nodes.len - 1));
    }

    fn parseBlock(self: *Parser) std.mem.Allocator.Error!?Node.Index {
        self.traceRuleEnter("parseBlock");
        defer self.traceRuleExit("parseBlock");
        const start_tok = self.index;
        if (self.tokens[self.index].tag != .l_brace) {
            try self.reportError(2005, "Expected '{' for block");
            return null;
        }
        self.index += 1;

        var stmts = std.ArrayList(Node.Index).empty;
        defer stmts.deinit(self.allocator);

        while (self.index < self.tokens.len and self.tokens[self.index].tag != .r_brace) {
            if (self.tokens[self.index].tag == .statement_end) {
                self.index += 1;
                continue;
            }
            const stmt = try self.parseStatement();
            if (stmt) |s| {
                try stmts.append(self.allocator, s);
            } else {
                self.recover();
            }
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

    fn parseStatement(self: *Parser) std.mem.Allocator.Error!?Node.Index {
        self.traceRuleEnter("parseStatement");
        defer self.traceRuleExit("parseStatement");
        const token = self.tokens[self.index];
        switch (token.tag) {
            .keyword_const, .keyword_var => {
                return self.parseVarDecl();
            },
            .keyword_fn => {
                return self.parseFnDecl();
            },
            .keyword_return => {
                const ret_tok = self.index;
                self.index += 1; // consume 'return'
                // optional expression after return
                const expr = try self.parseExpr(0);
                // skip optional statement_end
                if (self.index < self.tokens.len and self.tokens[self.index].tag == .statement_end) {
                    self.index += 1;
                }
                const expr_node: Node.Index = expr orelse 0;
                try self.nodes.append(self.allocator, .{
                    .tag = .return_stmt,
                    .main_token = ret_tok,
                    .data = .{ .lhs = 0, .rhs = expr_node },
                });
                return @as(u32, @intCast(self.nodes.len - 1));
            },
            else => {
                return self.parseExpr(0);
            },
        }
    }

    fn parseExpr(self: *Parser, binding_power: u8) std.mem.Allocator.Error!?Node.Index {
        self.traceRuleEnter("parseExpr");
        defer self.traceRuleExit("parseExpr");
        var lhs = try self.parsePrefix();
        if (lhs == null) return null;

        while (self.index < self.tokens.len) {
            const op_tok = self.tokens[self.index];

            if (op_tok.tag == .l_paren) {
                if (100 < binding_power) break;
                const call_tok = self.index;
                self.index += 1;

                var args = std.ArrayList(Node.Index).empty;
                defer args.deinit(self.allocator);

                while (self.index < self.tokens.len and self.tokens[self.index].tag != .r_paren) {
                    const arg = try self.parseExpr(0);
                    if (arg == null) return null;
                    try args.append(self.allocator, arg.?);

                    if (self.tokens[self.index].tag == .comma) {
                        self.index += 1;
                    } else {
                        break;
                    }
                }

                if (self.tokens[self.index].tag == .r_paren) {
                    self.index += 1;
                } else {
                    try self.reportError(2008, "Expected ')' after function arguments");
                    return null;
                }

                const extra_start = @as(u32, @intCast(self.extra_data.items.len));
                try self.extra_data.append(self.allocator, @as(u32, @intCast(args.items.len)));
                try self.extra_data.appendSlice(self.allocator, args.items);

                try self.nodes.append(self.allocator, .{
                    .tag = .call,
                    .main_token = call_tok,
                    .data = .{ .lhs = lhs.?, .rhs = extra_start },
                });

                lhs = @as(u32, @intCast(self.nodes.len - 1));
                continue;
            }

            const bp = getBindingPower(op_tok.tag);

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
        self.traceRuleEnter("parsePrefix");
        defer self.traceRuleExit("parsePrefix");
        const tok = self.tokens[self.index];
        switch (tok.tag) {
            .string => {
                try self.nodes.append(self.allocator, .{
                    .tag = .string_literal,
                    .main_token = self.index,
                    .data = .{ .lhs = 0, .rhs = 0 },
                });
                self.consumeToken("Consume string literal");
                return @as(u32, @intCast(self.nodes.len - 1));
            },
            .integer, .float => {
                try self.nodes.append(self.allocator, .{
                    .tag = .integer_literal,
                    .main_token = self.index,
                    .data = .{ .lhs = 0, .rhs = 0 },
                });
                self.consumeToken("Consume integer/float literal");
                return @as(u32, @intCast(self.nodes.len - 1));
            },
            .ident => {
                try self.nodes.append(self.allocator, .{
                    .tag = .identifier,
                    .main_token = self.index,
                    .data = .{ .lhs = 0, .rhs = 0 },
                });
                self.consumeToken("Consume identifier");
                return @as(u32, @intCast(self.nodes.len - 1));
            },
            .at => {
                // Builtin invocation like @nocopy, @move, @print(fmt, args...)
                const start_tok = self.index;
                self.consumeToken("Consume @");

                if (self.index >= self.tokens.len or self.tokens[self.index].tag != .ident) {
                    try self.reportError(2007, "Expected builtin identifier after @");
                    return null;
                }
                const builtin_tok = self.tokens[self.index];
                const src = self.diags.source_manager.getFile(self.source_id).?.content;
                const name = src[builtin_tok.start..builtin_tok.end];
                self.consumeToken("Consume builtin name");

                if (self.index >= self.tokens.len or self.tokens[self.index].tag != .l_paren) {
                    try self.reportError(2008, "Expected '(' after builtin");
                    return null;
                }
                self.consumeToken("Consume '('");

                // Determine if this is a known multi-arg builtin
                const is_multi_arg = std.mem.eql(u8, name, "print") or
                    std.mem.eql(u8, name, "import");

                if (is_multi_arg) {
                    // Parse multiple arguments like a normal call
                    var args = std.ArrayList(Node.Index).empty;
                    defer args.deinit(self.allocator);

                    while (self.index < self.tokens.len and self.tokens[self.index].tag != .r_paren) {
                        const arg = try self.parseExpr(0);
                        if (arg == null) return null;
                        try args.append(self.allocator, arg.?);
                        if (self.tokens[self.index].tag == .comma) {
                            self.index += 1;
                        } else {
                            break;
                        }
                    }
                    if (self.index < self.tokens.len and self.tokens[self.index].tag == .r_paren) {
                        self.consumeToken("Consume ')'");
                    }

                    const extra_start = @as(u32, @intCast(self.extra_data.items.len));
                    try self.extra_data.append(self.allocator, @as(u32, @intCast(args.items.len)));
                    try self.extra_data.appendSlice(self.allocator, args.items);

                    try self.nodes.append(self.allocator, .{
                        .tag = .builtin_call,
                        .main_token = start_tok,
                        .data = .{ .lhs = @as(u32, @intCast(self.index - 1)), .rhs = extra_start },
                    });
                    return @as(u32, @intCast(self.nodes.len - 1));
                } else {
                    // Single-arg builtin (legacy: @nocopy, @move, etc.)
                    const inner = try self.parseExpr(0);
                    if (inner == null) return null;

                    if (self.index >= self.tokens.len or self.tokens[self.index].tag != .r_paren) {
                        try self.reportError(2009, "Expected ')' after builtin argument");
                        return null;
                    }
                    self.consumeToken("Consume ')'");

                    const tag: Node.Tag = if (std.mem.eql(u8, name, "nocopy")) .nocopy_builtin else if (std.mem.eql(u8, name, "move")) .move_builtin else if (std.mem.eql(u8, name, "typeOf")) .typeof_builtin else if (std.mem.eql(u8, name, "sizeOf")) .sizeof_builtin else .builtin_call;

                    try self.nodes.append(self.allocator, .{
                        .tag = tag,
                        .main_token = start_tok,
                        .data = .{ .lhs = 0, .rhs = inner.? },
                    });
                    return @as(u32, @intCast(self.nodes.len - 1));
                }
            },
            .keyword_if => return self.parseIfExpr(),
            .keyword_while => return self.parseWhileLoop(),
            .keyword_return => {
                const start_tok = self.index;
                self.consumeToken("Consume return");
                const expr = try self.parseExpr(0);
                try self.nodes.append(self.allocator, .{
                    .tag = .return_stmt,
                    .main_token = start_tok,
                    .data = .{ .lhs = 0, .rhs = expr.? },
                });
                return @as(u32, @intCast(self.nodes.len - 1));
            },
            .l_brace => return self.parseBlock(),
            .l_paren => {
                self.consumeToken("Consume '('");
                const expr = try self.parseExpr(0);
                if (expr == null) return null;
                if (self.index >= self.tokens.len or self.tokens[self.index].tag != .r_paren) {
                    try self.reportError(2008, "Expected ')'");
                    return null;
                }
                self.consumeToken("Consume ')'");
                return expr;
            },
            else => {
                try self.reportError(2007, "Unexpected token in expression");
                return null;
            },
        }
    }

    fn parseIfExpr(self: *Parser) !?Node.Index {
        self.traceRuleEnter("parseIfExpr");
        defer self.traceRuleExit("parseIfExpr");
        const start_tok = self.index;
        self.consumeToken("Consume if");

        const cond = try self.parseExpr(0);
        if (cond == null) return null;

        const then_branch = try self.parseExpr(0);
        if (then_branch == null) return null;

        var else_branch: ?Node.Index = null;
        if (self.index < self.tokens.len and self.tokens[self.index].tag == .keyword_else) {
            self.consumeToken("Consume else");
            else_branch = try self.parseExpr(0);
            if (else_branch == null) return null;
        }

        const extra_start = @as(u32, @intCast(self.extra_data.items.len));
        try self.extra_data.append(self.allocator, cond.?);
        try self.extra_data.append(self.allocator, then_branch.?);
        if (else_branch) |e| {
            try self.extra_data.append(self.allocator, e);
        }
        const extra_end = @as(u32, @intCast(self.extra_data.items.len));

        try self.nodes.append(self.allocator, .{
            .tag = .if_stmt,
            .main_token = start_tok,
            .data = .{ .lhs = extra_start, .rhs = extra_end },
        });

        return @as(u32, @intCast(self.nodes.len - 1));
    }

    fn parseWhileLoop(self: *Parser) !?Node.Index {
        self.traceRuleEnter("parseWhileLoop");
        defer self.traceRuleExit("parseWhileLoop");
        const start_tok = self.index;
        self.consumeToken("Consume while");

        const cond = try self.parseExpr(0);
        if (cond == null) return null;

        const body = try self.parseExpr(0);
        if (body == null) return null;

        try self.nodes.append(self.allocator, .{
            .tag = .while_stmt,
            .main_token = start_tok,
            .data = .{ .lhs = cond.?, .rhs = body.? },
        });

        return @as(u32, @intCast(self.nodes.len - 1));
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
    var sm = @import("source_manager.zig").SourceManager.init(allocator);
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
    var sm = @import("source_manager.zig").SourceManager.init(allocator);
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

test "parser: pratt expression precedence" {
    const allocator = std.testing.allocator;
    var sm = @import("source_manager.zig").SourceManager.init(allocator);
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
    var sm = @import("source_manager.zig").SourceManager.init(allocator);
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
    const source = "pub extern(\"syscall\") fn raw() {}";

    var sm = @import("source_manager.zig").SourceManager.init(allocator);
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
        .{ .tag = .l_brace, .start = 31, .end = 32 },
        .{ .tag = .r_brace, .start = 32, .end = 33 },
        .{ .tag = .eof, .start = 33, .end = 33 },
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

    var sm = @import("source_manager.zig").SourceManager.init(allocator);
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
