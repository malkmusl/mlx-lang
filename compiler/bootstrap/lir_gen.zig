const std = @import("std");
const ast = @import("ast.zig");
const Node = ast.Node;
const TypePool = @import("type.zig").TypePool;
const Sema = @import("sema.zig").Sema;
const Lir = @import("lir.zig").Lir;
const Inst = @import("lir.zig").Inst;

pub const LirBuilder = struct {
    allocator: std.mem.Allocator,
    sema: *Sema,
    lir: Lir,
    current_block: ?u32,

    pub fn init(allocator: std.mem.Allocator, sema: *Sema) LirBuilder {
        return .{
            .allocator = allocator,
            .sema = sema,
            .lir = Lir.init(allocator),
            .current_block = null,
        };
    }

    pub fn deinit(self: *LirBuilder) void {
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
            .const_decl, .var_decl => {
                const type_id = self.sema.node_types.get(node_idx) orelse return null;
                const rhs_inst = try self.lowerNode(node.data.rhs) orelse return null;

                // Emulate variable allocation (addr) + store
                // We'll use the AST node index as a pseudo ID for the local variable
                const addr_inst = try self.emitInst(.{
                    .opcode = .addr,
                    .type_id = type_id,
                    .data = .{ .addr = node_idx },
                });

                result = try self.emitInst(.{
                    .opcode = .store,
                    .type_id = type_id,
                    .data = .{ .store = .{ .ptr = addr_inst, .val = rhs_inst } },
                });

                return result;
            },
            .identifier => {
                const type_id = self.sema.node_types.get(node_idx) orelse return null;
                // Placeholder load until we wire up symbol resolution to SSA
                result = try self.emitInst(.{
                    .opcode = .load,
                    .type_id = type_id,
                    .data = .{ .load = .{ .ptr = 0 } },
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
                const fn_block = try self.newBlock();
                self.current_block = fn_block;
                const body = node.data.rhs;
                _ = try self.lowerNode(body);
                _ = try self.emitInst(.{ .opcode = .ret, .type_id = 0, .data = .{ .ret = null } });
                self.current_block = 0; // return to root block
                return null;
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
                std.debug.print("  %{d} = {s} ", .{inst_idx, @tagName(inst.opcode)});
                switch (inst.opcode) {
                    .const_i => std.debug.print("{d}", .{inst.data.const_i}),
                    .add => std.debug.print("%{d}, %{d}", .{inst.data.add.lhs, inst.data.add.rhs}),
                    .sub => std.debug.print("%{d}, %{d}", .{inst.data.sub.lhs, inst.data.sub.rhs}),
                    .mul => std.debug.print("%{d}, %{d}", .{inst.data.mul.lhs, inst.data.mul.rhs}),
                    .div => std.debug.print("%{d}, %{d}", .{inst.data.div.lhs, inst.data.div.rhs}),
                    .addr => std.debug.print("local_{d}", .{inst.data.addr}),
                    .load => std.debug.print("ptr: %{d}", .{inst.data.load.ptr}),
                    .store => std.debug.print("ptr: %{d}, val: %{d}", .{inst.data.store.ptr, inst.data.store.val}),
                    .br => std.debug.print("dest: block_{d}", .{inst.data.br.dest}),
                    .condbr => std.debug.print("cond: %{d}, true_dest: block_{d}, false_dest: block_{d}", .{inst.data.condbr.cond, inst.data.condbr.true_dest, inst.data.condbr.false_dest}),
                    .ret => if (inst.data.ret) |r| std.debug.print("val: %{d}", .{r}) else std.debug.print("void", .{}),
                    else => std.debug.print("...", .{}),
                }
                std.debug.print(" (type: {d})\n", .{inst.type_id});
            }
        }
        std.debug.print("================\n", .{});
    }
};
