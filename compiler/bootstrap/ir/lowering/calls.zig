const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Type = @import("../../semantic/type.zig").Type;
const Inst = @import("../lir.zig").Inst;
const function_lowering = @import("functions.zig");

pub fn lower(builder: anytype, node_idx: Node.Index) std.mem.Allocator.Error!?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_idx);
    const target = node.data.lhs;
    const extra_start = node.data.rhs;
    const num_args = builder.sema.ast_tree.extra_data[extra_start];

    if (isSyscallTarget(builder, target)) {
        const syscall_start: u32 = @intCast(builder.lir.extra_data.items.len);
        try builder.lir.extra_data.append(builder.allocator, num_args);
        var syscall_arg: u32 = 0;
        while (syscall_arg < num_args) : (syscall_arg += 1) {
            const argument_node = builder.sema.ast_tree.extra_data[extra_start + 1 + syscall_arg];
            const argument = try builder.lowerNode(argument_node) orelse return null;
            try builder.lir.extra_data.append(builder.allocator, argument);
        }
        const syscall = try builder.emitInst(.{
            .opcode = .syscall,
            .type_id = builder.sema.node_types.get(node_idx) orelse 0,
            .data = .{ .syscall = syscall_start },
        });
        return syscall;
    }

    const generic_instance = builder.sema.generic_calls.get(node_idx);
    const target_inst = if (generic_instance) |instance_id|
        try builder.emitInst(.{
            .opcode = .func_sym,
            .type_id = builder.sema.node_types.get(target) orelse 0,
            .data = .{ .func_sym = try function_lowering.internGenericSymbol(builder, instance_id) },
        })
    else
        try builder.lowerNode(target);

    const args_start = builder.lir.extra_data.items.len;
    var i: u32 = 0;
    var runtime_args: u32 = 0;
    while (i < num_args) : (i += 1) {
        if (generic_instance != null and argumentIsComptime(builder, target, i)) continue;
        const argument_node = builder.sema.ast_tree.extra_data[extra_start + 1 + i];
        const argument = try builder.lowerNode(argument_node);
        const argument_type = builder.sema.node_types.get(argument_node) orelse 0;
        const abi_argument = if (isAggregate(builder, argument_type))
            try builder.emitInst(.{
                .opcode = .aggregate_copy,
                .type_id = argument_type,
                .data = .{ .aggregate_copy = argument.? },
            })
        else
            argument.?;
        try builder.lir.extra_data.append(builder.allocator, abi_argument);
        runtime_args += 1;
        if (isSlice(builder, argument_type)) {
            const length = builder.slice_lengths.get(argument.?) orelse return null;
            try builder.lir.extra_data.append(builder.allocator, length);
            runtime_args += 1;
        }
    }

    const type_id = builder.sema.node_types.get(node_idx) orelse 0;
    const call = try builder.emitInst(.{
        .opcode = .call,
        .type_id = type_id,
        .data = .{ .call = .{ .func = target_inst orelse 0, .args_start = @intCast(args_start), .args_count = runtime_args } },
    });
    if (isSlice(builder, type_id) or isSliceErrorUnion(builder, type_id)) {
        const length_type = try builder.sema.type_pool.internSizeInt(false);
        const length = try builder.emitInst(.{
            .opcode = .error_payload_part,
            .type_id = length_type,
            .data = .{ .error_payload_part = .{ .source = call, .part = 1 } },
        });
        try builder.slice_lengths.put(call, length);
    }
    return call;
}

fn isSyscallTarget(builder: anytype, target_node: Node.Index) bool {
    if (builder.sema.external_decls.get(target_node)) |external| return external.is_syscall;
    const declaration_index = builder.sema.resolved_decls.get(target_node) orelse return false;
    const declaration = builder.sema.ast_tree.nodes.get(declaration_index);
    if (declaration.tag != .fn_decl or !declaration.decl_flags.extern_decl) return false;
    if (declaration.extern_name_token == std.math.maxInt(u32)) return false;
    const token = builder.sema.ast_tree.tokens[declaration.extern_name_token];
    const source = builder.sema.diags.source_manager.getFile(builder.sema.source_id).?.content;
    const raw = source[token.start..token.end];
    return raw.len >= 2 and std.mem.eql(u8, raw[1 .. raw.len - 1], "syscall");
}

fn isSlice(builder: anytype, type_id: Type.Id) bool {
    const ty = builder.sema.type_pool.get(type_id);
    return ty.data == .pointer and ty.data.pointer.size == .Slice;
}

fn isAggregate(builder: anytype, type_id: Type.Id) bool {
    return switch (builder.sema.type_pool.get(type_id).data) {
        .array, .@"struct", .@"union", .tuple => true,
        else => false,
    };
}

fn argumentIsComptime(builder: anytype, target_node: Node.Index, argument_offset: u32) bool {
    const declaration_index = builder.sema.resolved_decls.get(target_node) orelse return false;
    const declaration = builder.sema.ast_tree.nodes.get(declaration_index);
    if (declaration.tag != .fn_decl) return false;
    const prototype = builder.sema.ast_tree.nodes.get(declaration.data.lhs);
    if (argument_offset + 1 >= prototype.data.rhs) return false;
    const parameter_index = builder.sema.ast_tree.extra_data[prototype.data.lhs + 1 + argument_offset];
    return builder.sema.ast_tree.nodes.get(parameter_index).decl_flags.comptime_param;
}

fn isSliceErrorUnion(builder: anytype, type_id: Type.Id) bool {
    const ty = builder.sema.type_pool.get(type_id);
    if (ty.data != .error_union) return false;
    const payload = builder.sema.type_pool.get(ty.data.error_union.payload);
    return payload.data == .pointer and payload.data.pointer.size == .Slice;
}
