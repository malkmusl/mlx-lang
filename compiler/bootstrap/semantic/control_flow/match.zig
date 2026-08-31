const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Scope = @import("../scope.zig").Scope;
const Type = @import("../type.zig").Type;

pub fn analyze(sema: anytype, node_index: Node.Index, scope: *Scope) !Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    const start = node.data.lhs;
    const subject_id = try sema.analyzeNode(sema.ast_tree.extra_data[start], scope);
    const subject = sema.type_pool.get(subject_id);
    if (!subject.isInteger() and subject.data != .@"enum" and subject.data != .@"union") {
        try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "match subject must be an integer, enum, or tagged union");
    }

    const arm_count = sema.ast_tree.extra_data[start + 1];
    var has_else = false;
    var named_members = std.StringHashMap(void).init(sema.allocator);
    defer named_members.deinit();
    var result_type: ?Type.Id = null;
    var arm: u32 = 0;
    while (arm < arm_count) : (arm += 1) {
        const arm_start = start + 2 + arm * 4;
        const kind = sema.ast_tree.extra_data[arm_start];
        const first = sema.ast_tree.extra_data[arm_start + 1];
        const second = sema.ast_tree.extra_data[arm_start + 2];
        const body = sema.ast_tree.extra_data[arm_start + 3];
        if (kind == 0) {
            if (has_else) try sema.reportError(4005, .sema, sema.ast_tree.tokens[node.main_token].start, "match contains more than one else arm");
            has_else = true;
        } else {
            if (kind == 3) {
                const pattern = sema.ast_tree.nodes.get(first);
                const token = sema.ast_tree.tokens[pattern.main_token];
                const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
                const name = source[token.start..token.end];
                const field = sema.type_pool.aggregateField(subject_id, name) orelse {
                    try sema.reportError(4006, .sema, token.start, "Unknown enum value in match pattern");
                    continue;
                };
                try sema.node_types.put(first, subject_id);
                try sema.const_values.put(first, field.value orelse 0);
                try named_members.put(name, {});
            } else try requirePatternType(sema, first, subject_id, scope);
            if (kind == 2) try requirePatternType(sema, second, subject_id, scope);
        }
        const body_type = try sema.analyzeNode(body, scope);
        if (result_type) |previous| {
            const previous_value = sema.type_pool.get(previous);
            const body_value = sema.type_pool.get(body_type);
            if (previous_value.data == .primitive and previous_value.data.primitive == .noreturn_type) {
                result_type = body_type;
            } else if (!(body_value.data == .primitive and body_value.data.primitive == .noreturn_type)) {
                if (sema.type_pool.isCoercible(body_type, previous)) {
                    // Keep the established result type.
                } else if (sema.type_pool.isCoercible(previous, body_type)) {
                    result_type = body_type;
                } else {
                    try sema.reportError(4001, .sema, sema.ast_tree.tokens[node.main_token].start, "match arms have incompatible result types");
                }
            }
        } else result_type = body_type;
    }
    const aggregate_info = if (subject.data == .@"enum" or subject.data == .@"union") sema.type_pool.aggregateInfo(subject_id) else null;
    const closed_aggregate_exhaustive = aggregate_info != null and !aggregate_info.?.is_nonexhaustive and
        named_members.count() == aggregate_info.?.fields_len;
    if (!has_else and !closed_aggregate_exhaustive and (subject.isInteger() or subject.data == .@"enum" or subject.data == .@"union")) {
        try sema.reportError(4005, .sema, sema.ast_tree.tokens[node.main_token].start, "match is not exhaustive; add an else arm");
    }
    const result = result_type orelse try sema.type_pool.internPrimitive(.void_type);
    try sema.node_types.put(node_index, result);
    return result;
}

fn requirePatternType(sema: anytype, pattern: Node.Index, subject: Type.Id, scope: *Scope) !void {
    const pattern_type = try sema.analyzeNode(pattern, scope);
    if (!sema.type_pool.isCoercible(pattern_type, subject) and !sema.type_pool.isCoercible(subject, pattern_type)) {
        try sema.reportError(4001, .sema, sema.ast_tree.tokens[sema.ast_tree.nodes.get(pattern).main_token].start, "match pattern type is incompatible with the subject");
    }
}
