const std = @import("std");
const ast = @import("ast.zig");
const Node = ast.Node;
const Type = @import("type.zig").Type;
const TypePool = @import("type.zig").TypePool;
const Scope = @import("scope.zig").Scope;
const DiagnosticEngine = @import("diagnostics.zig").DiagnosticEngine;
const Token = @import("token.zig").Token;

pub const LocalState = enum {
    uninitialized,
    initialized,
    moved,
    partially_moved,
};

pub const Sema = struct {
    allocator: std.mem.Allocator,
    ast_tree: ast.Ast,
    diags: *DiagnosticEngine,
    type_pool: *TypePool,
    root_scope: *Scope,
    
    // Map from declaration Node.Index to its current local state
    local_states: std.AutoHashMap(Node.Index, LocalState),
    // Map from expression Node.Index to its resolved Type.Id
    node_types: std.AutoHashMap(Node.Index, Type.Id),
    
    pub fn init(
        allocator: std.mem.Allocator, 
        ast_tree: ast.Ast, 
        diags: *DiagnosticEngine,
        type_pool: *TypePool,
        root_scope: *Scope,
    ) Sema {
        return .{
            .allocator = allocator,
            .ast_tree = ast_tree,
            .diags = diags,
            .type_pool = type_pool,
            .root_scope = root_scope,
            .local_states = std.AutoHashMap(Node.Index, LocalState).init(allocator),
            .node_types = std.AutoHashMap(Node.Index, Type.Id).init(allocator),
        };
    }

    pub fn deinit(self: *Sema) void {
        self.local_states.deinit();
        self.node_types.deinit();
    }

    pub fn analyze(self: *Sema) !void {
        std.debug.print("-> ENTER: Sema.analyze\n", .{});
        defer std.debug.print("<- EXIT: Sema.analyze\n", .{});
        // Traverse the AST starting from the root
        const root_node = self.ast_tree.nodes.get(self.ast_tree.nodes.len - 1);
        if (root_node.tag != .root) return error.InvalidAst;

        const extra_start = root_node.data.lhs;
        const extra_end = root_node.data.rhs;
        
        var i: u32 = extra_start;
        while (i < extra_end) : (i += 1) {
            const child_idx = self.ast_tree.extra_data[i];
            std.debug.print("   SEMA: Analyzing root child node {d}\n", .{child_idx});
            _ = try self.analyzeNode(child_idx, self.root_scope);
        }
    }

    fn analyzeNode(self: *Sema, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
        const node = self.ast_tree.nodes.get(node_idx);
        std.debug.print("-> ENTER: Sema.analyzeNode | Tag: {s}\n", .{@tagName(node.tag)});
        defer std.debug.print("<- EXIT: Sema.analyzeNode | Tag: {s}\n", .{@tagName(node.tag)});
        
        switch (node.tag) {
            .integer_literal => {
                const ty = try self.type_pool.intern(.{ .primitive = .comptime_int_type }, .copyable);
                try self.node_types.put(node_idx, ty);
                return ty;
            },
            .identifier => {
                const tok = self.ast_tree.tokens[node.main_token];
                // In a real compiler we'd look up the string. For now we use the token index as a unique ID 
                // or we extract the string from source. We'll use a hack to pass the name.
                // Actually, let's just use the string from the source manager.
                const src = self.diags.source_manager.getFile(0).?.content;
                const ident_name = src[tok.start..tok.end];
                
                if (scope.get(ident_name)) |sym| {
                    // Enforce move semantics
                    if (self.local_states.get(sym.decl_node)) |state| {
                        if (state == .moved) {
                            try self.reportError(tok.start, "Use of moved value");
                        }
                    }
                    try self.node_types.put(node_idx, sym.type_id);
                    return sym.type_id;
                } else {
                    try self.reportError(tok.start, "Use of undeclared identifier");
                    return 0;
                }
            },
            .const_decl, .var_decl => {
                const ident_tok = self.ast_tree.tokens[node.data.lhs];
                const src = self.diags.source_manager.getFile(0).?.content;
                const name = src[ident_tok.start..ident_tok.end];
                
                const rhs_idx = node.data.rhs;
                const rhs_type = try self.analyzeNode(rhs_idx, scope);
                
                // Enforce nocopy
                const copyability = self.type_pool.types.items[rhs_type].copyability;
                if (copyability != .copyable) {
                    // Check if it's a move, otherwise error
                    const rhs_node = self.ast_tree.nodes.get(rhs_idx);
                    if (rhs_node.tag != .move_builtin and rhs_node.tag != .nocopy_builtin) {
                        try self.reportError(ident_tok.start, "Cannot copy a non-copyable type"); // pass start index hack
                    }
                }

                if (node.tag == .const_decl) {
                    const comptime_vm = @import("comptime.zig");
                    var vm = comptime_vm.ComptimeVM.init(self.allocator, self.ast_tree, self);
                    const val = vm.evaluate(rhs_idx, src);
                    switch (val) {
                        .integer => |i| std.debug.print("COMPTIME EVAL: '{s}' = {d}\n", .{ name, i }),
                        .err => |e| std.debug.print("COMPTIME EVAL ERR: '{s}': {s}\n", .{ name, e }),
                    }
                }

                try scope.put(name, .{
                    .name = name,
                    .decl_node = node_idx,
                    .type_id = rhs_type,
                    .is_const = (node.tag == .const_decl),
                });
                
                try self.local_states.put(node_idx, .initialized);
                try self.node_types.put(node_idx, rhs_type);
                
                return rhs_type;
            },
            .nocopy_builtin => {
                // @nocopy(expr) -> wraps type
                // Wait, our parser AST puts the wrapped node in rhs
                const inner = node.data.rhs;
                const inner_type = try self.analyzeNode(inner, scope);
                // Return a nocopy version of inner type
                const ty = try self.type_pool.intern(self.type_pool.types.items[inner_type].data, .explicit_nocopy);
                try self.node_types.put(node_idx, ty);
                return ty;
            },
            .move_builtin => {
                const inner = node.data.rhs;
                const inner_type = try self.analyzeNode(inner, scope);
                
                // Find the declaration to transition to moved
                const inner_node = self.ast_tree.nodes.get(inner);
                if (inner_node.tag == .identifier) {
                    const tok = self.ast_tree.tokens[inner_node.main_token];
                    const src = self.diags.source_manager.getFile(0).?.content;
                    const name = src[tok.start..tok.end];
                    
                    if (scope.get(name)) |sym| {
                        try self.local_states.put(sym.decl_node, .moved);
                    }
                }
                
                try self.node_types.put(node_idx, inner_type);
                return inner_type;
            },
            .typeof_builtin => {
                const inner = node.data.rhs;
                const inner_type = try self.analyzeNode(inner, scope);
                _ = inner_type;
                
                // Return a type_type
                const ty = try self.type_pool.intern(.{ .primitive = .type_type }, .copyable);
                try self.node_types.put(node_idx, ty);
                return ty;
            },
            .sizeof_builtin => {
                const inner = node.data.rhs;
                const inner_type = try self.analyzeNode(inner, scope);
                _ = inner_type;
                
                // Return comptime_int
                const ty = try self.type_pool.intern(.{ .primitive = .comptime_int_type }, .copyable);
                try self.node_types.put(node_idx, ty);
                return ty;
            },
            .binary_op => {
                // Simple pass through for now
                const lhs = try self.analyzeNode(node.data.lhs, scope);
                _ = try self.analyzeNode(node.data.rhs, scope);
                try self.node_types.put(node_idx, lhs);
                return lhs;
            },
            .block => {
                const extra_start = node.data.lhs;
                const extra_end = node.data.rhs;
                
                var child_scope = Scope.init(self.allocator, scope);
                defer child_scope.deinit();
                
                var last_type: Type.Id = 0; // default void
                var i: u32 = extra_start;
                while (i < extra_end) : (i += 1) {
                    const child_idx = self.ast_tree.extra_data[i];
                    last_type = try self.analyzeNode(child_idx, &child_scope);
                }
                
                try self.node_types.put(node_idx, last_type);
                return last_type;
            },
            .if_stmt => {
                const extra_start = node.data.lhs;
                const extra_end = node.data.rhs;
                
                const cond_idx = self.ast_tree.extra_data[extra_start];
                _ = try self.analyzeNode(cond_idx, scope);
                
                const then_idx = self.ast_tree.extra_data[extra_start + 1];
                const then_type = try self.analyzeNode(then_idx, scope);
                
                var else_type: Type.Id = 0; // void by default
                if (extra_end > extra_start + 2) {
                    const else_idx = self.ast_tree.extra_data[extra_start + 2];
                    else_type = try self.analyzeNode(else_idx, scope);
                }
                
                // For now just assume they match and return then_type
                // Wait, if it's an if without else, the result should be void.
                // Let's just return then_type for now if it's an expression.
                const result_type = if (extra_end > extra_start + 2) then_type else 0;
                try self.node_types.put(node_idx, result_type);
                return result_type;
            },
            .while_stmt => {
                const cond = node.data.lhs;
                const body = node.data.rhs;
                
                _ = try self.analyzeNode(cond, scope);
                _ = try self.analyzeNode(body, scope);
                
                try self.node_types.put(node_idx, 0); // while evaluates to void
                return 0;
            },
            .fn_decl => {
                const body = node.data.rhs;
                const body_type = try self.analyzeNode(body, scope);
                
                try self.node_types.put(node_idx, body_type);
                return body_type;
            },
            .return_stmt => {
                const expr = node.data.rhs;
                const expr_type = try self.analyzeNode(expr, scope);
                
                // We should technically check this against the function's return type
                try self.node_types.put(node_idx, expr_type);
                return expr_type; // type is technically `noreturn`, but for now `expr_type` works
            },
            else => {
                // Return a dummy type
                return 0;
            }
        }
    }

    fn reportError(self: *Sema, start_byte: u32, msg: []const u8) !void {
        try self.diags.report(.{
            .code = 3000,
            .phase = .sema,
            .severity = .@"error",
            .primary_span = .{
                .file_id = 0,
                .start_byte = start_byte,
                .end_byte = start_byte + 1,
            },
            .message = msg,
        });
    }
};
