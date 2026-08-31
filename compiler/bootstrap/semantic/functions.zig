const std = @import("std");
const Node = @import("../syntax/ast.zig").Node;
const Type = @import("type.zig").Type;
const Scope = @import("scope.zig").Scope;

pub fn findRoot(sema: anytype, name: []const u8) ?Node.Index {
    const root = sema.ast_tree.nodes.get(sema.ast_tree.nodes.len - 1);
    if (root.tag != .root) return null;
    const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
    var index = root.data.lhs;
    while (index < root.data.rhs) : (index += 1) {
        const declaration_index = sema.ast_tree.extra_data[index];
        const declaration = sema.ast_tree.nodes.get(declaration_index);
        if (declaration.tag != .fn_decl) continue;
        const prototype = sema.ast_tree.nodes.get(declaration.data.lhs);
        const token = sema.ast_tree.tokens[prototype.main_token];
        if (std.mem.eql(u8, source[token.start..token.end], name)) return declaration_index;
    }
    return null;
}

pub fn nodeNamesGenericTypeParameter(sema: anytype, prototype_index: Node.Index, type_node_index: Node.Index) bool {
    const type_node = sema.ast_tree.nodes.get(type_node_index);
    if (type_node.tag != .identifier) return false;
    const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
    const type_token = sema.ast_tree.tokens[type_node.main_token];
    const prototype = sema.ast_tree.nodes.get(prototype_index);
    var offset: u32 = 1;
    while (offset < prototype.data.rhs) : (offset += 1) {
        const parameter = sema.ast_tree.nodes.get(sema.ast_tree.extra_data[prototype.data.lhs + offset]);
        if (!parameter.decl_flags.comptime_param) continue;
        const parameter_type = sema.ast_tree.nodes.get(parameter.data.rhs);
        if (parameter_type.tag != .identifier or sema.ast_tree.tokens[parameter_type.main_token].tag != .keyword_type) continue;
        const parameter_token = sema.ast_tree.tokens[parameter.main_token];
        if (std.mem.eql(u8, source[type_token.start..type_token.end], source[parameter_token.start..parameter_token.end])) return true;
    }
    return false;
}

pub fn declare(sema: anytype, declaration_index: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
    const declaration = sema.ast_tree.nodes.get(declaration_index);
    const prototype_index = declaration.data.lhs;
    const prototype = sema.ast_tree.nodes.get(prototype_index);
    const function_type = sema.node_types.get(prototype_index) orelse try sema.analyzeNode(prototype_index, scope);
    const token = sema.ast_tree.tokens[prototype.main_token];
    const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
    const name = source[token.start..token.end];
    try scope.put(name, .{
        .name = name,
        .decl_node = declaration_index,
        .type_id = function_type,
        .is_const = true,
    });
    try sema.node_types.put(declaration_index, function_type);
    return function_type;
}

pub fn bindParameters(sema: anytype, prototype_index: Node.Index, function_type: Type.Id, scope: *Scope) std.mem.Allocator.Error!void {
    const prototype = sema.ast_tree.nodes.get(prototype_index);
    const function = sema.type_pool.get(function_type).data.function;
    const parameter_types = sema.type_pool.functionParams(function);
    const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
    var parameter_offset: u32 = 0;
    while (parameter_offset < parameter_types.len) : (parameter_offset += 1) {
        const parameter_index = sema.ast_tree.extra_data[prototype.data.lhs + 1 + parameter_offset];
        const parameter = sema.ast_tree.nodes.get(parameter_index);
        const token = sema.ast_tree.tokens[parameter.main_token];
        const name = source[token.start..token.end];
        try scope.put(name, .{
            .name = name,
            .decl_node = parameter_index,
            .type_id = parameter_types[parameter_offset],
            .is_const = true,
        });
        try sema.local_states.put(parameter_index, .initialized);
        try sema.node_types.put(parameter_index, parameter_types[parameter_offset]);
    }
}
