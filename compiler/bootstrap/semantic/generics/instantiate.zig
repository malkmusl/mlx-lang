const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Scope = @import("../scope.zig").Scope;
const Type = @import("../type.zig").Type;
const model = @import("model.zig");
const integer_semantics = @import("../numbers/integer.zig");

pub const Outcome = struct {
    instance_id: u32,
    return_type: Type.Id,
    type_value: ?Type.Id = null,
};

/// Creates or reuses a canonical semantic instance for one generic call. The
/// instance owns a snapshot of typed AST nodes used later by LIR lowering, so
/// another specialization never overwrites its concrete body types.
pub fn analyzeCall(
    sema: anytype,
    call_index: Node.Index,
    declaration_index: Node.Index,
    argument_nodes: []const Node.Index,
    call_scope: *Scope,
) !Outcome {
    _ = call_index;
    const declaration = sema.ast_tree.nodes.get(declaration_index);
    const prototype_index = declaration.data.lhs;
    const prototype = sema.ast_tree.nodes.get(prototype_index);
    const parameter_count: usize = prototype.data.rhs - 1;
    if (argument_nodes.len != parameter_count) {
        try sema.reportError(4001, .sema, sema.ast_tree.tokens[prototype.main_token].start, "Generic argument count does not match its declaration");
    }

    const generic_function_type = sema.node_types.get(prototype_index) orelse try sema.analyzeNode(prototype_index, sema.root_scope);
    const generic_function = sema.type_pool.get(generic_function_type).data.function;
    const declared_parameter_types = sema.type_pool.functionParams(generic_function);

    var arguments = std.ArrayList(model.Argument).empty;
    defer arguments.deinit(sema.allocator);
    var concrete_parameter_types = std.ArrayList(Type.Id).empty;
    defer concrete_parameter_types.deinit(sema.allocator);

    const common_count = @min(argument_nodes.len, parameter_count);
    var parameter_offset: usize = 0;
    while (parameter_offset < common_count) : (parameter_offset += 1) {
        const argument_node = argument_nodes[parameter_offset];
        const actual_type = try sema.analyzeNode(argument_node, call_scope);
        const parameter_index = sema.ast_tree.extra_data[prototype.data.lhs + 1 + @as(u32, @intCast(parameter_offset))];
        const parameter = sema.ast_tree.nodes.get(parameter_index);
        const declared_type = declared_parameter_types[parameter_offset];
        const declared = sema.type_pool.get(declared_type);
        const is_anytype = declared.data == .primitive and declared.data.primitive == .anytype_type;
        const dependent_type = genericTypeBinding(sema, prototype, parameter.data.rhs, arguments.items);
        const concrete_type = dependent_type orelse if (is_anytype) actual_type else declared_type;

        if ((!is_anytype or dependent_type != null) and !sema.type_pool.isCoercible(actual_type, concrete_type)) {
            try sema.reportError(4001, .sema, sema.ast_tree.tokens[sema.ast_tree.nodes.get(argument_node).main_token].start, "Generic argument type does not match parameter type");
        }
        const type_value = sema.type_values.get(argument_node);
        const const_value = sema.const_values.get(argument_node);
        if (parameter.decl_flags.comptime_param and type_value == null and const_value == null) {
            try sema.reportError(5001, .@"comptime", sema.ast_tree.tokens[parameter.main_token].start, "comptime argument is not compile-time known");
        }
        if (const_value) |value| {
            const concrete = sema.type_pool.get(concrete_type);
            if ((concrete.data == .integer or concrete.data == .size_int) and !integer_semantics.valueFits(concrete, value)) {
                try sema.reportError(4002, .sema, sema.ast_tree.tokens[sema.ast_tree.nodes.get(argument_node).main_token].start, "Generic argument does not fit its instantiated parameter type");
            }
        }
        try concrete_parameter_types.append(sema.allocator, concrete_type);
        try arguments.append(sema.allocator, .{
            .type_id = actual_type,
            .type_value = if (parameter.decl_flags.comptime_param) type_value else null,
            .const_value = if (parameter.decl_flags.comptime_param) const_value else null,
        });
    }
    // Keep error recovery deterministic after an arity mismatch.
    while (concrete_parameter_types.items.len < parameter_count) {
        const fallback = declared_parameter_types[concrete_parameter_types.items.len];
        try concrete_parameter_types.append(sema.allocator, fallback);
        try arguments.append(sema.allocator, .{ .type_id = fallback });
    }

    if (findExisting(sema, declaration_index, arguments.items)) |instance_id| {
        const instance = sema.generic_instances.items[instance_id];
        return .{
            .instance_id = instance_id,
            .return_type = sema.type_pool.get(instance.function_type).data.function.ret_type,
            .type_value = instance.result_type_value,
        };
    }

    var function_scope = Scope.init(sema.allocator, sema.root_scope);
    defer function_scope.deinit();
    const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
    parameter_offset = 0;
    while (parameter_offset < parameter_count) : (parameter_offset += 1) {
        const parameter_index = sema.ast_tree.extra_data[prototype.data.lhs + 1 + @as(u32, @intCast(parameter_offset))];
        const parameter = sema.ast_tree.nodes.get(parameter_index);
        const token = sema.ast_tree.tokens[parameter.main_token];
        const name = source[token.start..token.end];
        const concrete_type = concrete_parameter_types.items[parameter_offset];
        try function_scope.put(name, .{
            .name = name,
            .decl_node = parameter_index,
            .type_id = concrete_type,
            .is_const = true,
        });
        try sema.node_types.put(parameter_index, concrete_type);
        try sema.local_states.put(parameter_index, .initialized);
        const argument = arguments.items[parameter_offset];
        if (argument.type_value) |type_value| try sema.type_values.put(parameter_index, type_value);
        if (argument.const_value) |const_value| try sema.const_values.put(parameter_index, const_value);
    }

    const return_node = sema.ast_tree.extra_data[prototype.data.lhs];
    var return_type = try sema.resolveBuiltinTypeArg(return_node, &function_scope) orelse blk: {
        try sema.reportError(4001, .sema, sema.ast_tree.tokens[prototype.main_token].start, "Generic return type could not be instantiated");
        break :blk try sema.type_pool.internPrimitive(.void_type);
    };

    const previous_return_type = sema.current_return_type;
    const previous_loop_depth = sema.loop_stack.items.len;
    sema.current_return_type = return_type;
    sema.loop_stack.clearRetainingCapacity();
    defer sema.current_return_type = previous_return_type;
    defer sema.loop_stack.items.len = previous_loop_depth;
    _ = try sema.analyzeNode(declaration.data.rhs, &function_scope);

    const return_value = sema.type_pool.get(return_type);
    if (return_value.data == .primitive and return_value.data.primitive == .anytype_type) {
        return_type = firstReturnType(sema, declaration.data.rhs) orelse blk: {
            try sema.reportError(4004, .sema, sema.ast_tree.tokens[prototype.main_token].start, "Generic anytype return could not be inferred");
            break :blk try sema.type_pool.internPrimitive(.void_type);
        };
    }

    const params_start = try sema.type_pool.appendParams(concrete_parameter_types.items);
    const instance_function_type = try sema.type_pool.intern(.{ .function = .{
        .ret_type = return_type,
        .params_start = params_start,
        .params_len = @intCast(concrete_parameter_types.items.len),
        .is_var_args = false,
    } }, .copyable);
    try sema.node_types.put(prototype_index, instance_function_type);
    try sema.node_types.put(declaration_index, instance_function_type);
    const result_type_value = if (sema.type_pool.get(return_type).data == .primitive and
        sema.type_pool.get(return_type).data.primitive == .type_type)
        firstReturnTypeValue(sema, declaration.data.rhs)
    else
        null;

    var node_types = std.AutoHashMap(Node.Index, Type.Id).init(sema.allocator);
    errdefer node_types.deinit();
    var node_iterator = sema.node_types.iterator();
    while (node_iterator.next()) |entry| try node_types.put(entry.key_ptr.*, entry.value_ptr.*);
    var const_values = std.AutoHashMap(Node.Index, u64).init(sema.allocator);
    errdefer const_values.deinit();
    var const_iterator = sema.const_values.iterator();
    while (const_iterator.next()) |entry| try const_values.put(entry.key_ptr.*, entry.value_ptr.*);

    const owned_arguments = try sema.allocator.dupe(model.Argument, arguments.items);
    errdefer sema.allocator.free(owned_arguments);
    const instance_id: u32 = @intCast(sema.generic_instances.items.len);
    try sema.generic_instances.append(sema.allocator, .{
        .declaration = declaration_index,
        .arguments = owned_arguments,
        .function_type = instance_function_type,
        .result_type_value = result_type_value,
        .node_types = node_types,
        .const_values = const_values,
    });
    return .{ .instance_id = instance_id, .return_type = return_type, .type_value = result_type_value };
}

