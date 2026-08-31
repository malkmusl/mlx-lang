const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Type = @import("../type.zig").Type;

pub fn definitelyReturns(sema: anytype, node_index: Node.Index) bool {
    const node = sema.ast_tree.nodes.get(node_index);
    return switch (node.tag) {
        .return_stmt => true,
        .block => blk: {
            var index = node.data.lhs;
            while (index < node.data.rhs) : (index += 1) {
                if (definitelyReturns(sema, sema.ast_tree.extra_data[index])) break :blk true;
            }
            break :blk false;
        },
        .if_stmt => blk: {
            const start = node.data.lhs;
            if (node.data.rhs < start + 3) break :blk false;
            break :blk definitelyReturns(sema, sema.ast_tree.extra_data[start + 1]) and
                definitelyReturns(sema, sema.ast_tree.extra_data[start + 2]);
        },
        .unsafe_block => definitelyReturns(sema, node.data.lhs),
        else => false,
    };
}

pub fn allowsSuccessfulFallthrough(sema: anytype, return_type_id: Type.Id) bool {
    const return_type = sema.type_pool.get(return_type_id);
    if (isPrimitive(return_type, .void_type)) return true;
    if (return_type.data != .error_union) return false;
    return isPrimitive(sema.type_pool.get(return_type.data.error_union.payload), .void_type);
}

pub fn isDiscardedExpression(sema: anytype, node_index: Node.Index) bool {
    return switch (sema.ast_tree.nodes.get(node_index).tag) {
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

pub fn findLoopTarget(sema: anytype, label_token: u32) ?usize {
    if (sema.loop_stack.items.len == 0) return null;
    if (label_token == std.math.maxInt(u32)) return sema.loop_stack.items.len - 1;
    const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
    const wanted_token = sema.ast_tree.tokens[label_token];
    const wanted = source[wanted_token.start..wanted_token.end];
    var index = sema.loop_stack.items.len;
    while (index > 0) {
        index -= 1;
        const candidate_token = sema.loop_stack.items[index].label_token;
        if (candidate_token == std.math.maxInt(u32)) continue;
        const candidate = sema.ast_tree.tokens[candidate_token];
        if (std.mem.eql(u8, wanted, source[candidate.start..candidate.end])) return index;
    }
    return null;
}

fn isPrimitive(value: Type, primitive: Type.Primitive) bool {
    return value.data == .primitive and value.data.primitive == primitive;
}
