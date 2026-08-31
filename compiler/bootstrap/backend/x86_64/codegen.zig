const std = @import("std");
const lir = @import("../../ir/lir.zig");
const x86 = @import("target.zig");
const abi = @import("abi.zig");
const ast_mod = @import("../../syntax/ast.zig");
const Encoder = @import("encoder.zig").Encoder;
const Condition = @import("encoder.zig").Condition;
const Reg = x86.Register;
const TypePool = @import("../../semantic/type.zig").TypePool;
const integer_instructions = @import("instructions/integer.zig");

/// x86_64 code generator for zin0.
/// Operates in two modes:
///   • text  — emits NASM syntax to a writer (used by --emit=asm)
///   • binary — emits raw machine code into an Encoder (default path)
pub const X86Gen = struct {
    pub const Operand = union(enum) {
        reg: []const u8,
        mem: i32,
    };

    allocator: std.mem.Allocator,
    lir: *lir.Lir,
    type_pool: *const TypePool,
    ast_tree: ast_mod.Ast,
    src: []const u8,
    vreg_to_op: std.AutoHashMap(lir.Inst.Index, Operand),
    addr_to_slot: std.AutoHashMap(u32, i32),
    error_tag_slots: std.AutoHashMap(lir.Inst.Index, i32),
    error_payload_extra_slots: std.AutoHashMap(lir.Inst.Index, i32),
    next_gp_reg: u8,
    next_stack_slot: i32,
    /// Arena for block-label strings — freed as a batch after applyFixups
    label_arena: std.heap.ArenaAllocator,
    /// Accumulates string literals for .rodata section
    rodata: std.ArrayList(u8),
    /// Maps LIR inst index of a string_literal to its byte offset in rodata
    string_offsets: std.AutoHashMap(lir.Inst.Index, u64),
    /// Virtual address of .rodata section (filled in by generateBinary)
    rodata_vaddr: u64,
    current_function_return_type: ?u32,
    current_hidden_payload_slot: ?i32,

    const gp_regs = [_][]const u8{
        "rdx", "rbx", "r10", "r11", "r12", "r13", "r14", "r15",
    };

    pub fn init(
        allocator: std.mem.Allocator,
        ir: *lir.Lir,
        type_pool: *const TypePool,
        ast_tree: ast_mod.Ast,
        src: []const u8,
    ) X86Gen {
        return .{
            .allocator = allocator,
            .lir = ir,
            .type_pool = type_pool,
            .ast_tree = ast_tree,
            .src = src,
            .vreg_to_op = std.AutoHashMap(lir.Inst.Index, Operand).init(allocator),
            .addr_to_slot = std.AutoHashMap(u32, i32).init(allocator),
            .error_tag_slots = std.AutoHashMap(lir.Inst.Index, i32).init(allocator),
            .error_payload_extra_slots = std.AutoHashMap(lir.Inst.Index, i32).init(allocator),
            .next_gp_reg = 0,
            .next_stack_slot = 8,
            .label_arena = std.heap.ArenaAllocator.init(allocator),
            .rodata = std.ArrayList(u8).empty,
            .string_offsets = std.AutoHashMap(lir.Inst.Index, u64).init(allocator),
            .rodata_vaddr = 0,
            .current_function_return_type = null,
            .current_hidden_payload_slot = null,
        };
    }

    pub fn deinit(self: *X86Gen) void {
        self.vreg_to_op.deinit();
        self.addr_to_slot.deinit();
        self.error_tag_slots.deinit();
        self.error_payload_extra_slots.deinit();
        self.label_arena.deinit();
        self.rodata.deinit(self.allocator);
        self.string_offsets.deinit();
    }

    // ── register allocator ───────────────────────────────────────────────────

    pub fn allocateOp(self: *X86Gen, vreg: lir.Inst.Index) !Operand {
        if (self.vreg_to_op.get(vreg)) |op| return op;
        if (self.next_gp_reg < gp_regs.len) {
            const reg = gp_regs[self.next_gp_reg];
            self.next_gp_reg += 1;
            const op = Operand{ .reg = reg };
            try self.vreg_to_op.put(vreg, op);
            return op;
        }
        const op = Operand{ .mem = @as(i32, @intCast(self.next_stack_slot)) };
        self.next_stack_slot += 8;
        try self.vreg_to_op.put(vreg, op);
        return op;
    }

    // ── text helpers ─────────────────────────────────────────────────────────

    pub fn printOp(writer: anytype, op: Operand) !void {
        switch (op) {
            .reg => |r| try writer.print("{s}", .{r}),
            .mem => |m| try writer.print("qword [rbp - {d}]", .{m}),
        }
    }

    pub fn opToReg(writer: anytype, op: Operand, scratch: []const u8) ![]const u8 {
        switch (op) {
            .reg => |r| return r,
            .mem => {
                try writer.print("  mov {s}, ", .{scratch});
                try printOp(writer, op);
                try writer.print("\n", .{});
                return scratch;
            },
        }
    }

    // ── binary helpers ───────────────────────────────────────────────────────

    pub fn nameToReg(name: []const u8) Reg {
        if (std.mem.eql(u8, name, "rax")) return .rax;
        if (std.mem.eql(u8, name, "rcx")) return .rcx;
        if (std.mem.eql(u8, name, "rdx")) return .rdx;
        if (std.mem.eql(u8, name, "rbx")) return .rbx;
        if (std.mem.eql(u8, name, "rsi")) return .rsi;
        if (std.mem.eql(u8, name, "rdi")) return .rdi;
        if (std.mem.eql(u8, name, "r8")) return .r8;
        if (std.mem.eql(u8, name, "r9")) return .r9;
        if (std.mem.eql(u8, name, "r10")) return .r10;
        if (std.mem.eql(u8, name, "r11")) return .r11;
        if (std.mem.eql(u8, name, "r12")) return .r12;
        if (std.mem.eql(u8, name, "r13")) return .r13;
        if (std.mem.eql(u8, name, "r14")) return .r14;
        if (std.mem.eql(u8, name, "r15")) return .r15;
        return .rax; // fallback
    }

    /// Load an operand into `scratch` register if it is a mem operand.
    /// Returns the effective register.
    pub fn opToRegBin(enc: *Encoder, op: Operand, scratch: Reg) !Reg {
        switch (op) {
            .reg => |r| return nameToReg(r),
            .mem => |m| {
                try enc.emitMovRegMem(scratch, m);
                return scratch;
            },
        }
    }

    /// Store `src_reg` into `op` if op is a mem operand. No-op for reg ops.
    fn storeToOp(enc: *Encoder, op: Operand, src_reg: Reg) !void {
        switch (op) {
            .reg => {}, // value already in register
            .mem => |m| try enc.emitMovMemReg(m, src_reg),
        }
    }

    // ── stack frame helpers ──────────────────────────────────────────────────

    fn getOrAllocSlot(self: *X86Gen, var_id: u32) !i32 {
        if (self.addr_to_slot.get(var_id)) |s| return s;
        const s = self.next_stack_slot;
        self.next_stack_slot += 8;
        try self.addr_to_slot.put(var_id, s);
        return s;
    }

    fn getOrAllocBlock(self: *X86Gen, var_id: u32, size: u32, alignment: u32) !i32 {
        if (self.addr_to_slot.get(var_id)) |slot| return slot;
        const safe_size: i32 = @intCast(@max(size, 1));
        const safe_alignment: i32 = @intCast(@max(alignment, 1));
        const unaligned_base = self.next_stack_slot + safe_size - 1;
        const base = @divTrunc(unaligned_base + safe_alignment - 1, safe_alignment) * safe_alignment;
        // The next qword slot spans [offset-7, offset]. Round strictly past
        // the block's highest occupied offset so neither region overlaps.
        self.next_stack_slot = (base + 15) & ~@as(i32, 7);
        try self.addr_to_slot.put(var_id, base);
        return base;
    }

    fn getOrAllocErrorTagSlot(self: *X86Gen, instruction: lir.Inst.Index) !i32 {
        if (self.error_tag_slots.get(instruction)) |slot| return slot;
        const slot = self.next_stack_slot;
        self.next_stack_slot += 8;
        try self.error_tag_slots.put(instruction, slot);
        return slot;
    }

    fn getOrAllocErrorPayloadExtraSlot(self: *X86Gen, instruction: lir.Inst.Index) !i32 {
        if (self.error_payload_extra_slots.get(instruction)) |slot| return slot;
        const slot = self.next_stack_slot;
        self.next_stack_slot += 8;
        try self.error_payload_extra_slots.put(instruction, slot);
        return slot;
    }

    fn isErrorUnion(self: *const X86Gen, type_id: u32) bool {
        return type_id < self.type_pool.types.items.len and self.type_pool.get(type_id).data == .error_union;
    }

    fn isSliceErrorUnion(self: *const X86Gen, type_id: u32) bool {
        if (!self.isErrorUnion(type_id)) return false;
        const payload_id = self.type_pool.get(type_id).data.error_union.payload;
        const payload = self.type_pool.get(payload_id);
        return payload.data == .pointer and payload.data.pointer.size == .Slice;
    }

    fn isByteType(self: *const X86Gen, type_id: u32) bool {
        if (type_id >= self.type_pool.types.items.len) return false;
        const bits = self.type_pool.bitSizeOf(type_id) catch return false;
        return bits > 0 and bits <= 8;
    }

    fn isSignedType(self: *const X86Gen, type_id: u32) bool {
        if (type_id >= self.type_pool.types.items.len) return false;
        return switch (self.type_pool.get(type_id).data) {
            .integer => |integer| integer.is_signed,
            .size_int => |integer| integer.is_signed,
            else => false,
        };
    }

    fn isFloatType(self: *const X86Gen, type_id: u32) bool {
        if (type_id >= self.type_pool.types.items.len) return false;
        return self.type_pool.get(type_id).isFloat();
    }

    fn isFloatErrorUnion(self: *const X86Gen, type_id: u32) bool {
        if (!self.isErrorUnion(type_id)) return false;
        return self.isFloatType(self.type_pool.get(type_id).data.error_union.payload);
    }

    fn isMemoryErrorUnion(self: *const X86Gen, type_id: u32) bool {
        if (!self.isErrorUnion(type_id)) return false;
        const payload_id = self.type_pool.get(type_id).data.error_union.payload;
        const size = self.type_pool.sizeOf(payload_id) catch return true;
        const alignment = self.type_pool.alignOf(payload_id) catch return true;
        return size > 16 or alignment > 16;
    }

    fn isRegisterAggregateErrorUnion(self: *const X86Gen, type_id: u32) bool {
        if (!self.isErrorUnion(type_id) or self.isMemoryErrorUnion(type_id)) return false;
        const payload_id = self.type_pool.get(type_id).data.error_union.payload;
        return switch (self.type_pool.get(payload_id).data) {
            .array, .@"struct", .@"union", .tuple => true,
            else => false,
        };
    }

    fn errorPayloadLayout(self: *const X86Gen, type_id: u32) struct { size: u32, alignment: u32 } {
        const payload_id = self.type_pool.get(type_id).data.error_union.payload;
        return .{
            .size = @intCast(self.type_pool.sizeOf(payload_id) catch 8),
            .alignment = @intCast(self.type_pool.alignOf(payload_id) catch 8),
        };
    }

    fn condition(predicate: lir.CmpPredicate) Condition {
        return switch (predicate) {
            .eq => .equal,
            .ne => .not_equal,
            .lt => .less,
            .le => .less_equal,
            .gt => .greater,
            .ge => .greater_equal,
            .ult => .below,
            .ule => .below_equal,
            .ugt => .above,
            .uge => .above_equal,
        };
    }

    fn conditionName(predicate: lir.CmpPredicate) []const u8 {
        return switch (predicate) {
            .eq => "e",
            .ne => "ne",
            .lt => "l",
            .le => "le",
            .gt => "g",
            .ge => "ge",
            .ult => "b",
            .ule => "be",
            .ugt => "a",
            .uge => "ae",
        };
    }

    // ── block label helpers ───────────────────────────────────────────────────

    fn blockLabel(self: *X86Gen, idx: usize) ![]u8 {
        return std.fmt.allocPrint(self.label_arena.allocator(), ".block_{d}", .{idx});
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  TEXT path (NASM output)
    // ─────────────────────────────────────────────────────────────────────────

    pub fn generate(self: *X86Gen, writer: anytype) !void {
        std.debug.print("[X86Gen] Starting text code generation\n", .{});

        try writer.print("global _start\nsection .text\n", .{});
        try writer.print("_start:\n", .{});
        try writer.print("  call main\n", .{});
        try writer.print("  mov rdi, rax\n", .{});
        try writer.print("  mov rax, 60\n", .{});
        try writer.print("  syscall\n", .{});

        for (self.lir.blocks.items, 0..) |blk, blk_idx| {
            std.debug.print("[X86Gen] Emitting block {d} ({d} insts)\n", .{ blk_idx, blk.insts.items.len });
            try writer.print(".block_{d}:\n", .{blk_idx});
            for (blk.insts.items) |inst_idx| {
                try self.emitInstText(writer, inst_idx);
            }
        }
        std.debug.print("[X86Gen] Text code generation complete\n", .{});
    }

    fn emitInstText(self: *X86Gen, writer: anytype, inst_idx: lir.Inst.Index) !void {
        const inst = self.lir.insts.items[inst_idx];
        std.debug.print("[X86Gen]   inst %{d} = {s}\n", .{ inst_idx, @tagName(inst.opcode) });

        switch (inst.opcode) {
            .const_i => {
                const op = try self.allocateOp(inst_idx);
                if (op == .mem) {
                    try writer.print("  mov rax, {d}\n", .{inst.data.const_i});
                    try writer.print("  mov ", .{});
                    try printOp(writer, op);
                    try writer.print(", rax\n", .{});
                } else {
                    try writer.print("  mov {s}, {d}\n", .{ op.reg, inst.data.const_i });
                }
            },
            .const_f => {
                const op = try self.allocateOp(inst_idx);
                const bits: u64 = @bitCast(inst.data.const_f);
                if (op == .mem) {
                    try writer.print("  mov rax, {d}\n  mov ", .{bits});
                    try printOp(writer, op);
                    try writer.print(", rax\n", .{});
                } else {
                    try writer.print("  mov {s}, {d}\n", .{ op.reg, bits });
                }
            },
            .add => {
                const op = try self.allocateOp(inst_idx);
                const lhs = try self.allocateOp(inst.data.add.lhs);
                const rhs = try self.allocateOp(inst.data.add.rhs);
                const lreg = try opToReg(writer, lhs, "rax");
                if (op == .mem) {
                    try writer.print("  mov rax, {s}\n", .{lreg});
                    const rreg = try opToReg(writer, rhs, "rcx");
                    try writer.print("  add rax, {s}\n", .{rreg});
                    try writer.print("  mov ", .{});
                    try printOp(writer, op);
                    try writer.print(", rax\n", .{});
                } else {
                    if (!std.mem.eql(u8, op.reg, lreg)) try writer.print("  mov {s}, {s}\n", .{ op.reg, lreg });
                    const rreg = try opToReg(writer, rhs, "rcx");
                    try writer.print("  add {s}, {s}\n", .{ op.reg, rreg });
                }
            },
            .sub => {
                const op = try self.allocateOp(inst_idx);
                const lhs = try self.allocateOp(inst.data.sub.lhs);
                const rhs = try self.allocateOp(inst.data.sub.rhs);
                const lreg = try opToReg(writer, lhs, "rax");
                if (op == .mem) {
                    try writer.print("  mov rax, {s}\n", .{lreg});
                    const rreg = try opToReg(writer, rhs, "rcx");
                    try writer.print("  sub rax, {s}\n", .{rreg});
                    try writer.print("  mov ", .{});
                    try printOp(writer, op);
                    try writer.print(", rax\n", .{});
                } else {
                    if (!std.mem.eql(u8, op.reg, lreg)) try writer.print("  mov {s}, {s}\n", .{ op.reg, lreg });
                    const rreg = try opToReg(writer, rhs, "rcx");
                    try writer.print("  sub {s}, {s}\n", .{ op.reg, rreg });
                }
            },
            .mul => {
                const op = try self.allocateOp(inst_idx);
                const lhs = try self.allocateOp(inst.data.mul.lhs);
                const rhs = try self.allocateOp(inst.data.mul.rhs);
                const lreg = try opToReg(writer, lhs, "rax");
                try writer.print("  mov rax, {s}\n", .{lreg});
                const rreg = try opToReg(writer, rhs, "rcx");
                try writer.print("  imul rax, {s}\n", .{rreg});
                if (op == .mem) {
                    try writer.print("  mov ", .{});
                    try printOp(writer, op);
                    try writer.print(", rax\n", .{});
                } else {
                    try writer.print("  mov {s}, rax\n", .{op.reg});
                }
            },
            .div => {
                try integer_instructions.emitText(self, writer, inst_idx);
            },
            .rem, .bit_and, .bit_or, .bit_xor, .shl, .shr => try integer_instructions.emitText(self, writer, inst_idx),
            .icmp => {
                const op = try self.allocateOp(inst_idx);
                const lhs = try self.allocateOp(inst.data.icmp.lhs);
                const rhs = try self.allocateOp(inst.data.icmp.rhs);
                const lhs_reg = try opToReg(writer, lhs, "rax");
                const rhs_reg = try opToReg(writer, rhs, "rcx");
                try writer.print("  cmp {s}, {s}\n", .{ lhs_reg, rhs_reg });
                try writer.print("  mov rax, 0\n  set{s} al\n", .{conditionName(inst.data.icmp.predicate)});
                if (op == .mem) {
                    try writer.print("  mov ", .{});
                    try printOp(writer, op);
                    try writer.print(", rax\n", .{});
                } else if (!std.mem.eql(u8, op.reg, "rax")) {
                    try writer.print("  mov {s}, rax\n", .{op.reg});
                }
            },
            .gep => {
                const op = try self.allocateOp(inst_idx);
                const base = try self.allocateOp(inst.data.gep.base);
                const index = try self.allocateOp(inst.data.gep.index);
                const base_reg = try opToReg(writer, base, "rax");
                if (!std.mem.eql(u8, base_reg, "rax")) try writer.print("  mov rax, {s}\n", .{base_reg});
                const index_reg = try opToReg(writer, index, "rcx");
                if (!std.mem.eql(u8, index_reg, "rcx")) try writer.print("  mov rcx, {s}\n", .{index_reg});
                const magnitude: u32 = @intCast(@abs(inst.data.gep.stride));
                if (magnitude != 1) try writer.print("  imul rcx, {d}\n", .{magnitude});
                try writer.print("  {s} rax, rcx\n", .{if (inst.data.gep.stride < 0) "sub" else "add"});
                switch (op) {
                    .reg => |register| if (!std.mem.eql(u8, register, "rax")) try writer.print("  mov {s}, rax\n", .{register}),
                    .mem => {
                        try writer.print("  mov ", .{});
                        try printOp(writer, op);
                        try writer.print(", rax\n", .{});
                    },
                }
            },
            .addr => {
                const op = try self.allocateOp(inst_idx);
                const var_id = inst.data.addr;
                const slot = try self.getOrAllocSlot(var_id);
                if (op == .mem) {
                    try writer.print("  lea rax, [rbp - {d}]\n", .{slot});
                    try writer.print("  mov ", .{});
                    try printOp(writer, op);
                    try writer.print(", rax\n", .{});
                } else {
                    try writer.print("  lea {s}, [rbp - {d}]\n", .{ op.reg, slot });
                }
            },
            .alloca => {
                const op = try self.allocateOp(inst_idx);
                const allocation = inst.data.alloca;
                const slot = try self.getOrAllocBlock(allocation.id, allocation.size, allocation.alignment);
                if (op == .mem) {
                    try writer.print("  lea rax, [rbp - {d}]\n  mov ", .{slot});
                    try printOp(writer, op);
                    try writer.print(", rax\n", .{});
                } else {
                    try writer.print("  lea {s}, [rbp - {d}]\n", .{ op.reg, slot });
                }
            },
            .store => {
                const ptr_op = try self.allocateOp(inst.data.store.ptr);
                const val_op = try self.allocateOp(inst.data.store.val);
                const ptr_reg = try opToReg(writer, ptr_op, "rax");
                const val_reg = try opToReg(writer, val_op, "rcx");
                if (self.isByteType(inst.type_id)) {
                    if (!std.mem.eql(u8, ptr_reg, "rdi")) try writer.print("  mov rdi, {s}\n", .{ptr_reg});
                    if (!std.mem.eql(u8, val_reg, "rax")) try writer.print("  mov rax, {s}\n", .{val_reg});
                    try writer.print("  mov byte [rdi], al\n", .{});
                } else {
                    try writer.print("  mov qword [{s}], {s}\n", .{ ptr_reg, val_reg });
                }
            },
            .load => {
                const op = try self.allocateOp(inst_idx);
                const ptr_op = try self.allocateOp(inst.data.load.ptr);
                const ptr_reg = try opToReg(writer, ptr_op, "rax");
                const byte_load = self.isByteType(inst.type_id);
                const load_instruction = if (byte_load and self.isSignedType(inst.type_id)) "movsx" else if (byte_load) "movzx" else "mov";
                if (op == .mem) {
                    try writer.print("  {s} rcx, {s}[{s}]\n", .{ load_instruction, if (byte_load) "byte " else "qword ", ptr_reg });
                    try writer.print("  mov ", .{});
                    try printOp(writer, op);
                    try writer.print(", rcx\n", .{});
                } else {
                    try writer.print("  {s} {s}, {s}[{s}]\n", .{ load_instruction, op.reg, if (byte_load) "byte " else "qword ", ptr_reg });
                }
            },
            .param => {
                const op = try self.allocateOp(inst_idx);
                const param_idx = inst.data.param + @intFromBool(if (self.current_function_return_type) |return_type| self.isMemoryErrorUnion(return_type) else false);
                if (param_idx < abi.integer_arg_regs.len) {
                    const arg_reg = abi.integer_arg_regs[param_idx];
                    if (op == .mem) {
                        try writer.print("  mov ", .{});
                        try printOp(writer, op);
                        try writer.print(", {s}\n", .{arg_reg});
                    } else {
                        try writer.print("  mov {s}, {s}\n", .{ op.reg, arg_reg });
                    }
                } else {
                    const stack_offset: i32 = 16 + @as(i32, @intCast((param_idx - 6) * 8));
                    try writer.print("  mov rax, qword [rbp + {d}]\n", .{stack_offset});
                    if (op == .mem) {
                        try writer.print("  mov ", .{});
                        try printOp(writer, op);
                        try writer.print(", rax\n", .{});
                    } else {
                        try writer.print("  mov {s}, rax\n", .{op.reg});
                    }
                }
            },
            .func_sym => {
                const op = try self.allocateOp(inst_idx);
                const fn_name = self.lir.symbols.items[inst.data.func_sym];
                if (op == .mem) {
                    try writer.print("  lea rax, [rel {s}]\n", .{fn_name});
                    try writer.print("  mov ", .{});
                    try printOp(writer, op);
                    try writer.print(", rax\n", .{});
                } else {
                    try writer.print("  lea {s}, [rel {s}]\n", .{ op.reg, fn_name });
                }
            },
            .label => {
                const fn_name = self.lir.symbols.items[inst.data.label];
                try writer.print("global {s}\n{s}:\n", .{ fn_name, fn_name });
                try writer.print("  push rbp\n  mov rbp, rsp\n  sub rsp, 4096\n", .{});
                self.current_function_return_type = inst.type_id;
                self.current_hidden_payload_slot = null;
                if (self.isMemoryErrorUnion(inst.type_id)) {
                    const slot = try self.getOrAllocSlot(0xb000_0000 | inst.data.label);
                    self.current_hidden_payload_slot = slot;
                    try writer.print("  mov qword [rbp - {d}], rdi\n", .{slot});
                }
            },
            .call => {
                const op = try self.allocateOp(inst_idx);
                const num_args = inst.data.call.args_count;
                const args_extra_start = inst.data.call.args_start;
                const memory_return = self.isMemoryErrorUnion(inst.type_id);
                const register_aggregate_return = self.isRegisterAggregateErrorUnion(inst.type_id);
                var memory_payload_slot: ?i32 = null;
                if (memory_return or register_aggregate_return) {
                    const layout = self.errorPayloadLayout(inst.type_id);
                    const slot = try self.getOrAllocBlock(0xa000_0000 | inst_idx, @max(layout.size, 8), layout.alignment);
                    memory_payload_slot = slot;
                    if (memory_return) try writer.print("  lea rdi, [rbp - {d}]\n", .{slot});
                }
                var arg_classes_buf: [32]abi.ArgClass = undefined;
                const n = @min(num_args + @intFromBool(memory_return), 32);
                for (0..n) |i| arg_classes_buf[i] = .INTEGER;
                const site = abi.describeCall(arg_classes_buf[0..n]);
                if (site.stack_count > 0) {
                    var j: i32 = @as(i32, @intCast(num_args)) - 1;
                    while (j >= @as(i32, @intCast(abi.integer_arg_regs.len))) : (j -= 1) {
                        const arg_inst = self.lir.extra_data.items[args_extra_start + @as(u32, @intCast(j))];
                        const arg_op = try self.allocateOp(arg_inst);
                        const arg_reg = try opToReg(writer, arg_op, "rax");
                        try writer.print("  push {s}\n", .{arg_reg});
                    }
                    if (site.needs_stack_align) try writer.print("  sub rsp, 8\n", .{});
                }
                const register_offset: u32 = @intFromBool(memory_return);
                var i: u32 = @min(num_args, @as(u32, abi.integer_arg_regs.len) - register_offset);
                while (i > 0) {
                    i -= 1;
                    const arg_inst = self.lir.extra_data.items[args_extra_start + i];
                    const arg_op = try self.allocateOp(arg_inst);
                    const dest_reg = abi.integer_arg_regs[i + register_offset];
                    const src_reg = try opToReg(writer, arg_op, "rax");
                    if (!std.mem.eql(u8, dest_reg, src_reg)) try writer.print("  mov {s}, {s}\n", .{ dest_reg, src_reg });
                }
                const func_inst = inst.data.call.func;
                const func_op = try self.allocateOp(func_inst);
                switch (func_op) {
                    .reg => |r| try writer.print("  call {s}\n", .{r}),
                    .mem => {
                        try writer.print("  mov rax, ", .{});
                        try printOp(writer, func_op);
                        try writer.print("\n", .{});
                        try writer.print("  call rax\n", .{});
                    },
                }
                if (site.stack_bytes > 0) {
                    var adjust = site.stack_bytes;
                    if (site.needs_stack_align) adjust += 8;
                    try writer.print("  add rsp, {d}\n", .{adjust});
                }
                if (self.isErrorUnion(inst.type_id)) {
                    const tag_slot = try self.getOrAllocErrorTagSlot(inst_idx);
                    try writer.print("  mov qword [rbp - {d}], rax\n", .{tag_slot});
                    if (self.isSliceErrorUnion(inst.type_id)) {
                        const length_slot = try self.getOrAllocErrorPayloadExtraSlot(inst_idx);
                        try writer.print("  mov qword [rbp - {d}], rcx\n", .{length_slot});
                    }
                    if (memory_return) {
                        const slot = memory_payload_slot.?;
                        if (op == .mem) {
                            try writer.print("  lea rax, [rbp - {d}]\n  mov ", .{slot});
                            try printOp(writer, op);
                            try writer.print(", rax\n", .{});
                        } else {
                            try writer.print("  lea {s}, [rbp - {d}]\n", .{ op.reg, slot });
                        }
                    } else if (register_aggregate_return) {
                        const slot = memory_payload_slot.?;
                        const layout = self.errorPayloadLayout(inst.type_id);
                        try writer.print("  lea rdi, [rbp - {d}]\n  mov qword [rdi], rdx\n", .{slot});
                        if (layout.size > 8) try writer.print("  mov qword [rdi + 8], rcx\n", .{});
                        if (op == .mem) {
                            try writer.print("  lea rax, [rbp - {d}]\n  mov ", .{slot});
                            try printOp(writer, op);
                            try writer.print(", rax\n", .{});
                        } else {
                            try writer.print("  lea {s}, [rbp - {d}]\n", .{ op.reg, slot });
                        }
                    } else if (self.isFloatErrorUnion(inst.type_id)) {
                        if (op == .mem) {
                            try writer.print("  movq rax, xmm0\n  mov ", .{});
                            try printOp(writer, op);
                            try writer.print(", rax\n", .{});
                        } else {
                            try writer.print("  movq {s}, xmm0\n", .{op.reg});
                        }
                    } else if (op == .mem) {
                        try writer.print("  mov ", .{});
                        try printOp(writer, op);
                        try writer.print(", rdx\n", .{});
                    } else if (!std.mem.eql(u8, op.reg, "rdx")) {
                        try writer.print("  mov {s}, rdx\n", .{op.reg});
                    }
                } else if (op == .mem) {
                    try writer.print("  mov ", .{});
                    try printOp(writer, op);
                    try writer.print(", rax\n", .{});
                } else if (!std.mem.eql(u8, op.reg, "rax")) {
                    try writer.print("  mov {s}, rax\n", .{op.reg});
                }
            },
            .error_test => {
                const op = try self.allocateOp(inst_idx);
                const tag_slot = try self.getOrAllocErrorTagSlot(inst.data.error_test);
                try writer.print("  mov rax, qword [rbp - {d}]\n", .{tag_slot});
                if (op == .mem) {
                    try writer.print("  mov ", .{});
                    try printOp(writer, op);
                    try writer.print(", rax\n", .{});
                } else if (!std.mem.eql(u8, op.reg, "rax")) {
                    try writer.print("  mov {s}, rax\n", .{op.reg});
                }
            },
            .error_payload => {
                const op = try self.allocateOp(inst_idx);
                const payload = try self.allocateOp(inst.data.error_payload);
                const payload_reg = try opToReg(writer, payload, "rax");
                if (op == .mem) {
                    try writer.print("  mov ", .{});
                    try printOp(writer, op);
                    try writer.print(", {s}\n", .{payload_reg});
                } else if (!std.mem.eql(u8, op.reg, payload_reg)) {
                    try writer.print("  mov {s}, {s}\n", .{ op.reg, payload_reg });
                }
            },
            .error_payload_part => {
                const op = try self.allocateOp(inst_idx);
                const slot = try self.getOrAllocErrorPayloadExtraSlot(inst.data.error_payload_part.source);
                if (op == .mem) {
                    try writer.print("  mov rax, qword [rbp - {d}]\n  mov ", .{slot});
                    try printOp(writer, op);
                    try writer.print(", rax\n", .{});
                } else {
                    try writer.print("  mov {s}, qword [rbp - {d}]\n", .{ op.reg, slot });
                }
            },
            .br => try writer.print("  jmp .block_{d}\n", .{inst.data.br.dest}),
            .condbr => {
                const cond_op = try self.allocateOp(inst.data.condbr.cond);
                switch (cond_op) {
                    .reg => |r| try writer.print("  test {s}, {s}\n", .{ r, r }),
                    .mem => {
                        try writer.print("  mov rax, ", .{});
                        try printOp(writer, cond_op);
                        try writer.print("\n", .{});
                        try writer.print("  test rax, rax\n", .{});
                    },
                }
                try writer.print("  jnz .block_{d}\n", .{inst.data.condbr.true_dest});
                try writer.print("  jmp .block_{d}\n", .{inst.data.condbr.false_dest});
            },
            .ret => {
                if (self.isErrorUnion(inst.type_id)) {
                    if (inst.data.ret) |r| {
                        const val_op = try self.allocateOp(r);
                        if (self.isMemoryErrorUnion(inst.type_id)) {
                            const source_reg = try opToReg(writer, val_op, "rsi");
                            if (!std.mem.eql(u8, source_reg, "rsi")) try writer.print("  mov rsi, {s}\n", .{source_reg});
                            try writer.print("  mov rdi, qword [rbp - {d}]\n", .{self.current_hidden_payload_slot.?});
                            const layout = self.errorPayloadLayout(inst.type_id);
                            var offset: u32 = 0;
                            while (offset + 8 <= layout.size) : (offset += 8) {
                                try writer.print("  mov rax, qword [rsi + {d}]\n  mov qword [rdi + {d}], rax\n", .{ offset, offset });
                            }
                            while (offset < layout.size) : (offset += 1) {
                                try writer.print("  mov al, byte [rsi + {d}]\n  mov byte [rdi + {d}], al\n", .{ offset, offset });
                            }
                        } else if (self.isRegisterAggregateErrorUnion(inst.type_id)) {
                            const source_reg = try opToReg(writer, val_op, "rsi");
                            if (!std.mem.eql(u8, source_reg, "rsi")) try writer.print("  mov rsi, {s}\n", .{source_reg});
                            try writer.print("  mov rdx, qword [rsi]\n", .{});
                            if (self.errorPayloadLayout(inst.type_id).size > 8) try writer.print("  mov rcx, qword [rsi + 8]\n", .{});
                        } else if (self.isFloatErrorUnion(inst.type_id)) {
                            const val_reg = try opToReg(writer, val_op, "rdx");
                            try writer.print("  movq xmm0, {s}\n", .{val_reg});
                        } else {
                            const val_reg = try opToReg(writer, val_op, "rdx");
                            if (!std.mem.eql(u8, val_reg, "rdx")) try writer.print("  mov rdx, {s}\n", .{val_reg});
                        }
                    }
                    try writer.print("  mov rax, 0\n", .{});
                } else if (inst.data.ret) |r| {
                    const val_op = try self.allocateOp(r);
                    switch (val_op) {
                        .reg => |reg| {
                            if (!std.mem.eql(u8, reg, "rax")) try writer.print("  mov rax, {s}\n", .{reg});
                        },
                        .mem => {
                            try writer.print("  mov rax, ", .{});
                            try printOp(writer, val_op);
                            try writer.print("\n", .{});
                        },
                    }
                }
                try writer.print("  mov rsp, rbp\n  pop rbp\n  ret\n", .{});
            },
            .ret_error => {
                const tag_op = try self.allocateOp(inst.data.ret_error);
                const tag_reg = try opToReg(writer, tag_op, "rax");
                if (!std.mem.eql(u8, tag_reg, "rax")) try writer.print("  mov rax, {s}\n", .{tag_reg});
                try writer.print("  mov rsp, rbp\n  pop rbp\n  ret\n", .{});
            },
            .ret_error_union => {
                const source = inst.data.ret_error_union;
                const tag_slot = try self.getOrAllocErrorTagSlot(source);
                const payload = try self.allocateOp(source);
                const payload_reg = try opToReg(writer, payload, if (self.isMemoryErrorUnion(inst.type_id)) "rsi" else "rdx");
                if (self.isMemoryErrorUnion(inst.type_id)) {
                    if (!std.mem.eql(u8, payload_reg, "rsi")) try writer.print("  mov rsi, {s}\n", .{payload_reg});
                    try writer.print("  mov rdi, qword [rbp - {d}]\n", .{self.current_hidden_payload_slot.?});
                    const layout = self.errorPayloadLayout(inst.type_id);
                    var offset: u32 = 0;
                    while (offset + 8 <= layout.size) : (offset += 8) {
                        try writer.print("  mov rax, qword [rsi + {d}]\n  mov qword [rdi + {d}], rax\n", .{ offset, offset });
                    }
                    while (offset < layout.size) : (offset += 1) {
                        try writer.print("  mov al, byte [rsi + {d}]\n  mov byte [rdi + {d}], al\n", .{ offset, offset });
                    }
                } else if (self.isRegisterAggregateErrorUnion(inst.type_id)) {
                    if (!std.mem.eql(u8, payload_reg, "rsi")) try writer.print("  mov rsi, {s}\n", .{payload_reg});
                    try writer.print("  mov rdx, qword [rsi]\n", .{});
                    if (self.errorPayloadLayout(inst.type_id).size > 8) try writer.print("  mov rcx, qword [rsi + 8]\n", .{});
                }
                try writer.print("  mov rax, qword [rbp - {d}]\n", .{tag_slot});
                if (self.isFloatErrorUnion(inst.type_id)) {
                    try writer.print("  movq xmm0, {s}\n", .{payload_reg});
                } else if (!self.isMemoryErrorUnion(inst.type_id) and !self.isRegisterAggregateErrorUnion(inst.type_id) and !std.mem.eql(u8, payload_reg, "rdx")) {
                    try writer.print("  mov rdx, {s}\n", .{payload_reg});
                }
                try writer.print("  mov rsp, rbp\n  pop rbp\n  ret\n", .{});
            },
            .ret_error_slice => {
                const pointer = try self.allocateOp(inst.data.ret_error_slice.ptr);
                const length = try self.allocateOp(inst.data.ret_error_slice.len);
                const pointer_reg = try opToReg(writer, pointer, "rdx");
                const length_reg = try opToReg(writer, length, "rcx");
                try writer.print("  mov rax, 0\n", .{});
                if (!std.mem.eql(u8, pointer_reg, "rdx")) try writer.print("  mov rdx, {s}\n", .{pointer_reg});
                if (!std.mem.eql(u8, length_reg, "rcx")) try writer.print("  mov rcx, {s}\n", .{length_reg});
                try writer.print("  mov rsp, rbp\n  pop rbp\n  ret\n", .{});
            },
            .ret_error_union_slice => {
                const source = inst.data.ret_error_union_slice.source;
                const tag_slot = try self.getOrAllocErrorTagSlot(source);
                const pointer = try self.allocateOp(source);
                const length = try self.allocateOp(inst.data.ret_error_union_slice.len);
                const pointer_reg = try opToReg(writer, pointer, "rdx");
                const length_reg = try opToReg(writer, length, "rcx");
                try writer.print("  mov rax, qword [rbp - {d}]\n", .{tag_slot});
                if (!std.mem.eql(u8, pointer_reg, "rdx")) try writer.print("  mov rdx, {s}\n", .{pointer_reg});
                if (!std.mem.eql(u8, length_reg, "rcx")) try writer.print("  mov rcx, {s}\n", .{length_reg});
                try writer.print("  mov rsp, rbp\n  pop rbp\n  ret\n", .{});
            },
            .unreachable_inst => try writer.print("  ud2\n", .{}),
            else => try writer.print("  ; [unhandled] {s}\n", .{@tagName(inst.opcode)}),
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  BINARY path (machine code via Encoder)
    // ─────────────────────────────────────────────────────────────────────────

    pub fn generateBinary(self: *X86Gen, enc: *Encoder) !void {
        std.debug.print("[X86Gen] Starting binary code generation\n", .{});

        // Pass 0: collect all string literals into rodata before emitting any code.
        // We need rodata_vaddr, but that is computed by elf64 after we finish.
        // Strategy: first pass collects strings, second pass emits code using offsets.
        // rodata_vaddr will be set by caller after generateBinary returns.
        // For now collect strings so offsets are stable.
        for (self.lir.insts.items, 0..) |inst, idx| {
            if (inst.opcode == .string_literal) {
                const ast_node_idx = inst.data.string_literal;
                const node = self.ast_tree.nodes.get(ast_node_idx);
                const tok = self.ast_tree.tokens[node.main_token];
                // Token text includes surrounding quotes — strip them.
                const raw = self.src[tok.start..tok.end];
                const content = if (raw.len >= 2 and raw[0] == '"') raw[1 .. raw.len - 1] else raw;
                const str_off: u64 = @as(u64, @intCast(self.rodata.items.len));
                try self.string_offsets.put(@as(lir.Inst.Index, @intCast(idx)), str_off);
                // Unescape \n → 0x0A
                var i: usize = 0;
                while (i < content.len) : (i += 1) {
                    if (content[i] == '\\' and i + 1 < content.len and content[i + 1] == 'n') {
                        try self.rodata.append(self.allocator, 0x0A);
                        i += 1;
                    } else {
                        try self.rodata.append(self.allocator, content[i]);
                    }
                }
                std.debug.print("[X86Gen] rodata: string %{d} at offset {d} (len {d})\n", .{ idx, str_off, self.rodata.items.len - @as(usize, @intCast(str_off)) });
            }
        }

        // _start stub: only emit if a 'main' symbol will be defined.
        // We check by scanning for an unmangled root-module `main` symbol.
        var has_main = false;
        for (self.lir.insts.items) |inst| {
            if (inst.opcode == .label) {
                const fn_name = self.lir.symbols.items[inst.data.label];
                if (std.mem.eql(u8, fn_name, "main")) {
                    has_main = true;
                    break;
                }
            }
        }

        if (has_main) {
            // _start stub: call main → exit(rax) or exit(0) if main is void
            // Pattern: call main; mov rdi, rax; xor rax,rax; mov rax,60; syscall
            // For void main: ret in main doesn't set rax, so we zero rdi first
            try enc.defineSymbol("_start");
            try enc.emitMovRegImm64(.rdi, 0); // pre-zero exit code (void main case)
            try enc.emitCallRel("main");
            try enc.emitMovRegReg(.rdi, .rax); // use main's return value if any
            try enc.emitMovRegImm64(.rax, 60); // SYS_exit
            try enc.emitSyscall();
        }

        // Two-pass block emission
        for (self.lir.blocks.items, 0..) |blk, blk_idx| {
            std.debug.print("[X86Gen] Binary block {d} ({d} insts)\n", .{ blk_idx, blk.insts.items.len });
            const label = try self.blockLabel(blk_idx);
            try enc.defineSymbol(label);

            for (blk.insts.items) |inst_idx| {
                try self.emitInstBinary(enc, inst_idx);
            }
        }

        std.debug.print("[X86Gen] Binary code generation complete — applying fixups\n", .{});
        try enc.applyFixups();
        _ = self.label_arena.reset(.free_all);
    }

    fn emitInstBinary(self: *X86Gen, enc: *Encoder, inst_idx: lir.Inst.Index) !void {
        const inst = self.lir.insts.items[inst_idx];
        std.debug.print("[X86Gen-bin]   inst %{d} = {s}\n", .{ inst_idx, @tagName(inst.opcode) });

        switch (inst.opcode) {

            // ── constants ────────────────────────────────────────────────────
            .const_i => {
                const op = try self.allocateOp(inst_idx);
                const v: i64 = @as(i64, @bitCast(inst.data.const_i));
                switch (op) {
                    .reg => |r| try enc.emitMovRegImm64(nameToReg(r), v),
                    .mem => |m| {
                        try enc.emitMovRegImm64(.rax, v);
                        try enc.emitMovMemReg(m, .rax);
                    },
                }
            },

            .const_f => {
                const op = try self.allocateOp(inst_idx);
                const bits: u64 = @bitCast(inst.data.const_f);
                switch (op) {
                    .reg => |register| try enc.emitMovRegImm64(nameToReg(register), @bitCast(bits)),
                    .mem => |slot| {
                        try enc.emitMovRegImm64(.rax, @bitCast(bits));
                        try enc.emitMovMemReg(slot, .rax);
                    },
                }
            },

            // ── arithmetic ───────────────────────────────────────────────────
            .add => {
                const op = try self.allocateOp(inst_idx);
                const lhs = try self.allocateOp(inst.data.add.lhs);
                const rhs = try self.allocateOp(inst.data.add.rhs);
                const lreg = try opToRegBin(enc, lhs, .rax);
                const rreg = try opToRegBin(enc, rhs, .rcx);
                switch (op) {
                    .reg => |r| {
                        const dst = nameToReg(r);
                        try enc.emitMovRegReg(dst, lreg);
                        try enc.emitAdd(dst, rreg);
                    },
                    .mem => |m| {
                        try enc.emitMovRegReg(.rax, lreg);
                        try enc.emitAdd(.rax, rreg);
                        try enc.emitMovMemReg(m, .rax);
                    },
                }
            },

            .sub => {
                const op = try self.allocateOp(inst_idx);
                const lhs = try self.allocateOp(inst.data.sub.lhs);
                const rhs = try self.allocateOp(inst.data.sub.rhs);
                const lreg = try opToRegBin(enc, lhs, .rax);
                const rreg = try opToRegBin(enc, rhs, .rcx);
                switch (op) {
                    .reg => |r| {
                        const dst = nameToReg(r);
                        try enc.emitMovRegReg(dst, lreg);
                        try enc.emitSub(dst, rreg);
                    },
                    .mem => |m| {
                        try enc.emitMovRegReg(.rax, lreg);
                        try enc.emitSub(.rax, rreg);
                        try enc.emitMovMemReg(m, .rax);
                    },
                }
            },

            .mul => {
                const op = try self.allocateOp(inst_idx);
                const lhs = try self.allocateOp(inst.data.mul.lhs);
                const rhs = try self.allocateOp(inst.data.mul.rhs);
                const lreg = try opToRegBin(enc, lhs, .rax);
                const rreg = try opToRegBin(enc, rhs, .rcx);
                try enc.emitMovRegReg(.rax, lreg);
                try enc.emitIMul(.rax, rreg);
                switch (op) {
                    .reg => |r| try enc.emitMovRegReg(nameToReg(r), .rax),
                    .mem => |m| try enc.emitMovMemReg(m, .rax),
                }
            },

            .div, .rem, .bit_and, .bit_or, .bit_xor, .shl, .shr => try integer_instructions.emitBinary(self, enc, inst_idx),

            .icmp => {
                const op = try self.allocateOp(inst_idx);
                const lhs = try self.allocateOp(inst.data.icmp.lhs);
                const rhs = try self.allocateOp(inst.data.icmp.rhs);
                const lhs_reg = try opToRegBin(enc, lhs, .rax);
                const rhs_reg = try opToRegBin(enc, rhs, .rcx);
                try enc.emitCmp(lhs_reg, rhs_reg);
                const dst_reg = switch (op) {
                    .reg => |reg| nameToReg(reg),
                    .mem => .rax,
                };
                try enc.emitSetcc(dst_reg, condition(inst.data.icmp.predicate));
                if (op == .mem) try enc.emitMovMemReg(op.mem, .rax);
            },

            .gep => {
                const op = try self.allocateOp(inst_idx);
                const base = try self.allocateOp(inst.data.gep.base);
                const index = try self.allocateOp(inst.data.gep.index);
                const base_reg = try opToRegBin(enc, base, .rax);
                try enc.emitMovRegReg(.rax, base_reg);
                const index_reg = try opToRegBin(enc, index, .rcx);
                try enc.emitMovRegReg(.rcx, index_reg);
                const magnitude: u64 = @intCast(@abs(inst.data.gep.stride));
                if (magnitude != 1) {
                    try enc.emitMovRegImm64(.rdi, @intCast(magnitude));
                    try enc.emitIMul(.rcx, .rdi);
                }
                if (inst.data.gep.stride < 0) {
                    try enc.emitSub(.rax, .rcx);
                } else {
                    try enc.emitAdd(.rax, .rcx);
                }
                switch (op) {
                    .reg => |register| try enc.emitMovRegReg(nameToReg(register), .rax),
                    .mem => |slot| try enc.emitMovMemReg(slot, .rax),
                }
            },

            // ── memory ───────────────────────────────────────────────────────
            .addr => {
                const op = try self.allocateOp(inst_idx);
                const var_id = inst.data.addr;
                const slot = try self.getOrAllocSlot(var_id);
                switch (op) {
                    .reg => |r| try enc.emitLeaRegMem(nameToReg(r), slot),
                    .mem => |m| {
                        try enc.emitLeaRegMem(.rax, slot);
                        try enc.emitMovMemReg(m, .rax);
                    },
                }
            },

            .alloca => {
                const op = try self.allocateOp(inst_idx);
                const allocation = inst.data.alloca;
                const slot = try self.getOrAllocBlock(allocation.id, allocation.size, allocation.alignment);
                switch (op) {
                    .reg => |register| try enc.emitLeaRegMem(nameToReg(register), slot),
                    .mem => |destination| {
                        try enc.emitLeaRegMem(.rax, slot);
                        try enc.emitMovMemReg(destination, .rax);
                    },
                }
            },

            .store => {
                const ptr_op = try self.allocateOp(inst.data.store.ptr);
                const val_op = try self.allocateOp(inst.data.store.val);
                const ptr_r = try opToRegBin(enc, ptr_op, .rax);
                const val_r = try opToRegBin(enc, val_op, .rcx);
                // MOV [ptr_r], val_r  — store through pointer
                // We encode this as:  REX.W 89 ModRM(00, val_r, ptr_r)
                // using emitMovMemReg semantics via encoder internals.
                // For now: if ptr_r == rax, emit "mov [rax], val_r"
                // Since our `addr` stores rbp-relative addresses in registers,
                // we use the register as a pointer.
                if (self.isByteType(inst.type_id)) {
                    try emitStoreByteViaPtr(enc, ptr_r, val_r);
                } else {
                    try emitStoreViaPtr(enc, ptr_r, val_r);
                }
            },

            .load => {
                const op = try self.allocateOp(inst_idx);
                const ptr_op = try self.allocateOp(inst.data.load.ptr);
                const ptr_r = try opToRegBin(enc, ptr_op, .rax);
                const dst_r = switch (op) {
                    .reg => |r| nameToReg(r),
                    .mem => .rcx,
                };
                if (self.isByteType(inst.type_id)) {
                    if (self.isSignedType(inst.type_id)) {
                        try emitLoadSignedByteViaPtr(enc, dst_r, ptr_r);
                    } else {
                        try emitLoadByteViaPtr(enc, dst_r, ptr_r);
                    }
                } else {
                    try emitLoadViaPtr(enc, dst_r, ptr_r);
                }
                if (op == .mem) {
                    try enc.emitMovMemReg(op.mem, .rcx);
                }
            },

            // ── ABI: parameters ──────────────────────────────────────────────
            .param => {
                const op = try self.allocateOp(inst_idx);
                const param_idx = inst.data.param + @intFromBool(if (self.current_function_return_type) |return_type| self.isMemoryErrorUnion(return_type) else false);
                const arg_regs_bin = [_]Reg{ .rdi, .rsi, .rdx, .rcx, .r8, .r9 };
                if (param_idx < arg_regs_bin.len) {
                    const src_r = arg_regs_bin[param_idx];
                    switch (op) {
                        .reg => |r| try enc.emitMovRegReg(nameToReg(r), src_r),
                        .mem => |m| try enc.emitMovMemReg(m, src_r),
                    }
                } else {
                    // Stack param: [rbp + 16 + (n-6)*8]
                    const sp_off: i32 = 16 + @as(i32, @intCast((param_idx - 6) * 8));
                    try enc.emitMovRegMem(.rax, -sp_off); // use negative because emitMovRegMem negates
                    switch (op) {
                        .reg => |r| try enc.emitMovRegReg(nameToReg(r), .rax),
                        .mem => |m| try enc.emitMovMemReg(m, .rax),
                    }
                }
            },

            // ── func_sym: get function address ───────────────────────────────
            .func_sym => {
                const op = try self.allocateOp(inst_idx);
                const fn_name = self.lir.symbols.items[inst.data.func_sym];
                const dst_r = switch (op) {
                    .reg => |r| nameToReg(r),
                    .mem => .rax,
                };
                try enc.emitLeaRipRel(dst_r, fn_name);
                if (op == .mem) try enc.emitMovMemReg(op.mem, .rax);
            },

            // ── function label + prologue ────────────────────────────────────
            .label => {
                const fn_name = self.lir.symbols.items[inst.data.label];
                try enc.defineSymbol(fn_name);
                try enc.emitPushRbp();
                try enc.emitMovRbpRsp();
                try enc.emitSubRspImm32(4096);
                self.current_function_return_type = inst.type_id;
                self.current_hidden_payload_slot = null;
                if (self.isMemoryErrorUnion(inst.type_id)) {
                    const slot = try self.getOrAllocSlot(0xb000_0000 | inst.data.label);
                    self.current_hidden_payload_slot = slot;
                    try enc.emitMovMemReg(slot, .rdi);
                }
            },

            // ── call ─────────────────────────────────────────────────────────
            .call => {
                const op = try self.allocateOp(inst_idx);
                const num_args = inst.data.call.args_count;
                const args_extra_start = inst.data.call.args_start;
                const arg_regs_bin = [_]Reg{ .rdi, .rsi, .rdx, .rcx, .r8, .r9 };
                const memory_return = self.isMemoryErrorUnion(inst.type_id);
                const register_aggregate_return = self.isRegisterAggregateErrorUnion(inst.type_id);
                var memory_payload_slot: ?i32 = null;
                if (memory_return or register_aggregate_return) {
                    const layout = self.errorPayloadLayout(inst.type_id);
                    const slot = try self.getOrAllocBlock(0xa000_0000 | inst_idx, @max(layout.size, 8), layout.alignment);
                    memory_payload_slot = slot;
                    if (memory_return) try enc.emitLeaRegMem(.rdi, slot);
                }

                // Move register args (last first to avoid clobbering)
                const register_offset: u32 = @intFromBool(memory_return);
                var i: u32 = @min(num_args, @as(u32, arg_regs_bin.len) - register_offset);
                while (i > 0) {
                    i -= 1;
                    const arg_inst = self.lir.extra_data.items[args_extra_start + i];
                    const arg_op = try self.allocateOp(arg_inst);
                    const dst_r = arg_regs_bin[i + register_offset];
                    const src_r = try opToRegBin(enc, arg_op, .rax);
                    try enc.emitMovRegReg(dst_r, src_r);
                }

                // Emit call via func_sym (must be in a register)
                const func_inst = inst.data.call.func;
                const func_op = try self.allocateOp(func_inst);
                switch (func_op) {
                    .reg => |r| try enc.emitCallReg(nameToReg(r)),
                    .mem => |m| {
                        try enc.emitMovRegMem(.rax, m);
                        try enc.emitCallReg(.rax);
                    },
                }

                // Error unions return the tag in rax and the first integer payload in rdx.
                if (self.isErrorUnion(inst.type_id)) {
                    const tag_slot = try self.getOrAllocErrorTagSlot(inst_idx);
                    try enc.emitMovMemReg(tag_slot, .rax);
                    if (self.isSliceErrorUnion(inst.type_id)) {
                        const length_slot = try self.getOrAllocErrorPayloadExtraSlot(inst_idx);
                        try enc.emitMovMemReg(length_slot, .rcx);
                    }
                    if (memory_return) {
                        const slot = memory_payload_slot.?;
                        switch (op) {
                            .reg => |register| try enc.emitLeaRegMem(nameToReg(register), slot),
                            .mem => |destination| {
                                try enc.emitLeaRegMem(.rax, slot);
                                try enc.emitMovMemReg(destination, .rax);
                            },
                        }
                    } else if (register_aggregate_return) {
                        const slot = memory_payload_slot.?;
                        const layout = self.errorPayloadLayout(inst.type_id);
                        try enc.emitLeaRegMem(.rdi, slot);
                        try emitStoreViaPtr(enc, .rdi, .rdx);
                        if (layout.size > 8) {
                            try enc.emitMovRegImm64(.r8, 8);
                            try enc.emitAdd(.rdi, .r8);
                            try emitStoreViaPtr(enc, .rdi, .rcx);
                        }
                        switch (op) {
                            .reg => |register| try enc.emitLeaRegMem(nameToReg(register), slot),
                            .mem => |destination| {
                                try enc.emitLeaRegMem(.rax, slot);
                                try enc.emitMovMemReg(destination, .rax);
                            },
                        }
                    } else if (self.isFloatErrorUnion(inst.type_id)) {
                        switch (op) {
                            .reg => |r| try emitMovqGpFromXmm0(enc, nameToReg(r)),
                            .mem => |m| {
                                try emitMovqGpFromXmm0(enc, .rax);
                                try enc.emitMovMemReg(m, .rax);
                            },
                        }
                    } else switch (op) {
                        .reg => |r| try enc.emitMovRegReg(nameToReg(r), .rdx),
                        .mem => |m| try enc.emitMovMemReg(m, .rdx),
                    }
                } else switch (op) {
                    .reg => |r| try enc.emitMovRegReg(nameToReg(r), .rax),
                    .mem => |m| try enc.emitMovMemReg(m, .rax),
                }
            },

            .error_test => {
                const op = try self.allocateOp(inst_idx);
                const tag_slot = try self.getOrAllocErrorTagSlot(inst.data.error_test);
                switch (op) {
                    .reg => |reg| try enc.emitMovRegMem(nameToReg(reg), tag_slot),
                    .mem => |slot| {
                        try enc.emitMovRegMem(.rax, tag_slot);
                        try enc.emitMovMemReg(slot, .rax);
                    },
                }
            },

            .error_payload => {
                const op = try self.allocateOp(inst_idx);
                const payload = try self.allocateOp(inst.data.error_payload);
                const payload_reg = try opToRegBin(enc, payload, .rax);
                switch (op) {
                    .reg => |reg| try enc.emitMovRegReg(nameToReg(reg), payload_reg),
                    .mem => |slot| try enc.emitMovMemReg(slot, payload_reg),
                }
            },

            .error_payload_part => {
                const op = try self.allocateOp(inst_idx);
                const slot = try self.getOrAllocErrorPayloadExtraSlot(inst.data.error_payload_part.source);
                switch (op) {
                    .reg => |register| try enc.emitMovRegMem(nameToReg(register), slot),
                    .mem => |destination| {
                        try enc.emitMovRegMem(.rax, slot);
                        try enc.emitMovMemReg(destination, .rax);
                    },
                }
            },

            // ── control flow ─────────────────────────────────────────────────
            .br => {
                const label = try self.blockLabel(inst.data.br.dest);
                try enc.emitJmpRel(label);
            },

            .condbr => {
                const cond_op = try self.allocateOp(inst.data.condbr.cond);
                const cond_r = try opToRegBin(enc, cond_op, .rax);
                try enc.emitTest(cond_r);
                const true_label = try self.blockLabel(inst.data.condbr.true_dest);
                const false_label = try self.blockLabel(inst.data.condbr.false_dest);
                try enc.emitJnzRel(true_label);
                try enc.emitJmpRel(false_label);
            },

            .ret => {
                if (self.isErrorUnion(inst.type_id)) {
                    if (inst.data.ret) |r| {
                        const val_op = try self.allocateOp(r);
                        const val_r = try opToRegBin(enc, val_op, if (self.isMemoryErrorUnion(inst.type_id)) .rsi else .rdx);
                        if (self.isMemoryErrorUnion(inst.type_id)) {
                            try enc.emitMovRegReg(.rsi, val_r);
                            try enc.emitMovRegMem(.rdi, self.current_hidden_payload_slot.?);
                            try emitMemoryCopy(enc, self.errorPayloadLayout(inst.type_id).size);
                        } else if (self.isRegisterAggregateErrorUnion(inst.type_id)) {
                            try enc.emitMovRegReg(.rsi, val_r);
                            try emitLoadViaPtr(enc, .rdx, .rsi);
                            if (self.errorPayloadLayout(inst.type_id).size > 8) {
                                try enc.emitMovRegImm64(.r8, 8);
                                try enc.emitAdd(.rsi, .r8);
                                try emitLoadViaPtr(enc, .rcx, .rsi);
                            }
                        } else if (self.isFloatErrorUnion(inst.type_id)) {
                            try emitMovqXmm0FromGp(enc, val_r);
                        } else {
                            try enc.emitMovRegReg(.rdx, val_r);
                        }
                    }
                    try enc.emitMovRegImm64(.rax, 0);
                } else if (inst.data.ret) |r| {
                    const val_op = try self.allocateOp(r);
                    const val_r = try opToRegBin(enc, val_op, .rax);
                    try enc.emitMovRegReg(.rax, val_r);
                }
                try enc.emitMovRspRbp();
                try enc.emitPopRbp();
                try enc.emitRet();
            },

            .ret_error => {
                const tag_op = try self.allocateOp(inst.data.ret_error);
                const tag_reg = try opToRegBin(enc, tag_op, .rax);
                try enc.emitMovRegReg(.rax, tag_reg);
                try enc.emitMovRspRbp();
                try enc.emitPopRbp();
                try enc.emitRet();
            },

            .ret_error_union => {
                const source = inst.data.ret_error_union;
                const tag_slot = try self.getOrAllocErrorTagSlot(source);
                const payload_op = try self.allocateOp(source);
                const payload_reg = try opToRegBin(enc, payload_op, if (self.isMemoryErrorUnion(inst.type_id)) .rsi else .rdx);
                if (self.isMemoryErrorUnion(inst.type_id)) {
                    try enc.emitMovRegReg(.rsi, payload_reg);
                    try enc.emitMovRegMem(.rdi, self.current_hidden_payload_slot.?);
                    try emitMemoryCopy(enc, self.errorPayloadLayout(inst.type_id).size);
                } else if (self.isRegisterAggregateErrorUnion(inst.type_id)) {
                    try enc.emitMovRegReg(.rsi, payload_reg);
                    try emitLoadViaPtr(enc, .rdx, .rsi);
                    if (self.errorPayloadLayout(inst.type_id).size > 8) {
                        try enc.emitMovRegImm64(.r8, 8);
                        try enc.emitAdd(.rsi, .r8);
                        try emitLoadViaPtr(enc, .rcx, .rsi);
                    }
                }
                try enc.emitMovRegMem(.rax, tag_slot);
                if (self.isFloatErrorUnion(inst.type_id)) {
                    try emitMovqXmm0FromGp(enc, payload_reg);
                } else if (!self.isMemoryErrorUnion(inst.type_id) and !self.isRegisterAggregateErrorUnion(inst.type_id)) {
                    try enc.emitMovRegReg(.rdx, payload_reg);
                }
                try enc.emitMovRspRbp();
                try enc.emitPopRbp();
                try enc.emitRet();
            },

            .ret_error_slice => {
                const pointer = try self.allocateOp(inst.data.ret_error_slice.ptr);
                const length = try self.allocateOp(inst.data.ret_error_slice.len);
                const pointer_reg = try opToRegBin(enc, pointer, .rdx);
                const length_reg = try opToRegBin(enc, length, .rcx);
                try enc.emitMovRegImm64(.rax, 0);
                try enc.emitMovRegReg(.rdx, pointer_reg);
                try enc.emitMovRegReg(.rcx, length_reg);
                try enc.emitMovRspRbp();
                try enc.emitPopRbp();
                try enc.emitRet();
            },

            .ret_error_union_slice => {
                const source = inst.data.ret_error_union_slice.source;
                const tag_slot = try self.getOrAllocErrorTagSlot(source);
                const pointer = try self.allocateOp(source);
                const length = try self.allocateOp(inst.data.ret_error_union_slice.len);
                const pointer_reg = try opToRegBin(enc, pointer, .rdx);
                const length_reg = try opToRegBin(enc, length, .rcx);
                try enc.emitMovRegMem(.rax, tag_slot);
                try enc.emitMovRegReg(.rdx, pointer_reg);
                try enc.emitMovRegReg(.rcx, length_reg);
                try enc.emitMovRspRbp();
                try enc.emitPopRbp();
                try enc.emitRet();
            },
            .unreachable_inst => try enc.emitTrap(),

            // ── string literal ────────────────────────────────────────────────
            // Load the virtual address of the string in .rodata into a register.
            // rodata_vaddr is set by the caller after ELF layout is determined.
            // At code-gen time we emit: MOV reg, <rodata_vaddr + str_off>
            // Since rodata_vaddr is 0 at this point, we patch it in main.zig
            // by calling generateBinary a second time — actually we just
            // compute and store the absolute vaddr after buildExecutable returns.
            // Simple approach: store offset now, caller will set rodata_vaddr before
            // generateBinary is called (via a two-phase approach in main.zig).
            .string_literal => {
                const op = try self.allocateOp(inst_idx);
                const str_off = self.string_offsets.get(inst_idx) orelse {
                    std.debug.print("[X86Gen-bin]   string_literal %{d}: offset not found!\n", .{inst_idx});
                    return;
                };
                const str_vaddr: i64 = @as(i64, @bitCast(self.rodata_vaddr + str_off));
                std.debug.print("[X86Gen-bin]   string_literal %{d} vaddr=0x{x}\n", .{ inst_idx, @as(u64, @bitCast(str_vaddr)) });
                switch (op) {
                    .reg => |r| try enc.emitMovRegImm64(nameToReg(r), str_vaddr),
                    .mem => |m| {
                        try enc.emitMovRegImm64(.rax, str_vaddr);
                        try enc.emitMovMemReg(m, .rax);
                    },
                }
            },

            .tuple_literal => {
                std.debug.print("[X86Gen-bin]   tuple lowering is not implemented for %{d}\n", .{inst_idx});
            },

            else => {
                std.debug.print("[X86Gen-bin]   [unhandled] {s}\n", .{@tagName(inst.opcode)});
            },
        }
    }
};

// ── low-level helpers (pointer-based store/load) ─────────────────────────────

/// MOV [ptr_reg], val_reg   (REX.W 89 ModRM(00, val, ptr))
fn emitStoreViaPtr(enc: *Encoder, ptr_r: Reg, val_r: Reg) !void {
    const ptr_idx = @intFromEnum(ptr_r);
    const val_idx = @intFromEnum(val_r);
    const needs_rex_r = val_idx >= 8;
    const needs_rex_b = ptr_idx >= 8;
    const rex_byte: u8 = 0x48 |
        (if (needs_rex_r) @as(u8, 0x04) else 0) |
        (if (needs_rex_b) @as(u8, 0x01) else 0);
    const ptr_low: u8 = @as(u8, @truncate(ptr_idx)) & 7;
    const mod_bits: u8 = if (ptr_low == 5) 0b01 else 0b00;
    const modrm_byte: u8 = mod_bits << 6 | (@as(u8, @truncate(val_idx)) & 7) << 3 | ptr_low;
    try enc.buf.append(enc.allocator, rex_byte);
    try enc.buf.append(enc.allocator, 0x89);
    try enc.buf.append(enc.allocator, modrm_byte);
    if (ptr_low == 4) try enc.buf.append(enc.allocator, 0x24); // [rsp]/[r12] SIB
    if (ptr_low == 5) try enc.buf.append(enc.allocator, 0x00); // [rbp]/[r13]+disp8(0)
}

/// MOV byte [ptr_reg], val_reg.low8 (REX 88 /r)
fn emitStoreByteViaPtr(enc: *Encoder, ptr_r: Reg, val_r: Reg) !void {
    const ptr_idx = @intFromEnum(ptr_r);
    const val_idx = @intFromEnum(val_r);
    const rex_byte: u8 = 0x40 |
        (if (val_idx >= 8) @as(u8, 0x04) else 0) |
        (if (ptr_idx >= 8) @as(u8, 0x01) else 0);
    const ptr_low: u8 = @as(u8, @truncate(ptr_idx)) & 7;
    const mod_bits: u8 = if (ptr_low == 5) 0b01 else 0b00;
    const modrm_byte: u8 = mod_bits << 6 | (@as(u8, @truncate(val_idx)) & 7) << 3 | ptr_low;
    try enc.buf.append(enc.allocator, rex_byte);
    try enc.buf.appendSlice(enc.allocator, &.{ 0x88, modrm_byte });
    if (ptr_low == 4) try enc.buf.append(enc.allocator, 0x24);
    if (ptr_low == 5) try enc.buf.append(enc.allocator, 0x00);
}

/// MOV dst_reg, [ptr_reg]   (REX.W 8B ModRM(00, dst, ptr))
fn emitLoadViaPtr(enc: *Encoder, dst_r: Reg, ptr_r: Reg) !void {
    const ptr_idx = @intFromEnum(ptr_r);
    const dst_idx = @intFromEnum(dst_r);
    const needs_rex_r = dst_idx >= 8;
    const needs_rex_b = ptr_idx >= 8;
    const rex_byte: u8 = 0x48 |
        (if (needs_rex_r) @as(u8, 0x04) else 0) |
        (if (needs_rex_b) @as(u8, 0x01) else 0);
    const ptr_low: u8 = @as(u8, @truncate(ptr_idx)) & 7;
    const mod_bits: u8 = if (ptr_low == 5) 0b01 else 0b00;
    const modrm_byte: u8 = mod_bits << 6 | (@as(u8, @truncate(dst_idx)) & 7) << 3 | ptr_low;
    try enc.buf.append(enc.allocator, rex_byte);
    try enc.buf.append(enc.allocator, 0x8B);
    try enc.buf.append(enc.allocator, modrm_byte);
    if (ptr_low == 4) try enc.buf.append(enc.allocator, 0x24); // [rsp]/[r12] SIB
    if (ptr_low == 5) try enc.buf.append(enc.allocator, 0x00); // [rbp]/[r13]+disp8(0)
}

/// MOVZX dst_reg, byte [ptr_reg] (REX.W 0F B6 /r)
fn emitLoadByteViaPtr(enc: *Encoder, dst_r: Reg, ptr_r: Reg) !void {
    const ptr_idx = @intFromEnum(ptr_r);
    const dst_idx = @intFromEnum(dst_r);
    const needs_rex_r = dst_idx >= 8;
    const needs_rex_b = ptr_idx >= 8;
    const rex_byte: u8 = 0x48 |
        (if (needs_rex_r) @as(u8, 0x04) else 0) |
        (if (needs_rex_b) @as(u8, 0x01) else 0);
    const ptr_low: u8 = @as(u8, @truncate(ptr_idx)) & 7;
    const mod_bits: u8 = if (ptr_low == 5) 0b01 else 0b00;
    const modrm_byte: u8 = mod_bits << 6 | (@as(u8, @truncate(dst_idx)) & 7) << 3 | ptr_low;
    try enc.buf.append(enc.allocator, rex_byte);
    try enc.buf.appendSlice(enc.allocator, &.{ 0x0f, 0xb6, modrm_byte });
    if (ptr_low == 4) try enc.buf.append(enc.allocator, 0x24);
    if (ptr_low == 5) try enc.buf.append(enc.allocator, 0x00);
}

/// MOVSX dst_reg, byte [ptr_reg] (REX.W 0F BE /r)
fn emitLoadSignedByteViaPtr(enc: *Encoder, dst_r: Reg, ptr_r: Reg) !void {
    const ptr_idx = @intFromEnum(ptr_r);
    const dst_idx = @intFromEnum(dst_r);
    const needs_rex_r = dst_idx >= 8;
    const needs_rex_b = ptr_idx >= 8;
    const rex_byte: u8 = 0x48 |
        (if (needs_rex_r) @as(u8, 0x04) else 0) |
        (if (needs_rex_b) @as(u8, 0x01) else 0);
    const ptr_low: u8 = @as(u8, @truncate(ptr_idx)) & 7;
    const mod_bits: u8 = if (ptr_low == 5) 0b01 else 0b00;
    const modrm_byte: u8 = mod_bits << 6 | (@as(u8, @truncate(dst_idx)) & 7) << 3 | ptr_low;
    try enc.buf.append(enc.allocator, rex_byte);
    try enc.buf.appendSlice(enc.allocator, &.{ 0x0f, 0xbe, modrm_byte });
    if (ptr_low == 4) try enc.buf.append(enc.allocator, 0x24);
    if (ptr_low == 5) try enc.buf.append(enc.allocator, 0x00);
}

/// MOVQ xmm0, src_gp (66 REX.W 0F 6E /r)
fn emitMovqXmm0FromGp(enc: *Encoder, src: Reg) !void {
    const source: u8 = @intFromEnum(src);
    try enc.buf.append(enc.allocator, 0x66);
    try enc.buf.append(enc.allocator, 0x48 | (if (source >= 8) @as(u8, 0x01) else 0));
    try enc.buf.appendSlice(enc.allocator, &.{ 0x0f, 0x6e, 0xc0 | (source & 7) });
}

/// MOVQ dst_gp, xmm0 (66 REX.W 0F 7E /r)
fn emitMovqGpFromXmm0(enc: *Encoder, destination: Reg) !void {
    const target: u8 = @intFromEnum(destination);
    try enc.buf.append(enc.allocator, 0x66);
    try enc.buf.append(enc.allocator, 0x48 | (if (target >= 8) @as(u8, 0x01) else 0));
    try enc.buf.appendSlice(enc.allocator, &.{ 0x0f, 0x7e, 0xc0 | (target & 7) });
}

/// Copy a compile-time-sized payload from rsi to rdi.
fn emitMemoryCopy(enc: *Encoder, size: u32) !void {
    var remaining = size;
    while (remaining >= 8) {
        try emitLoadViaPtr(enc, .rax, .rsi);
        try emitStoreViaPtr(enc, .rdi, .rax);
        remaining -= 8;
        if (remaining > 0) {
            try enc.emitMovRegImm64(.r8, 8);
            try enc.emitAdd(.rsi, .r8);
            try enc.emitAdd(.rdi, .r8);
        }
    }
    while (remaining > 0) : (remaining -= 1) {
        try emitLoadByteViaPtr(enc, .rax, .rsi);
        try emitStoreByteViaPtr(enc, .rdi, .rax);
        if (remaining > 1) {
            try enc.emitMovRegImm64(.r8, 1);
            try enc.emitAdd(.rsi, .r8);
            try enc.emitAdd(.rdi, .r8);
        }
    }
}

test "pointer loads and stores encode r12/r13 base registers" {
    var enc = Encoder.init(std.testing.allocator);
    defer enc.deinit();

    try emitStoreViaPtr(&enc, .r13, .r12);
    try std.testing.expectEqualSlices(u8, &.{ 0x4d, 0x89, 0x65, 0x00 }, enc.buf.items);

    enc.buf.clearRetainingCapacity();
    try emitLoadViaPtr(&enc, .r14, .r12);
    try std.testing.expectEqualSlices(u8, &.{ 0x4d, 0x8b, 0x34, 0x24 }, enc.buf.items);
}

test "byte pointer stores encode extended registers" {
    var enc = Encoder.init(std.testing.allocator);
    defer enc.deinit();

    try emitStoreByteViaPtr(&enc, .r13, .r12);
    try std.testing.expectEqualSlices(u8, &.{ 0x45, 0x88, 0x65, 0x00 }, enc.buf.items);
}

test "byte pointer load zero-extends" {
    var enc = Encoder.init(std.testing.allocator);
    defer enc.deinit();

    try emitLoadByteViaPtr(&enc, .r14, .r12);
    try std.testing.expectEqualSlices(u8, &.{ 0x4d, 0x0f, 0xb6, 0x34, 0x24 }, enc.buf.items);
}

test "signed byte pointer load sign-extends" {
    var enc = Encoder.init(std.testing.allocator);
    defer enc.deinit();

    try emitLoadSignedByteViaPtr(&enc, .r14, .r12);
    try std.testing.expectEqualSlices(u8, &.{ 0x4d, 0x0f, 0xbe, 0x34, 0x24 }, enc.buf.items);
}

test "SSE payload transport moves f64 bits through xmm0" {
    var enc = Encoder.init(std.testing.allocator);
    defer enc.deinit();

    try emitMovqXmm0FromGp(&enc, .r9);
    try emitMovqGpFromXmm0(&enc, .r10);
    try std.testing.expectEqualSlices(u8, &.{
        0x66, 0x49, 0x0f, 0x6e, 0xc1,
        0x66, 0x49, 0x0f, 0x7e, 0xc2,
    }, enc.buf.items);
}
