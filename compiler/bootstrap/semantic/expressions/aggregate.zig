const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Scope = @import("../scope.zig").Scope;
const Type = @import("../type.zig").Type;

pub fn analyze(sema: anytype, node_index: Node.Index, scope: *Scope) !Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    _ = try sema.analyzeNode(node.data.lhs, scope);
    const aggregate_type = sema.type_values.get(node.data.lhs) orelse {
        try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "Aggregate literal prefix must name an aggregate type");
        const fallback = try sema.type_pool.internPrimitive(.void_type);
        try sema.node_types.put(node_index, fallback);
        return fallback;
    };
    const aggregate = sema.type_pool.get(aggregate_type);
    if (aggregate.data != .@"struct" and aggregate.data != .@"union") {
        try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "Aggregate literal requires a struct or union type");
    }

    const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
    const start = node.data.rhs;
    const count = sema.ast_tree.extra_data[start];
    var initialized = std.StringHashMap(void).init(sema.allocator);
    defer initialized.deinit();
    var index: u32 = 0;
    while (index < count) : (index += 1) {
        const name_token_index = sema.ast_tree.extra_data[start + 1 + index * 2];
        const value_node = sema.ast_tree.extra_data[start + 2 + index * 2];
        const token = sema.ast_tree.tokens[name_token_index];
        const name = source[token.start..token.end];
        const field = sema.type_pool.aggregateField(aggregate_type, name) orelse {
            try sema.reportError(3001, .resolve, token.start, "Unknown aggregate field");
            _ = try sema.analyzeNode(value_node, scope);
            continue;
        };
        if (initialized.contains(name)) try sema.reportError(3002, .resolve, token.start, "Aggregate field initialized more than once");
        try initialized.put(name, {});
        const value_type = try sema.analyzeNode(value_node, scope);
        if (!sema.type_pool.isCoercible(value_type, field.type_id)) {
            try sema.reportError(4001, .sema, token.start, "Aggregate field initializer has the wrong type");
        }
    }
    if (aggregate.data == .@"struct") {
        if (sema.type_pool.aggregateFields(aggregate_type)) |fields| for (fields) |field| {
            if (!initialized.contains(field.name)) {
                try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "Aggregate literal does not initialize every field");
                break;
            }
        };
    } else if (count != 1) {
        try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "Union literal must initialize exactly one field");
    }
    try sema.node_types.put(node_index, aggregate_type);
    return aggregate_type;
}
