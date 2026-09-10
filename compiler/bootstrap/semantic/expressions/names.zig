const std = @import("std");
const ast = @import("../../syntax/ast.zig");
const Node = ast.Node;
const Type = @import("../type.zig").Type;
const Scope = @import("../scope.zig").Scope;
const function_semantics = @import("../functions.zig");

pub fn analyze(sema: anytype, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
    return switch (sema.ast_tree.nodes.get(node_idx).tag) {
        .identifier => analyzeIdentifier(sema, node_idx, scope),
        .field_access => analyzeFieldAccess(sema, node_idx, scope),
        else => unreachable,
    };
}

fn analyzeIdentifier(sema: anytype, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
    const node = sema.ast_tree.nodes.get(node_idx);
    const token = sema.ast_tree.tokens[node.main_token];
    const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
    const name = source[token.start..token.end];

    if (scope.get(name)) |symbol| {
        if (sema.local_states.get(symbol.decl_node)) |state| {
            if (state == .moved) try sema.reportError(6001, .sema, token.start, "Use of moved value");
        }
        try sema.resolved_decls.put(node_idx, symbol.decl_node);
        if (sema.module_values.get(symbol.decl_node)) |value| try sema.module_values.put(node_idx, value);
        if (sema.type_values.get(symbol.decl_node)) |value| try sema.type_values.put(node_idx, value);
        if (sema.const_values.get(symbol.decl_node)) |value| try sema.const_values.put(node_idx, value);
        try sema.node_types.put(node_idx, symbol.type_id);
        return symbol.type_id;
    }

    if (function_semantics.findRoot(sema, name)) |function_decl| {
        const function_type = try function_semantics.declare(sema, function_decl, sema.root_scope);
        try sema.resolved_decls.put(node_idx, function_decl);
        try sema.node_types.put(node_idx, function_type);
        return function_type;
    }

    const builtin_primitive: ?Type.Primitive = if (std.mem.eql(u8, name, "comptime_int"))
        .comptime_int_type
    else if (std.mem.eql(u8, name, "comptime_float"))
        .comptime_float_type
    else if (std.mem.eql(u8, name, "bool"))
        .bool_type
    else if (std.mem.eql(u8, name, "f32"))
        .f32_type
    else if (std.mem.eql(u8, name, "f64"))
        .f64_type
    else if (std.mem.eql(u8, name, "void"))
        .void_type
    else if (std.mem.eql(u8, name, "type"))
        .type_type
    else if (std.mem.eql(u8, name, "anytype"))
        .anytype_type
    else if (std.mem.eql(u8, name, "anyopaque"))
        .anyopaque_type
    else
        null;

    if (builtin_primitive) |primitive| {
        const type_value = try sema.type_pool.intern(.{ .primitive = primitive }, .copyable);
        try sema.type_values.put(node_idx, type_value);
        const type_type = try sema.type_pool.internPrimitive(.type_type);
        try sema.node_types.put(node_idx, type_type);
        return type_type;
    }

    if (name.len >= 2) {
        const signed = name[0] == 'i';
        const unsigned = name[0] == 'u';
        if ((signed or unsigned) and name.len <= 5) {
            const bits = std.fmt.parseInt(u16, name[1..], 10) catch 0;
            if (bits > 0 and bits <= 4096) {
                const type_value = try sema.type_pool.intern(.{ .integer = .{ .is_signed = signed, .bits = bits } }, .copyable);
                try sema.type_values.put(node_idx, type_value);
                const type_type = try sema.type_pool.internPrimitive(.type_type);
                try sema.node_types.put(node_idx, type_type);
                return type_type;
            }
        }
    }
    if (std.mem.eql(u8, name, "usize") or std.mem.eql(u8, name, "isize")) {
        const type_value = try sema.type_pool.internSizeInt(std.mem.eql(u8, name, "isize"));
        try sema.type_values.put(node_idx, type_value);
        const type_type = try sema.type_pool.internPrimitive(.type_type);
        try sema.node_types.put(node_idx, type_type);
        return type_type;
    }

    try sema.reportError(3001, .resolve, token.start, "Use of undeclared identifier");
    return 0;
}

