const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Inst = @import("../lir.zig").Inst;

pub fn lower(builder: anytype, node_idx: Node.Index) std.mem.Allocator.Error!?Inst.Index {
    return switch (builder.sema.ast_tree.nodes.get(node_idx).tag) {
        .fn_decl => lowerDeclaration(builder, node_idx),
        .fn_proto => lowerPrototype(builder, node_idx),
        .param_decl => lowerParameter(builder, node_idx),
        else => unreachable,
    };
}

fn lowerDeclaration(builder: anytype, node_idx: Node.Index) std.mem.Allocator.Error!?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_idx);
    if (node.decl_flags.extern_decl) return null;
    const proto_idx = node.data.lhs;
    const body = node.data.rhs;

    const function_block = try builder.newBlock();
    builder.current_block = function_block;
    builder.param_counter = 0;
    builder.var_addresses.clearRetainingCapacity();
    builder.var_slice_lengths.clearRetainingCapacity();
    builder.slice_lengths.clearRetainingCapacity();
    builder.loop_stack.clearRetainingCapacity();
    builder.cleanup_stack.clearRetainingCapacity();

    const prototype = builder.sema.ast_tree.nodes.get(proto_idx);
    const function_type = builder.sema.node_types.get(proto_idx).?;
    builder.current_return_type = builder.sema.type_pool.get(function_type).data.function.ret_type;
    const token = builder.sema.ast_tree.tokens[prototype.main_token];
    const source = builder.sema.diags.source_manager.getFile(builder.sema.source_id).?.content;
    const symbol = if (builder.current_generic_instance) |instance_id|
        try internGenericSymbol(builder, instance_id)
    else
        try builder.lir.internModuleSymbol(builder.sema.module_id orelse 0, source[token.start..token.end]);
    _ = try builder.emitInst(.{
        .opcode = .label,
        .type_id = builder.current_return_type.?,
        .data = .{ .label = symbol },
    });

    _ = try builder.lowerNode(proto_idx);
    _ = try builder.lowerNode(body);
    if (!builder.currentBlockTerminatedPublic()) {
        _ = try builder.emitInst(.{ .opcode = .ret, .type_id = builder.current_return_type.?, .data = .{ .ret = null } });
    }
    builder.current_return_type = null;
    builder.current_block = null;
    return null;
}

fn lowerPrototype(builder: anytype, node_idx: Node.Index) std.mem.Allocator.Error!?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_idx);
    var i: u32 = 1;
    while (i < node.data.rhs) : (i += 1) {
        _ = try builder.lowerNode(builder.sema.ast_tree.extra_data[node.data.lhs + i]);
    }
    return null;
}

fn lowerParameter(builder: anytype, node_idx: Node.Index) std.mem.Allocator.Error!?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_idx);
    if (node.decl_flags.comptime_param) return null;
    const name_token_index = node.data.lhs;
    const type_id = builder.sema.node_types.get(node_idx) orelse 0;

    const parameter = try builder.emitInst(.{
        .opcode = .param,
        .type_id = type_id,
        .data = .{ .param = builder.param_counter },
    });
    builder.param_counter += 1;
    const address = try builder.emitInst(.{
        .opcode = .addr,
        .type_id = type_id,
        .data = .{ .addr = name_token_index },
    });
    _ = try builder.emitInst(.{
        .opcode = .store,
        .type_id = type_id,
        .data = .{ .store = .{ .ptr = address, .val = parameter } },
    });

    const token = builder.sema.ast_tree.tokens[name_token_index];
    const source = builder.sema.diags.source_manager.getFile(builder.sema.source_id).?.content;
    const name = source[token.start..token.end];
    try builder.var_addresses.put(name, address);
    if (isSlice(builder, type_id)) {
        const length_type = try builder.sema.type_pool.internSizeInt(false);
        const length = try builder.emitInst(.{
            .opcode = .param,
            .type_id = length_type,
            .data = .{ .param = builder.param_counter },
        });
        builder.param_counter += 1;
        try builder.var_slice_lengths.put(name, length);
    }
    return null;
}

fn isSlice(builder: anytype, type_id: u32) bool {
    const ty = builder.sema.type_pool.get(type_id);
    return ty.data == .pointer and ty.data.pointer.size == .Slice;
}

pub fn internGenericSymbol(builder: anytype, instance_id: u32) std.mem.Allocator.Error!u32 {
    const instance = builder.sema.generic_instances.items[instance_id];
    const declaration = builder.sema.ast_tree.nodes.get(instance.declaration);
    const prototype = builder.sema.ast_tree.nodes.get(declaration.data.lhs);
    const token = builder.sema.ast_tree.tokens[prototype.main_token];
    const source = builder.sema.diags.source_manager.getFile(builder.sema.source_id).?.content;
    const specialized_name = try std.fmt.allocPrint(builder.allocator, "{s}__g{d}", .{ source[token.start..token.end], instance_id });
    defer builder.allocator.free(specialized_name);
    return builder.lir.internModuleSymbol(builder.sema.module_id orelse 0, specialized_name);
}