fn findExisting(sema: anytype, declaration: Node.Index, arguments: []const model.Argument) ?u32 {
    for (sema.generic_instances.items, 0..) |instance, index| {
        if (instance.declaration != declaration or instance.arguments.len != arguments.len) continue;
        var equal = true;
        for (instance.arguments, arguments) |left, right| {
            if (!left.eql(right)) {
                equal = false;
                break;
            }
        }
        if (equal) return @intCast(index);
    }
    return null;
}

fn genericTypeBinding(sema: anytype, prototype: Node, type_node_index: Node.Index, prior_arguments: []const model.Argument) ?Type.Id {
    const type_node = sema.ast_tree.nodes.get(type_node_index);
    if (type_node.tag != .identifier) return null;
    const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
    const wanted = sema.ast_tree.tokens[type_node.main_token];
    var offset: usize = 0;
    while (offset < prior_arguments.len) : (offset += 1) {
        const parameter_index = sema.ast_tree.extra_data[prototype.data.lhs + 1 + @as(u32, @intCast(offset))];
        const parameter = sema.ast_tree.nodes.get(parameter_index);
        if (!parameter.decl_flags.comptime_param) continue;
        const parameter_type = sema.ast_tree.nodes.get(parameter.data.rhs);
        if (parameter_type.tag != .identifier or sema.ast_tree.tokens[parameter_type.main_token].tag != .keyword_type) continue;
        const name = sema.ast_tree.tokens[parameter.main_token];
        if (std.mem.eql(u8, source[wanted.start..wanted.end], source[name.start..name.end])) return prior_arguments[offset].type_value;
    }
    return null;
}

