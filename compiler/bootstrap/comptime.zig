const std = @import("std");
const ast = @import("ast.zig");
const Node = ast.Node;

pub const ComptimeVM = struct {
    pub const Value = union(enum) {
        integer: u64,
        err: []const u8,
    };

    allocator: std.mem.Allocator,
    ast_tree: ast.Ast,
    sema: *anyopaque, // use anyopaque to avoid circular import, or just import sema

    pub fn init(allocator: std.mem.Allocator, ast_tree: ast.Ast, sema: *anyopaque) ComptimeVM {
        return .{
            .allocator = allocator,
            .ast_tree = ast_tree,
            .sema = sema,
        };
    }

    pub fn evaluate(self: *ComptimeVM, node_idx: Node.Index, src: []const u8) Value {
        const node = self.ast_tree.nodes.get(node_idx);
        std.debug.print("-> ENTER: ComptimeVM.evaluate | Tag: {s}\n", .{@tagName(node.tag)});
        defer std.debug.print("<- EXIT: ComptimeVM.evaluate | Tag: {s}\n", .{@tagName(node.tag)});

        switch (node.tag) {
            .integer_literal => {
                const tok = self.ast_tree.tokens[node.main_token];
                const text = src[tok.start..tok.end];
                const val = std.fmt.parseInt(u64, text, 10) catch 0;
                return .{ .integer = val };
            },
            .binary_op => {
                const lhs_val = self.evaluate(node.data.lhs, src);
                const rhs_val = self.evaluate(node.data.rhs, src);
                
                if (lhs_val == .err) return lhs_val;
                if (rhs_val == .err) return rhs_val;

                const op_tok = self.ast_tree.tokens[node.main_token];
                switch (op_tok.tag) {
                    .plus => return .{ .integer = lhs_val.integer + rhs_val.integer },
                    .minus => return .{ .integer = lhs_val.integer - rhs_val.integer },
                    .asterisk => return .{ .integer = lhs_val.integer * rhs_val.integer },
                    .slash => {
                        if (rhs_val.integer == 0) return .{ .err = "Division by zero" };
                        return .{ .integer = lhs_val.integer / rhs_val.integer };
                    },
                    else => return .{ .err = "Unsupported operator in comptime" },
                }
            },
            .typeof_builtin => {
                const sema = @as(*@import("sema.zig").Sema, @ptrCast(@alignCast(self.sema)));
                // @typeOf evaluates to a Type ID value
                if (sema.node_types.get(node_idx)) |type_id| {
                    // Normally a comptime type value would have a `.type` variant in Value
                    // For now, let's just use `.integer = type_id` to show it works
                    return .{ .integer = type_id };
                }
                return .{ .err = "Type not resolved" };
            },
            .sizeof_builtin => {
                const sema = @as(*@import("sema.zig").Sema, @ptrCast(@alignCast(self.sema)));
                const inner = node.data.rhs;
                if (sema.node_types.get(inner)) |type_id| {
                    const ty = sema.type_pool.get(type_id);
                    // Dummy size calculation based on primitive
                    const size: u64 = switch (ty.data) {
                        .primitive => |p| switch (p) {
                            .comptime_int_type => 8, // dummy
                            .type_type => 4, // dummy
                            else => 1,
                        },
                        else => 0,
                    };
                    return .{ .integer = size };
                }
                return .{ .err = "Size of type not resolved" };
            },
            else => return .{ .err = "Expression cannot be evaluated at comptime" },
        }
    }
};
