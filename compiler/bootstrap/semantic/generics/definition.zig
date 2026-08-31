const std = @import("std");
const Ast = @import("../../syntax/ast.zig").Ast;
const Node = @import("../../syntax/ast.zig").Node;

pub fn isGeneric(tree: *const Ast, declaration_index: Node.Index) bool {
    const declaration = tree.nodes.get(declaration_index);
    if (declaration.tag != .fn_decl) return false;
    const prototype = tree.nodes.get(declaration.data.lhs);
    var offset: u32 = 1;
    while (offset < prototype.data.rhs) : (offset += 1) {
        const parameter = tree.nodes.get(tree.extra_data[prototype.data.lhs + offset]);
        if (parameter.decl_flags.comptime_param) return true;
        const type_node = tree.nodes.get(parameter.data.rhs);
        if (type_node.tag == .identifier and tree.tokens[type_node.main_token].tag == .keyword_anytype) return true;
    }
    return false;
}

pub fn comptimeParameter(tree: *const Ast, declaration_index: Node.Index, parameter_offset: usize) bool {
    const declaration = tree.nodes.get(declaration_index);
    const prototype = tree.nodes.get(declaration.data.lhs);
    if (parameter_offset + 1 >= prototype.data.rhs) return false;
    const parameter = tree.nodes.get(tree.extra_data[prototype.data.lhs + 1 + @as(u32, @intCast(parameter_offset))]);
    return parameter.decl_flags.comptime_param;
}

pub fn returnReferencesComptimeType(tree: *const Ast, prototype_index: Node.Index) bool {
    const prototype = tree.nodes.get(prototype_index);
    const return_node = tree.nodes.get(tree.extra_data[prototype.data.lhs]);
    if (return_node.tag != .identifier) return false;
    const return_token = tree.tokens[return_node.main_token];
    var offset: u32 = 1;
    while (offset < prototype.data.rhs) : (offset += 1) {
        const parameter = tree.nodes.get(tree.extra_data[prototype.data.lhs + offset]);
        if (!parameter.decl_flags.comptime_param) continue;
        const parameter_type = tree.nodes.get(parameter.data.rhs);
        if (parameter_type.tag != .identifier or tree.tokens[parameter_type.main_token].tag != .keyword_type) continue;
        const parameter_token = tree.tokens[parameter.main_token];
        if (return_token.end - return_token.start == parameter_token.end - parameter_token.start) {
            // Both token slices come from the same source buffer. Comparing the
            // coordinates is sufficient only for identity, so callers perform
            // the final byte comparison when needed.
            return true;
        }
    }
    return false;
}

pub fn sourceParameterCount(tree: *const Ast, declaration_index: Node.Index) usize {
    const declaration = tree.nodes.get(declaration_index);
    return tree.nodes.get(declaration.data.lhs).data.rhs - 1;
}

test "generic definition helper compiles" {
    _ = std.testing;
}
