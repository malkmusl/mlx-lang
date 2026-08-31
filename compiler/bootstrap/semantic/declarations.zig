const std = @import("std");
const ast = @import("../syntax/ast.zig");
const Node = ast.Node;
const Type = @import("type.zig").Type;
const Scope = @import("scope.zig").Scope;
const integer_semantics = @import("numbers/integer.zig");
const ownership_state = @import("ownership/state.zig");
const function_semantics = @import("functions.zig");
const flow_analysis = @import("control_flow/analysis.zig");
const generic_definition = @import("generics/definition.zig");
const ComptimeVM = @import("comptime.zig").ComptimeVM;

const trace_enabled = false;

pub fn analyze(sema: anytype, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
    return switch (sema.ast_tree.nodes.get(node_idx).tag) {
        .const_decl, .var_decl => analyzeBinding(sema, node_idx, scope),
        .fn_proto => analyzeFunctionPrototype(sema, node_idx, scope),
        .fn_decl => analyzeFunctionDeclaration(sema, node_idx, scope),
        else => unreachable,
    };
}

fn analyzeBinding(sema: anytype, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
    const node = sema.ast_tree.nodes.get(node_idx);
    const ident_tok_idx = node.data.lhs;
    const extra_start = node.data.rhs;
    const type_node = sema.ast_tree.extra_data[extra_start];
    const init_node = sema.ast_tree.extra_data[extra_start + 1];

    const src = sema.diags.source_manager.getFile(sema.source_id).?.content;
    const ident_tok = sema.ast_tree.tokens[ident_tok_idx];
    const name = src[ident_tok.start..ident_tok.end];
    const inferred_type = try sema.analyzeNode(init_node, scope);

    var declared_type: Type.Id = inferred_type;
    if (type_node != 0) {
        declared_type = try sema.resolveTypeExpr(type_node);
        if (!sema.type_pool.isCoercible(inferred_type, declared_type)) {
            var from_buf: [64]u8 = undefined;
            var to_buf: [64]u8 = undefined;
            const from_name = sema.type_pool.typeName(inferred_type, &from_buf) catch "<unknown>";
            const to_name = sema.type_pool.typeName(declared_type, &to_buf) catch "<unknown>";
            if (trace_enabled) std.debug.print("TYPE ERROR: cannot coerce '{s}' to '{s}' for variable '{s}'\n", .{ from_name, to_name, name });
            try sema.reportError(4001, .sema, ident_tok.start, "Type mismatch");
        }
        if (sema.const_values.get(init_node)) |value| {
            const target = sema.type_pool.get(declared_type);
            if (target.isInteger() and !integer_semantics.valueFits(target, value)) {
                try sema.reportError(4002, .sema, ident_tok.start, "Comptime integer does not fit the declared type");
            }
        }
    }

    {
        var name_buf: [64]u8 = undefined;
        const type_name = sema.type_pool.typeName(declared_type, &name_buf) catch "<unknown>";
        if (trace_enabled) std.debug.print("[sema] {s} {s}: {s}\n", .{
            if (node.tag == .const_decl) "const" else "var",
            name,
            type_name,
        });
    }

    const copyability = sema.type_pool.types.items[inferred_type].copyability;
    if (copyability != .copyable) {
        const rhs_node = sema.ast_tree.nodes.get(init_node);
        const copies_storage = rhs_node.tag == .identifier or rhs_node.tag == .field_access;
        if (copies_storage and !ownership_state.sourceIsMoved(sema, init_node, scope)) {
            try sema.reportError(6002, .sema, ident_tok.start, "Cannot copy a non-copyable type");
        }
    }

    if (node.tag == .const_decl) {
        var vm = ComptimeVM.init(sema.allocator, sema.ast_tree, sema);
        const val = vm.evaluate(init_node, src);
        switch (val) {
            .integer => |i| if (trace_enabled) std.debug.print("[comptime] '{s}' = {d}\n", .{ name, i }),
            .err => |err_message| if (trace_enabled) std.debug.print("[comptime] '{s}' cannot be evaluated at compile time: {s}\n", .{ name, err_message }),
        }
    }

    try scope.put(name, .{
        .name = name,
        .decl_node = node_idx,
        .type_id = declared_type,
        .is_const = node.tag == .const_decl,
    });

    try sema.local_states.put(node_idx, .initialized);
    try sema.node_types.put(node_idx, declared_type);
    if (node.tag == .const_decl) {
        if (sema.type_values.get(init_node)) |type_value| try sema.type_values.put(node_idx, type_value);
        if (sema.const_values.get(init_node)) |const_value| try sema.const_values.put(node_idx, const_value);
        if (sema.module_values.get(init_node)) |module_value| try sema.module_values.put(node_idx, module_value);
    }
    return declared_type;
}