fn analyzeFieldAccess(sema: anytype, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
    const node = sema.ast_tree.nodes.get(node_idx);
    const base_type = try sema.analyzeNode(node.data.lhs, scope);
    const field_token = sema.ast_tree.tokens[node.main_token];
    const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
    const field_name = source[field_token.start..field_token.end];

    if (sema.module_values.get(node.data.lhs)) |module_id| {
        const registry = sema.module_registry orelse {
            try sema.reportError(3001, .resolve, field_token.start, "Module namespace is unavailable");
            return sema.putBuiltinResult(node_idx, try sema.type_pool.internPrimitive(.void_type));
        };
        const declaration = registry.get(module_id, field_name) orelse {
            try sema.reportError(3001, .resolve, field_token.start, "Unknown module declaration");
            return sema.putBuiltinResult(node_idx, try sema.type_pool.internPrimitive(.void_type));
        };
        if (!declaration.public) try sema.reportError(3003, .resolve, field_token.start, "Module declaration is private");
        try sema.node_types.put(node_idx, declaration.type_id);
        if (declaration.const_value) |value| try sema.const_values.put(node_idx, value);
        if (declaration.type_value) |value| try sema.type_values.put(node_idx, value);
        if (declaration.module_value) |value| try sema.module_values.put(node_idx, value);
        try sema.external_decls.put(node_idx, .{
            .module_id = module_id,
            .name = field_name,
            .is_function = declaration.is_function,
            .is_syscall = declaration.is_syscall,
            .declaration = declaration.declaration,
            .analysis_address = declaration.analysis_address,
        });
        if (declaration.type_value) |owner_type| {
            if (declaration.analysis_address != 0) {
                const SemaType = @TypeOf(sema.*);
                const target_sema: *SemaType = @ptrFromInt(declaration.analysis_address);
                for (target_sema.aggregate_methods.items) |method| {
                    if (method.owner_type != owner_type) continue;
                    const method_sema: *SemaType = @ptrFromInt(method.analysis_address);
                    const method_node = method_sema.ast_tree.nodes.get(method.declaration);
                    if (method_node.decl_flags.public) try sema.registerExternalAggregateMethod(method);
                }
            }
        }
        return declaration.type_id;
    }

    if (sema.type_values.get(node.data.lhs)) |type_value| {
        const reflected = sema.type_pool.get(type_value);
        if (sema.external_decls.get(node.data.lhs)) |external_type| {
            if (external_type.analysis_address != 0) {
                const SemaType = @TypeOf(sema.*);
                const target_sema: *SemaType = @ptrFromInt(external_type.analysis_address);
                if (target_sema.aggregateMethod(type_value, field_name)) |method| {
                    const method_sema: *SemaType = @ptrFromInt(method.analysis_address);
                    const method_node = method_sema.ast_tree.nodes.get(method.declaration);
                    if (!method_node.decl_flags.public) try sema.reportError(3003, .resolve, field_token.start, "Aggregate method is private");
                    const method_type = method_sema.node_types.get(method.declaration) orelse method_sema.node_types.get(method_node.data.lhs) orelse {
                        try sema.reportError(4001, .sema, field_token.start, "Aggregate method type is unavailable");
                        return sema.putBuiltinResult(node_idx, try sema.type_pool.internPrimitive(.void_type));
                    };
                    try sema.registerExternalAggregateMethod(method);
                    try sema.node_types.put(node_idx, method_type);
                    try sema.external_decls.put(node_idx, .{
                        .module_id = external_type.module_id,
                        .name = method.qualified_name,
                        .is_function = true,
                        .is_syscall = false,
                        .declaration = method.declaration,
                        .analysis_address = method.analysis_address,
                    });
                    return method_type;
                }
            }
        }
        if (sema.aggregateMethod(type_value, field_name)) |method| {
            const SemaType = @TypeOf(sema.*);
            const method_sema: *SemaType = @ptrFromInt(method.analysis_address);
            const method_node = method_sema.ast_tree.nodes.get(method.declaration);
            const method_type = method_sema.node_types.get(method.declaration) orelse method_sema.node_types.get(method_node.data.lhs) orelse {
                try sema.reportError(4001, .sema, field_token.start, "Aggregate method type is unavailable");
                return sema.putBuiltinResult(node_idx, try sema.type_pool.internPrimitive(.void_type));
            };
            if (method.analysis_address == @intFromPtr(sema)) {
                try sema.resolved_decls.put(node_idx, method.declaration);
            } else {
                try sema.external_decls.put(node_idx, .{
                    .module_id = method.module_id,
                    .name = method.qualified_name,
                    .is_function = true,
                    .is_syscall = false,
                    .declaration = method.declaration,
                    .analysis_address = method.analysis_address,
                });
            }
            try sema.node_types.put(node_idx, method_type);
            return method_type;
        }
        if (reflected.data == .@"enum" or reflected.data == .error_set) {
            const member = sema.type_pool.aggregateField(type_value, field_name) orelse {
                try sema.reportError(3001, .resolve, field_token.start, if (reflected.data == .error_set) "Unknown error-set member" else "Unknown enum member");
                try sema.node_types.put(node_idx, type_value);
                return type_value;
            };
            try sema.node_types.put(node_idx, type_value);
            try sema.const_values.put(node_idx, member.value orelse 0);
            return type_value;
        }
    }

    const base = sema.type_pool.get(base_type);
    if (base.data == .primitive and base.data.primitive == .anyopaque_type) {
        try sema.node_types.put(node_idx, base_type);
        return base_type;
    }
    const method_owner: ?Type.Id = switch (base.data) {
        .@"struct", .@"enum", .@"union" => base_type,
        .pointer => |pointer| switch (sema.type_pool.get(pointer.child_type).data) {
            .@"struct", .@"enum", .@"union" => pointer.child_type,
            else => null,
        },
        else => null,
    };
    if (method_owner) |owner| if (sema.aggregateMethod(owner, field_name)) |method| {
        const SemaType = @TypeOf(sema.*);
        const target_sema: *SemaType = @ptrFromInt(method.analysis_address);
        const method_node = target_sema.ast_tree.nodes.get(method.declaration);
        const method_type = target_sema.node_types.get(method.declaration) orelse target_sema.node_types.get(method_node.data.lhs) orelse {
            try sema.reportError(4001, .sema, field_token.start, "Aggregate method type is unavailable");
            return sema.putBuiltinResult(node_idx, try sema.type_pool.internPrimitive(.void_type));
        };
        if (method.analysis_address == @intFromPtr(sema)) {
            try sema.resolved_decls.put(node_idx, method.declaration);
        } else {
            try sema.external_decls.put(node_idx, .{
                .module_id = method.module_id,
                .name = method.qualified_name,
                .is_function = true,
                .is_syscall = false,
                .declaration = method.declaration,
                .analysis_address = method.analysis_address,
            });
        }
        try sema.node_types.put(node_idx, method_type);
        return method_type;
    };
    if (sema.type_pool.aggregateField(base_type, field_name)) |field| {
        try sema.node_types.put(node_idx, field.type_id);
        return field.type_id;
    }
    try sema.reportError(3001, .resolve, field_token.start, "Unknown field or declaration");
    try sema.node_types.put(node_idx, base_type);
    return base_type;
}
