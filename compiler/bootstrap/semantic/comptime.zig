const std = @import("std");
const ast = @import("../syntax/ast.zig");
const Node = ast.Node;

pub const ComptimeVM = struct {
    const trace_enabled = false;
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
        const sema = @as(*@import("sema.zig").Sema, @ptrCast(@alignCast(self.sema)));
        if (sema.const_values.get(node_idx)) |value| return .{ .integer = value };
        if (sema.type_values.get(node_idx)) |type_value| return .{ .integer = type_value };

        const node = self.ast_tree.nodes.get(node_idx);
        if (trace_enabled) std.debug.print("-> ENTER: ComptimeVM.evaluate | Tag: {s}\n", .{@tagName(node.tag)});
        defer if (trace_enabled) std.debug.print("<- EXIT: ComptimeVM.evaluate | Tag: {s}\n", .{@tagName(node.tag)});

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
            else => return .{ .err = "Expression cannot be evaluated at comptime" },
        }
    }
};
