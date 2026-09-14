const std = @import("std");
const ast = @import("../syntax/ast.zig");
const Node = ast.Node;
const Type = @import("../semantic/type.zig").Type;
const TypePool = @import("../semantic/type.zig").TypePool;
const Sema = @import("../semantic/sema.zig").Sema;
const Lir = @import("lir.zig").Lir;
const Inst = @import("lir.zig").Inst;
const postfix = @import("lowering/postfix.zig");
const prefix = @import("lowering/prefix.zig");
const operator_lowering = @import("lowering/operators.zig");
const cleanup_lowering = @import("lowering/cleanup.zig");
const match_lowering = @import("lowering/match.zig");
const aggregate_lowering = @import("lowering/aggregate.zig");
const generic_definition = @import("../semantic/generics/definition.zig");
const conditional_lowering = @import("lowering/conditional.zig");
const loop_lowering = @import("lowering/loops.zig");
const function_lowering = @import("lowering/functions.zig");
const call_lowering = @import("lowering/calls.zig");
const return_lowering = @import("lowering/returns.zig");
const binding_lowering = @import("lowering/bindings.zig");
const builtin_lowering = @import("lowering/builtins.zig");

const LoopTargets = struct {
    label_token: u32,
    break_dest: u32,
    continue_dest: u32,
    result_addr: ?Inst.Index = null,
    result_type: Type.Id = 0,
    cleanup_depth: usize = 0,
};

const Cleanup = union(enum) {
    explicit: struct {
        node_index: Node.Index,
        error_only: bool,
    },
    auto_drop: struct {
        address: Inst.Index,
        initialized: Inst.Index,
        type_id: Type.Id,
        declaration: Node.Index,
    },
};

