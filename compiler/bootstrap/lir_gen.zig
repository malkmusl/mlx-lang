const std = @import("std");
const ast = @import("ast.zig");
const Node = ast.Node;
const TypePool = @import("type.zig").TypePool;
const Sema = @import("sema.zig").Sema;
const Lir = @import("lir.zig").Lir;
const Inst = @import("lir.zig").Inst;
const builtin = @import("builtin.zig");

pub const LirBuilder = struct {
    allocator: std.mem.Allocator,
    sema: *Sema,
    lir: Lir,
    current_block: ?u32,
    var_addresses: std.StringHashMap(u32),
    param_counter: u32 = 0,

    pub fn init(allocator: std.mem.Allocator, sema: *Sema) LirBuilder {
        return .{
            .allocator = allocator,
            .sema = sema,
            .lir = Lir.init(allocator),
            .current_block = null,
            .var_addresses = std.StringHashMap(u32).init(allocator),
        };
    }

    pub fn deinit(self: *LirBuilder) void {
        self.var_addresses.deinit();
        self.lir.deinit();
    }

    pub fn generate(self: *LirBuilder) !void {
        std.debug.print("-> ENTER: LirBuilder.generate\n", .{});
        defer std.debug.print("<- EXIT: LirBuilder.generate\n", .{});

        // Ensure at least one block exists
        try self.lir.blocks.append(self.allocator, @import("lir.zig").BasicBlock.init());
        self.current_block = 0;

        const root_node = self.sema.ast_tree.nodes.get(self.sema.ast_tree.nodes.len - 1);
        if (root_node.tag != .root) return error.InvalidAst;

        const extra_start = root_node.data.lhs;
        const extra_end = root_node.data.rhs;

        var i: u32 = extra_start;
        while (i < extra_end) : (i += 1) {
            const child_idx = self.sema.ast_tree.extra_data[i];
            _ = try self.lowerNode(child_idx);
        }
    }

    fn emitInst(self: *LirBuilder, inst: Inst) !Inst.Index {
        const idx = @as(Inst.Index, @intCast(self.lir.insts.items.len));
        try self.lir.insts.append(self.allocator, inst);

        if (self.current_block) |blk_idx| {
            try self.lir.blocks.items[blk_idx].insts.append(self.allocator, idx);
        }

        return idx;
    }

    fn lowerNode(self: *LirBuilder, node_idx: Node.Index) std.mem.Allocator.Error!?Inst.Index {
        const node = self.sema.ast_tree.nodes.get(node_idx);
        std.debug.print("-> ENTER: LirBuilder.lowerNode | Tag: {s}\n", .{@tagName(node.tag)});

        var result: ?Inst.Index = null;
        defer {
            std.debug.print("<- EXIT: LirBuilder.lowerNode | Tag: {s} | Result: ", .{@tagName(node.tag)});
            if (result) |r| std.debug.print("{d}\n", .{r}) else std.debug.print("null\n", .{});
        }

        switch (node.tag) {
            .integer_literal => {
                const type_id = self.sema.node_types.get(node_idx) orelse return null;
                const src = self.sema.diags.source_manager.getFile(0).?.content;
                const tok = self.sema.ast_tree.tokens[node.main_token];
                const text = src[tok.start..tok.end];
                const val = std.fmt.parseInt(u64, text, 10) catch 0;

                result = try self.emitInst(.{
                    .opcode = .const_i,
                    .type_id = type_id,
                    .data = .{ .const_i = val },
                });
                return result;
            },
            .binary_op => {
                const type_id = self.sema.node_types.get(node_idx) orelse return null;
                const lhs_inst = try self.lowerNode(node.data.lhs) orelse return null;
                const rhs_inst = try self.lowerNode(node.data.rhs) orelse return null;

                const op_tok = self.sema.ast_tree.tokens[node.main_token];
                const opcode: @import("lir.zig").Opcode = switch (op_tok.tag) {
                    .plus => .add,
                    .minus => .sub,
                    .asterisk => .mul,
                    .slash => .div,
                    else => return null,
                };

                result = try self.emitInst(.{
                    .opcode = opcode,
                    .type_id = type_id,
                    .data = switch (opcode) {
                        .add => .{ .add = .{ .lhs = lhs_inst, .rhs = rhs_inst } },
                        .sub => .{ .sub = .{ .lhs = lhs_inst, .rhs = rhs_inst } },
                        .mul => .{ .mul = .{ .lhs = lhs_inst, .rhs = rhs_inst } },
                        .div => .{ .div = .{ .lhs = lhs_inst, .rhs = rhs_inst } },
                        else => unreachable,
                    },
                });
                return result;
            },
            .const_decl => {
                // New layout: lhs = ident_tok, rhs = extra_start
                // extra_data[rhs + 0] = type_node (0 = inferred)
                // extra_data[rhs + 1] = init_expr node
                const ident_tok_idx = node.data.lhs;
                const extra_start = node.data.rhs;
                const expr_node = self.sema.ast_tree.extra_data[extra_start + 1];

                const tok = self.sema.ast_tree.tokens[ident_tok_idx];
                const src = self.sema.diags.source_manager.getFile(0).?.content;
                const ident_name = src[tok.start..tok.end];

                const expr_inst = (try self.lowerNode(expr_node)) orelse return null;
                const type_id = self.sema.node_types.get(node_idx) orelse 0;

                const addr_inst = try self.emitInst(.{
                    .opcode = .addr,
                    .type_id = type_id,
                    .data = .{ .addr = ident_tok_idx },
                });

                try self.var_addresses.put(ident_name, addr_inst);

                _ = try self.emitInst(.{
                    .opcode = .store,
                    .type_id = type_id,
                    .data = .{ .store = .{ .ptr = addr_inst, .val = expr_inst } },
                });
                return expr_inst;
            },
            .var_decl => {
                // New layout: lhs = ident_tok, rhs = extra_start
                // extra_data[rhs + 0] = type_node (0 = inferred)
                // extra_data[rhs + 1] = init_expr node
                const ident_tok_idx = node.data.lhs;
                const extra_start = node.data.rhs;
                const init_node = self.sema.ast_tree.extra_data[extra_start + 1];

                const type_id = self.sema.node_types.get(node_idx) orelse return null;
                const rhs_inst = try self.lowerNode(init_node) orelse return null;

                const tok = self.sema.ast_tree.tokens[ident_tok_idx];
                const src = self.sema.diags.source_manager.getFile(0).?.content;
                const ident_name = src[tok.start..tok.end];

                const addr_inst = try self.emitInst(.{
                    .opcode = .addr,
                    .type_id = type_id,
                    .data = .{ .addr = ident_tok_idx },
                });

                try self.var_addresses.put(ident_name, addr_inst);

                result = try self.emitInst(.{
                    .opcode = .store,
                    .type_id = type_id,
                    .data = .{ .store = .{ .ptr = addr_inst, .val = rhs_inst } },
                });

                return result;
            },
            .identifier => {
                const type_id = self.sema.node_types.get(node_idx) orelse return null;
                const type_data = self.sema.type_pool.types.items[type_id];

                if (type_data.data == .function) {
                    return try self.emitInst(.{
                        .opcode = .func_sym,
                        .type_id = type_id,
                        .data = .{ .func_sym = node_idx },
                    });
                }

                const tok = self.sema.ast_tree.tokens[self.sema.ast_tree.nodes.get(node_idx).main_token];
                const src = self.sema.diags.source_manager.getFile(0).?.content;
                const ident_name = src[tok.start..tok.end];

                const addr_inst = self.var_addresses.get(ident_name) orelse return null;

                result = try self.emitInst(.{
                    .opcode = .load,
                    .type_id = type_id,
                    .data = .{ .load = .{ .ptr = addr_inst } },
                });
                return result;
            },
            .block => {
                const extra_start = node.data.lhs;
                const extra_end = node.data.rhs;

                var i: u32 = extra_start;
                var last_inst: ?Inst.Index = null;
                while (i < extra_end) : (i += 1) {
                    const child_idx = self.sema.ast_tree.extra_data[i];
                    last_inst = try self.lowerNode(child_idx);
                }

                return last_inst;
            },
            .if_stmt => {
                const extra_start = node.data.lhs;
                const extra_end = node.data.rhs;

                const cond_idx = self.sema.ast_tree.extra_data[extra_start];
                const cond_inst = (try self.lowerNode(cond_idx)) orelse return null;

                const then_block = try self.newBlock();
                const else_block = try self.newBlock();
                const merge_block = try self.newBlock();

                const has_else = extra_end > extra_start + 2;

                _ = try self.emitInst(.{
                    .opcode = .condbr,
                    .type_id = 0,
                    .data = .{ .condbr = .{ .cond = cond_inst, .true_dest = then_block, .false_dest = if (has_else) else_block else merge_block } },
                });

                // Then branch
                self.current_block = then_block;
                const then_idx = self.sema.ast_tree.extra_data[extra_start + 1];
                const then_inst = try self.lowerNode(then_idx);
                _ = try self.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = merge_block } } });

                // Else branch
                if (has_else) {
                    self.current_block = else_block;
                    const else_idx = self.sema.ast_tree.extra_data[extra_start + 2];
                    _ = try self.lowerNode(else_idx);
                    _ = try self.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = merge_block } } });
                }

                self.current_block = merge_block;

                return then_inst;
            },
            .while_stmt => {
                const cond_node = node.data.lhs;
                const body_node = node.data.rhs;

                const cond_block = try self.newBlock();
                const body_block = try self.newBlock();
                const end_block = try self.newBlock();

                _ = try self.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = cond_block } } });

                self.current_block = cond_block;
                const cond_inst = (try self.lowerNode(cond_node)) orelse return null;
                _ = try self.emitInst(.{
                    .opcode = .condbr,
                    .type_id = 0,
                    .data = .{ .condbr = .{ .cond = cond_inst, .true_dest = body_block, .false_dest = end_block } },
                });

                self.current_block = body_block;
                _ = try self.lowerNode(body_node);
                _ = try self.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = cond_block } } });

                self.current_block = end_block;
                return null;
            },
            .fn_decl => {
                const proto_idx = node.data.lhs;
                const body = node.data.rhs;

                const fn_block = try self.newBlock();
                self.current_block = fn_block;
                self.param_counter = 0;

                const proto_node = self.sema.ast_tree.nodes.get(proto_idx);
                _ = try self.emitInst(.{
                    .opcode = .label,
                    .type_id = 0,
                    .data = .{ .label = proto_node.main_token }, // We pass the token index of the identifier!
                });

                _ = try self.lowerNode(proto_idx);
                _ = try self.lowerNode(body);
                _ = try self.emitInst(.{ .opcode = .ret, .type_id = 0, .data = .{ .ret = null } });
                self.current_block = null; // return to root block
                return null;
            },
            .fn_proto => {
                const extra_start = node.data.lhs;
                const extra_len = node.data.rhs;

                var i: u32 = 1;
                while (i < extra_len) : (i += 1) {
                    const param_idx = self.sema.ast_tree.extra_data[extra_start + i];
                    _ = try self.lowerNode(param_idx);
                }
                return null;
            },
            .param_decl => {
                const name_tok_idx = node.data.lhs;
                const type_id = self.sema.node_types.get(node_idx) orelse 0;

                const param_inst = try self.emitInst(.{
                    .opcode = .param,
                    .type_id = type_id,
                    .data = .{ .param = self.param_counter },
                });
                self.param_counter += 1;

                const addr_inst = try self.emitInst(.{
                    .opcode = .addr,
                    .type_id = type_id,
                    .data = .{ .addr = name_tok_idx },
                });

                _ = try self.emitInst(.{
                    .opcode = .store,
                    .type_id = type_id,
                    .data = .{ .store = .{ .ptr = addr_inst, .val = param_inst } },
                });

                const tok = self.sema.ast_tree.tokens[name_tok_idx];
                const src = self.sema.diags.source_manager.getFile(0).?.content;
                const ident_name = src[tok.start..tok.end];
                try self.var_addresses.put(ident_name, addr_inst);

                return null;
            },
            .builtin_call => {
                if (self.sema.const_values.get(node_idx)) |value| {
                    result = try self.emitInst(.{
                        .opcode = .const_i,
                        .type_id = self.sema.node_types.get(node_idx) orelse 0,
                        .data = .{ .const_i = value },
                    });
                    return result;
                }

                const name_token = self.sema.ast_tree.tokens[node.data.lhs];
                const src = self.sema.diags.source_manager.getFile(0).?.content;
                const kind = builtin.lookup(src[name_token.start..name_token.end]) orelse return null;
                const extra_start = node.data.rhs;
                const arg_count = self.sema.ast_tree.extra_data[extra_start];
                const value_arg: ?u32 = switch (kind) {
                    .move, .discardError, .intFromPtr, .intFromEnum, .tagOf => 0,
                    .intCast,
                    .floatCast,
                    .floatFromInt,
                    .intFromFloat,
                    .ptrCast,
                    .alignCast,
                    .bitCast,
                    .ptrFromInt,
                    .enumFromInt,
                    => 1,
                    else => null,
                };
                if (value_arg) |arg_index| {
                    if (arg_index < arg_count) {
                        result = try self.lowerNode(self.sema.ast_tree.extra_data[extra_start + 1 + arg_index]);
                    }
                }
                return result;
            },
            .unary_op, .unsafe_block => {
                result = try self.lowerNode(node.data.lhs);
                return result;
            },
            .tuple_literal => {
                result = try self.emitInst(.{
                    .opcode = .tuple_literal,
                    .type_id = self.sema.node_types.get(node_idx) orelse 0,
                    .data = .{ .tuple_literal = node_idx },
                });
                return result;
            },
            .string_literal => {
                result = try self.emitInst(.{
                    .opcode = .string_literal,
                    .type_id = self.sema.node_types.get(node_idx) orelse 0,
                    .data = .{ .string_literal = node_idx },
                });
                return result;
            },
            .call => {
                const target = node.data.lhs;
                const extra_start = node.data.rhs;
                const num_args = self.sema.ast_tree.extra_data[extra_start];

                // Normal call
                const target_inst = try self.lowerNode(target);

                const args_start = self.lir.extra_data.items.len;
                var i: u32 = 0;
                while (i < num_args) : (i += 1) {
                    const arg_node = self.sema.ast_tree.extra_data[extra_start + 1 + i];
                    const arg_inst = try self.lowerNode(arg_node);
                    try self.lir.extra_data.append(self.allocator, arg_inst.?);
                }

                const type_id = self.sema.node_types.get(node_idx) orelse 0;

                return try self.emitInst(.{
                    .opcode = .call,
                    .type_id = type_id,
                    .data = .{ .call = .{ .func = target_inst orelse 0, .args_start = @as(u32, @intCast(args_start)), .args_count = num_args } },
                });
            },
            .return_stmt => {
                const expr = node.data.rhs;
                const expr_inst = try self.lowerNode(expr);
                _ = try self.emitInst(.{ .opcode = .ret, .type_id = 0, .data = .{ .ret = expr_inst } });
                return null;
            },
            else => return null, // Unimplemented for now
        }
    }

    fn newBlock(self: *LirBuilder) !u32 {
        const blk_idx = @as(u32, @intCast(self.lir.blocks.items.len));
        try self.lir.blocks.append(self.allocator, @import("lir.zig").BasicBlock.init());
        return blk_idx;
    }

    pub fn printLir(self: *LirBuilder) void {
        std.debug.print("=== LIR DUMP ===\n", .{});
        for (self.lir.blocks.items, 0..) |blk, blk_idx| {
            std.debug.print("Block {d}:\n", .{blk_idx});
            for (blk.insts.items) |inst_idx| {
                const inst = self.lir.insts.items[inst_idx];
                std.debug.print("  %{d} = {s} ", .{ inst_idx, @tagName(inst.opcode) });
                switch (inst.opcode) {
                    .const_i => std.debug.print("{d}", .{inst.data.const_i}),
                    .add => std.debug.print("%{d}, %{d}", .{ inst.data.add.lhs, inst.data.add.rhs }),
                    .sub => std.debug.print("%{d}, %{d}", .{ inst.data.sub.lhs, inst.data.sub.rhs }),
                    .mul => std.debug.print("%{d}, %{d}", .{ inst.data.mul.lhs, inst.data.mul.rhs }),
                    .div => std.debug.print("%{d}, %{d}", .{ inst.data.div.lhs, inst.data.div.rhs }),
                    .addr => std.debug.print("local_{d}", .{inst.data.addr}),
                    .load => std.debug.print("ptr: %{d}", .{inst.data.load.ptr}),
                    .store => std.debug.print("ptr: %{d}, val: %{d}", .{ inst.data.store.ptr, inst.data.store.val }),
                    .br => std.debug.print("dest: block_{d}", .{inst.data.br.dest}),
                    .condbr => std.debug.print("cond: %{d}, true_dest: block_{d}, false_dest: block_{d}", .{ inst.data.condbr.cond, inst.data.condbr.true_dest, inst.data.condbr.false_dest }),
                    .ret => if (inst.data.ret) |r| std.debug.print("val: %{d}", .{r}) else std.debug.print("void", .{}),
                    else => std.debug.print("...", .{}),
                }
                std.debug.print(" (type: {d})\n", .{inst.type_id});
            }
        }
        std.debug.print("================\n", .{});
    }
};
