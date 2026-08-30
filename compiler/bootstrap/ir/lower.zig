const std = @import("std");
const ast = @import("../syntax/ast.zig");
const Node = ast.Node;
const Type = @import("../semantic/type.zig").Type;
const TypePool = @import("../semantic/type.zig").TypePool;
const Sema = @import("../semantic/sema.zig").Sema;
const Lir = @import("lir.zig").Lir;
const Inst = @import("lir.zig").Inst;
const builtin = @import("../semantic/builtin.zig");
const postfix = @import("lowering/postfix.zig");
const operator_lowering = @import("lowering/operators.zig");

const LoopTargets = struct {
    label_token: u32,
    break_dest: u32,
    continue_dest: u32,
    result_addr: ?Inst.Index = null,
    result_type: Type.Id = 0,
};

pub const LirBuilder = struct {
    allocator: std.mem.Allocator,
    sema: *Sema,
    lir: Lir,
    current_block: ?u32,
    var_addresses: std.StringHashMap(u32),
    var_slice_lengths: std.StringHashMap(Inst.Index),
    slice_lengths: std.AutoHashMap(Inst.Index, Inst.Index),
    synthetic_local_counter: u32,
    param_counter: u32 = 0,
    current_return_type: ?Type.Id,
    loop_stack: std.ArrayList(LoopTargets),

    pub fn init(allocator: std.mem.Allocator, sema: *Sema) LirBuilder {
        return .{
            .allocator = allocator,
            .sema = sema,
            .lir = Lir.init(allocator),
            .current_block = null,
            .var_addresses = std.StringHashMap(u32).init(allocator),
            .var_slice_lengths = std.StringHashMap(Inst.Index).init(allocator),
            .slice_lengths = std.AutoHashMap(Inst.Index, Inst.Index).init(allocator),
            .synthetic_local_counter = std.math.maxInt(u32),
            .current_return_type = null,
            .loop_stack = std.ArrayList(LoopTargets).empty,
        };
    }

    pub fn deinit(self: *LirBuilder) void {
        self.var_addresses.deinit();
        self.var_slice_lengths.deinit();
        self.slice_lengths.deinit();
        self.loop_stack.deinit(self.allocator);
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

    pub fn emitInst(self: *LirBuilder, inst: Inst) !Inst.Index {
        const idx = @as(Inst.Index, @intCast(self.lir.insts.items.len));
        try self.lir.insts.append(self.allocator, inst);

        if (self.current_block) |blk_idx| {
            try self.lir.blocks.items[blk_idx].insts.append(self.allocator, idx);
        }

        return idx;
    }

    pub fn lowerNode(self: *LirBuilder, node_idx: Node.Index) std.mem.Allocator.Error!?Inst.Index {
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
            .float_literal => {
                const type_id = self.sema.node_types.get(node_idx) orelse return null;
                const source = self.sema.diags.source_manager.getFile(0).?.content;
                const token = self.sema.ast_tree.tokens[node.main_token];
                const value = std.fmt.parseFloat(f64, source[token.start..token.end]) catch 0;
                result = try self.emitInst(.{
                    .opcode = .const_f,
                    .type_id = type_id,
                    .data = .{ .const_f = value },
                });
                return result;
            },
            .bool_literal => {
                const type_id = self.sema.node_types.get(node_idx) orelse return null;
                const value = self.sema.const_values.get(node_idx) orelse 0;
                result = try self.emitInst(.{
                    .opcode = .const_i,
                    .type_id = type_id,
                    .data = .{ .const_i = value },
                });
                return result;
            },
            .binary_op => return operator_lowering.lower(self, node_idx),
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
                if (self.slice_lengths.get(expr_inst)) |length| try self.var_slice_lengths.put(ident_name, length);

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
                if (self.slice_lengths.get(rhs_inst)) |length| try self.var_slice_lengths.put(ident_name, length);

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
                if (self.var_slice_lengths.get(ident_name)) |length| try self.slice_lengths.put(result.?, length);
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
                    if (self.currentBlockTerminated()) break;
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
                const result_type = self.sema.node_types.get(node_idx) orelse 0;
                const result_value = has_else and self.hasRuntimeValue(result_type);
                const result_addr = if (result_value)
                    try self.emitInst(.{
                        .opcode = .addr,
                        .type_id = result_type,
                        .data = .{ .addr = 0xc000_0000 | node_idx },
                    })
                else
                    null;

                _ = try self.emitInst(.{
                    .opcode = .condbr,
                    .type_id = 0,
                    .data = .{ .condbr = .{ .cond = cond_inst, .true_dest = then_block, .false_dest = if (has_else) else_block else merge_block } },
                });

                // Then branch
                self.current_block = then_block;
                const then_idx = self.sema.ast_tree.extra_data[extra_start + 1];
                const then_inst = try self.lowerNode(then_idx);
                if (!self.currentBlockTerminated()) {
                    if (result_addr) |addr| {
                        if (then_inst) |value| _ = try self.emitInst(.{ .opcode = .store, .type_id = result_type, .data = .{ .store = .{ .ptr = addr, .val = value } } });
                    }
                    _ = try self.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = merge_block } } });
                }

                // Else branch
                if (has_else) {
                    self.current_block = else_block;
                    const else_idx = self.sema.ast_tree.extra_data[extra_start + 2];
                    const else_inst = try self.lowerNode(else_idx);
                    if (!self.currentBlockTerminated()) {
                        if (result_addr) |addr| {
                            if (else_inst) |value| _ = try self.emitInst(.{ .opcode = .store, .type_id = result_type, .data = .{ .store = .{ .ptr = addr, .val = value } } });
                        }
                        _ = try self.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = merge_block } } });
                    }
                }

                self.current_block = merge_block;
                if (result_addr) |addr| {
                    return try self.emitInst(.{ .opcode = .load, .type_id = result_type, .data = .{ .load = .{ .ptr = addr } } });
                }
                return null;
            },
            .while_stmt => {
                const extra_start = node.data.lhs;
                const label_token = self.sema.ast_tree.extra_data[extra_start];
                const cond_node = self.sema.ast_tree.extra_data[extra_start + 1];
                const body_node = self.sema.ast_tree.extra_data[extra_start + 2];

                const result_type = self.sema.node_types.get(node_idx) orelse 0;
                const result_addr = if (self.hasRuntimeValue(result_type))
                    try self.emitInst(.{
                        .opcode = .addr,
                        .type_id = result_type,
                        .data = .{ .addr = 0xd000_0000 | node_idx },
                    })
                else
                    null;

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
                try self.loop_stack.append(self.allocator, .{
                    .label_token = label_token,
                    .break_dest = end_block,
                    .continue_dest = cond_block,
                    .result_addr = result_addr,
                    .result_type = result_type,
                });
                _ = try self.lowerNode(body_node);
                _ = self.loop_stack.pop();
                if (!self.currentBlockTerminated()) {
                    _ = try self.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = cond_block } } });
                }

                self.current_block = end_block;
                if (result_addr) |address| {
                    return try self.emitInst(.{ .opcode = .load, .type_id = result_type, .data = .{ .load = .{ .ptr = address } } });
                }
                return null;
            },
            .for_stmt => return self.lowerFor(node_idx),
            .break_stmt => {
                const targets = self.findLoopTarget(node.data.lhs) orelse return null;
                if (node.data.rhs != std.math.maxInt(u32)) {
                    const value = try self.lowerNode(node.data.rhs) orelse return null;
                    if (targets.result_addr) |address| {
                        _ = try self.emitInst(.{
                            .opcode = .store,
                            .type_id = targets.result_type,
                            .data = .{ .store = .{ .ptr = address, .val = value } },
                        });
                    }
                }
                _ = try self.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = targets.break_dest } } });
                return null;
            },
            .continue_stmt => {
                const targets = self.findLoopTarget(node.data.lhs) orelse return null;
                _ = try self.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = targets.continue_dest } } });
                return null;
            },
            .fn_decl => {
                const proto_idx = node.data.lhs;
                const body = node.data.rhs;

                const fn_block = try self.newBlock();
                self.current_block = fn_block;
                self.param_counter = 0;
                self.var_addresses.clearRetainingCapacity();
                self.var_slice_lengths.clearRetainingCapacity();
                self.slice_lengths.clearRetainingCapacity();
                self.loop_stack.clearRetainingCapacity();

                const proto_node = self.sema.ast_tree.nodes.get(proto_idx);
                const function_type = self.sema.node_types.get(proto_idx).?;
                self.current_return_type = self.sema.type_pool.get(function_type).data.function.ret_type;
                _ = try self.emitInst(.{
                    .opcode = .label,
                    .type_id = self.current_return_type.?,
                    .data = .{ .label = proto_node.main_token }, // We pass the token index of the identifier!
                });

                _ = try self.lowerNode(proto_idx);
                _ = try self.lowerNode(body);
                if (!self.currentBlockTerminated()) {
                    _ = try self.emitInst(.{ .opcode = .ret, .type_id = self.current_return_type.?, .data = .{ .ret = null } });
                }
                self.current_return_type = null;
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
            .unary_op => {
                const operator = self.sema.ast_tree.tokens[node.main_token].tag;
                if (operator == .dot_asterisk or operator == .dot_question) return postfix.lowerUnarySuffix(self, node_idx);
                if (operator == .keyword_try) {
                    const operand = try self.lowerNode(node.data.lhs) orelse return null;
                    const bool_type = try self.sema.type_pool.internPrimitive(.bool_type);
                    const error_test = try self.emitInst(.{
                        .opcode = .error_test,
                        .type_id = bool_type,
                        .data = .{ .error_test = operand },
                    });
                    const error_block = try self.newBlock();
                    const success_block = try self.newBlock();
                    _ = try self.emitInst(.{
                        .opcode = .condbr,
                        .type_id = 0,
                        .data = .{ .condbr = .{ .cond = error_test, .true_dest = error_block, .false_dest = success_block } },
                    });

                    self.current_block = error_block;
                    _ = try self.emitInst(.{
                        .opcode = .ret_error,
                        .type_id = self.current_return_type.?,
                        .data = .{ .ret_error = error_test },
                    });

                    self.current_block = success_block;
                    result = try self.emitInst(.{
                        .opcode = .error_payload,
                        .type_id = self.sema.node_types.get(node_idx) orelse 0,
                        .data = .{ .error_payload = operand },
                    });
                    if (self.slice_lengths.get(operand)) |length| try self.slice_lengths.put(result.?, length);
                    return result;
                }
                result = try self.lowerNode(node.data.lhs);
                return result;
            },
            .array_access, .slice => return postfix.lower(self, node_idx),
            .unsafe_block => {
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
            .array_literal => {
                const extra_start = node.data.lhs;
                const count = self.sema.ast_tree.extra_data[extra_start + 2];
                const array_type = self.sema.type_pool.get(self.sema.node_types.get(node_idx) orelse return null).data.array;
                const element_size: u32 = @intCast(@max(self.sema.type_pool.sizeOf(array_type.child_type) catch 8, 1));
                const element_alignment: u32 = @intCast(self.sema.type_pool.alignOf(array_type.child_type) catch 8);
                const allocation_size = std.math.mul(u32, @max(count, 1), element_size) catch return null;
                const allocation_id = self.synthetic_local_counter;
                self.synthetic_local_counter -= 1;
                const base_address = try self.emitInst(.{
                    .opcode = .alloca,
                    .type_id = self.sema.node_types.get(node_idx).?,
                    .data = .{ .alloca = .{ .id = allocation_id, .size = allocation_size, .alignment = element_alignment } },
                });
                var index: u32 = 0;
                const index_type = try self.sema.type_pool.internSizeInt(false);
                const pointer_type = try self.sema.type_pool.internPtr(array_type.child_type, false);
                while (index < count) : (index += 1) {
                    const element_index = try self.emitInst(.{ .opcode = .const_i, .type_id = index_type, .data = .{ .const_i = index } });
                    const address = try self.emitInst(.{
                        .opcode = .gep,
                        .type_id = pointer_type,
                        .data = .{ .gep = .{ .base = base_address, .index = element_index, .stride = @intCast(element_size) } },
                    });
                    const value = try self.lowerNode(self.sema.ast_tree.extra_data[extra_start + 3 + index]) orelse return null;
                    _ = try self.emitInst(.{
                        .opcode = .store,
                        .type_id = array_type.child_type,
                        .data = .{ .store = .{ .ptr = address, .val = value } },
                    });
                }
                const length_type = try self.sema.type_pool.internSizeInt(false);
                const length = try self.emitInst(.{ .opcode = .const_i, .type_id = length_type, .data = .{ .const_i = count } });
                try self.slice_lengths.put(base_address, length);
                return base_address;
            },
            .string_literal => {
                result = try self.emitInst(.{
                    .opcode = .string_literal,
                    .type_id = self.sema.node_types.get(node_idx) orelse 0,
                    .data = .{ .string_literal = node_idx },
                });
                const token = self.sema.ast_tree.tokens[node.main_token];
                const raw = self.sema.diags.source_manager.getFile(0).?.content[token.start..token.end];
                const length_type = try self.sema.type_pool.internSizeInt(false);
                const length = try self.emitInst(.{
                    .opcode = .const_i,
                    .type_id = length_type,
                    .data = .{ .const_i = if (raw.len >= 2) raw.len - 2 else 0 },
                });
                try self.slice_lengths.put(result.?, length);
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

                const call_inst = try self.emitInst(.{
                    .opcode = .call,
                    .type_id = type_id,
                    .data = .{ .call = .{ .func = target_inst orelse 0, .args_start = @as(u32, @intCast(args_start)), .args_count = num_args } },
                });
                if (self.isSliceErrorUnion(type_id)) {
                    const length_type = try self.sema.type_pool.internSizeInt(false);
                    const length = try self.emitInst(.{
                        .opcode = .error_payload_part,
                        .type_id = length_type,
                        .data = .{ .error_payload_part = .{ .source = call_inst, .part = 1 } },
                    });
                    try self.slice_lengths.put(call_inst, length);
                }
                return call_inst;
            },
            .return_stmt => {
                const expr_inst = if (node.data.rhs == std.math.maxInt(u32))
                    null
                else
                    try self.lowerNode(node.data.rhs);
                const return_type = self.current_return_type.?;
                const expression_is_error_union = expr_inst != null and self.sema.type_pool.get(self.sema.node_types.get(node.data.rhs).?).data == .error_union;
                if (expression_is_error_union and self.isSliceErrorUnion(return_type)) {
                    const length = self.slice_lengths.get(expr_inst.?) orelse return null;
                    _ = try self.emitInst(.{
                        .opcode = .ret_error_union_slice,
                        .type_id = return_type,
                        .data = .{ .ret_error_union_slice = .{ .source = expr_inst.?, .len = length } },
                    });
                } else if (expression_is_error_union) {
                    _ = try self.emitInst(.{ .opcode = .ret_error_union, .type_id = return_type, .data = .{ .ret_error_union = expr_inst.? } });
                } else if (expr_inst != null and self.isSliceErrorUnion(return_type)) {
                    const length = self.slice_lengths.get(expr_inst.?) orelse return null;
                    _ = try self.emitInst(.{
                        .opcode = .ret_error_slice,
                        .type_id = return_type,
                        .data = .{ .ret_error_slice = .{ .ptr = expr_inst.?, .len = length } },
                    });
                } else {
                    _ = try self.emitInst(.{ .opcode = .ret, .type_id = return_type, .data = .{ .ret = expr_inst } });
                }
                return null;
            },
            else => return null, // Unimplemented for now
        }
    }

    pub fn newBlock(self: *LirBuilder) !u32 {
        const blk_idx = @as(u32, @intCast(self.lir.blocks.items.len));
        try self.lir.blocks.append(self.allocator, @import("lir.zig").BasicBlock.init());
        return blk_idx;
    }

    fn currentBlockTerminated(self: *const LirBuilder) bool {
        const block_index = self.current_block orelse return false;
        const instructions = self.lir.blocks.items[block_index].insts.items;
        if (instructions.len == 0) return false;
        return switch (self.lir.insts.items[instructions[instructions.len - 1]].opcode) {
            .br, .condbr, .ret, .ret_error, .ret_error_union, .ret_error_slice, .ret_error_union_slice, .unreachable_inst => true,
            else => false,
        };
    }

    fn hasRuntimeValue(self: *const LirBuilder, type_id: Type.Id) bool {
        const ty = self.sema.type_pool.get(type_id);
        return !(ty.data == .primitive and (ty.data.primitive == .void_type or ty.data.primitive == .noreturn_type));
    }

    fn isSliceErrorUnion(self: *const LirBuilder, type_id: Type.Id) bool {
        const ty = self.sema.type_pool.get(type_id);
        if (ty.data != .error_union) return false;
        const payload = self.sema.type_pool.get(ty.data.error_union.payload);
        return payload.data == .pointer and payload.data.pointer.size == .Slice;
    }

    fn findLoopTarget(self: *const LirBuilder, label_token: u32) ?LoopTargets {
        if (self.loop_stack.items.len == 0) return null;
        if (label_token == std.math.maxInt(u32)) return self.loop_stack.items[self.loop_stack.items.len - 1];

        const source = self.sema.diags.source_manager.getFile(0).?.content;
        const wanted_token = self.sema.ast_tree.tokens[label_token];
        const wanted = source[wanted_token.start..wanted_token.end];
        var index = self.loop_stack.items.len;
        while (index > 0) {
            index -= 1;
            const target = self.loop_stack.items[index];
            if (target.label_token == std.math.maxInt(u32)) continue;
            const candidate_token = self.sema.ast_tree.tokens[target.label_token];
            if (std.mem.eql(u8, wanted, source[candidate_token.start..candidate_token.end])) return target;
        }
        return null;
    }

    fn lowerFor(self: *LirBuilder, node_idx: Node.Index) std.mem.Allocator.Error!?Inst.Index {
        const node = self.sema.ast_tree.nodes.get(node_idx);
        const extra_start = node.data.lhs;
        const label_token = self.sema.ast_tree.extra_data[extra_start];
        const capture_flags = self.sema.ast_tree.extra_data[extra_start + 1];
        const item_token = self.sema.ast_tree.extra_data[extra_start + 2];
        const index_token = self.sema.ast_tree.extra_data[extra_start + 3];
        const range_node_idx = self.sema.ast_tree.extra_data[extra_start + 4];
        const body_node = self.sema.ast_tree.extra_data[extra_start + 5];
        const range_node = self.sema.ast_tree.nodes.get(range_node_idx);

        if (range_node.tag != .range) {
            return self.lowerIterableFor(label_token, capture_flags, item_token, index_token, range_node_idx, body_node);
        }

        const start_inst = try self.lowerNode(range_node.data.lhs) orelse return null;
        const end_inst = try self.lowerNode(range_node.data.rhs) orelse return null;
        const item_type = self.sema.node_types.get(range_node.data.lhs) orelse 0;
        const item_addr = try self.emitInst(.{ .opcode = .addr, .type_id = item_type, .data = .{ .addr = item_token } });
        _ = try self.emitInst(.{ .opcode = .store, .type_id = item_type, .data = .{ .store = .{ .ptr = item_addr, .val = start_inst } } });

        const src = self.sema.diags.source_manager.getFile(0).?.content;
        const item_tok = self.sema.ast_tree.tokens[item_token];
        const item_name = src[item_tok.start..item_tok.end];
        const previous_item = self.var_addresses.get(item_name);
        try self.var_addresses.put(item_name, item_addr);
        defer if (previous_item) |address| {
            self.var_addresses.put(item_name, address) catch {};
        } else {
            _ = self.var_addresses.remove(item_name);
        };

        var index_addr: ?Inst.Index = null;
        var index_name: ?[]const u8 = null;
        var previous_index: ?u32 = null;
        if (index_token != std.math.maxInt(u32)) {
            const index_type = try self.sema.type_pool.internSizeInt(false);
            const zero = try self.emitInst(.{ .opcode = .const_i, .type_id = index_type, .data = .{ .const_i = 0 } });
            index_addr = try self.emitInst(.{ .opcode = .addr, .type_id = index_type, .data = .{ .addr = index_token } });
            _ = try self.emitInst(.{ .opcode = .store, .type_id = index_type, .data = .{ .store = .{ .ptr = index_addr.?, .val = zero } } });
            const index_tok = self.sema.ast_tree.tokens[index_token];
            index_name = src[index_tok.start..index_tok.end];
            previous_index = self.var_addresses.get(index_name.?);
            try self.var_addresses.put(index_name.?, index_addr.?);
        }
        defer if (index_name) |name| {
            if (previous_index) |address| {
                self.var_addresses.put(name, address) catch {};
            } else {
                _ = self.var_addresses.remove(name);
            }
        };

        const cond_block = try self.newBlock();
        const body_block = try self.newBlock();
        const increment_block = try self.newBlock();
        const end_block = try self.newBlock();
        _ = try self.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = cond_block } } });

        self.current_block = cond_block;
        const current_item = try self.emitInst(.{ .opcode = .load, .type_id = item_type, .data = .{ .load = .{ .ptr = item_addr } } });
        const bool_type = try self.sema.type_pool.internPrimitive(.bool_type);
        const condition = try self.emitInst(.{ .opcode = .icmp, .type_id = bool_type, .data = .{ .icmp = .{ .predicate = .lt, .lhs = current_item, .rhs = end_inst } } });
        _ = try self.emitInst(.{ .opcode = .condbr, .type_id = 0, .data = .{ .condbr = .{ .cond = condition, .true_dest = body_block, .false_dest = end_block } } });

        self.current_block = body_block;
        try self.loop_stack.append(self.allocator, .{
            .label_token = label_token,
            .break_dest = end_block,
            .continue_dest = increment_block,
        });
        _ = try self.lowerNode(body_node);
        _ = self.loop_stack.pop();
        if (!self.currentBlockTerminated()) _ = try self.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = increment_block } } });

        self.current_block = increment_block;
        const item_value = try self.emitInst(.{ .opcode = .load, .type_id = item_type, .data = .{ .load = .{ .ptr = item_addr } } });
        const one = try self.emitInst(.{ .opcode = .const_i, .type_id = item_type, .data = .{ .const_i = 1 } });
        const next_item = try self.emitInst(.{ .opcode = .add, .type_id = item_type, .data = .{ .add = .{ .lhs = item_value, .rhs = one } } });
        _ = try self.emitInst(.{ .opcode = .store, .type_id = item_type, .data = .{ .store = .{ .ptr = item_addr, .val = next_item } } });
        if (index_addr) |address| {
            const index_type = try self.sema.type_pool.internSizeInt(false);
            const index_value = try self.emitInst(.{ .opcode = .load, .type_id = index_type, .data = .{ .load = .{ .ptr = address } } });
            const index_one = try self.emitInst(.{ .opcode = .const_i, .type_id = index_type, .data = .{ .const_i = 1 } });
            const next_index = try self.emitInst(.{ .opcode = .add, .type_id = index_type, .data = .{ .add = .{ .lhs = index_value, .rhs = index_one } } });
            _ = try self.emitInst(.{ .opcode = .store, .type_id = index_type, .data = .{ .store = .{ .ptr = address, .val = next_index } } });
        }
        _ = try self.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = cond_block } } });

        self.current_block = end_block;
        return null;
    }

    fn lowerIterableFor(
        self: *LirBuilder,
        label_token: u32,
        capture_flags: u32,
        item_token: u32,
        index_token: u32,
        iterable_node: Node.Index,
        body_node: Node.Index,
    ) std.mem.Allocator.Error!?Inst.Index {
        const iterable_type_id = self.sema.node_types.get(iterable_node) orelse return null;
        const iterable_type = self.sema.type_pool.get(iterable_type_id);
        const child_type: Type.Id = switch (iterable_type.data) {
            .array => |array| array.child_type,
            .pointer => |pointer| pointer.child_type,
            else => return null,
        };
        const base = try self.lowerNode(iterable_node) orelse return null;
        const index_type = try self.sema.type_pool.internSizeInt(false);
        const length = switch (iterable_type.data) {
            .array => |array| try self.emitInst(.{ .opcode = .const_i, .type_id = index_type, .data = .{ .const_i = array.len } }),
            .pointer => self.slice_lengths.get(base) orelse
                try self.emitInst(.{ .opcode = .const_i, .type_id = index_type, .data = .{ .const_i = 0 } }),
            else => unreachable,
        };
        // Fixed arrays use their physical element stride. Slices retain their
        // source element stride (notably one byte for []u8).
        const stride: i32 = switch (iterable_type.data) {
            .array => @intCast(@max(self.sema.type_pool.sizeOf(child_type) catch 8, 1)),
            .pointer => @intCast(@max(self.sema.type_pool.sizeOf(child_type) catch 8, 1)),
            else => unreachable,
        };

        const zero = try self.emitInst(.{ .opcode = .const_i, .type_id = index_type, .data = .{ .const_i = 0 } });
        const counter_id = self.synthetic_local_counter;
        self.synthetic_local_counter -= 1;
        const counter_addr = try self.emitInst(.{ .opcode = .addr, .type_id = index_type, .data = .{ .addr = counter_id } });
        _ = try self.emitInst(.{ .opcode = .store, .type_id = index_type, .data = .{ .store = .{ .ptr = counter_addr, .val = zero } } });

        const item_type = if ((capture_flags & 1) != 0)
            try self.sema.type_pool.internPtr(child_type, false)
        else
            child_type;
        const item_addr = try self.emitInst(.{ .opcode = .addr, .type_id = item_type, .data = .{ .addr = item_token } });

        const source = self.sema.diags.source_manager.getFile(0).?.content;
        const item_source_token = self.sema.ast_tree.tokens[item_token];
        const item_name = source[item_source_token.start..item_source_token.end];
        const previous_item = self.var_addresses.get(item_name);
        try self.var_addresses.put(item_name, item_addr);
        defer if (previous_item) |address| {
            self.var_addresses.put(item_name, address) catch {};
        } else {
            _ = self.var_addresses.remove(item_name);
        };

        var index_name: ?[]const u8 = null;
        var previous_index: ?Inst.Index = null;
        if (index_token != std.math.maxInt(u32)) {
            const index_source_token = self.sema.ast_tree.tokens[index_token];
            index_name = source[index_source_token.start..index_source_token.end];
            previous_index = self.var_addresses.get(index_name.?);
            try self.var_addresses.put(index_name.?, counter_addr);
        }
        defer if (index_name) |name| {
            if (previous_index) |address| {
                self.var_addresses.put(name, address) catch {};
            } else {
                _ = self.var_addresses.remove(name);
            }
        };

        const cond_block = try self.newBlock();
        const body_block = try self.newBlock();
        const increment_block = try self.newBlock();
        const end_block = try self.newBlock();
        _ = try self.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = cond_block } } });

        self.current_block = cond_block;
        const current_index = try self.emitInst(.{ .opcode = .load, .type_id = index_type, .data = .{ .load = .{ .ptr = counter_addr } } });
        const bool_type = try self.sema.type_pool.internPrimitive(.bool_type);
        const condition = try self.emitInst(.{ .opcode = .icmp, .type_id = bool_type, .data = .{ .icmp = .{ .predicate = .lt, .lhs = current_index, .rhs = length } } });
        _ = try self.emitInst(.{ .opcode = .condbr, .type_id = 0, .data = .{ .condbr = .{ .cond = condition, .true_dest = body_block, .false_dest = end_block } } });

        self.current_block = body_block;
        const body_index = try self.emitInst(.{ .opcode = .load, .type_id = index_type, .data = .{ .load = .{ .ptr = counter_addr } } });
        const element_pointer_type = try self.sema.type_pool.internPtr(child_type, false);
        const element_address = try self.emitInst(.{
            .opcode = .gep,
            .type_id = element_pointer_type,
            .data = .{ .gep = .{ .base = base, .index = body_index, .stride = stride } },
        });
        const captured_value = if ((capture_flags & 1) != 0)
            element_address
        else
            try self.emitInst(.{ .opcode = .load, .type_id = child_type, .data = .{ .load = .{ .ptr = element_address } } });
        _ = try self.emitInst(.{ .opcode = .store, .type_id = item_type, .data = .{ .store = .{ .ptr = item_addr, .val = captured_value } } });

        try self.loop_stack.append(self.allocator, .{
            .label_token = label_token,
            .break_dest = end_block,
            .continue_dest = increment_block,
        });
        _ = try self.lowerNode(body_node);
        _ = self.loop_stack.pop();
        if (!self.currentBlockTerminated()) _ = try self.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = increment_block } } });

        self.current_block = increment_block;
        const old_index = try self.emitInst(.{ .opcode = .load, .type_id = index_type, .data = .{ .load = .{ .ptr = counter_addr } } });
        const one = try self.emitInst(.{ .opcode = .const_i, .type_id = index_type, .data = .{ .const_i = 1 } });
        const next_index = try self.emitInst(.{ .opcode = .add, .type_id = index_type, .data = .{ .add = .{ .lhs = old_index, .rhs = one } } });
        _ = try self.emitInst(.{ .opcode = .store, .type_id = index_type, .data = .{ .store = .{ .ptr = counter_addr, .val = next_index } } });
        _ = try self.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = cond_block } } });

        self.current_block = end_block;
        return null;
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
                    .const_f => std.debug.print("{d}", .{inst.data.const_f}),
                    .add => std.debug.print("%{d}, %{d}", .{ inst.data.add.lhs, inst.data.add.rhs }),
                    .sub => std.debug.print("%{d}, %{d}", .{ inst.data.sub.lhs, inst.data.sub.rhs }),
                    .mul => std.debug.print("%{d}, %{d}", .{ inst.data.mul.lhs, inst.data.mul.rhs }),
                    .div => std.debug.print("%{d}, %{d}", .{ inst.data.div.lhs, inst.data.div.rhs }),
                    .rem => std.debug.print("%{d}, %{d}", .{ inst.data.rem.lhs, inst.data.rem.rhs }),
                    .bit_and => std.debug.print("%{d}, %{d}", .{ inst.data.bit_and.lhs, inst.data.bit_and.rhs }),
                    .bit_or => std.debug.print("%{d}, %{d}", .{ inst.data.bit_or.lhs, inst.data.bit_or.rhs }),
                    .bit_xor => std.debug.print("%{d}, %{d}", .{ inst.data.bit_xor.lhs, inst.data.bit_xor.rhs }),
                    .shl => std.debug.print("%{d}, %{d}", .{ inst.data.shl.lhs, inst.data.shl.rhs }),
                    .shr => std.debug.print("%{d}, %{d}", .{ inst.data.shr.lhs, inst.data.shr.rhs }),
                    .icmp => std.debug.print("{s} %{d}, %{d}", .{ @tagName(inst.data.icmp.predicate), inst.data.icmp.lhs, inst.data.icmp.rhs }),
                    .addr => std.debug.print("local_{d}", .{inst.data.addr}),
                    .alloca => std.debug.print("local_{d}, size: {d}, align: {d}", .{ inst.data.alloca.id, inst.data.alloca.size, inst.data.alloca.alignment }),
                    .load => std.debug.print("ptr: %{d}", .{inst.data.load.ptr}),
                    .store => std.debug.print("ptr: %{d}, val: %{d}", .{ inst.data.store.ptr, inst.data.store.val }),
                    .gep => std.debug.print("base: %{d}, index: %{d}, stride: {d}", .{ inst.data.gep.base, inst.data.gep.index, inst.data.gep.stride }),
                    .br => std.debug.print("dest: block_{d}", .{inst.data.br.dest}),
                    .condbr => std.debug.print("cond: %{d}, true_dest: block_{d}, false_dest: block_{d}", .{ inst.data.condbr.cond, inst.data.condbr.true_dest, inst.data.condbr.false_dest }),
                    .ret => if (inst.data.ret) |r| std.debug.print("val: %{d}", .{r}) else std.debug.print("void", .{}),
                    .ret_error => std.debug.print("tag: %{d}", .{inst.data.ret_error}),
                    .ret_error_union => std.debug.print("value: %{d}", .{inst.data.ret_error_union}),
                    .ret_error_slice => std.debug.print("ptr: %{d}, len: %{d}", .{ inst.data.ret_error_slice.ptr, inst.data.ret_error_slice.len }),
                    .ret_error_union_slice => std.debug.print("value: %{d}, len: %{d}", .{ inst.data.ret_error_union_slice.source, inst.data.ret_error_union_slice.len }),
                    .error_test => std.debug.print("value: %{d}", .{inst.data.error_test}),
                    .error_payload => std.debug.print("value: %{d}", .{inst.data.error_payload}),
                    .error_payload_part => std.debug.print("value: %{d}, part: {d}", .{ inst.data.error_payload_part.source, inst.data.error_payload_part.part }),
                    else => std.debug.print("...", .{}),
                }
                std.debug.print(" (type: {d})\n", .{inst.type_id});
            }
        }
        std.debug.print("================\n", .{});
    }
};
