//! Extracts a module's declaration namespace after semantic analysis.

const std = @import("std");
const Sema = @import("sema.zig").Sema;
const namespace = @import("../modules/namespace.zig");
const ModuleId = @import("../modules/resolver.zig").ModuleId;

pub fn collect(sema: *Sema, registry: *namespace.Registry, module_id: ModuleId) !void {
    const tree = &sema.ast_tree;
    const root = tree.nodes.get(tree.nodes.len - 1);
    const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
    var offset = root.data.lhs;
    while (offset < root.data.rhs) : (offset += 1) {
        const declaration_index = tree.extra_data[offset];
        const declaration = tree.nodes.get(declaration_index);
        const name_token = switch (declaration.tag) {
            .const_decl, .var_decl => declaration.data.lhs,
            .fn_decl => tree.nodes.get(declaration.data.lhs).main_token,
            else => continue,
        };
        const token = tree.tokens[name_token];
        const name = source[token.start..token.end];
        const type_id = sema.node_types.get(declaration_index) orelse continue;
        try registry.put(module_id, name, .{
            .type_id = type_id,
            .public = declaration.decl_flags.public,
            .const_value = sema.const_values.get(declaration_index),
            .type_value = sema.type_values.get(declaration_index),
            .module_value = sema.module_values.get(declaration_index),
            .is_function = declaration.tag == .fn_decl,
        });
    }
}
