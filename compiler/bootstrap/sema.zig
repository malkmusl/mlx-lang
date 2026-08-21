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
                const src = self.diags.source_manager.getFile(0).?.content;
                const ident_name = src[tok.start..tok.end];
                
                // First check scope
                if (scope.get(ident_name)) |sym| {
                    // Enforce move semantics
                    if (self.local_states.get(sym.decl_node)) |state| {
                        if (state == .moved) {
                            try self.reportError(tok.start, "Use of moved value");
                        }
                    }
                    try self.node_types.put(node_idx, sym.type_id);
                    return sym.type_id;
                }
                
                // Then check builtin types
                const builtin_prim: ?Type.Primitive = if (std.mem.eql(u8, ident_name, "comptime_int"))
                    .comptime_int_type
                else if (std.mem.eql(u8, ident_name, "comptime_float"))
                    .comptime_float_type
                else if (std.mem.eql(u8, ident_name, "bool"))
                    .bool_type
                else if (std.mem.eql(u8, ident_name, "f32"))
                    .f32_type
                else if (std.mem.eql(u8, ident_name, "f64"))
                    .f64_type
                else if (std.mem.eql(u8, ident_name, "void"))
                    .void_type
                else if (std.mem.eql(u8, ident_name, "type"))
                    .type_type
                else if (std.mem.eql(u8, ident_name, "anytype"))
                    .anytype_type
                else if (std.mem.eql(u8, ident_name, "anyopaque"))
                    .anyopaque_type
                else
                    null;
                
                if (builtin_prim) |prim| {
                    const ty = try self.type_pool.intern(.{ .primitive = prim }, .copyable);
                    try self.node_types.put(node_idx, ty);
                    return ty;
                }
                
                // Check builtin integer types: u8, i8, u16, i16, u32, i32, u64, i64, usize, isize
                if (ident_name.len >= 2) {
                    const signed = ident_name[0] == 'i';
                    const unsigned = ident_name[0] == 'u';
                    if ((signed or unsigned) and ident_name.len <= 5) {
                        const bits = std.fmt.parseInt(u16, ident_name[1..], 10) catch 0;
                        if (bits > 0 and bits <= 128) {
                            const ty = try self.type_pool.intern(.{ .integer = .{ .is_signed = signed, .bits = bits } }, .copyable);
                            try self.node_types.put(node_idx, ty);
                            return ty;
                        }
                    }
                }
                
                try self.reportError(tok.start, "Use of undeclared identifier");
                return 0;
            },
            .const_decl, .var_decl => {
                // New layout:
                //   node.lhs = ident token index
                //   node.rhs = extra_start where:
                //     extra_data[rhs + 0] = type annotation node (0 = inferred)
                //     extra_data[rhs + 1] = init expression node
                const ident_tok_idx = node.data.lhs;
                const extra_start = node.data.rhs;
                const type_node = self.ast_tree.extra_data[extra_start];
                const init_node = self.ast_tree.extra_data[extra_start + 1];

                const src = self.diags.source_manager.getFile(0).?.content;
                const ident_tok = self.ast_tree.tokens[ident_tok_idx];
                const name = src[ident_tok.start..ident_tok.end];

                // Analyze init expression → inferred type
                const inferred_type = try self.analyzeNode(init_node, scope);

                // Resolve annotated type if present
                var declared_type: Type.Id = inferred_type;
                if (type_node != 0) {
                    declared_type = try self.resolveTypeExpr(type_node);

                    // Type check: is the init expression coercible to the declared type?
                    const from_ty = self.type_pool.types.items[inferred_type];
                    const to_ty = self.type_pool.types.items[declared_type];

                    if (!Type.isCoercible(from_ty, to_ty)) {
                        var from_buf: [64]u8 = undefined;
                        var to_buf: [64]u8 = undefined;
                        const from_name = self.type_pool.typeName(inferred_type, &from_buf) catch "<unknown>";
                        const to_name = self.type_pool.typeName(declared_type, &to_buf) catch "<unknown>";
                        std.debug.print("TYPE ERROR: cannot coerce '{s}' to '{s}' for variable '{s}'\n", .{ from_name, to_name, name });
                        try self.reportError(ident_tok.start, "ZIN-E4001: Type mismatch");
                    }
                }

                // Log the resolved type
                {
                    var name_buf: [64]u8 = undefined;
                    const type_name = self.type_pool.typeName(declared_type, &name_buf) catch "<unknown>";
                    std.debug.print("[sema] {s} {s}: {s}\n", .{
                        if (node.tag == .const_decl) "const" else "var",
                        name,
                        type_name,
                    });
                }

                // Enforce nocopy
                const copyability = self.type_pool.types.items[declared_type].copyability;
                if (copyability != .copyable) {
                    const rhs_node = self.ast_tree.nodes.get(init_node);
                    if (rhs_node.tag != .move_builtin and rhs_node.tag != .nocopy_builtin) {
                        try self.reportError(ident_tok.start, "Cannot copy a non-copyable type");
                    }
                }

                // Comptime eval for const declarations
                if (node.tag == .const_decl) {
                    const comptime_vm = @import("comptime.zig");
                    var vm = comptime_vm.ComptimeVM.init(self.allocator, self.ast_tree, self);
                    const val = vm.evaluate(init_node, src);
                    switch (val) {
                        .integer => |i| std.debug.print("[comptime] '{s}' = {d}\n", .{ name, i }),
                        .err => |e| std.debug.print("[comptime] '{s}' cannot be evaluated at compile time: {s}\n", .{ name, e }),
                    }
                }

                try scope.put(name, .{
                    .name = name,
                    .decl_node = node_idx,
                    .type_id = declared_type,
                    .is_const = (node.tag == .const_decl),
                });

                try self.local_states.put(node_idx, .initialized);
                try self.node_types.put(node_idx, declared_type);

                return declared_type;
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
            .fn_proto => {
                const extra_start = node.data.lhs;
                const extra_len = node.data.rhs;
                
                const ret_type_node = self.ast_tree.extra_data[extra_start];
                var ret_type: Type.Id = 0;
                if (ret_type_node != std.math.maxInt(u32)) {
                    ret_type = try self.analyzeNode(ret_type_node, scope);
                }
                
                var i: u32 = 1;
                while (i < extra_len) : (i += 1) {
                    const param_idx = self.ast_tree.extra_data[extra_start + i];
                    const param_node = self.ast_tree.nodes.get(param_idx);
                    
                    const param_type_node = param_node.data.rhs;
                    const param_type = try self.analyzeNode(param_type_node, scope);
                    
                    const param_name_tok = param_node.data.lhs;
                    const tok = self.ast_tree.tokens[param_name_tok];
                    const src = self.diags.source_manager.getFile(0).?.content;
                    const name = src[tok.start..tok.end];
                    
                    try scope.put(name, .{
                        .name = name,
                        .decl_node = param_idx,
                        .type_id = param_type,
                        .is_const = true,
                    });
                    
                    try self.local_states.put(param_idx, .initialized);
                    try self.node_types.put(param_idx, param_type);
                }
                
                const fn_type = try self.type_pool.intern(.{ .function = .{
                    .ret_type = ret_type,
                    .params_start = 0,
                    .params_len = 0,
                    .is_var_args = false,
                } }, .copyable);
                try self.node_types.put(node_idx, fn_type);
                return fn_type;
            },
            .fn_decl => {
                var child_scope = Scope.init(self.allocator, scope);
                defer child_scope.deinit();
                
                const proto = node.data.lhs;
                const body = node.data.rhs;
                
                const fn_type = try self.analyzeNode(proto, &child_scope);
                const body_type = try self.analyzeNode(body, &child_scope);
                _ = body_type; // For now
                
                const proto_node = self.ast_tree.nodes.get(proto);
                const name_tok_idx = proto_node.main_token;
                const fn_tok = self.ast_tree.tokens[name_tok_idx];
                const fn_src = self.diags.source_manager.getFile(0).?.content;
                const fn_name = fn_src[fn_tok.start..fn_tok.end];
                
                try scope.put(fn_name, .{
                    .name = fn_name,
                    .decl_node = node_idx,
                    .type_id = fn_type,
                    .is_const = true,
                });
                
                try self.node_types.put(node_idx, fn_type);
                return fn_type;
            },
            .call => {
                const target = node.data.lhs;
                const extra_start = node.data.rhs;
                
                const target_type_id = try self.analyzeNode(target, scope);
                
                const num_args = self.ast_tree.extra_data[extra_start];
                var i: u32 = 0;
                while (i < num_args) : (i += 1) {
                    const arg_node = self.ast_tree.extra_data[extra_start + 1 + i];
                    _ = try self.analyzeNode(arg_node, scope);
                }
                
                const target_type = self.type_pool.types.items[target_type_id];
                var ret_type: Type.Id = 0;
                if (target_type.data == .function) {
                    ret_type = target_type.data.function.ret_type;
                }
                
                try self.node_types.put(node_idx, ret_type);
                return ret_type;
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

    /// Resolve a type-expression AST node into a Type.Id.
    /// Handles pointer_type, slice_type, optional_type, array_type, error_union_type,
    /// and identifier nodes (mapped to builtin / user-defined types).
    pub fn resolveTypeExpr(self: *Sema, node_idx: Node.Index) !Type.Id {
        const node = self.ast_tree.nodes.get(node_idx);
        const src = self.diags.source_manager.getFile(0).?.content;

        switch (node.tag) {
            .identifier => {
                // Resolve built-in type names
                const tok = self.ast_tree.tokens[node.main_token];
                const name = src[tok.start..tok.end];
                return self.resolveBuiltinTypeName(name, tok.start);
            },

            .pointer_type => {
                const child = try self.resolveTypeExpr(node.data.lhs);
                const flags = node.data.rhs;
                const is_const = (flags & 1) != 0;
                const is_many = (flags & 2) != 0;
                const size: Type.PointerSize = if (is_many) .Many else .One;
                return self.type_pool.intern(.{ .pointer = .{
                    .child_type = child,
                    .is_const = is_const,
                    .is_volatile = false,
                    .is_allowzero = false,
                    .is_optional = false,
                    .size = size,
                    .sentinel = null,
                }}, .copyable);
            },

            .slice_type => {
                const child = try self.resolveTypeExpr(node.data.lhs);
                const is_const = (node.data.rhs & 1) != 0;
                return self.type_pool.intern(.{ .pointer = .{
                    .child_type = child,
                    .is_const = is_const,
                    .is_volatile = false,
                    .is_allowzero = false,
                    .is_optional = false,
                    .size = .Slice,
                    .sentinel = null,
                }}, .copyable);
            },

            .optional_type => {
                const child = try self.resolveTypeExpr(node.data.lhs);
                return self.type_pool.intern(.{ .optional = .{ .child_type = child } }, .copyable);
            },

            .array_type => {
                // lhs = extra_start: [size_expr, elem_type]
                const extra_start = node.data.lhs;
                const elem_node = self.ast_tree.extra_data[extra_start + 1];
                const elem_type = try self.resolveTypeExpr(elem_node);
                // Evaluate size comptime (simplification: try to get the integer value)
                const size_node_idx = self.ast_tree.extra_data[extra_start];
                const size_node = self.ast_tree.nodes.get(size_node_idx);
                var array_len: u64 = 0;
                if (size_node.tag == .integer_literal) {
                    const size_tok = self.ast_tree.tokens[size_node.main_token];
                    const size_text = src[size_tok.start..size_tok.end];
                    array_len = std.fmt.parseInt(u64, size_text, 10) catch 0;
                }
                return self.type_pool.intern(.{ .array = .{
                    .child_type = elem_type,
                    .len = array_len,
                    .sentinel = null,
                }}, .copyable);
            },

            .error_union_type => {
                // lhs = error set node (0 = inferred), rhs = payload type node
                const payload = try self.resolveTypeExpr(node.data.rhs);
                const err_set: Type.Id = if (node.data.lhs != 0)
                    try self.resolveTypeExpr(node.data.lhs)
                else
                    try self.type_pool.internPrimitive(.void_type); // placeholder for inferred
                return self.type_pool.intern(.{ .error_union = .{ .err_set = err_set, .payload = payload }}, .copyable);
            },

            else => {
                // Unknown type expression — return void as a fallback
                return self.type_pool.internPrimitive(.void_type);
            },
        }
    }

    /// Resolve a plain identifier to a built-in type.
    fn resolveBuiltinTypeName(self: *Sema, name: []const u8, start_byte: u32) !Type.Id {
        // Primitive names
        if (std.mem.eql(u8, name, "void"))       return self.type_pool.internPrimitive(.void_type);
        if (std.mem.eql(u8, name, "noreturn"))   return self.type_pool.internPrimitive(.noreturn_type);
        if (std.mem.eql(u8, name, "bool"))       return self.type_pool.internPrimitive(.bool_type);
        if (std.mem.eql(u8, name, "type"))       return self.type_pool.internPrimitive(.type_type);
        if (std.mem.eql(u8, name, "anytype"))    return self.type_pool.internPrimitive(.anytype_type);
        if (std.mem.eql(u8, name, "anyopaque"))  return self.type_pool.internPrimitive(.anyopaque_type);
        if (std.mem.eql(u8, name, "comptime_int"))   return self.type_pool.internPrimitive(.comptime_int_type);
        if (std.mem.eql(u8, name, "comptime_float")) return self.type_pool.internPrimitive(.comptime_float_type);
        // Float types
        if (std.mem.eql(u8, name, "f16"))  return self.type_pool.internPrimitive(.f16_type);
        if (std.mem.eql(u8, name, "f32"))  return self.type_pool.internPrimitive(.f32_type);
        if (std.mem.eql(u8, name, "f64"))  return self.type_pool.internPrimitive(.f64_type);
        if (std.mem.eql(u8, name, "f80"))  return self.type_pool.internPrimitive(.f80_type);
        if (std.mem.eql(u8, name, "f128")) return self.type_pool.internPrimitive(.f128_type);
        // Platform-sized integers
        if (std.mem.eql(u8, name, "usize")) return self.type_pool.internSizeInt(false);
        if (std.mem.eql(u8, name, "isize")) return self.type_pool.internSizeInt(true);
        // Fixed-width integers: u8, i8, u16, i16 … u128, i128
        if (name.len >= 2) {
            const signed = name[0] == 'i';
            const unsigned = name[0] == 'u';
            if ((signed or unsigned) and name.len <= 5) {
                const bits = std.fmt.parseInt(u16, name[1..], 10) catch 0;
                if (bits > 0 and bits <= 128) {
                    return self.type_pool.internInt(signed, bits);
                }
            }
        }
        // Unknown — report and return void
        try self.reportError(start_byte, "ZIN-E3001: Unknown type name");
        return self.type_pool.internPrimitive(.void_type);
    }
};