fn firstReturnType(sema: anytype, node_index: Node.Index) ?Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    return switch (node.tag) {
        .return_stmt => if (node.data.rhs == std.math.maxInt(u32)) null else sema.node_types.get(node.data.rhs),
        .block => blk: {
            var offset = node.data.lhs;
            while (offset < node.data.rhs) : (offset += 1) {
                if (firstReturnType(sema, sema.ast_tree.extra_data[offset])) |result| break :blk result;
            }
            break :blk null;
        },
        .if_stmt => blk: {
            const start = node.data.lhs;
            if (firstReturnType(sema, sema.ast_tree.extra_data[start + 1])) |result| break :blk result;
            if (node.data.rhs > start + 2) break :blk firstReturnType(sema, sema.ast_tree.extra_data[start + 2]);
            break :blk null;
        },
        else => null,
    };
}

fn firstReturnTypeValue(sema: anytype, node_index: Node.Index) ?Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    return switch (node.tag) {
        .return_stmt => if (node.data.rhs == std.math.maxInt(u32)) null else sema.type_values.get(node.data.rhs),
        .block => blk: {
            var offset = node.data.lhs;
            while (offset < node.data.rhs) : (offset += 1) {
                if (firstReturnTypeValue(sema, sema.ast_tree.extra_data[offset])) |result| break :blk result;
            }
            break :blk null;
        },
        .if_stmt => blk: {
            const start = node.data.lhs;
            if (firstReturnTypeValue(sema, sema.ast_tree.extra_data[start + 1])) |result| break :blk result;
            if (node.data.rhs > start + 2) break :blk firstReturnTypeValue(sema, sema.ast_tree.extra_data[start + 2]);
            break :blk null;
        },
        else => null,
    };
}
