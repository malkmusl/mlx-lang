const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Type = @import("../../semantic/type.zig").Type;
const Inst = @import("../lir.zig").Inst;

pub fn lower(builder: anytype, node_idx: Node.Index) std.mem.Allocator.Error!?Inst.Index {
    const node = builder.sema.ast_tree.nodes.get(node_idx);
    const expression = if (node.data.rhs == std.math.maxInt(u32))
        null
    else
        try builder.lowerNode(node.data.rhs);
    const return_type = builder.current_return_type.?;
    const expression_type = if (expression != null) builder.sema.type_pool.get(builder.sema.node_types.get(node.data.rhs).?) else null;
    const is_error_union = expression_type != null and expression_type.?.data == .error_union;
    const is_error_value = expression_type != null and expression_type.?.data == .error_set;
    try builder.emitCleanups(0, is_error_value);
    if (is_error_value) {
        _ = try builder.emitInst(.{ .opcode = .ret_error, .type_id = return_type, .data = .{ .ret_error = expression.? } });
    } else if (is_error_union and isSliceErrorUnion(builder, return_type)) {
        const length = builder.slice_lengths.get(expression.?) orelse return null;
        _ = try builder.emitInst(.{
            .opcode = .ret_error_union_slice,
            .type_id = return_type,
            .data = .{ .ret_error_union_slice = .{ .source = expression.?, .len = length } },
        });
    } else if (is_error_union) {
        _ = try builder.emitInst(.{ .opcode = .ret_error_union, .type_id = return_type, .data = .{ .ret_error_union = expression.? } });
    } else if (expression != null and isSliceErrorUnion(builder, return_type)) {
        const length = builder.slice_lengths.get(expression.?) orelse return null;
        _ = try builder.emitInst(.{
            .opcode = .ret_error_slice,
            .type_id = return_type,
            .data = .{ .ret_error_slice = .{ .ptr = expression.?, .len = length } },
        });
    } else {
        _ = try builder.emitInst(.{ .opcode = .ret, .type_id = return_type, .data = .{ .ret = expression } });
    }
    return null;
}

fn isSliceErrorUnion(builder: anytype, type_id: Type.Id) bool {
    const ty = builder.sema.type_pool.get(type_id);
    if (ty.data != .error_union) return false;
    const payload = builder.sema.type_pool.get(ty.data.error_union.payload);
    return payload.data == .pointer and payload.data.pointer.size == .Slice;
}
