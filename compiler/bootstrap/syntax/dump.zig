const std = @import("std");
const ast = @import("ast.zig");
const Token = @import("token.zig").Token;

pub fn AstDumperType(comptime WriterType: type) type {
    return struct {
        const Self = @This();
        tree: *const ast.Ast,
        source: []const u8,
        writer: WriterType,

        pub fn init(tree: *const ast.Ast, source: []const u8, writer: WriterType) Self {
            return .{
                .tree = tree,
                .source = source,
                .writer = writer,
            };
        }

        pub fn dump(self: Self) !void {
            if (self.tree.nodes.len == 0) {
                try self.writer.print("Empty AST\n", .{});
                return;
            }
            const root_idx: u32 = @intCast(self.tree.nodes.len - 1);
            try self.dumpNode(root_idx, 0);
        }

        fn printIndent(self: Self, indent: usize) !void {
            var i: usize = 0;
            while (i < indent) : (i += 1) {
                try self.writer.print("  ", .{});
            }
        }

        fn tokenString(self: Self, token_idx: u32) []const u8 {
            if (token_idx >= self.tree.tokens.len) return "";
            const tok = self.tree.tokens[token_idx];
            return self.source[tok.start..tok.end];
        }

        fn dumpNode(self: Self, node_idx: ast.Node.Index, indent: usize) anyerror!void {
            if (node_idx >= self.tree.nodes.len) {
                try self.printIndent(indent);
                try self.writer.print("<<INVALID NODE {d}>>\n", .{node_idx});
                return;
            }

            const tag = self.tree.nodes.items(.tag)[node_idx];
            const main_token = self.tree.nodes.items(.main_token)[node_idx];
            const data = self.tree.nodes.items(.data)[node_idx];

            try self.printIndent(indent);
            try self.writer.print("{s}", .{@tagName(tag)});
            
            // Print main token if it's meaningful for the node type
            switch (tag) {
                .identifier, .integer_literal, .float_literal, .string_literal, .char_literal, .binary_op, .unary_op => {
                    try self.writer.print(" '{s}'", .{self.tokenString(main_token)});
                },
                else => {},
            }
            try self.writer.print("\n", .{});

            const child_indent = indent + 1;

            switch (tag) {
                .root => {
                    // lhs is extra_start, rhs is extra_end
                    const extra = self.tree.extra_data[data.lhs..data.rhs];
                    for (extra) |child| {
                        try self.dumpNode(child, child_indent);
                    }
                },
                .const_decl, .var_decl => {
                    try self.printIndent(child_indent);
                    try self.writer.print("name: '{s}'\n", .{self.tokenString(data.lhs)});
                    if (data.rhs != 0) {
                        const type_node = self.tree.extra_data[data.rhs + 0];
                        const init_node = self.tree.extra_data[data.rhs + 1];
                        if (type_node != 0) {
                            try self.printIndent(child_indent);
                            try self.writer.print("type:\n", .{});
                            try self.dumpNode(type_node, child_indent + 1);
                        }
                        if (init_node != 0) {
                            try self.printIndent(child_indent);
                            try self.writer.print("init:\n", .{});
                            try self.dumpNode(init_node, child_indent + 1);
                        }
                    } else {
                        if (data.rhs != 0) {
                            try self.printIndent(child_indent);
                            try self.writer.print("init:\n", .{});
                            try self.dumpNode(data.rhs, child_indent + 1);
                        }
                    }
                },
                .fn_decl => {
                    try self.dumpNode(data.lhs, child_indent);
                    if (data.rhs != 0) {
                        try self.dumpNode(data.rhs, child_indent);
                    }
                },
                .fn_proto => {
                    try self.printIndent(child_indent);
                    try self.writer.print("name: '{s}'\n", .{self.tokenString(main_token)});
                    const ret_type = self.tree.extra_data[data.lhs];
                    if (ret_type != 0) {
                        try self.printIndent(child_indent);
                        try self.writer.print("return_type:\n", .{});
                        try self.dumpNode(ret_type, child_indent + 1);
                    }
                    if (data.rhs > 1) {
                        const param_count = data.rhs - 1;
                        try self.printIndent(child_indent);
                        try self.writer.print("params:\n", .{});
                        const params = self.tree.extra_data[data.lhs + 1 .. data.lhs + 1 + param_count];
                        for (params) |param| {
                            try self.dumpNode(param, child_indent + 1);
                        }
                    }
                },
                .param_decl => {
                    if (data.lhs != std.math.maxInt(u32)) {
                        try self.printIndent(child_indent);
                        try self.writer.print("name: '{s}'\n", .{self.tokenString(data.lhs)});
                    }
                    if (data.rhs != 0) {
                        try self.dumpNode(data.rhs, child_indent);
                    }
                },
                .block => {
                    const extra = self.tree.extra_data[data.lhs..data.rhs];
                    for (extra) |child| {
                        try self.dumpNode(child, child_indent);
                    }
                },
                .binary_op => {
                    try self.dumpNode(data.lhs, child_indent);
                    try self.dumpNode(data.rhs, child_indent);
                },
                .unary_op, .pointer_type, .optional_type => {
                    if (data.lhs != 0) try self.dumpNode(data.lhs, child_indent);
                },
                .call => {
                    try self.printIndent(child_indent);
                    try self.writer.print("callee:\n", .{});
                    try self.dumpNode(data.lhs, child_indent + 1);
                    
                    const arg_count = self.tree.extra_data[data.rhs];
                    if (arg_count > 0) {
                        try self.printIndent(child_indent);
                        try self.writer.print("args:\n", .{});
                        const args = self.tree.extra_data[data.rhs + 1 .. data.rhs + 1 + arg_count];
                        for (args) |arg| {
                            try self.dumpNode(arg, child_indent + 1);
                        }
                    }
                },
                .array_type => {
                    const size_expr = self.tree.extra_data[data.lhs + 0];
                    const elem_type = self.tree.extra_data[data.lhs + 1];
                    if (size_expr != 0) {
                        try self.printIndent(child_indent);
                        try self.writer.print("size:\n", .{});
                        try self.dumpNode(size_expr, child_indent + 1);
                    }
                    if (elem_type != 0) {
                        try self.printIndent(child_indent);
                        try self.writer.print("child:\n", .{});
                        try self.dumpNode(elem_type, child_indent + 1);
                    }
                },
                .if_stmt => {
                    const cond = self.tree.extra_data[data.lhs + 0];
                    const then_blk = self.tree.extra_data[data.lhs + 1];
                    try self.printIndent(child_indent);
                    try self.writer.print("condition:\n", .{});
                    try self.dumpNode(cond, child_indent + 1);
                    
                    try self.printIndent(child_indent);
                    try self.writer.print("then:\n", .{});
                    try self.dumpNode(then_blk, child_indent + 1);
                    
                    if (data.rhs > data.lhs + 2) {
                        const else_blk = self.tree.extra_data[data.lhs + 2];
                        try self.printIndent(child_indent);
                        try self.writer.print("else:\n", .{});
                        try self.dumpNode(else_blk, child_indent + 1);
                    }
                },
                .while_stmt => {
                    const cond = self.tree.extra_data[data.lhs + 1];
                    const body = self.tree.extra_data[data.lhs + 2];
                    try self.printIndent(child_indent);
                    try self.writer.print("condition:\n", .{});
                    try self.dumpNode(cond, child_indent + 1);
                    
                    try self.printIndent(child_indent);
                    try self.writer.print("body:\n", .{});
                    try self.dumpNode(body, child_indent + 1);
                },
                .return_stmt => {
                    if (data.rhs != std.math.maxInt(u32) and data.rhs != 0) {
                        try self.dumpNode(data.rhs, child_indent);
                    }
                },
                .field_access => {
                    try self.printIndent(child_indent);
                    try self.writer.print("base:\n", .{});
                    try self.dumpNode(data.lhs, child_indent + 1);
                    try self.printIndent(child_indent);
                    try self.writer.print("field: '{s}'\n", .{self.tokenString(data.rhs)});
                },
                .struct_decl, .enum_decl, .union_decl => {
                    const extra = self.tree.extra_data[data.lhs..data.rhs];
                    for (extra) |child| {
                        try self.dumpNode(child, child_indent);
                    }
                },
                .field_decl => {
                    try self.printIndent(child_indent);
                    try self.writer.print("name: '{s}'\n", .{self.tokenString(main_token)});
                    if (data.lhs != 0) {
                        try self.printIndent(child_indent);
                        try self.writer.print("type:\n", .{});
                        try self.dumpNode(data.lhs, child_indent + 1);
                    }
                    if (data.rhs != 0) {
                        try self.printIndent(child_indent);
                        try self.writer.print("init:\n", .{});
                        try self.dumpNode(data.rhs, child_indent + 1);
                    }
                },
                .builtin_call => {
                    try self.printIndent(child_indent);
                    try self.writer.print("builtin: '{s}'\n", .{self.tokenString(data.lhs)});
                    const arg_count = self.tree.extra_data[data.rhs];
                    if (arg_count > 0) {
                        try self.printIndent(child_indent);
                        try self.writer.print("args:\n", .{});
                        const args = self.tree.extra_data[data.rhs + 1 .. data.rhs + 1 + arg_count];
                        for (args) |arg| {
                            try self.dumpNode(arg, child_indent + 1);
                        }
                    }
                },
                else => {
                    // Do not blindly recurse into lhs and rhs, they might be token indices or extra_data indices!
                    try self.printIndent(child_indent);
                    try self.writer.print("lhs: {d}\n", .{data.lhs});
                    try self.printIndent(child_indent);
                    try self.writer.print("rhs: {d}\n", .{data.rhs});
                }
            }
        }
    };
}

pub fn dump(tree: *const ast.Ast, source: []const u8, writer: anytype) !void {
    const Dumper = AstDumperType(@TypeOf(writer));
    var dumper = Dumper.init(tree, source, writer);
    try dumper.dump();
}
