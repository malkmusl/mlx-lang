const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Scope = @import("../scope.zig").Scope;
const Type = @import("../type.zig").Type;
const TypePool = @import("../type.zig").TypePool;

/// Error tags use non-zero internal values because zincc reserves zero for a
/// successful error-union return. The values are deliberately not a stable
/// external ABI; explicitly-backed enums serve that purpose in Zin.
pub fn analyze(sema: anytype, node_index: Node.Index, scope: *Scope) !Type.Id {
    _ = scope;
    if (sema.type_values.get(node_index)) |_| return sema.type_pool.internPrimitive(.type_type);
    const node = sema.ast_tree.nodes.get(node_index);
    const backing = try sema.type_pool.internInt(false, 32);
    var fields = std.ArrayList(TypePool.AggregateFieldInput).empty;
    defer fields.deinit(sema.allocator);
    var names = std.StringHashMap(void).init(sema.allocator);
    defer names.deinit();
    const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
    var offset = node.data.lhs;
    var value: u64 = 1;
    while (offset < node.data.rhs) : (offset += 1) {
        const member = sema.ast_tree.nodes.get(sema.ast_tree.extra_data[offset]);
        const token = sema.ast_tree.tokens[member.main_token];
        const name = source[token.start..token.end];
        if (names.contains(name)) {
            try sema.reportError(3002, .resolve, token.start, "Error-set member is declared more than once");
        } else try names.put(name, {});
        try fields.append(sema.allocator, .{ .name = name, .type_id = backing, .value = value });
        value += 1;
    }
    const error_set = sema.type_pool.internAggregate(.error_set, fields.items, backing, null, false) catch {
        try sema.reportError(5005, .@"comptime", sema.ast_tree.tokens[node.main_token].start, "Error-set layout could not be computed");
        return sema.type_pool.internPrimitive(.type_type);
    };
    try sema.type_values.put(node_index, error_set);
    const type_type = try sema.type_pool.internPrimitive(.type_type);
    try sema.node_types.put(node_index, type_type);
    return type_type;
}
