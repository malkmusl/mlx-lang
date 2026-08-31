const Node = @import("../../syntax/ast.zig").Node;
const Scope = @import("../scope.zig").Scope;

pub fn sourceIsMoved(sema: anytype, node_index: Node.Index, scope: *Scope) bool {
    const node = sema.ast_tree.nodes.get(node_index);
    if (node.tag != .identifier) return false;
    const token = sema.ast_tree.tokens[node.main_token];
    const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
    const symbol = scope.get(source[token.start..token.end]) orelse return false;
    return (sema.local_states.get(symbol.decl_node) orelse return false) == .moved;
}