pub const LirBuilder = struct {
    allocator: std.mem.Allocator,
    sema: *Sema,
    lir: Lir,
    current_block: ?u32,
    var_addresses: std.StringHashMap(u32),
    drop_flags: std.StringHashMap(Inst.Index),
    var_slice_lengths: std.StringHashMap(Inst.Index),
    slice_lengths: std.AutoHashMap(Inst.Index, Inst.Index),
    synthetic_local_counter: u32,
    param_counter: u32 = 0,
    current_return_type: ?Type.Id,
    loop_stack: std.ArrayList(LoopTargets),
    cleanup_stack: std.ArrayList(Cleanup),
    current_generic_instance: ?u32 = null,
    verbose: bool,

    pub fn init(allocator: std.mem.Allocator, sema: *Sema, verbose: bool) LirBuilder {
        return .{
            .allocator = allocator,
            .sema = sema,
            .lir = Lir.init(allocator),
            .current_block = null,
            .var_addresses = std.StringHashMap(u32).init(allocator),
            .drop_flags = std.StringHashMap(Inst.Index).init(allocator),
            .var_slice_lengths = std.StringHashMap(Inst.Index).init(allocator),
            .slice_lengths = std.AutoHashMap(Inst.Index, Inst.Index).init(allocator),
            .synthetic_local_counter = std.math.maxInt(u32),
            .current_return_type = null,
            .loop_stack = std.ArrayList(LoopTargets).empty,
            .cleanup_stack = std.ArrayList(Cleanup).empty,
            .verbose = verbose,
        };
    }

    pub fn deinit(self: *LirBuilder) void {
        self.var_addresses.deinit();
        self.drop_flags.deinit();
        self.var_slice_lengths.deinit();
        self.slice_lengths.deinit();
        self.loop_stack.deinit(self.allocator);
        self.cleanup_stack.deinit(self.allocator);
        self.lir.deinit();
    }

    pub fn generate(self: *LirBuilder) !void {
        if (self.verbose) std.debug.print("-> ENTER: LirBuilder.generate\n", .{});
        defer {
            if (self.verbose) std.debug.print("<- EXIT: LirBuilder.generate\n", .{});
        }

        // Ensure at least one block exists
        try self.lir.blocks.append(self.allocator, @import("lir.zig").BasicBlock.init());
        self.current_block = 0;

        try self.generateCurrentModule();
    }

    pub fn generateModule(self: *LirBuilder, module_sema: *Sema) !void {
        const previous = self.sema;
        self.sema = module_sema;
        defer self.sema = previous;
        try self.generateCurrentModule();
    }

    fn generateCurrentModule(self: *LirBuilder) !void {
        const root_node = self.sema.ast_tree.nodes.get(self.sema.ast_tree.nodes.len - 1);
        if (root_node.tag != .root) return error.InvalidAst;

        const extra_start = root_node.data.lhs;
        const extra_end = root_node.data.rhs;

        var i: u32 = extra_start;
        while (i < extra_end) : (i += 1) {
            const child_idx = self.sema.ast_tree.extra_data[i];
            if (generic_definition.isGeneric(&self.sema.ast_tree, child_idx)) continue;
            _ = try self.lowerNode(child_idx);
            try self.lowerAggregateMethods(child_idx);
        }
        var instance_id: u32 = 0;
        while (instance_id < self.sema.generic_instances.items.len) : (instance_id += 1) {
            try self.lowerGenericInstance(instance_id);
        }
    }

    fn lowerAggregateMethods(self: *LirBuilder, declaration_index: Node.Index) !void {
        const declaration = self.sema.ast_tree.nodes.get(declaration_index);
        if (declaration.tag != .const_decl and declaration.tag != .var_decl) return;
        var aggregate_index = self.sema.ast_tree.extra_data[declaration.data.rhs + 1];
        var aggregate = self.sema.ast_tree.nodes.get(aggregate_index);
        if (aggregate.tag == .builtin_call) {
            const token = self.sema.ast_tree.tokens[aggregate.data.lhs];
            const source = self.sema.diags.source_manager.getFile(self.sema.source_id).?.content;
            const name = source[token.start..token.end];
            const is_noncopy = std.mem.eql(u8, name, "noncopy") or std.mem.eql(u8, name, "nocopy");
            const argument_count = self.sema.ast_tree.extra_data[aggregate.data.rhs];
            if (!is_noncopy or argument_count != 1) return;
            aggregate_index = self.sema.ast_tree.extra_data[aggregate.data.rhs + 1];
            aggregate = self.sema.ast_tree.nodes.get(aggregate_index);
        }
        if (aggregate.tag != .struct_decl and aggregate.tag != .enum_decl and aggregate.tag != .union_decl) return;
        var extra_index = aggregate.data.lhs + 1;
        while (extra_index < aggregate.data.rhs) : (extra_index += 1) {
            const member = self.sema.ast_tree.extra_data[extra_index];
            if (self.sema.ast_tree.nodes.get(member).tag != .fn_decl) continue;
            if (generic_definition.isGeneric(&self.sema.ast_tree, member)) continue;
            _ = try self.lowerNode(member);
        }
    }

    fn lowerGenericInstance(self: *LirBuilder, instance_id: u32) !void {
        const function = self.sema.type_pool.get(self.sema.generic_instances.items[instance_id].function_type).data.function;
        const return_type = self.sema.type_pool.get(function.ret_type);
        if (return_type.data == .primitive and return_type.data.primitive == .type_type) return;
        const TypeMap = std.AutoHashMap(Node.Index, Type.Id);
        const ValueMap = std.AutoHashMap(Node.Index, u64);
        const StringMap = std.AutoHashMap(Node.Index, []const u8);
        const DynamicFieldMap = std.AutoHashMap(Node.Index, Sema.DynamicField);
        const GenericCallMap = @TypeOf(self.sema.generic_calls);
        std.mem.swap(TypeMap, &self.sema.node_types, &self.sema.generic_instances.items[instance_id].node_types);
        std.mem.swap(ValueMap, &self.sema.const_values, &self.sema.generic_instances.items[instance_id].const_values);
        std.mem.swap(TypeMap, &self.sema.type_values, &self.sema.generic_instances.items[instance_id].type_values);
        std.mem.swap(DynamicFieldMap, &self.sema.dynamic_fields, &self.sema.generic_instances.items[instance_id].dynamic_fields);
        std.mem.swap(StringMap, &self.sema.reflected_strings, &self.sema.generic_instances.items[instance_id].reflected_strings);
        std.mem.swap(GenericCallMap, &self.sema.generic_calls, &self.sema.generic_instances.items[instance_id].generic_calls);
        defer std.mem.swap(TypeMap, &self.sema.node_types, &self.sema.generic_instances.items[instance_id].node_types);
        defer std.mem.swap(ValueMap, &self.sema.const_values, &self.sema.generic_instances.items[instance_id].const_values);
        defer std.mem.swap(TypeMap, &self.sema.type_values, &self.sema.generic_instances.items[instance_id].type_values);
        defer std.mem.swap(DynamicFieldMap, &self.sema.dynamic_fields, &self.sema.generic_instances.items[instance_id].dynamic_fields);
        defer std.mem.swap(StringMap, &self.sema.reflected_strings, &self.sema.generic_instances.items[instance_id].reflected_strings);
        defer std.mem.swap(GenericCallMap, &self.sema.generic_calls, &self.sema.generic_instances.items[instance_id].generic_calls);
        const previous = self.current_generic_instance;
        self.current_generic_instance = instance_id;
        defer self.current_generic_instance = previous;
        _ = try self.lowerNode(self.sema.generic_instances.items[instance_id].declaration);
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
        if (self.verbose) std.debug.print("-> ENTER: LirBuilder.lowerNode | Tag: {s}\n", .{@tagName(node.tag)});

        var result: ?Inst.Index = null;
        defer {
            if (self.verbose) {
                std.debug.print("<- EXIT: LirBuilder.lowerNode | Tag: {s} | Result: ", .{@tagName(node.tag)});
                if (result) |r| std.debug.print("{d}\n", .{r}) else std.debug.print("null\n", .{});
            }
        }

        switch (node.tag) {
            .integer_literal => {
                const type_id = self.sema.node_types.get(node_idx) orelse return null;
                const src = self.sema.diags.source_manager.getFile(self.sema.source_id).?.content;
                const tok = self.sema.ast_tree.tokens[node.main_token];
                const text = src[tok.start..tok.end];
                const val = std.fmt.parseInt(u64, text, 0) catch 0;

                result = try self.emitInst(.{
                    .opcode = .const_i,
                    .type_id = type_id,
                    .data = .{ .const_i = val },
                });
                return result;
            },
            .float_literal => {
                const type_id = self.sema.node_types.get(node_idx) orelse return null;
                const source = self.sema.diags.source_manager.getFile(self.sema.source_id).?.content;
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
            .null_literal, .undefined_literal => {
                const type_id = self.sema.node_types.get(node_idx) orelse return null;
                return try self.emitInst(.{ .opcode = .const_i, .type_id = type_id, .data = .{ .const_i = 0 } });
            },
            .enum_literal => {
                const type_id = self.sema.node_types.get(node_idx) orelse return null;
                const value = self.sema.const_values.get(node_idx) orelse 0;
                return try self.emitInst(.{ .opcode = .const_i, .type_id = type_id, .data = .{ .const_i = value } });
            },
            .binary_op => return operator_lowering.lower(self, node_idx),
            .const_decl, .var_decl, .identifier => return binding_lowering.lower(self, node_idx),
            .block => {
                const extra_start = node.data.lhs;
                const extra_end = node.data.rhs;
                const cleanup_marker = self.cleanup_stack.items.len;
                defer self.cleanup_stack.shrinkRetainingCapacity(cleanup_marker);

                var i: u32 = extra_start;
                var last_inst: ?Inst.Index = null;
                while (i < extra_end) : (i += 1) {
                    const child_idx = self.sema.ast_tree.extra_data[i];
                    last_inst = try self.lowerNode(child_idx);
                    if (self.currentBlockTerminated()) break;
                }

                if (!self.currentBlockTerminated()) try self.emitCleanups(cleanup_marker, false);

                return last_inst;
            },
            .if_stmt => return conditional_lowering.lower(self, node_idx),
            .while_stmt, .for_stmt, .break_stmt, .continue_stmt => return loop_lowering.lower(self, node_idx),
            .fn_decl, .fn_proto, .param_decl => return function_lowering.lower(self, node_idx),
            .builtin_call => return builtin_lowering.lower(self, node_idx),
            .unary_op => {
                const operator = self.sema.ast_tree.tokens[node.main_token].tag;
                if (operator == .dot_asterisk or operator == .dot_question) return postfix.lowerUnarySuffix(self, node_idx);
                return prefix.lower(self, node_idx);
            },
            .array_access, .slice => return postfix.lower(self, node_idx),
            .defer_stmt, .errdefer_stmt => return cleanup_lowering.lower(self, node_idx),
            .match_stmt => return match_lowering.lower(self, node_idx),
            .unsafe_block => {
                result = try self.lowerNode(node.data.lhs);
                return result;
            },
            .tuple_literal => {
                result = try aggregate_lowering.lowerTupleLiteral(self, node_idx);
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
            .aggregate_literal => return aggregate_lowering.lowerLiteral(self, node_idx),
            .string_literal => {
                const token = self.sema.ast_tree.tokens[node.main_token];
                const source = self.sema.diags.source_manager.getFile(self.sema.source_id).?.content;
                const literal_id = try self.lir.addStringLiteral(source[token.start..token.end]);
                result = try self.emitInst(.{
                    .opcode = .string_literal,
                    .type_id = self.sema.node_types.get(node_idx) orelse 0,
                    .data = .{ .string_literal = literal_id },
                });
                const length_type = try self.sema.type_pool.internSizeInt(false);
                const length = try self.emitInst(.{
                    .opcode = .const_i,
                    .type_id = length_type,
                    .data = .{ .const_i = self.lir.string_literals.items[literal_id].len },
                });
                try self.slice_lengths.put(result.?, length);
                return result;
            },
            .field_access => {
                if (self.sema.resolved_decls.get(node_idx)) |declaration| {
                    if (self.sema.methodForDeclaration(declaration)) |method| {
                        const symbol = try self.lir.internModuleSymbol(self.sema.module_id orelse 0, method.qualified_name);
                        return try self.emitInst(.{
                            .opcode = .func_sym,
                            .type_id = self.sema.node_types.get(node_idx) orelse 0,
                            .data = .{ .func_sym = symbol },
                        });
                    }
                }
                if (self.sema.external_decls.get(node_idx)) |external| {
                    if (external.is_function) {
                        const symbol = try self.lir.internModuleSymbol(external.module_id, external.name);
                        return try self.emitInst(.{
                            .opcode = .func_sym,
                            .type_id = self.sema.node_types.get(node_idx) orelse 0,
                            .data = .{ .func_sym = symbol },
                        });
                    }
                }
                if (self.sema.const_values.get(node_idx)) |value| {
                    return try self.emitInst(.{
                        .opcode = .const_i,
                        .type_id = self.sema.node_types.get(node_idx) orelse 0,
                        .data = .{ .const_i = value },
                    });
                }
                return aggregate_lowering.lowerField(self, node_idx);
            },
            .call => return call_lowering.lower(self, node_idx),
            .return_stmt => return return_lowering.lower(self, node_idx),
            else => return null, // Unimplemented for now
        }
    }

    pub fn newBlock(self: *LirBuilder) !u32 {
        const blk_idx = @as(u32, @intCast(self.lir.blocks.items.len));
        try self.lir.blocks.append(self.allocator, @import("lir.zig").BasicBlock.init());
        return blk_idx;
    }

    pub fn pushCleanup(self: *LirBuilder, node_index: Node.Index, error_only: bool) !void {
        try self.cleanup_stack.append(self.allocator, .{ .explicit = .{ .node_index = node_index, .error_only = error_only } });
    }

    pub fn pushAutoDrop(self: *LirBuilder, address: Inst.Index, type_id: Type.Id, declaration: Node.Index) !void {
        if (self.sema.type_pool.get(type_id).copyability == .copyable) return;
        if (self.sema.aggregateMethod(type_id, "deinit") == null) return;
        const bool_type = try self.sema.type_pool.internPrimitive(.bool_type);
        const initialized = try self.emitInst(.{
            .opcode = .addr,
            .type_id = bool_type,
            .data = .{ .addr = self.nextSyntheticLocal() },
        });
        const one = try self.emitInst(.{ .opcode = .const_i, .type_id = bool_type, .data = .{ .const_i = 1 } });
        _ = try self.emitInst(.{
            .opcode = .store,
            .type_id = bool_type,
            .data = .{ .store = .{ .ptr = initialized, .val = one } },
        });
        const node = self.sema.ast_tree.nodes.get(declaration);
        const token = self.sema.ast_tree.tokens[node.data.lhs];
        const source = self.sema.diags.source_manager.getFile(self.sema.source_id).?.content;
        try self.drop_flags.put(source[token.start..token.end], initialized);
        try self.cleanup_stack.append(self.allocator, .{ .auto_drop = .{
            .address = address,
            .initialized = initialized,
            .type_id = type_id,
            .declaration = declaration,
        } });
    }

    pub fn markMoved(self: *LirBuilder, node_index: Node.Index) !void {
        try self.setDropFlag(node_index, 0);
    }

    pub fn markInitialized(self: *LirBuilder, node_index: Node.Index) !void {
        try self.setDropFlag(node_index, 1);
    }

    fn setDropFlag(self: *LirBuilder, node_index: Node.Index, value: u64) !void {
        const node = self.sema.ast_tree.nodes.get(node_index);
        if (node.tag != .identifier) return;
        const token = self.sema.ast_tree.tokens[node.main_token];
        const source = self.sema.diags.source_manager.getFile(self.sema.source_id).?.content;
        const address = self.drop_flags.get(source[token.start..token.end]) orelse return;
        const bool_type = try self.sema.type_pool.internPrimitive(.bool_type);
        const flag = try self.emitInst(.{ .opcode = .const_i, .type_id = bool_type, .data = .{ .const_i = value } });
        _ = try self.emitInst(.{
            .opcode = .store,
            .type_id = bool_type,
            .data = .{ .store = .{ .ptr = address, .val = flag } },
        });
    }

    pub fn nextSyntheticLocal(self: *LirBuilder) u32 {
        const result = self.synthetic_local_counter;
        self.synthetic_local_counter -= 1;
        return result;
    }

    pub fn emitCleanups(self: *LirBuilder, first: usize, is_error: bool) !void {
        var index = self.cleanup_stack.items.len;
        while (index > first) {
            index -= 1;
            const cleanup = self.cleanup_stack.items[index];
            switch (cleanup) {
                .explicit => |entry| {
                    if (entry.error_only and !is_error) continue;
                    _ = try self.lowerNode(entry.node_index);
                },
                .auto_drop => |entry| {
                    try self.emitAutoDrop(entry.address, entry.initialized, entry.type_id);
                },
            }
        }
    }

    fn emitAutoDrop(self: *LirBuilder, address: Inst.Index, initialized: Inst.Index, type_id: Type.Id) !void {
        const method = self.sema.aggregateMethod(type_id, "deinit") orelse return;
        const bool_type = try self.sema.type_pool.internPrimitive(.bool_type);
        const condition = try self.emitInst(.{
            .opcode = .load,
            .type_id = bool_type,
            .data = .{ .load = .{ .ptr = initialized } },
        });
        const drop_block = try self.newBlock();
        const end_block = try self.newBlock();
        _ = try self.emitInst(.{
            .opcode = .condbr,
            .type_id = 0,
            .data = .{ .condbr = .{ .cond = condition, .true_dest = drop_block, .false_dest = end_block } },
        });
        self.current_block = drop_block;
        const value = try self.emitInst(.{
            .opcode = .load,
            .type_id = type_id,
            .data = .{ .load = .{ .ptr = address } },
        });
        const function_type = blk: {
            const SemaType = @TypeOf(self.sema.*);
            const method_sema: *SemaType = @ptrFromInt(method.analysis_address);
            const declaration = method_sema.ast_tree.nodes.get(method.declaration);
            break :blk method_sema.node_types.get(method.declaration) orelse method_sema.node_types.get(declaration.data.lhs) orelse return;
        };
        const function_symbol = try self.emitInst(.{
            .opcode = .func_sym,
            .type_id = function_type,
            .data = .{ .func_sym = try self.lir.internModuleSymbol(method.module_id, method.qualified_name) },
        });
        const arguments_start: u32 = @intCast(self.lir.extra_data.items.len);
        try self.lir.extra_data.append(self.allocator, value);
        const void_type = try self.sema.type_pool.internPrimitive(.void_type);
        _ = try self.emitInst(.{
            .opcode = .call,
            .type_id = void_type,
            .data = .{ .call = .{ .func = function_symbol, .args_start = arguments_start, .args_count = 1 } },
        });
        _ = try self.emitInst(.{ .opcode = .br, .type_id = 0, .data = .{ .br = .{ .dest = end_block } } });
        self.current_block = end_block;
    }

    fn currentBlockTerminated(self: *const LirBuilder) bool {
        const block_index = self.current_block orelse return false;
        const instructions = self.lir.blocks.items[block_index].insts.items;
        if (instructions.len == 0) return false;
        return switch (self.lir.insts.items[instructions[instructions.len - 1]].opcode) {
            .br, .condbr, .ret, .ret_slice, .ret_error, .ret_error_union, .ret_error_slice, .ret_error_union_slice, .unreachable_inst => true,
            else => false,
        };
    }

    pub fn currentBlockTerminatedPublic(self: *const LirBuilder) bool {
        return self.currentBlockTerminated();
    }

    fn hasRuntimeValue(self: *const LirBuilder, type_id: Type.Id) bool {
        const ty = self.sema.type_pool.get(type_id);
        return !(ty.data == .primitive and (ty.data.primitive == .void_type or ty.data.primitive == .noreturn_type));
    }

    pub fn hasRuntimeValuePublic(self: *const LirBuilder, type_id: Type.Id) bool {
        return self.hasRuntimeValue(type_id);
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
                    .aggregate_copy => std.debug.print("value: %{d}", .{inst.data.aggregate_copy}),
                    .memory_copy => std.debug.print("destination: %{d}, source: %{d}, size: {d}", .{
                        inst.data.memory_copy.destination,
                        inst.data.memory_copy.source,
                        inst.data.memory_copy.size,
                    }),
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
                    .syscall => std.debug.print("args: extra[{d}]", .{inst.data.syscall}),
                    .ret => if (inst.data.ret) |r| std.debug.print("val: %{d}", .{r}) else std.debug.print("void", .{}),
                    .ret_slice => std.debug.print("ptr: %{d}, len: %{d}", .{ inst.data.ret_slice.ptr, inst.data.ret_slice.len }),
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
