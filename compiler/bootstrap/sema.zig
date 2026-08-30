const std = @import("std");
const ast = @import("ast.zig");
const Node = ast.Node;
const Type = @import("type.zig").Type;
const TypePool = @import("type.zig").TypePool;
const Scope = @import("scope.zig").Scope;
const DiagnosticEngine = @import("diagnostics.zig").DiagnosticEngine;
const Phase = @import("diagnostics.zig").Phase;
const Token = @import("token.zig").Token;
const builtin = @import("builtin.zig");

pub const LocalState = enum {
    uninitialized,
    initialized,
    moved,
    partially_moved,
};

const LoopContext = struct {
    label_token: u32,
    break_type: ?Type.Id = null,
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
    // Comptime values materialized by builtin evaluation.
    const_values: std.AutoHashMap(Node.Index, u64),
    // Type-valued expressions map to the type value they denote.
    type_values: std.AutoHashMap(Node.Index, Type.Id),
    unsafe_depth: u32,
    eval_branch_quota: u64,
    current_return_type: ?Type.Id,
    loop_stack: std.ArrayList(LoopContext),

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
            .const_values = std.AutoHashMap(Node.Index, u64).init(allocator),
            .type_values = std.AutoHashMap(Node.Index, Type.Id).init(allocator),
            .unsafe_depth = 0,
            .eval_branch_quota = 1_000_000,
            .current_return_type = null,
            .loop_stack = std.ArrayList(LoopContext).empty,
        };
    }

    pub fn deinit(self: *Sema) void {
        self.local_states.deinit();
        self.node_types.deinit();
        self.const_values.deinit();
        self.type_values.deinit();
        self.loop_stack.deinit(self.allocator);
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
                const tok = self.ast_tree.tokens[node.main_token];
                const src = self.diags.source_manager.getFile(0).?.content;
                if (std.fmt.parseInt(u64, src[tok.start..tok.end], 0)) |value| {
                    try self.const_values.put(node_idx, value);
                } else |_| {}
                return ty;
            },
            .float_literal => {
                const ty = try self.type_pool.internPrimitive(.comptime_float_type);
                try self.node_types.put(node_idx, ty);
                return ty;
            },
            .bool_literal => {
                const ty = try self.type_pool.internPrimitive(.bool_type);
                const tok = self.ast_tree.tokens[node.main_token];
                const src = self.diags.source_manager.getFile(0).?.content;
                try self.const_values.put(node_idx, if (std.mem.eql(u8, src[tok.start..tok.end], "true")) 1 else 0);
                try self.node_types.put(node_idx, ty);
                return ty;
            },
            .string_literal => {
                const u8_type = try self.type_pool.internInt(false, 8);
                const ty = try self.type_pool.internSlice(u8_type, true);
                try self.node_types.put(node_idx, ty);
                return ty;
            },
            .pointer_type, .slice_type, .array_type, .optional_type, .error_union_type => {
                const ty = try self.resolveTypeExpr(node_idx);
                try self.node_types.put(node_idx, ty);
                return ty;
            },
            .struct_decl, .enum_decl, .union_decl => return self.analyzeAggregateType(node_idx, scope),
            .identifier => {
                const tok = self.ast_tree.tokens[node.main_token];
                const src = self.diags.source_manager.getFile(0).?.content;
                const ident_name = src[tok.start..tok.end];

                // First check scope
                if (scope.get(ident_name)) |sym| {
                    // Enforce move semantics
                    if (self.local_states.get(sym.decl_node)) |state| {
                        if (state == .moved) {
                            try self.reportError(6001, .sema, tok.start, "Use of moved value");
                        }
                    }
                    try self.node_types.put(node_idx, sym.type_id);
                    return sym.type_id;
                }

                // Function declarations are order-independent at module scope.
                // Materialize a later function's signature on first reference;
                // its body is still analyzed in the normal source traversal.
                if (self.findRootFunction(ident_name)) |function_decl| {
                    const function_type = try self.declareFunction(function_decl, self.root_scope);
                    try self.node_types.put(node_idx, function_type);
                    return function_type;
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
                        if (bits > 0 and bits <= 4096) {
                            const ty = try self.type_pool.intern(.{ .integer = .{ .is_signed = signed, .bits = bits } }, .copyable);
                            try self.node_types.put(node_idx, ty);
                            return ty;
                        }
                    }
                }

                try self.reportError(3001, .resolve, tok.start, "Use of undeclared identifier");
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
                        try self.reportError(4001, .sema, ident_tok.start, "Type mismatch");
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

                // A fresh rvalue may initialize non-copyable storage. The forbidden
                // operation is reading an existing addressable value by copy; such a
                // transfer must be written with @move.
                const copyability = self.type_pool.types.items[inferred_type].copyability;
                if (copyability != .copyable) {
                    const rhs_node = self.ast_tree.nodes.get(init_node);
                    const copies_storage = rhs_node.tag == .identifier or rhs_node.tag == .field_access;
                    if (copies_storage and !self.sourceIsMoved(init_node, scope)) {
                        try self.reportError(6002, .sema, ident_tok.start, "Cannot copy a non-copyable type");
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
                if (self.type_values.get(init_node)) |type_value| {
                    try self.type_values.put(node_idx, type_value);
                }
                if (self.const_values.get(init_node)) |const_value| {
                    try self.const_values.put(node_idx, const_value);
                }

                return declared_type;
            },
            .builtin_call => return self.analyzeBuiltin(node_idx, scope),
            .binary_op => {
                const operator = self.ast_tree.tokens[node.main_token].tag;
                if (operator == .equal) {
                    const target_node = self.ast_tree.nodes.get(node.data.lhs);
                    const target_type = try self.analyzeNode(node.data.lhs, scope);
                    const value_type = try self.analyzeNode(node.data.rhs, scope);
                    if (target_node.tag != .identifier) {
                        try self.reportError(4001, .sema, self.ast_tree.tokens[node.main_token].start, "Assignment target must be an identifier");
                    } else {
                        const target_token = self.ast_tree.tokens[target_node.main_token];
                        const src = self.diags.source_manager.getFile(0).?.content;
                        if (scope.get(src[target_token.start..target_token.end])) |symbol| {
                            if (symbol.is_const) {
                                try self.reportError(4001, .sema, target_token.start, "Cannot assign to a const value");
                            }
                        }
                    }
                    if (!Type.isCoercible(self.type_pool.get(value_type), self.type_pool.get(target_type))) {
                        try self.reportError(4001, .sema, self.ast_tree.tokens[node.main_token].start, "Assigned expression does not match target type");
                    }
                    const void_type = try self.type_pool.internPrimitive(.void_type);
                    try self.node_types.put(node_idx, void_type);
                    return void_type;
                }
                const lhs = try self.analyzeNode(node.data.lhs, scope);
                const rhs = try self.analyzeNode(node.data.rhs, scope);
                const is_comparison = switch (operator) {
                    .equal_equal,
                    .bang_equal,
                    .angle_bracket_left,
                    .angle_bracket_left_equal,
                    .angle_bracket_right,
                    .angle_bracket_right_equal,
                    => true,
                    else => false,
                };
                if (is_comparison) {
                    const lhs_type = self.type_pool.get(lhs);
                    const rhs_type = self.type_pool.get(rhs);
                    if (!Type.isCoercible(lhs_type, rhs_type) and !Type.isCoercible(rhs_type, lhs_type)) {
                        try self.reportError(4014, .sema, self.ast_tree.tokens[node.main_token].start, "Comparison operands have incompatible types");
                    }
                    const bool_type = try self.type_pool.internPrimitive(.bool_type);
                    try self.node_types.put(node_idx, bool_type);
                    return bool_type;
                }
                try self.node_types.put(node_idx, lhs);
                return lhs;
            },
            .unary_op => {
                const operand_type = try self.analyzeNode(node.data.lhs, scope);
                const operator = self.ast_tree.tokens[node.main_token].tag;
                if (operator == .keyword_try) {
                    const operand = self.type_pool.get(operand_type);
                    if (operand.data != .error_union) {
                        try self.reportError(4014, .sema, self.ast_tree.tokens[node.main_token].start, "try requires an error-union operand");
                        try self.node_types.put(node_idx, operand_type);
                        return operand_type;
                    }
                    const return_type = if (self.current_return_type) |type_id|
                        self.type_pool.get(type_id)
                    else
                        null;
                    if (return_type == null or return_type.?.data != .error_union) {
                        try self.reportError(4008, .sema, self.ast_tree.tokens[node.main_token].start, "try can only propagate from an error-capable function");
                    }
                    const payload = operand.data.error_union.payload;
                    try self.node_types.put(node_idx, payload);
                    return payload;
                }
                try self.node_types.put(node_idx, operand_type);
                return operand_type;
            },
            .tuple_literal => {
                const extra_start = node.data.lhs;
                const count = self.ast_tree.extra_data[extra_start];
                var fields = std.ArrayList(TypePool.AggregateFieldInput).empty;
                defer fields.deinit(self.allocator);
                var i: u32 = 0;
                while (i < count) : (i += 1) {
                    const element_type = try self.analyzeNode(self.ast_tree.extra_data[extra_start + 1 + i], scope);
                    try fields.append(self.allocator, .{ .name = "", .type_id = element_type });
                }
                const ty = self.type_pool.internAggregate(.tuple, fields.items, null, null) catch {
                    try self.reportError(5005, .@"comptime", self.ast_tree.tokens[node.main_token].start, "Tuple layout could not be computed");
                    return self.type_pool.internPrimitive(.void_type);
                };
                try self.node_types.put(node_idx, ty);
                return ty;
            },
            .array_literal => {
                const extra_start = node.data.lhs;
                const length_node = self.ast_tree.extra_data[extra_start];
                const element_type_node = self.ast_tree.extra_data[extra_start + 1];
                const count = self.ast_tree.extra_data[extra_start + 2];
                const element_type = try self.resolveTypeExpr(element_type_node);
                const declared_length = if (length_node == std.math.maxInt(u32))
                    @as(u64, count)
                else
                    self.comptimeInteger(length_node) orelse 0;
                if (declared_length != count) {
                    try self.reportError(4001, .sema, self.ast_tree.tokens[node.main_token].start, "Array literal element count does not match its declared length");
                }
                var index: u32 = 0;
                while (index < count) : (index += 1) {
                    const element_node = self.ast_tree.extra_data[extra_start + 3 + index];
                    const actual_type = try self.analyzeNode(element_node, scope);
                    if (!Type.isCoercible(self.type_pool.get(actual_type), self.type_pool.get(element_type))) {
                        try self.reportError(4001, .sema, self.ast_tree.tokens[self.ast_tree.nodes.get(element_node).main_token].start, "Array element does not match the declared element type");
                    }
                }
                const array_type = try self.type_pool.internArray(element_type, declared_length);
                try self.node_types.put(node_idx, array_type);
                return array_type;
            },
            .field_access => {
                const base_type = try self.analyzeNode(node.data.lhs, scope);
                const base = self.type_pool.get(base_type);
                if (base.data == .primitive and base.data.primitive == .anyopaque_type) {
                    try self.node_types.put(node_idx, base_type);
                    return base_type;
                }
                const field_token = self.ast_tree.tokens[node.main_token];
                const src = self.diags.source_manager.getFile(0).?.content;
                if (self.type_pool.aggregateField(base_type, src[field_token.start..field_token.end])) |field| {
                    try self.node_types.put(node_idx, field.type_id);
                    return field.type_id;
                }
                const tok = self.ast_tree.tokens[node.main_token];
                try self.reportError(3001, .resolve, tok.start, "Unknown field or declaration");
                try self.node_types.put(node_idx, base_type);
                return base_type;
            },
            .unsafe_block => {
                self.unsafe_depth += 1;
                defer self.unsafe_depth -= 1;
                const ty = try self.analyzeNode(node.data.lhs, scope);
                try self.node_types.put(node_idx, ty);
                return ty;
            },
            .block => {
                const extra_start = node.data.lhs;
                const extra_end = node.data.rhs;

                var child_scope = Scope.init(self.allocator, scope);
                defer child_scope.deinit();

                var last_type = try self.type_pool.internPrimitive(.void_type);
                var i: u32 = extra_start;
                while (i < extra_end) : (i += 1) {
                    const child_idx = self.ast_tree.extra_data[i];
                    last_type = try self.analyzeNode(child_idx, &child_scope);
                    if (self.type_pool.get(last_type).data == .error_union and self.isDiscardedExpression(child_idx)) {
                        const child = self.ast_tree.nodes.get(child_idx);
                        try self.reportError(4008, .sema, self.ast_tree.tokens[child.main_token].start, "Error-union value must be handled explicitly");
                    }
                }

                try self.node_types.put(node_idx, last_type);
                return last_type;
            },
            .if_stmt => {
                const extra_start = node.data.lhs;
                const extra_end = node.data.rhs;

                const cond_idx = self.ast_tree.extra_data[extra_start];
                const condition_type = try self.analyzeNode(cond_idx, scope);
                if (!isPrimitive(self.type_pool.get(condition_type), .bool_type)) {
                    try self.reportError(4001, .sema, self.ast_tree.tokens[self.ast_tree.nodes.get(cond_idx).main_token].start, "if condition must have type bool");
                }

                const then_idx = self.ast_tree.extra_data[extra_start + 1];
                const then_type = try self.analyzeNode(then_idx, scope);

                var else_type = try self.type_pool.internPrimitive(.void_type);
                if (extra_end > extra_start + 2) {
                    const else_idx = self.ast_tree.extra_data[extra_start + 2];
                    else_type = try self.analyzeNode(else_idx, scope);
                }

                const has_else = extra_end > extra_start + 2;
                var result_type = try self.type_pool.internPrimitive(.void_type);
                if (has_else) {
                    const then_value = self.type_pool.get(then_type);
                    const else_value = self.type_pool.get(else_type);
                    if (isPrimitive(then_value, .noreturn_type)) {
                        result_type = else_type;
                    } else if (isPrimitive(else_value, .noreturn_type)) {
                        result_type = then_type;
                    } else if (Type.isCoercible(then_value, else_value)) {
                        result_type = else_type;
                    } else if (Type.isCoercible(else_value, then_value)) {
                        result_type = then_type;
                    } else {
                        try self.reportError(4001, .sema, self.ast_tree.tokens[node.main_token].start, "if and else branches have incompatible types");
                        result_type = then_type;
                    }
                }
                try self.node_types.put(node_idx, result_type);
                return result_type;
            },
            .while_stmt => {
                const extra_start = node.data.lhs;
                const label_token = self.ast_tree.extra_data[extra_start];
                const cond = self.ast_tree.extra_data[extra_start + 1];
                const body = self.ast_tree.extra_data[extra_start + 2];

                const condition_type = try self.analyzeNode(cond, scope);
                if (!isPrimitive(self.type_pool.get(condition_type), .bool_type)) {
                    try self.reportError(4001, .sema, self.ast_tree.tokens[self.ast_tree.nodes.get(cond).main_token].start, "while condition must have type bool");
                }
                try self.loop_stack.append(self.allocator, .{ .label_token = label_token });
                _ = try self.analyzeNode(body, scope);
                const loop = self.loop_stack.pop().?;

                const void_type = try self.type_pool.internPrimitive(.void_type);
                var result_type = loop.break_type orelse void_type;
                if (!isPrimitive(self.type_pool.get(result_type), .void_type) and
                    self.const_values.get(cond) != 1)
                {
                    try self.reportError(4001, .sema, self.ast_tree.tokens[node.main_token].start, "A value-producing while loop must have a statically true condition");
                    result_type = void_type;
                }
                try self.node_types.put(node_idx, result_type);
                return result_type;
            },
            .for_stmt => {
                const extra_start = node.data.lhs;
                const label_token = self.ast_tree.extra_data[extra_start];
                const capture_flags = self.ast_tree.extra_data[extra_start + 1];
                const item_token = self.ast_tree.extra_data[extra_start + 2];
                const index_token = self.ast_tree.extra_data[extra_start + 3];
                const iterable_node = self.ast_tree.extra_data[extra_start + 4];
                const body_node = self.ast_tree.extra_data[extra_start + 5];
                const iterable = self.ast_tree.nodes.get(iterable_node);
                var item_type: Type.Id = undefined;
                if (iterable.tag == .range) {
                    const start_type = try self.analyzeNode(iterable.data.lhs, scope);
                    const end_type = try self.analyzeNode(iterable.data.rhs, scope);
                    const start_value = self.type_pool.get(start_type);
                    const end_value = self.type_pool.get(end_type);
                    if (!start_value.isInteger() or !end_value.isInteger() or
                        (!Type.isCoercible(start_value, end_value) and !Type.isCoercible(end_value, start_value)))
                    {
                        try self.reportError(4001, .sema, self.ast_tree.tokens[iterable.main_token].start, "for range bounds must have compatible integer types");
                    }
                    try self.node_types.put(iterable_node, start_type);
                    item_type = start_type;
                    if ((capture_flags & 1) != 0) {
                        try self.reportError(4001, .sema, self.ast_tree.tokens[item_token].start, "A range item has no address and cannot use pointer capture");
                    }
                } else {
                    const iterable_type_id = try self.analyzeNode(iterable_node, scope);
                    const iterable_type = self.type_pool.get(iterable_type_id);
                    const child_type: Type.Id = switch (iterable_type.data) {
                        .array => |array| array.child_type,
                        .pointer => |pointer| if (pointer.size == .Slice)
                            pointer.child_type
                        else blk: {
                            try self.reportError(4001, .sema, self.ast_tree.tokens[node.main_token].start, "for iterable must be an array, slice, or range");
                            break :blk try self.type_pool.internPrimitive(.void_type);
                        },
                        else => blk: {
                            try self.reportError(4001, .sema, self.ast_tree.tokens[node.main_token].start, "for iterable must be an array, slice, or range");
                            break :blk try self.type_pool.internPrimitive(.void_type);
                        },
                    };
                    if ((capture_flags & 1) != 0) {
                        var is_const_storage = switch (iterable_type.data) {
                            .pointer => |pointer| pointer.is_const,
                            else => false,
                        };
                        if (iterable.tag == .identifier) {
                            const iterable_token = self.ast_tree.tokens[iterable.main_token];
                            const source = self.diags.source_manager.getFile(0).?.content;
                            if (scope.get(source[iterable_token.start..iterable_token.end])) |symbol| {
                                is_const_storage = is_const_storage or symbol.is_const;
                            }
                        }
                        if (is_const_storage) {
                            try self.reportError(4001, .sema, self.ast_tree.tokens[item_token].start, "Pointer capture requires a mutable iterable");
                        }
                        item_type = try self.type_pool.internPtr(child_type, false);
                    } else {
                        item_type = child_type;
                    }
                }

                var loop_scope = Scope.init(self.allocator, scope);
                defer loop_scope.deinit();
                const src = self.diags.source_manager.getFile(0).?.content;
                const item_tok = self.ast_tree.tokens[item_token];
                try loop_scope.put(src[item_tok.start..item_tok.end], .{
                    .name = src[item_tok.start..item_tok.end],
                    .decl_node = node_idx,
                    .type_id = item_type,
                    .is_const = true,
                });
                if (index_token != std.math.maxInt(u32)) {
                    const index_tok = self.ast_tree.tokens[index_token];
                    const index_type = try self.type_pool.internSizeInt(false);
                    try loop_scope.put(src[index_tok.start..index_tok.end], .{
                        .name = src[index_tok.start..index_tok.end],
                        .decl_node = node_idx,
                        .type_id = index_type,
                        .is_const = true,
                    });
                }

                try self.loop_stack.append(self.allocator, .{ .label_token = label_token });
                _ = try self.analyzeNode(body_node, &loop_scope);
                const loop = self.loop_stack.pop().?;

                const void_type = try self.type_pool.internPrimitive(.void_type);
                if (loop.break_type) |break_type| {
                    if (!isPrimitive(self.type_pool.get(break_type), .void_type)) {
                        try self.reportError(4001, .sema, self.ast_tree.tokens[node.main_token].start, "A finite for loop cannot produce a value because it may end without break");
                    }
                }
                try self.node_types.put(node_idx, void_type);
                return void_type;
            },
            .break_stmt, .continue_stmt => {
                if (self.loop_stack.items.len == 0) {
                    try self.reportError(4001, .sema, self.ast_tree.tokens[node.main_token].start, "Loop control statement used outside a loop");
                } else {
                    const target_index = self.findLoopTarget(node.data.lhs) orelse {
                        try self.reportError(4001, .sema, self.ast_tree.tokens[node.main_token].start, "Unknown loop label");
                        const noreturn_type = try self.type_pool.internPrimitive(.noreturn_type);
                        try self.node_types.put(node_idx, noreturn_type);
                        return noreturn_type;
                    };
                    if (node.tag == .break_stmt) {
                        const break_type = if (node.data.rhs == std.math.maxInt(u32))
                            try self.type_pool.internPrimitive(.void_type)
                        else
                            try self.analyzeNode(node.data.rhs, scope);
                        if (self.loop_stack.items[target_index].break_type) |previous_type| {
                            const previous = self.type_pool.get(previous_type);
                            const current = self.type_pool.get(break_type);
                            if (Type.isCoercible(current, previous)) {
                                // Keep the wider/established result type.
                            } else if (Type.isCoercible(previous, current)) {
                                self.loop_stack.items[target_index].break_type = break_type;
                            } else {
                                try self.reportError(4001, .sema, self.ast_tree.tokens[node.main_token].start, "Break values for the same loop have incompatible types");
                            }
                        } else {
                            self.loop_stack.items[target_index].break_type = break_type;
                        }
                    }
                }
                const noreturn_type = try self.type_pool.internPrimitive(.noreturn_type);
                try self.node_types.put(node_idx, noreturn_type);
                return noreturn_type;
            },
            .fn_proto => {
                const extra_start = node.data.lhs;
                const extra_len = node.data.rhs;

                const ret_type_node = self.ast_tree.extra_data[extra_start];
                const ret_type = try self.resolveBuiltinTypeArg(ret_type_node, scope) orelse {
                    try self.reportError(4001, .sema, self.ast_tree.tokens[node.main_token].start, "Function return type could not be resolved");
                    return self.type_pool.internPrimitive(.void_type);
                };

                var param_types = std.ArrayList(Type.Id).empty;
                defer param_types.deinit(self.allocator);
                var i: u32 = 1;
                while (i < extra_len) : (i += 1) {
                    const param_idx = self.ast_tree.extra_data[extra_start + i];
                    const param_node = self.ast_tree.nodes.get(param_idx);

                    const param_type_node = param_node.data.rhs;
                    const param_type = try self.resolveBuiltinTypeArg(param_type_node, scope) orelse {
                        try self.reportError(4001, .sema, self.ast_tree.tokens[param_node.main_token].start, "Function parameter type could not be resolved");
                        continue;
                    };
                    try param_types.append(self.allocator, param_type);
                    try self.node_types.put(param_idx, param_type);
                }

                const params_start = try self.type_pool.appendParams(param_types.items);
                const fn_type = try self.type_pool.intern(.{ .function = .{
                    .ret_type = ret_type,
                    .params_start = params_start,
                    .params_len = @intCast(param_types.items.len),
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

                const fn_type = try self.declareFunction(node_idx, scope);
                try self.bindFunctionParameters(proto, fn_type, &child_scope);
                const proto_node = self.ast_tree.nodes.get(proto);
                const name_tok_idx = proto_node.main_token;
                const fn_tok = self.ast_tree.tokens[name_tok_idx];

                const function = self.type_pool.get(fn_type).data.function;
                const previous_return_type = self.current_return_type;
                const previous_loop_depth = self.loop_stack.items.len;
                self.current_return_type = function.ret_type;
                self.loop_stack.clearRetainingCapacity();
                defer self.current_return_type = previous_return_type;
                defer self.loop_stack.items.len = previous_loop_depth;

                _ = try self.analyzeNode(body, &child_scope);

                const return_type = self.type_pool.get(function.ret_type);
                const may_fall_through = !self.nodeDefinitelyReturns(body);
                if (may_fall_through and !self.allowsSuccessfulFallthrough(function.ret_type)) {
                    const code: u32 = if (isPrimitive(return_type, .noreturn_type)) 4010 else 4004;
                    const message = if (code == 4010)
                        "noreturn function has a returning path"
                    else
                        "Function can reach the end without returning a value";
                    try self.reportError(code, .sema, fn_tok.start, message);
                }

                try self.node_types.put(node_idx, fn_type);
                return fn_type;
            },
            .call => {
                const target = node.data.lhs;
                const extra_start = node.data.rhs;

                const target_type_id = try self.analyzeNode(target, scope);

                const num_args = self.ast_tree.extra_data[extra_start];
                const target_type = self.type_pool.get(target_type_id);
                if (target_type.data != .function) {
                    var i: u32 = 0;
                    while (i < num_args) : (i += 1) {
                        _ = try self.analyzeNode(self.ast_tree.extra_data[extra_start + 1 + i], scope);
                    }
                    try self.reportError(4001, .sema, self.ast_tree.tokens[node.main_token].start, "Called expression is not a function");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
                }

                const function = target_type.data.function;
                const params = self.type_pool.functionParams(function);
                if (num_args != params.len) {
                    try self.reportError(4001, .sema, self.ast_tree.tokens[node.main_token].start, "Function argument count does not match its declaration");
                }
                var i: u32 = 0;
                while (i < num_args) : (i += 1) {
                    const arg_node = self.ast_tree.extra_data[extra_start + 1 + i];
                    const arg_type = try self.analyzeNode(arg_node, scope);
                    if (i < params.len) {
                        const parameter_type = self.type_pool.get(params[i]);
                        const accepts_anytype = isPrimitive(parameter_type, .anytype_type);
                        if (!accepts_anytype and !Type.isCoercible(self.type_pool.get(arg_type), parameter_type)) {
                            try self.reportError(4001, .sema, self.ast_tree.tokens[self.ast_tree.nodes.get(arg_node).main_token].start, "Function argument type does not match parameter type");
                        }
                    }
                }

                try self.node_types.put(node_idx, function.ret_type);
                return function.ret_type;
            },
            .return_stmt => {
                const expected_type = self.current_return_type orelse {
                    try self.reportError(4001, .sema, self.ast_tree.tokens[node.main_token].start, "return used outside a function");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.noreturn_type));
                };
                const expected = self.type_pool.get(expected_type);
                if (node.data.rhs == std.math.maxInt(u32)) {
                    const accepts_empty_return = isPrimitive(expected, .void_type) or
                        (expected.data == .error_union and
                            isPrimitive(self.type_pool.get(expected.data.error_union.payload), .void_type));
                    if (!accepts_empty_return) {
                        try self.reportError(4001, .sema, self.ast_tree.tokens[node.main_token].start, "Return value required by function return type");
                    }
                } else {
                    const actual_type = try self.analyzeNode(node.data.rhs, scope);
                    const actual = self.type_pool.get(actual_type);
                    const return_matches = if (expected.data == .error_union)
                        if (actual.data == .error_union)
                            Type.isCoercible(actual, expected)
                        else
                            Type.isCoercible(actual, self.type_pool.get(expected.data.error_union.payload))
                    else
                        !isPrimitive(expected, .void_type) and Type.isCoercible(actual, expected);
                    if (!return_matches) {
                        try self.reportError(4001, .sema, self.ast_tree.tokens[node.main_token].start, "Returned expression does not match function return type");
                    }
                }
                const noreturn_type = try self.type_pool.internPrimitive(.noreturn_type);
                try self.node_types.put(node_idx, noreturn_type);
                return noreturn_type;
            },
            else => {
                // Return a dummy type
                return 0;
            },
        }
    }

    fn analyzeBuiltin(self: *Sema, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
        const node = self.ast_tree.nodes.get(node_idx);
        const src = self.diags.source_manager.getFile(0).?.content;
        const name_token = self.ast_tree.tokens[node.data.lhs];
        const name = src[name_token.start..name_token.end];
        const kind = builtin.lookup(name) orelse {
            try self.reportError(3001, .resolve, name_token.start, "Unknown builtin");
            return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
        };

        const extra_start = node.data.rhs;
        const arg_count = self.ast_tree.extra_data[extra_start];
        const args = self.ast_tree.extra_data[extra_start + 1 .. extra_start + 1 + arg_count];
        if (builtin.arity(kind)) |expected| {
            if (!expected.accepts(args.len)) {
                try self.reportError(5005, .@"comptime", name_token.start, "Invalid builtin argument count");
                return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
            }
        }

        switch (kind) {
            .typeOf => {
                const value_type = try self.analyzeNode(args[0], scope);
                try self.type_values.put(node_idx, value_type);
                return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
            },
            .nocopy => {
                const wrapped = try self.resolveBuiltinTypeArg(args[0], scope) orelse {
                    try self.reportError(5005, .@"comptime", name_token.start, "@nocopy requires a type argument");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
                };
                const wrapped_type = self.type_pool.get(wrapped);
                const nocopy_type = try self.type_pool.intern(wrapped_type.data, .explicit_nocopy);
                try self.type_values.put(node_idx, nocopy_type);
                return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
            },
            .move => {
                const arg_node = self.ast_tree.nodes.get(args[0]);
                const value_type = try self.analyzeNode(args[0], scope);
                if (arg_node.tag != .identifier and arg_node.tag != .field_access) {
                    try self.reportError(6003, .sema, name_token.start, "@move source must be addressable storage");
                    return self.putBuiltinResult(node_idx, value_type);
                }
                if (arg_node.tag == .identifier) {
                    const token = self.ast_tree.tokens[arg_node.main_token];
                    const value_name = src[token.start..token.end];
                    if (scope.get(value_name)) |symbol| {
                        try self.local_states.put(symbol.decl_node, .moved);
                    }
                }
                if (self.type_pool.get(value_type).isCopyable()) {
                    try self.reportWarning(6001, .sema, name_token.start, "Redundant move of a copyable value");
                }
                return self.putBuiltinResult(node_idx, value_type);
            },
            .sizeOf, .alignOf, .bitSizeOf => {
                const type_value = try self.resolveBuiltinTypeArg(args[0], scope) orelse {
                    try self.reportError(5005, .@"comptime", name_token.start, "Layout builtin requires a type argument");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
                };
                const value = switch (kind) {
                    .sizeOf => self.type_pool.sizeOf(type_value) catch {
                        try self.reportError(5005, .@"comptime", name_token.start, "Type has no runtime size");
                        return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
                    },
                    .alignOf => self.type_pool.alignOf(type_value) catch {
                        try self.reportError(5005, .@"comptime", name_token.start, "Type has no runtime alignment");
                        return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
                    },
                    .bitSizeOf => self.type_pool.bitSizeOf(type_value) catch {
                        try self.reportError(5005, .@"comptime", name_token.start, "Type has no runtime bit size");
                        return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
                    },
                    else => unreachable,
                };
                try self.const_values.put(node_idx, value);
                return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
            },
            .isInteger,
            .isFloat,
            .isStruct,
            .isEnum,
            .isUnion,
            .isPointer,
            .isSlice,
            .isArray,
            .isOptional,
            .isErrorUnion,
            .isCopyable,
            => {
                const type_value = try self.resolveBuiltinTypeArg(args[0], scope) orelse {
                    try self.reportError(5005, .@"comptime", name_token.start, "Type predicate requires a type argument");
                    return self.putBoolBuiltinResult(node_idx, false);
                };
                const ty = self.type_pool.get(type_value);
                const result = switch (kind) {
                    .isInteger => ty.isInteger(),
                    .isFloat => ty.isFloat(),
                    .isStruct => ty.data == .@"struct",
                    .isEnum => ty.data == .@"enum",
                    .isUnion => ty.data == .@"union",
                    .isPointer => ty.data == .pointer and ty.data.pointer.size != .Slice,
                    .isSlice => ty.data == .pointer and ty.data.pointer.size == .Slice,
                    .isArray => ty.data == .array,
                    .isOptional => ty.data == .optional,
                    .isErrorUnion => ty.data == .error_union,
                    .isCopyable => ty.isCopyable(),
                    else => unreachable,
                };
                return self.putBoolBuiltinResult(node_idx, result);
            },
            .Vector => {
                _ = try self.analyzeNode(args[0], scope);
                const len = self.const_values.get(args[0]) orelse {
                    try self.reportError(5001, .@"comptime", name_token.start, "@Vector length must be comptime-known");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
                };
                if (len == 0 or len > std.math.maxInt(u32)) {
                    try self.reportError(4002, .sema, name_token.start, "Invalid vector length");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
                }
                const child_type = try self.resolveBuiltinTypeArg(args[1], scope) orelse {
                    try self.reportError(5005, .@"comptime", name_token.start, "@Vector element must be a type");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
                };
                const child = self.type_pool.get(child_type);
                if (!child.isInteger() and !child.isFloat() and !(child.data == .primitive and child.data.primitive == .bool_type)) {
                    try self.reportError(4001, .sema, name_token.start, "Vector element type must be integer, float, or bool");
                }
                const vector_type = try self.type_pool.intern(.{ .vector = .{ .len = @intCast(len), .child_type = child_type } }, .copyable);
                try self.type_values.put(node_idx, vector_type);
                return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
            },
            .intCast,
            .floatCast,
            .floatFromInt,
            .intFromFloat,
            .ptrCast,
            .alignCast,
            .bitCast,
            .ptrFromInt,
            .enumFromInt,
            => return self.analyzeCastBuiltin(kind, node_idx, args, scope, name_token.start),
            .intFromPtr => {
                const source_type = try self.analyzeNode(args[0], scope);
                if (self.type_pool.get(source_type).data != .pointer) {
                    try self.reportError(4003, .sema, name_token.start, "@intFromPtr requires a pointer");
                }
                return self.putBuiltinResult(node_idx, try self.type_pool.internSizeInt(false));
            },
            .intFromEnum => {
                const source_type = try self.analyzeNode(args[0], scope);
                if (self.type_pool.get(source_type).data != .@"enum") {
                    try self.reportError(4003, .sema, name_token.start, "@intFromEnum requires an enum value");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internSizeInt(false));
                }
                const backing = (self.type_pool.aggregateInfo(source_type) orelse unreachable).backing_type orelse unreachable;
                return self.putBuiltinResult(node_idx, backing);
            },
            .discardError => {
                const value_type = try self.analyzeNode(args[0], scope);
                if (self.type_pool.get(value_type).data != .error_union) {
                    try self.reportError(4001, .sema, name_token.start, "@discardError requires an error union");
                }
                return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
            },
            .compileError => {
                const message = self.stringLiteralContent(args[0]) orelse "@compileError requires a string literal";
                try self.reportError(5003, .@"comptime", self.ast_tree.tokens[node.main_token].start, message);
                return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.noreturn_type));
            },
            .setEvalBranchQuota => {
                _ = try self.analyzeNode(args[0], scope);
                self.eval_branch_quota = self.const_values.get(args[0]) orelse {
                    try self.reportError(5001, .@"comptime", name_token.start, "Evaluation quota must be comptime-known");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
                };
                return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
            },
            .import => {
                if (self.stringLiteralContent(args[0]) == null) {
                    try self.reportError(5001, .@"comptime", name_token.start, "@import path must be a comptime string literal");
                }
                return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.anyopaque_type));
            },
            .fieldCount => {
                const aggregate_type = try self.resolveBuiltinTypeArg(args[0], scope) orelse {
                    try self.reportError(5005, .@"comptime", name_token.start, "@fieldCount requires a type argument");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
                };
                const info = self.type_pool.aggregateInfo(aggregate_type) orelse {
                    try self.reportError(5005, .@"comptime", name_token.start, "@fieldCount requires an aggregate type");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
                };
                try self.const_values.put(node_idx, info.fields_len);
                return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
            },
            .fieldType => {
                const aggregate_type = try self.resolveBuiltinTypeArg(args[0], scope) orelse {
                    try self.reportError(5005, .@"comptime", name_token.start, "@fieldType requires an aggregate type");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
                };
                const field_name = self.stringLiteralContent(args[1]) orelse {
                    try self.reportError(5001, .@"comptime", name_token.start, "@fieldType field name must be a comptime string");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
                };
                const field = self.type_pool.aggregateField(aggregate_type, field_name) orelse {
                    try self.reportError(5005, .@"comptime", name_token.start, "Unknown aggregate field");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
                };
                try self.type_values.put(node_idx, field.type_id);
                return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
            },
            .hasField => {
                const aggregate_type = try self.resolveBuiltinTypeArg(args[0], scope) orelse {
                    try self.reportError(5005, .@"comptime", name_token.start, "@hasField requires an aggregate type");
                    return self.putBoolBuiltinResult(node_idx, false);
                };
                const field_name = self.stringLiteralContent(args[1]) orelse {
                    try self.reportError(5001, .@"comptime", name_token.start, "@hasField field name must be a comptime string");
                    return self.putBoolBuiltinResult(node_idx, false);
                };
                return self.putBoolBuiltinResult(node_idx, self.type_pool.aggregateField(aggregate_type, field_name) != null);
            },
            .offsetOf => {
                const aggregate_type = try self.resolveBuiltinTypeArg(args[0], scope) orelse {
                    try self.reportError(5005, .@"comptime", name_token.start, "@offsetOf requires an aggregate type");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
                };
                const field_name = self.stringLiteralContent(args[1]) orelse {
                    try self.reportError(5001, .@"comptime", name_token.start, "@offsetOf field name must be a comptime string");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
                };
                const field = self.type_pool.aggregateField(aggregate_type, field_name) orelse {
                    try self.reportError(5005, .@"comptime", name_token.start, "Unknown aggregate field");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
                };
                try self.const_values.put(node_idx, field.offset);
                return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
            },
            .field => {
                const aggregate_type = try self.analyzeNode(args[0], scope);
                const field_name = self.stringLiteralContent(args[1]) orelse {
                    try self.reportError(5001, .@"comptime", name_token.start, "@field field name must be a comptime string");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
                };
                const field = self.type_pool.aggregateField(aggregate_type, field_name) orelse {
                    try self.reportError(5005, .@"comptime", name_token.start, "Unknown aggregate field");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
                };
                try self.reportError(9001, .lowering, name_token.start, "Dynamic aggregate field lowering is not available in Stage 0 yet");
                return self.putBuiltinResult(node_idx, field.type_id);
            },
            .tagOf => {
                const value_type = try self.analyzeNode(args[0], scope);
                const info = self.type_pool.aggregateInfo(value_type) orelse {
                    try self.reportError(5005, .@"comptime", name_token.start, "@tagOf requires an enum or tagged union value");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
                };
                const backing = info.backing_type orelse {
                    try self.reportError(5005, .@"comptime", name_token.start, "@tagOf requires an enum or tagged union value");
                    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
                };
                return self.putBuiltinResult(node_idx, backing);
            },
            .typeInfo,
            .hasDecl,
            .decl,
            .languageVersion,
            => {
                try self.reportError(5005, .@"comptime", name_token.start, "Reflection builtin requires aggregate/module metadata not implemented in Stage 0 yet");
                return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.anyopaque_type));
            },
            .atomicLoad,
            .atomicRmw,
            .atomicStore,
            .cmpxchgStrong,
            .cmpxchgWeak,
            .fence,
            .splat,
            .shuffle,
            .reduce,
            .select,
            => {
                for (args) |arg| _ = try self.analyzeNode(arg, scope);
                try self.reportError(9001, .lowering, name_token.start, "Builtin lowering is not available in the Stage-0 backend yet");
                return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
            },
        }
    }

    fn nodeDefinitelyReturns(self: *const Sema, node_idx: Node.Index) bool {
        const node = self.ast_tree.nodes.get(node_idx);
        return switch (node.tag) {
            .return_stmt => true,
            .block => blk: {
                var index = node.data.lhs;
                while (index < node.data.rhs) : (index += 1) {
                    if (self.nodeDefinitelyReturns(self.ast_tree.extra_data[index])) break :blk true;
                }
                break :blk false;
            },
            .if_stmt => blk: {
                const start = node.data.lhs;
                if (node.data.rhs < start + 3) break :blk false;
                break :blk self.nodeDefinitelyReturns(self.ast_tree.extra_data[start + 1]) and
                    self.nodeDefinitelyReturns(self.ast_tree.extra_data[start + 2]);
            },
            .unsafe_block => self.nodeDefinitelyReturns(node.data.lhs),
            else => false,
        };
    }

    fn allowsSuccessfulFallthrough(self: *const Sema, return_type_id: Type.Id) bool {
        const return_type = self.type_pool.get(return_type_id);
        if (isPrimitive(return_type, .void_type)) return true;
        if (return_type.data != .error_union) return false;
        return isPrimitive(self.type_pool.get(return_type.data.error_union.payload), .void_type);
    }

    fn isDiscardedExpression(self: *const Sema, node_idx: Node.Index) bool {
        return switch (self.ast_tree.nodes.get(node_idx).tag) {
            .binary_op,
            .unary_op,
            .call,
            .field_access,
            .array_access,
            .slice,
            .identifier,
            .integer_literal,
            .float_literal,
            .string_literal,
            .char_literal,
            .bool_literal,
            .null_literal,
            .undefined_literal,
            .tuple_literal,
            .array_literal,
            .builtin_call,
            .if_stmt,
            .match_stmt,
            .unsafe_block,
            => true,
            else => false,
        };
    }

    fn findLoopTarget(self: *const Sema, label_token: u32) ?usize {
        if (self.loop_stack.items.len == 0) return null;
        if (label_token == std.math.maxInt(u32)) return self.loop_stack.items.len - 1;

        const source = self.diags.source_manager.getFile(0).?.content;
        const wanted_token = self.ast_tree.tokens[label_token];
        const wanted = source[wanted_token.start..wanted_token.end];
        var index = self.loop_stack.items.len;
        while (index > 0) {
            index -= 1;
            const candidate_token = self.loop_stack.items[index].label_token;
            if (candidate_token == std.math.maxInt(u32)) continue;
            const candidate = self.ast_tree.tokens[candidate_token];
            if (std.mem.eql(u8, wanted, source[candidate.start..candidate.end])) return index;
        }
        return null;
    }

    fn findRootFunction(self: *const Sema, name: []const u8) ?Node.Index {
        const root = self.ast_tree.nodes.get(self.ast_tree.nodes.len - 1);
        if (root.tag != .root) return null;
        const src = self.diags.source_manager.getFile(0).?.content;
        var index = root.data.lhs;
        while (index < root.data.rhs) : (index += 1) {
            const declaration_index = self.ast_tree.extra_data[index];
            const declaration = self.ast_tree.nodes.get(declaration_index);
            if (declaration.tag != .fn_decl) continue;
            const prototype = self.ast_tree.nodes.get(declaration.data.lhs);
            const token = self.ast_tree.tokens[prototype.main_token];
            if (std.mem.eql(u8, src[token.start..token.end], name)) return declaration_index;
        }
        return null;
    }

    fn declareFunction(self: *Sema, declaration_index: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
        const declaration = self.ast_tree.nodes.get(declaration_index);
        const prototype_index = declaration.data.lhs;
        const prototype = self.ast_tree.nodes.get(prototype_index);
        const function_type = self.node_types.get(prototype_index) orelse try self.analyzeNode(prototype_index, scope);
        const token = self.ast_tree.tokens[prototype.main_token];
        const src = self.diags.source_manager.getFile(0).?.content;
        const name = src[token.start..token.end];
        try scope.put(name, .{
            .name = name,
            .decl_node = declaration_index,
            .type_id = function_type,
            .is_const = true,
        });
        try self.node_types.put(declaration_index, function_type);
        return function_type;
    }

    fn bindFunctionParameters(self: *Sema, prototype_index: Node.Index, function_type: Type.Id, scope: *Scope) std.mem.Allocator.Error!void {
        const prototype = self.ast_tree.nodes.get(prototype_index);
        const function = self.type_pool.get(function_type).data.function;
        const parameter_types = self.type_pool.functionParams(function);
        const src = self.diags.source_manager.getFile(0).?.content;
        var parameter_offset: u32 = 0;
        while (parameter_offset < parameter_types.len) : (parameter_offset += 1) {
            const parameter_index = self.ast_tree.extra_data[prototype.data.lhs + 1 + parameter_offset];
            const parameter = self.ast_tree.nodes.get(parameter_index);
            const token = self.ast_tree.tokens[parameter.main_token];
            const name = src[token.start..token.end];
            try scope.put(name, .{
                .name = name,
                .decl_node = parameter_index,
                .type_id = parameter_types[parameter_offset],
                .is_const = true,
            });
            try self.local_states.put(parameter_index, .initialized);
            try self.node_types.put(parameter_index, parameter_types[parameter_offset]);
        }
    }

    fn sourceIsMoved(self: *Sema, node_idx: Node.Index, scope: *Scope) bool {
        const node = self.ast_tree.nodes.get(node_idx);
        if (node.tag != .identifier) return false;
        const token = self.ast_tree.tokens[node.main_token];
        const src = self.diags.source_manager.getFile(0).?.content;
        const symbol = scope.get(src[token.start..token.end]) orelse return false;
        return (self.local_states.get(symbol.decl_node) orelse return false) == .moved;
    }

    fn analyzeAggregateType(self: *Sema, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
        if (self.type_values.get(node_idx)) |_| return self.type_pool.internPrimitive(.type_type);

        const node = self.ast_tree.nodes.get(node_idx);
        const kind: TypePool.AggregateKind = switch (node.tag) {
            .struct_decl => .@"struct",
            .enum_decl => .@"enum",
            .union_decl => .@"union",
            else => unreachable,
        };
        const backing_node = self.ast_tree.extra_data[node.data.lhs];
        var backing_type: ?Type.Id = null;
        if (backing_node != std.math.maxInt(u32)) {
            backing_type = try self.resolveBuiltinTypeArg(backing_node, scope) orelse {
                try self.reportError(5005, .@"comptime", self.ast_tree.tokens[node.main_token].start, "Aggregate backing/tag must be a type");
                return self.type_pool.internPrimitive(.type_type);
            };
            if (kind == .@"enum" and !self.type_pool.get(backing_type.?).isInteger()) {
                try self.reportError(4001, .sema, self.ast_tree.tokens[node.main_token].start, "Enum backing type must be an integer");
            }
        }

        var fields = std.ArrayList(TypePool.AggregateFieldInput).empty;
        defer fields.deinit(self.allocator);
        const src = self.diags.source_manager.getFile(0).?.content;
        var extra_index = node.data.lhs + 1;
        while (extra_index < node.data.rhs) : (extra_index += 1) {
            const member_index = self.ast_tree.extra_data[extra_index];
            const member = self.ast_tree.nodes.get(member_index);
            const name_token = self.ast_tree.tokens[member.main_token];
            const member_type: Type.Id = switch (member.tag) {
                .field_decl => try self.resolveBuiltinTypeArg(member.data.lhs, scope) orelse {
                    try self.reportError(5005, .@"comptime", name_token.start, "Struct field type could not be resolved");
                    continue;
                },
                .union_member => if (member.data.lhs == std.math.maxInt(u32))
                    try self.type_pool.internPrimitive(.void_type)
                else
                    try self.resolveBuiltinTypeArg(member.data.lhs, scope) orelse {
                        try self.reportError(5005, .@"comptime", name_token.start, "Union member type could not be resolved");
                        continue;
                    },
                .enum_member => backing_type orelse {
                    try self.reportError(5005, .@"comptime", name_token.start, "Enum has no backing type");
                    continue;
                },
                else => continue,
            };
            if (member.data.rhs != std.math.maxInt(u32)) _ = try self.analyzeNode(member.data.rhs, scope);
            try fields.append(self.allocator, .{
                .name = src[name_token.start..name_token.end],
                .type_id = member_type,
            });
        }

        const aggregate_type = self.type_pool.internAggregate(kind, fields.items, backing_type, null) catch {
            try self.reportError(5005, .@"comptime", self.ast_tree.tokens[node.main_token].start, "Aggregate layout could not be computed");
            return self.type_pool.internPrimitive(.type_type);
        };
        try self.type_values.put(node_idx, aggregate_type);
        const result_type = try self.type_pool.internPrimitive(.type_type);
        try self.node_types.put(node_idx, result_type);
        return result_type;
    }

    fn analyzeCastBuiltin(
        self: *Sema,
        kind: builtin.Kind,
        node_idx: Node.Index,
        args: []const u32,
        scope: *Scope,
        start_byte: u32,
    ) std.mem.Allocator.Error!Type.Id {
        const target_type = try self.resolveBuiltinTypeArg(args[0], scope) orelse {
            try self.reportError(4003, .sema, start_byte, "Cast target must be a type");
            return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
        };
        const source_type = try self.analyzeNode(args[1], scope);
        const target = self.type_pool.get(target_type);
        const source = self.type_pool.get(source_type);

        const valid = switch (kind) {
            .intCast => target.isInteger() and source.isInteger(),
            .floatCast => target.isFloat() and source.isFloat(),
            .floatFromInt => target.isFloat() and source.isInteger(),
            .intFromFloat => target.isInteger() and source.isFloat(),
            .ptrCast, .alignCast => target.data == .pointer and source.data == .pointer,
            .bitCast => blk: {
                const target_bits = self.type_pool.bitSizeOf(target_type) catch break :blk false;
                const source_bits = self.type_pool.bitSizeOf(source_type) catch break :blk false;
                break :blk target_bits == source_bits;
            },
            .ptrFromInt => target.data == .pointer and source.isInteger(),
            .enumFromInt => target.data == .@"enum" and source.isInteger(),
            else => false,
        };
        if (!valid) try self.reportError(4003, .sema, start_byte, "Invalid cast operands");
        // A same-width bitCast is representation-only and is lowered as an
        // identity vreg operation, including GP-held floating bit patterns.
        const requires_float_lowering = kind == .floatCast or kind == .floatFromInt or kind == .intFromFloat;
        if (valid and requires_float_lowering) {
            try self.reportError(9001, .lowering, start_byte, "Floating-point builtin lowering is not available in Stage 0 yet");
        }
        if (valid and kind == .intCast) {
            if (self.const_values.get(args[1])) |value| {
                if (!integerValueFits(target, value)) {
                    try self.reportError(4002, .sema, start_byte, "Integer value is out of range for cast target");
                } else {
                    try self.const_values.put(node_idx, value);
                }
            }
        } else if (valid and (kind == .bitCast or kind == .ptrFromInt or kind == .enumFromInt)) {
            if (self.const_values.get(args[1])) |value| try self.const_values.put(node_idx, value);
        }
        if ((kind == .ptrFromInt or kind == .ptrCast) and self.unsafe_depth == 0) {
            try self.reportError(7004, .sema, start_byte, "Pointer provenance cast requires unsafe block");
        }
        return self.putBuiltinResult(node_idx, target_type);
    }

    fn integerValueFits(target: Type, value: u64) bool {
        return switch (target.data) {
            .integer => |int| if (int.is_signed)
                int.bits > 64 or value <= (@as(u64, 1) << @intCast(int.bits - 1)) - 1
            else
                int.bits >= 64 or value <= (@as(u64, 1) << @intCast(int.bits)) - 1,
            .size_int => true,
            else => false,
        };
    }

    fn resolveBuiltinTypeArg(self: *Sema, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!?Type.Id {
        const node = self.ast_tree.nodes.get(node_idx);
        switch (node.tag) {
            .identifier => {
                const token = self.ast_tree.tokens[node.main_token];
                const src = self.diags.source_manager.getFile(0).?.content;
                if (scope.get(src[token.start..token.end])) |symbol| {
                    if (self.type_values.get(symbol.decl_node)) |type_value| return type_value;
                }
                return try self.resolveTypeExpr(node_idx);
            },
            .pointer_type, .slice_type, .array_type, .optional_type, .error_union_type => {
                return try self.resolveTypeExpr(node_idx);
            },
            .struct_decl, .enum_decl, .union_decl => {
                _ = try self.analyzeAggregateType(node_idx, scope);
                return self.type_values.get(node_idx);
            },
            .builtin_call => {
                _ = try self.analyzeNode(node_idx, scope);
                return self.type_values.get(node_idx);
            },

            else => return null,
        }
    }

    fn putBuiltinResult(self: *Sema, node_idx: Node.Index, type_id: Type.Id) std.mem.Allocator.Error!Type.Id {
        try self.node_types.put(node_idx, type_id);
        return type_id;
    }

    fn putBoolBuiltinResult(self: *Sema, node_idx: Node.Index, value: bool) std.mem.Allocator.Error!Type.Id {
        try self.const_values.put(node_idx, @intFromBool(value));
        return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.bool_type));
    }

    fn stringLiteralContent(self: *Sema, node_idx: Node.Index) ?[]const u8 {
        const node = self.ast_tree.nodes.get(node_idx);
        if (node.tag != .string_literal) return null;
        const token = self.ast_tree.tokens[node.main_token];
        const src = self.diags.source_manager.getFile(0).?.content;
        const raw = src[token.start..token.end];
        if (raw.len < 2) return null;
        return raw[1 .. raw.len - 1];
    }

    fn reportError(self: *Sema, code: u32, phase: Phase, start_byte: u32, msg: []const u8) !void {
        try self.diags.report(.{
            .code = code,
            .phase = phase,
            .severity = .@"error",
            .primary_span = .{
                .file_id = 0,
                .start_byte = start_byte,
                .end_byte = start_byte + 1,
            },
            .message = msg,
        });
    }

    fn reportWarning(self: *Sema, code: u32, phase: Phase, start_byte: u32, msg: []const u8) !void {
        try self.diags.report(.{
            .code = code,
            .phase = phase,
            .severity = .warning,
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
                if (self.root_scope.get(name)) |symbol| {
                    if (self.type_values.get(symbol.decl_node)) |type_value| return type_value;
                }
                return self.resolveBuiltinTypeName(name, tok.start);
            },

            .builtin_call => {
                if (self.type_values.get(node_idx)) |type_value| return type_value;
                try self.reportError(5005, .@"comptime", self.ast_tree.tokens[node.main_token].start, "Builtin expression does not denote a type");
                return self.type_pool.internPrimitive(.void_type);
            },

            .struct_decl, .enum_decl, .union_decl => {
                if (self.type_values.get(node_idx)) |type_value| return type_value;
                _ = try self.analyzeAggregateType(node_idx, self.root_scope);
                return self.type_values.get(node_idx) orelse self.type_pool.internPrimitive(.void_type);
            },

            .pointer_type => {
                const child = try self.resolveTypeExpr(node.data.lhs);
                var flags = node.data.rhs;
                var explicit_alignment: ?u64 = null;
                if ((flags & 0x8000_0000) != 0) {
                    const qualifier_start = flags & 0x7fff_ffff;
                    flags = self.ast_tree.extra_data[qualifier_start];
                    const alignment_node = self.ast_tree.extra_data[qualifier_start + 1];
                    explicit_alignment = self.comptimeInteger(alignment_node);
                    if (explicit_alignment == null or explicit_alignment.? == 0 or !std.math.isPowerOfTwo(explicit_alignment.?)) {
                        try self.reportError(7002, .sema, self.ast_tree.tokens[node.main_token].start, "Pointer alignment must be a comptime power of two");
                        explicit_alignment = null;
                    }
                }
                const is_const = (flags & 1) != 0;
                const is_many = (flags & 2) != 0;
                const is_volatile = (flags & 8) != 0;
                const size: Type.PointerSize = if (is_many) .Many else .One;
                return self.type_pool.intern(.{ .pointer = .{
                    .child_type = child,
                    .is_const = is_const,
                    .is_volatile = is_volatile,
                    .is_allowzero = false,
                    .is_optional = false,
                    .alignment = explicit_alignment,
                    .size = size,
                    .sentinel = null,
                } }, .copyable);
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
                    .alignment = null,
                    .size = .Slice,
                    .sentinel = null,
                } }, .copyable);
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
                } }, .copyable);
            },

            .error_union_type => {
                // lhs = error set node (0 = inferred), rhs = payload type node
                const payload = try self.resolveTypeExpr(node.data.rhs);
                const err_set: Type.Id = if (node.data.lhs != 0)
                    try self.resolveTypeExpr(node.data.lhs)
                else
                    try self.type_pool.internPrimitive(.void_type); // placeholder for inferred
                return self.type_pool.intern(.{ .error_union = .{ .err_set = err_set, .payload = payload } }, .copyable);
            },

            else => {
                // Unknown type expression — return void as a fallback
                return self.type_pool.internPrimitive(.void_type);
            },
        }
    }

    fn comptimeInteger(self: *Sema, node_idx: Node.Index) ?u64 {
        if (self.const_values.get(node_idx)) |value| return value;
        const node = self.ast_tree.nodes.get(node_idx);
        if (node.tag != .integer_literal) return null;
        const token = self.ast_tree.tokens[node.main_token];
        const src = self.diags.source_manager.getFile(0).?.content;
        return std.fmt.parseInt(u64, src[token.start..token.end], 0) catch null;
    }

    /// Resolve a plain identifier to a built-in type.
    fn resolveBuiltinTypeName(self: *Sema, name: []const u8, start_byte: u32) !Type.Id {
        // Primitive names
        if (std.mem.eql(u8, name, "void")) return self.type_pool.internPrimitive(.void_type);
        if (std.mem.eql(u8, name, "noreturn")) return self.type_pool.internPrimitive(.noreturn_type);
        if (std.mem.eql(u8, name, "bool")) return self.type_pool.internPrimitive(.bool_type);
        if (std.mem.eql(u8, name, "type")) return self.type_pool.internPrimitive(.type_type);
        if (std.mem.eql(u8, name, "anytype")) return self.type_pool.internPrimitive(.anytype_type);
        if (std.mem.eql(u8, name, "anyopaque")) return self.type_pool.internPrimitive(.anyopaque_type);
        if (std.mem.eql(u8, name, "comptime_int")) return self.type_pool.internPrimitive(.comptime_int_type);
        if (std.mem.eql(u8, name, "comptime_float")) return self.type_pool.internPrimitive(.comptime_float_type);
        // Float types
        if (std.mem.eql(u8, name, "f16")) return self.type_pool.internPrimitive(.f16_type);
        if (std.mem.eql(u8, name, "f32")) return self.type_pool.internPrimitive(.f32_type);
        if (std.mem.eql(u8, name, "f64")) return self.type_pool.internPrimitive(.f64_type);
        if (std.mem.eql(u8, name, "f80")) return self.type_pool.internPrimitive(.f80_type);
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
                if (bits > 0 and bits <= 4096) {
                    return self.type_pool.internInt(signed, bits);
                }
            }
        }
        // Unknown — report and return void
        try self.reportError(3001, .resolve, start_byte, "Unknown type name");
        return self.type_pool.internPrimitive(.void_type);
    }
};

fn isPrimitive(ty: Type, primitive: Type.Primitive) bool {
    return ty.data == .primitive and ty.data.primitive == primitive;
}
