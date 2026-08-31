const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Scope = @import("../scope.zig").Scope;
const Type = @import("../type.zig").Type;
const TypePool = @import("../type.zig").TypePool;
const integer_semantics = @import("../numbers/integer.zig");

/// Resolves and validates a struct, enum, or union type declaration. Keeping
/// aggregate construction here prevents the central semantic dispatcher from
/// accumulating layout, tag, and member-validation policy.
pub fn analyze(sema: anytype, node_index: Node.Index, scope: *Scope) !Type.Id {
    if (sema.type_values.get(node_index)) |_| return sema.type_pool.internPrimitive(.type_type);

    const node = sema.ast_tree.nodes.get(node_index);
    const kind: TypePool.AggregateKind = switch (node.tag) {
        .struct_decl => .@"struct",
        .enum_decl => .@"enum",
        .union_decl => .@"union",
        else => unreachable,
    };
    const backing_node = sema.ast_tree.extra_data[node.data.lhs];
    var backing_type: ?Type.Id = null;
    if (backing_node != std.math.maxInt(u32)) {
        backing_type = try sema.resolveBuiltinTypeArg(backing_node, scope) orelse {
            try sema.reportError(5005, .@"comptime", sema.ast_tree.tokens[node.main_token].start, "Aggregate backing/tag must be a type");
            return sema.type_pool.internPrimitive(.type_type);
        };
        const backing = sema.type_pool.get(backing_type.?);
        if (kind == .@"enum" and !backing.isInteger()) {
            try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "Enum backing type must be an integer");
        }
        if (kind == .@"union" and !backing.isInteger() and backing.data != .@"enum") {
            try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "Tagged union tag must be an integer-backed enum or integer type");
        }
    }

    var fields = std.ArrayList(TypePool.AggregateFieldInput).empty;
    defer fields.deinit(sema.allocator);
    var names = std.StringHashMap(void).init(sema.allocator);
    defer names.deinit();
    var enum_values = std.AutoHashMap(u64, void).init(sema.allocator);
    defer enum_values.deinit();

    const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
    var extra_index = node.data.lhs + 1;
    var next_tag_value: u64 = 0;
    while (extra_index < node.data.rhs) : (extra_index += 1) {
        const member_index = sema.ast_tree.extra_data[extra_index];
        const member = sema.ast_tree.nodes.get(member_index);
        const name_token = sema.ast_tree.tokens[member.main_token];
        const name = source[name_token.start..name_token.end];
        if (names.contains(name)) {
            try sema.reportError(3002, .resolve, name_token.start, "Aggregate member is declared more than once");
        } else try names.put(name, {});

        const member_type: Type.Id = switch (member.tag) {
            .field_decl => try sema.resolveBuiltinTypeArg(member.data.lhs, scope) orelse {
                try sema.reportError(5005, .@"comptime", name_token.start, "Struct field type could not be resolved");
                continue;
            },
            .union_member => if (member.data.lhs == std.math.maxInt(u32))
                try sema.type_pool.internPrimitive(.void_type)
            else
                try sema.resolveBuiltinTypeArg(member.data.lhs, scope) orelse {
                    try sema.reportError(5005, .@"comptime", name_token.start, "Union member type could not be resolved");
                    continue;
                },
            .enum_member => backing_type orelse {
                try sema.reportError(5005, .@"comptime", name_token.start, "Enum has no backing type");
                continue;
            },
            else => continue,
        };

        var tag_value: ?u64 = null;
        if (member.tag == .enum_member) {
            if (member.data.rhs != std.math.maxInt(u32)) {
                _ = try sema.analyzeNode(member.data.rhs, scope);
                tag_value = sema.const_values.get(member.data.rhs) orelse blk: {
                    try sema.reportError(5001, .@"comptime", name_token.start, "Enum value must be a comptime integer");
                    break :blk next_tag_value;
                };
            } else tag_value = next_tag_value;
            next_tag_value = tag_value.? +% 1;
            if (!integer_semantics.valueFits(sema.type_pool.get(backing_type.?), tag_value.?)) {
                try sema.reportError(4002, .sema, name_token.start, "Enum value does not fit its backing type");
            }
            if (enum_values.contains(tag_value.?)) {
                try sema.reportError(4007, .sema, name_token.start, "Enum values must be unique");
            } else try enum_values.put(tag_value.?, {});
        } else if (member.tag == .union_member and backing_type != null) {
            const backing = sema.type_pool.get(backing_type.?);
            if (backing.data == .@"enum") {
                const tag_member = sema.type_pool.aggregateField(backing_type.?, name) orelse {
                    try sema.reportError(4001, .sema, name_token.start, "Union member has no matching explicit tag-enum member");
                    continue;
                };
                tag_value = tag_member.value;
            } else {
                tag_value = next_tag_value;
                next_tag_value +%= 1;
                if (!integer_semantics.valueFits(backing, tag_value.?)) {
                    try sema.reportError(4002, .sema, name_token.start, "Union discriminator does not fit its backing type");
                }
            }
        }

        try fields.append(sema.allocator, .{ .name = name, .type_id = member_type, .value = tag_value });
    }

    if (kind == .@"union" and backing_type != null and sema.type_pool.get(backing_type.?).data == .@"enum") {
        const expected = sema.type_pool.aggregateInfo(backing_type.?) orelse unreachable;
        if (fields.items.len != expected.fields_len) {
            try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "Explicit union tag enum and union members must have matching names");
        }
    }

    const aggregate_type = sema.type_pool.internAggregate(
        kind,
        fields.items,
        backing_type,
        null,
        node.decl_flags.aggregate_nonexhaustive,
    ) catch {
        try sema.reportError(5005, .@"comptime", sema.ast_tree.tokens[node.main_token].start, "Aggregate layout could not be computed");
        return sema.type_pool.internPrimitive(.type_type);
    };
    try sema.type_values.put(node_index, aggregate_type);
    const result_type = try sema.type_pool.internPrimitive(.type_type);
    try sema.node_types.put(node_index, result_type);
    return result_type;
}
