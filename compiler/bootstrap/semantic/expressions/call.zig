const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Type = @import("../type.zig").Type;
const Scope = @import("../scope.zig").Scope;
const generic_definition = @import("../generics/definition.zig");
const generic_instantiation = @import("../generics/instantiate.zig");
const generic_model = @import("../generics/model.zig");

pub fn analyze(sema: anytype, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
    const node = sema.ast_tree.nodes.get(node_idx);
    const target = node.data.lhs;
    const extra_start = node.data.rhs;
    const target_type_id = try sema.analyzeNode(target, scope);
    const num_args = sema.ast_tree.extra_data[extra_start];
    const target_type = sema.type_pool.get(target_type_id);

    if (sema.resolved_decls.get(target)) |target_declaration| {
        if (generic_definition.isGeneric(&sema.ast_tree, target_declaration)) {
            const outcome = try generic_instantiation.analyzeCall(
                sema,
                node_idx,
                target_declaration,
                sema.ast_tree.extra_data[extra_start + 1 .. extra_start + 1 + num_args],
                scope,
            );
            try sema.generic_calls.put(node_idx, .{
                .module_id = sema.module_id orelse 0,
                .instance_id = outcome.instance_id,
            });
            try sema.node_types.put(node_idx, outcome.return_type);
            if (outcome.type_value) |type_value| try sema.type_values.put(node_idx, type_value);
            return outcome.return_type;
        }
    }

    if (sema.external_decls.get(target)) |external| {
        if (external.is_function and external.analysis_address != 0) {
            const SemaType = @TypeOf(sema.*);
            const target_sema: *SemaType = @ptrFromInt(external.analysis_address);
            if (generic_definition.isGeneric(&target_sema.ast_tree, external.declaration)) {
                var arguments = std.ArrayList(generic_model.Argument).empty;
                defer arguments.deinit(sema.allocator);
                var argument_offset: u32 = 0;
                while (argument_offset < num_args) : (argument_offset += 1) {
                    const argument_node = sema.ast_tree.extra_data[extra_start + 1 + argument_offset];
                    const actual_type = try sema.analyzeNode(argument_node, scope);
                    try arguments.append(sema.allocator, .{
                        .type_id = actual_type,
                        .type_value = sema.type_values.get(argument_node),
                        .const_value = sema.const_values.get(argument_node),
                    });
                }
                const outcome = try generic_instantiation.analyzePreparedCall(target_sema, external.declaration, arguments.items);
                try sema.generic_calls.put(node_idx, .{
                    .module_id = external.module_id,
                    .instance_id = outcome.instance_id,
                });
                try sema.node_types.put(node_idx, outcome.return_type);
                if (outcome.type_value) |type_value| try sema.type_values.put(node_idx, type_value);
                return outcome.return_type;
            }
        }
    }

    if (target_type.data != .function) {
        var i: u32 = 0;
        while (i < num_args) : (i += 1) {
            _ = try sema.analyzeNode(sema.ast_tree.extra_data[extra_start + 1 + i], scope);
        }
        try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "Called expression is not a function");
        return sema.putBuiltinResult(node_idx, try sema.type_pool.internPrimitive(.void_type));
    }

    const function = target_type.data.function;
    const params = sema.type_pool.functionParams(function);
    if (num_args != params.len) {
        try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "Function argument count does not match its declaration");
    }
    var i: u32 = 0;
    while (i < num_args) : (i += 1) {
        const arg_node = sema.ast_tree.extra_data[extra_start + 1 + i];
        const arg_type = try sema.analyzeNode(arg_node, scope);
        if (i < params.len) {
            const parameter_type = sema.type_pool.get(params[i]);
            const accepts_anytype = isPrimitive(parameter_type, .anytype_type);
            if (!accepts_anytype and !sema.type_pool.isCoercible(arg_type, params[i])) {
                try sema.reportError(4001, .sema, sema.ast_tree.tokens[sema.ast_tree.nodes.get(arg_node).main_token].start, "Function argument type does not match parameter type");
            }
        }
    }

    try sema.node_types.put(node_idx, function.ret_type);
    return function.ret_type;
}

fn isPrimitive(ty: Type, primitive: Type.Primitive) bool {
    return ty.data == .primitive and ty.data.primitive == primitive;
}
