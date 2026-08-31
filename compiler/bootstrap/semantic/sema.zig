const std = @import("std");
const ast = @import("../syntax/ast.zig");
const Node = ast.Node;
const Type = @import("type.zig").Type;
const TypePool = @import("type.zig").TypePool;
const Scope = @import("scope.zig").Scope;
const DiagnosticEngine = @import("../source/diagnostics.zig").DiagnosticEngine;
const Phase = @import("../source/diagnostics.zig").Phase;
const Token = @import("../syntax/token.zig").Token;
const builtin = @import("builtin.zig");
const postfix = @import("expressions/postfix.zig");
const prefix = @import("expressions/prefix.zig");
const operator_semantics = @import("expressions/operators.zig");
const module_namespace = @import("../modules/namespace.zig");
const ModuleId = @import("../modules/resolver.zig").ModuleId;
const cleanup_semantics = @import("control_flow/cleanup.zig");
const match_semantics = @import("control_flow/match.zig");
const aggregate_semantics = @import("expressions/aggregate.zig");
const aggregate_type_semantics = @import("types/aggregate.zig");
const error_set_semantics = @import("types/error_set.zig");
const optional_semantics = @import("expressions/optional.zig");
const generic_model = @import("generics/model.zig");
const conditional_semantics = @import("control_flow/conditional.zig");
const integer_semantics = @import("numbers/integer.zig");
const flow_semantics = @import("control_flow/analyze.zig");
const type_resolution = @import("types/resolve.zig");
const builtin_analysis = @import("builtins/analyze.zig");
const declaration_semantics = @import("declarations.zig");
const name_semantics = @import("expressions/names.zig");
const call_semantics = @import("expressions/call.zig");
const return_semantics = @import("control_flow/return.zig");

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
    const trace_enabled = false;
    pub const ExternalDecl = struct { module_id: ModuleId, name: []const u8, is_function: bool };
    pub const DynamicField = struct { base_node: Node.Index, name: []const u8 };

    allocator: std.mem.Allocator,
    ast_tree: ast.Ast,
    source_id: u32,
    diags: *DiagnosticEngine,
    type_pool: *TypePool,
    root_scope: *Scope,

    // Map from declaration Node.Index to its current local state
    local_states: std.AutoHashMap(Node.Index, LocalState),
    // Map from expression Node.Index to its resolved Type.Id
    node_types: std.AutoHashMap(Node.Index, Type.Id),
    // Comptime values materialized by builtin evaluation.
    const_values: std.AutoHashMap(Node.Index, u64),
    // Comptime evaluated type values
    type_values: std.AutoHashMap(Node.Index, Type.Id),

    resolved_decls: std.AutoHashMap(Node.Index, Node.Index),
    module_values: std.AutoHashMap(Node.Index, ModuleId),
    external_decls: std.AutoHashMap(Node.Index, ExternalDecl),
    dynamic_fields: std.AutoHashMap(Node.Index, DynamicField),
    generic_instances: std.ArrayList(generic_model.Instance),
    generic_calls: std.AutoHashMap(Node.Index, u32),
    module_id: ?ModuleId = null,
    import_ids: ?*const std.AutoHashMap(Node.Index, ModuleId) = null,
    module_registry: ?*const module_namespace.Registry = null,

    unsafe_depth: u32 = 0,
    eval_branch_quota: u64,
    current_return_type: ?Type.Id,
    loop_stack: std.ArrayList(LoopContext),

    pub fn init(
        allocator: std.mem.Allocator,
        ast_tree: ast.Ast,
        source_id: u32,
        diags: *DiagnosticEngine,
        type_pool: *TypePool,
        root_scope: *Scope,
    ) Sema {
        return .{
            .allocator = allocator,
            .ast_tree = ast_tree,
            .source_id = source_id,
            .diags = diags,
            .type_pool = type_pool,
            .root_scope = root_scope,
            .local_states = std.AutoHashMap(Node.Index, LocalState).init(allocator),
            .node_types = std.AutoHashMap(Node.Index, Type.Id).init(allocator),
            .const_values = std.AutoHashMap(Node.Index, u64).init(allocator),
            .type_values = std.AutoHashMap(Node.Index, Type.Id).init(allocator),
            .resolved_decls = std.AutoHashMap(Node.Index, Node.Index).init(allocator),
            .module_values = std.AutoHashMap(Node.Index, ModuleId).init(allocator),
            .external_decls = std.AutoHashMap(Node.Index, ExternalDecl).init(allocator),
            .dynamic_fields = std.AutoHashMap(Node.Index, DynamicField).init(allocator),
            .generic_instances = std.ArrayList(generic_model.Instance).empty,
            .generic_calls = std.AutoHashMap(Node.Index, u32).init(allocator),
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
        self.resolved_decls.deinit();
        self.module_values.deinit();
        self.external_decls.deinit();
        self.dynamic_fields.deinit();
        for (self.generic_instances.items) |*instance| instance.deinit(self.allocator);
        self.generic_instances.deinit(self.allocator);
        self.generic_calls.deinit();
        self.loop_stack.deinit(self.allocator);
    }

    pub fn configureModules(
        self: *Sema,
        module_id: ModuleId,
        import_ids: *const std.AutoHashMap(Node.Index, ModuleId),
        registry: *const module_namespace.Registry,
    ) void {
        self.module_id = module_id;
        self.import_ids = import_ids;
        self.module_registry = registry;
    }

    pub fn analyze(self: *Sema) !void {
        if (trace_enabled) std.debug.print("-> ENTER: Sema.analyze\n", .{});
        defer if (trace_enabled) std.debug.print("<- EXIT: Sema.analyze\n", .{});
        // Traverse the AST starting from the root
        const root_node = self.ast_tree.nodes.get(self.ast_tree.nodes.len - 1);
        if (root_node.tag != .root) return error.InvalidAst;

        const extra_start = root_node.data.lhs;
        const extra_end = root_node.data.rhs;

        var i: u32 = extra_start;
        while (i < extra_end) : (i += 1) {
            const child_idx = self.ast_tree.extra_data[i];
            if (trace_enabled) std.debug.print("   SEMA: Analyzing root child node {d}\n", .{child_idx});
            _ = try self.analyzeNode(child_idx, self.root_scope);
        }
    }

    pub fn analyzeNode(self: *Sema, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
        const node = self.ast_tree.nodes.get(node_idx);
        if (trace_enabled) std.debug.print("-> ENTER: Sema.analyzeNode | Tag: {s}\n", .{@tagName(node.tag)});
        defer if (trace_enabled) std.debug.print("<- EXIT: Sema.analyzeNode | Tag: {s}\n", .{@tagName(node.tag)});

        switch (node.tag) {
            .integer_literal => {
                const ty = try self.type_pool.intern(.{ .primitive = .comptime_int_type }, .copyable);
                try self.node_types.put(node_idx, ty);
                const tok = self.ast_tree.tokens[node.main_token];
                const src = self.diags.source_manager.getFile(self.source_id).?.content;
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
                const src = self.diags.source_manager.getFile(self.source_id).?.content;
                try self.const_values.put(node_idx, if (std.mem.eql(u8, src[tok.start..tok.end], "true")) 1 else 0);
                try self.node_types.put(node_idx, ty);
                return ty;
            },
            .null_literal, .undefined_literal => return optional_semantics.analyzeLiteral(self, node_idx),
            .string_literal => {
                const u8_type = try self.type_pool.internInt(false, 8);
                const ty = try self.type_pool.internSlice(u8_type, true);
                try self.node_types.put(node_idx, ty);
                return ty;
            },
            .pointer_type, .slice_type, .array_type, .optional_type, .error_union_type, .tuple_type => {
                const ty = try self.resolveTypeExpr(node_idx);
                try self.node_types.put(node_idx, ty);
                return ty;
            },
            .struct_decl, .enum_decl, .union_decl => return aggregate_type_semantics.analyze(self, node_idx, scope),
            .error_set_decl => return error_set_semantics.analyze(self, node_idx, scope),
            .identifier, .field_access => return name_semantics.analyze(self, node_idx, scope),
            .const_decl, .var_decl, .fn_proto, .fn_decl => return declaration_semantics.analyze(self, node_idx, scope),
            .builtin_call => return self.analyzeBuiltin(node_idx, scope),
            .binary_op => return operator_semantics.analyze(self, node_idx, scope),
            .unary_op => {
                if (try postfix.analyzeUnarySuffix(self, node_idx, scope)) |type_id| return type_id;
                if (try prefix.analyze(self, node_idx, scope)) |type_id| return type_id;
                unreachable;
            },
            .array_access, .slice => return postfix.analyze(self, node_idx, scope),
            .aggregate_literal => return aggregate_semantics.analyze(self, node_idx, scope),
            .defer_stmt, .errdefer_stmt => return cleanup_semantics.analyze(self, node_idx, scope),
            .match_stmt => return match_semantics.analyze(self, node_idx, scope),
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
                const ty = self.type_pool.internAggregate(.tuple, fields.items, null, null, false) catch {
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
                    type_resolution.comptimeInteger(self, length_node) orelse 0;
                if (declared_length != count) {
                    try self.reportError(4001, .sema, self.ast_tree.tokens[node.main_token].start, "Array literal element count does not match its declared length");
                }
                var index: u32 = 0;
                while (index < count) : (index += 1) {
                    const element_node = self.ast_tree.extra_data[extra_start + 3 + index];
                    const actual_type = try self.analyzeNode(element_node, scope);
                    if (!self.type_pool.isCoercible(actual_type, element_type)) {
                        try self.reportError(4001, .sema, self.ast_tree.tokens[self.ast_tree.nodes.get(element_node).main_token].start, "Array element does not match the declared element type");
                    }
                }
                const array_type = try self.type_pool.internArray(element_type, declared_length);
                try self.node_types.put(node_idx, array_type);
                return array_type;
            },
            .unsafe_block, .block, .while_stmt, .for_stmt, .break_stmt, .continue_stmt => return flow_semantics.analyze(self, node_idx, scope),
            .if_stmt => return conditional_semantics.analyze(self, node_idx, scope),
            .call => return call_semantics.analyze(self, node_idx, scope),
            .return_stmt => return return_semantics.analyze(self, node_idx, scope),
            else => {
                // Return a dummy type
                return 0;
            },
        }
    }

    fn analyzeBuiltin(self: *Sema, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
        return builtin_analysis.analyze(self, node_idx, scope);
    }
    pub fn analyzeCastBuiltin(
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
        return integer_semantics.valueFits(target, value);
    }

    pub fn resolveBuiltinTypeArg(self: *Sema, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!?Type.Id {
        const node = self.ast_tree.nodes.get(node_idx);
        switch (node.tag) {
            .identifier => {
                const token = self.ast_tree.tokens[node.main_token];
                const src = self.diags.source_manager.getFile(self.source_id).?.content;
                if (scope.get(src[token.start..token.end])) |symbol| {
                    if (self.type_values.get(symbol.decl_node)) |type_value| return type_value;
                }
                return try self.resolveTypeExpr(node_idx);
            },
            .pointer_type, .slice_type, .array_type, .optional_type, .error_union_type, .tuple_type => {
                return try self.resolveTypeExpr(node_idx);
            },
            .struct_decl, .enum_decl, .union_decl => {
                _ = try aggregate_type_semantics.analyze(self, node_idx, scope);
                return self.type_values.get(node_idx);
            },
            .error_set_decl => {
                _ = try error_set_semantics.analyze(self, node_idx, scope);
                return self.type_values.get(node_idx);
            },
            .builtin_call => {
                _ = try self.analyzeNode(node_idx, scope);
                return self.type_values.get(node_idx);
            },

            else => return null,
        }
    }

    pub fn putBuiltinResult(self: *Sema, node_idx: Node.Index, type_id: Type.Id) std.mem.Allocator.Error!Type.Id {
        try self.node_types.put(node_idx, type_id);
        return type_id;
    }

    pub fn putBoolBuiltinResult(self: *Sema, node_idx: Node.Index, value: bool) std.mem.Allocator.Error!Type.Id {
        try self.const_values.put(node_idx, @intFromBool(value));
        return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.bool_type));
    }

    pub fn stringLiteralContent(self: *Sema, node_idx: Node.Index) ?[]const u8 {
        const node = self.ast_tree.nodes.get(node_idx);
        if (node.tag != .string_literal) return null;
        const token = self.ast_tree.tokens[node.main_token];
        const src = self.diags.source_manager.getFile(self.source_id).?.content;
        const raw = src[token.start..token.end];
        if (raw.len < 2) return null;
        return raw[1 .. raw.len - 1];
    }

    pub fn reportError(self: *Sema, code: u32, phase: Phase, start_byte: u32, msg: []const u8) !void {
        try self.diags.report(.{
            .code = code,
            .phase = phase,
            .severity = .@"error",
            .primary_span = .{
                .file_id = self.source_id,
                .start_byte = start_byte,
                .end_byte = start_byte + 1,
            },
            .message = msg,
        });
    }

    pub fn reportWarningPublic(self: *Sema, code: u32, phase: Phase, start_byte: u32, msg: []const u8) !void {
        try self.diags.report(.{
            .code = code,
            .phase = phase,
            .severity = .warning,
            .primary_span = .{
                .file_id = self.source_id,
                .start_byte = start_byte,
                .end_byte = start_byte + 1,
            },
            .message = msg,
        });
    }

    pub fn resolveTypeExpr(self: *Sema, node_idx: Node.Index) !Type.Id {
        return type_resolution.resolve(self, node_idx);
    }
};

fn isPrimitive(ty: Type, primitive: Type.Primitive) bool {
    return ty.data == .primitive and ty.data.primitive == primitive;
}
