const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Inst = @import("../lir.zig").Inst;

pub fn lower(builder: anytype, node_idx: Node.Index) std.mem.Allocator.Error!?Inst.Index {
    return switch (builder.sema.ast_tree.nodes.get(node_idx).tag) {
        .const_decl => lowerConst(builder, node_idx),
        .var_decl => lowerVar(builder, node_idx),
        .identifier => lowerIdentifier(builder, node_idx),
        else => unreachable,
    };
}

fn lowerConst(builder: anytype, node_idx: Node.Index) std.mem.Allocator.Error!?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_idx);
    const identifier_token_index = node.data.lhs;
    const expression_node = builder.sema.ast_tree.extra_data[node.data.rhs + 1];
    const type_id = builder.sema.node_types.get(node_idx) orelse 0;
    const declaration_type = builder.sema.type_pool.get(type_id);
    if (declaration_type.data == .primitive and declaration_type.data.primitive == .type_type) return null;

    const token = builder.sema.ast_tree.tokens[identifier_token_index];
    const source = builder.sema.diags.source_manager.getFile(builder.sema.source_id).?.content;
    const name = source[token.start..token.end];
    const expression = (try builder.lowerNode(expression_node)) orelse return null;
    const address = try builder.emitInst(.{
        .opcode = .addr,
        .type_id = type_id,
        .data = .{ .addr = identifier_token_index },
    });

    try builder.var_addresses.put(name, address);
    if (builder.slice_lengths.get(expression)) |length| try builder.var_slice_lengths.put(name, length);
    _ = try builder.emitInst(.{
        .opcode = .store,
        .type_id = type_id,
        .data = .{ .store = .{ .ptr = address, .val = expression } },
    });
    return expression;
}

fn lowerVar(builder: anytype, node_idx: Node.Index) std.mem.Allocator.Error!?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_idx);
    const identifier_token_index = node.data.lhs;
    const init_node = builder.sema.ast_tree.extra_data[node.data.rhs + 1];
    const type_id = builder.sema.node_types.get(node_idx) orelse return null;
    const rhs = try builder.lowerNode(init_node) orelse return null;

    const token = builder.sema.ast_tree.tokens[identifier_token_index];
    const source = builder.sema.diags.source_manager.getFile(builder.sema.source_id).?.content;
    const name = source[token.start..token.end];
    const address = try builder.emitInst(.{
        .opcode = .addr,
        .type_id = type_id,
        .data = .{ .addr = identifier_token_index },
    });

    try builder.var_addresses.put(name, address);
    if (builder.slice_lengths.get(rhs)) |length| try builder.var_slice_lengths.put(name, length);
    const store = try builder.emitInst(.{
        .opcode = .store,
        .type_id = type_id,
        .data = .{ .store = .{ .ptr = address, .val = rhs } },
    });
    return @as(?Inst.Index, store);
}

fn lowerIdentifier(builder: anytype, node_idx: Node.Index) std.mem.Allocator.Error!?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_idx);
    const type_id = builder.sema.node_types.get(node_idx) orelse return null;
    if (builder.sema.const_values.get(node_idx)) |value| {
        return try builder.emitInst(.{ .opcode = .const_i, .type_id = type_id, .data = .{ .const_i = value } });
    }
    const type_data = builder.sema.type_pool.types.items[type_id];
    const token = builder.sema.ast_tree.tokens[node.main_token];
    const source = builder.sema.diags.source_manager.getFile(builder.sema.source_id).?.content;
    const name = source[token.start..token.end];

    if (type_data.data == .function) {
        const symbol = try builder.lir.internModuleSymbol(builder.sema.module_id orelse 0, name);
        return try builder.emitInst(.{
            .opcode = .func_sym,
            .type_id = type_id,
            .data = .{ .func_sym = symbol },
        });
    }

    const address = builder.var_addresses.get(name) orelse return null;
    const result = try builder.emitInst(.{
        .opcode = .load,
        .type_id = type_id,
        .data = .{ .load = .{ .ptr = address } },
    });
    if (builder.var_slice_lengths.get(name)) |length| try builder.slice_lengths.put(result, length);
    return result;
}