fn analyzeFunctionPrototype(sema: anytype, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
    const node = sema.ast_tree.nodes.get(node_idx);
    const extra_start = node.data.lhs;
    const extra_len = node.data.rhs;
    const ret_type_node = sema.ast_tree.extra_data[extra_start];
    const ret_type = if (function_semantics.nodeNamesGenericTypeParameter(sema, node_idx, ret_type_node))
        try sema.type_pool.internPrimitive(.anytype_type)
    else
        try sema.resolveBuiltinTypeArg(ret_type_node, scope) orelse {
            try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "Function return type could not be resolved");
            return sema.type_pool.internPrimitive(.void_type);
        };

    var param_types = std.ArrayList(Type.Id).empty;
    defer param_types.deinit(sema.allocator);
    var i: u32 = 1;
    while (i < extra_len) : (i += 1) {
        const param_idx = sema.ast_tree.extra_data[extra_start + i];
        const param_node = sema.ast_tree.nodes.get(param_idx);
        const param_type_node = param_node.data.rhs;
        const param_type = if (function_semantics.nodeNamesGenericTypeParameter(sema, node_idx, param_type_node))
            try sema.type_pool.internPrimitive(.anytype_type)
        else
            try sema.resolveBuiltinTypeArg(param_type_node, scope) orelse {
                try sema.reportError(4001, .sema, sema.ast_tree.tokens[param_node.main_token].start, "Function parameter type could not be resolved");
                continue;
            };
        try param_types.append(sema.allocator, param_type);
        try sema.node_types.put(param_idx, param_type);
    }

    const params_start = try sema.type_pool.appendParams(param_types.items);
    const fn_type = try sema.type_pool.intern(.{ .function = .{
        .ret_type = ret_type,
        .params_start = params_start,
        .params_len = @intCast(param_types.items.len),
        .is_var_args = false,
    } }, .copyable);
    try sema.node_types.put(node_idx, fn_type);
    return fn_type;
}

fn analyzeFunctionDeclaration(sema: anytype, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
    const node = sema.ast_tree.nodes.get(node_idx);
    var child_scope = Scope.init(sema.allocator, scope);
    defer child_scope.deinit();

    const proto = node.data.lhs;
    const body = node.data.rhs;
    const fn_type = try function_semantics.declare(sema, node_idx, scope);
    if (node.decl_flags.extern_decl) {
        try validateExternFunction(sema, node_idx, fn_type);
        try sema.node_types.put(node_idx, fn_type);
        return fn_type;
    }
    if (generic_definition.isGeneric(&sema.ast_tree, node_idx)) {
        try sema.node_types.put(node_idx, fn_type);
        return fn_type;
    }
    try function_semantics.bindParameters(sema, proto, fn_type, &child_scope);
    const proto_node = sema.ast_tree.nodes.get(proto);
    const fn_tok = sema.ast_tree.tokens[proto_node.main_token];

    const function = sema.type_pool.get(fn_type).data.function;
    const previous_return_type = sema.current_return_type;
    const previous_loop_depth = sema.loop_stack.items.len;
    sema.current_return_type = function.ret_type;
    sema.loop_stack.clearRetainingCapacity();
    defer sema.current_return_type = previous_return_type;
    defer sema.loop_stack.items.len = previous_loop_depth;

    _ = try sema.analyzeNode(body, &child_scope);

    const return_type = sema.type_pool.get(function.ret_type);
    const may_fall_through = !flow_analysis.definitelyReturns(sema, body);
    if (may_fall_through and !flow_analysis.allowsSuccessfulFallthrough(sema, function.ret_type)) {
        const code: u32 = if (isPrimitive(return_type, .noreturn_type)) 4010 else 4004;
        const message = if (code == 4010)
            "noreturn function has a returning path"
        else
            "Function can reach the end without returning a value";
        try sema.reportError(code, .sema, fn_tok.start, message);
    }

    try sema.node_types.put(node_idx, fn_type);
    return fn_type;
}

fn validateExternFunction(sema: anytype, node_idx: Node.Index, function_type: Type.Id) std.mem.Allocator.Error!void {
    const declaration = sema.ast_tree.nodes.get(node_idx);
    const prototype = sema.ast_tree.nodes.get(declaration.data.lhs);
    const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
    const start = sema.ast_tree.tokens[prototype.main_token].start;
    if (declaration.extern_name_token == std.math.maxInt(u32)) {
        try sema.reportError(4001, .sema, start, "extern function requires an ABI name");
        return;
    }
    const abi_token = sema.ast_tree.tokens[declaration.extern_name_token];
    const raw_abi = source[abi_token.start..abi_token.end];
    const abi_name = if (raw_abi.len >= 2) raw_abi[1 .. raw_abi.len - 1] else raw_abi;
    if (!std.mem.eql(u8, abi_name, "syscall")) {
        try sema.reportError(9001, .lowering, start, "Stage 0 currently supports only extern(\"syscall\")");
        return;
    }

    const function = sema.type_pool.get(function_type).data.function;
    const parameters = sema.type_pool.functionParams(function);
    if (parameters.len == 0 or parameters.len > 7) {
        try sema.reportError(4001, .sema, start, "extern(\"syscall\") requires the syscall number and at most six arguments");
    }
    for (parameters) |parameter| {
        const ty = sema.type_pool.get(parameter);
        if (!ty.isInteger() and ty.data != .pointer and ty.data != .@"enum") {
            try sema.reportError(4001, .sema, start, "Raw syscall parameters must be integers, pointers, or enums");
            break;
        }
    }
    const body = sema.ast_tree.nodes.get(declaration.data.rhs);
    if (body.data.lhs != body.data.rhs) {
        try sema.reportError(4001, .sema, start, "extern functions must have an empty body");
    }
}

fn isPrimitive(ty: Type, primitive: Type.Primitive) bool {
    return ty.data == .primitive and ty.data.primitive == primitive;
}
