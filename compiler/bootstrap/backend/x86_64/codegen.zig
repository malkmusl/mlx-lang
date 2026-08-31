const std = @import("std");
const lir = @import("../../ir/lir.zig");
const Encoder = @import("encoder.zig").Encoder;
const TypePool = @import("../../semantic/type.zig").TypePool;
const text_codegen = @import("codegen/text.zig");
const binary_codegen = @import("codegen/binary.zig");
const memory_codegen = @import("codegen/memory.zig");

/// x86_64 code generator for zin0.
/// Operates in two modes:
///   • text  — emits NASM syntax to a writer (used by --emit=asm)
///   • binary — emits raw machine code into an Encoder (default path)
pub const X86Gen = struct {
    pub const Layout = struct { size: u32, alignment: u32 };

    pub const Operand = union(enum) {
        reg: []const u8,
        mem: i32,
    };

    allocator: std.mem.Allocator,
    lir: *lir.Lir,
    type_pool: *const TypePool,
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

    pub fn init(
        allocator: std.mem.Allocator,
        ir: *lir.Lir,
        type_pool: *const TypePool,
    ) X86Gen {
        return .{
            .allocator = allocator,
            .lir = ir,
            .type_pool = type_pool,
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
        // Stage0 deliberately spills every persistent virtual register. Values
        // therefore survive ordinary calls without a liveness-aware allocator
        // or callee-save insertion. Scratch registers remain instruction-local.
        const op = Operand{ .mem = @as(i32, @intCast(self.next_stack_slot)) };
        self.next_stack_slot += 8;
        try self.vreg_to_op.put(vreg, op);
        return op;
    }

    /// Start a fresh function-local allocation domain and return a conservative
    /// frame size derived from the function's LIR. The estimate intentionally
    /// over-allocates per instruction; Stage0 values are stack-resident and
    /// correctness matters more than compact frames during self-host bootstrap.
    pub fn beginFunction(self: *X86Gen, label_instruction: lir.Inst.Index) u32 {
        self.vreg_to_op.clearRetainingCapacity();
        self.addr_to_slot.clearRetainingCapacity();
        self.error_tag_slots.clearRetainingCapacity();
        self.error_payload_extra_slots.clearRetainingCapacity();
        self.next_gp_reg = 0;
        self.next_stack_slot = 8;
        self.current_hidden_payload_slot = null;

        var bytes: u64 = 256;
        var index: usize = @as(usize, label_instruction) + 1;
        while (index < self.lir.insts.items.len and self.lir.insts.items[index].opcode != .label) : (index += 1) {
            const instruction = self.lir.insts.items[index];
            bytes += 32;
            if (instruction.opcode == .alloca) {
                bytes += instruction.data.alloca.size + instruction.data.alloca.alignment;
            } else if (instruction.opcode == .aggregate_copy) {
                bytes += (self.type_pool.sizeOf(instruction.type_id) catch 8) +
                    (self.type_pool.alignOf(instruction.type_id) catch 8);
            } else if (instruction.opcode == .call and self.type_pool.aggregateInfo(instruction.type_id) != null) {
                bytes += (self.type_pool.sizeOf(instruction.type_id) catch 16) +
                    (self.type_pool.alignOf(instruction.type_id) catch 8);
            }
        }
        bytes = (bytes + 15) & ~@as(u64, 15);
        return @intCast(@min(bytes, std.math.maxInt(u32)));
    }

    // ── stack frame helpers ──────────────────────────────────────────────────

    pub fn getOrAllocSlot(self: *X86Gen, var_id: u32) !i32 {
        if (self.addr_to_slot.get(var_id)) |s| return s;
        const s = self.next_stack_slot;
        self.next_stack_slot += 8;
        try self.addr_to_slot.put(var_id, s);
        return s;
    }

    pub fn getOrAllocBlock(self: *X86Gen, var_id: u32, size: u32, alignment: u32) !i32 {
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

    pub fn getOrAllocErrorTagSlot(self: *X86Gen, instruction: lir.Inst.Index) !i32 {
        if (self.error_tag_slots.get(instruction)) |slot| return slot;
        const slot = self.next_stack_slot;
        self.next_stack_slot += 8;
        try self.error_tag_slots.put(instruction, slot);
        return slot;
    }

    pub fn getOrAllocErrorPayloadExtraSlot(self: *X86Gen, instruction: lir.Inst.Index) !i32 {
        if (self.error_payload_extra_slots.get(instruction)) |slot| return slot;
        const slot = self.next_stack_slot;
        self.next_stack_slot += 8;
        try self.error_payload_extra_slots.put(instruction, slot);
        return slot;
    }

    /// Argument registers are caller-saved and the ordinary instruction
    /// lowering uses some of them as scratch registers. Capture all register
    /// parameters in the prologue before lowering any parameter bindings.
    pub fn getOrAllocParameterSlot(self: *X86Gen, parameter_index: u32) !i32 {
        return self.getOrAllocSlot(0xc000_0000 | parameter_index);
    }

    pub fn isErrorUnion(self: *const X86Gen, type_id: u32) bool {
        return type_id < self.type_pool.types.items.len and self.type_pool.get(type_id).data == .error_union;
    }

    pub fn isSliceErrorUnion(self: *const X86Gen, type_id: u32) bool {
        if (!self.isErrorUnion(type_id)) return false;
        const payload_id = self.type_pool.get(type_id).data.error_union.payload;
        const payload = self.type_pool.get(payload_id);
        return payload.data == .pointer and payload.data.pointer.size == .Slice;
    }

    pub fn isSlice(self: *const X86Gen, type_id: u32) bool {
        if (type_id >= self.type_pool.types.items.len) return false;
        const ty = self.type_pool.get(type_id);
        return ty.data == .pointer and ty.data.pointer.size == .Slice;
    }

    pub fn isByteType(self: *const X86Gen, type_id: u32) bool {
        if (type_id >= self.type_pool.types.items.len) return false;
        const value_type = self.type_pool.get(type_id);
        return switch (value_type.data) {
            .integer => |integer| integer.bits <= 8,
            .primitive => |primitive| primitive == .bool_type,
            .@"enum", .error_set => blk: {
                const bits = self.type_pool.bitSizeOf(type_id) catch break :blk false;
                break :blk bits > 0 and bits <= 8;
            },
            // Arrays and other aggregates are represented by an address in
            // Stage 0 even when their payload happens to occupy one byte.
            else => false,
        };
    }

    /// Width used by scalar loads and stores. Stage 0 represents aggregates
    /// by address, so only actual scalar values may use a narrow access.
    pub fn scalarMemorySize(self: *const X86Gen, type_id: u32) u8 {
        if (type_id >= self.type_pool.types.items.len or self.isAggregate(type_id)) return 8;
        const size = self.type_pool.sizeOf(type_id) catch return 8;
        return switch (size) {
            1 => 1,
            2 => 2,
            3, 4 => 4,
            else => 8,
        };
    }

    pub fn isSignedType(self: *const X86Gen, type_id: u32) bool {
        if (type_id >= self.type_pool.types.items.len) return false;
        return switch (self.type_pool.get(type_id).data) {
            .integer => |integer| integer.is_signed,
            .size_int => |integer| integer.is_signed,
            else => false,
        };
    }

    pub fn isVoidType(self: *const X86Gen, type_id: u32) bool {
        if (type_id >= self.type_pool.types.items.len) return false;
        const value_type = self.type_pool.get(type_id);
        return value_type.data == .primitive and value_type.data.primitive == .void_type;
    }

    fn mainReturnsVoid(self: *const X86Gen) bool {
        for (self.lir.insts.items) |inst| {
            if (inst.opcode != .label) continue;
            const name = self.lir.symbols.items[inst.data.label];
            if (std.mem.eql(u8, name, "main")) return self.isVoidType(inst.type_id);
        }
        return false;
    }

    pub fn isFloatType(self: *const X86Gen, type_id: u32) bool {
        if (type_id >= self.type_pool.types.items.len) return false;
        return self.type_pool.get(type_id).isFloat();
    }

    pub fn isFloatErrorUnion(self: *const X86Gen, type_id: u32) bool {
        if (!self.isErrorUnion(type_id)) return false;
        return self.isFloatType(self.type_pool.get(type_id).data.error_union.payload);
    }

    pub fn isMemoryErrorUnion(self: *const X86Gen, type_id: u32) bool {
        if (!self.isErrorUnion(type_id)) return false;
        const payload_id = self.type_pool.get(type_id).data.error_union.payload;
        const size = self.type_pool.sizeOf(payload_id) catch return true;
        const alignment = self.type_pool.alignOf(payload_id) catch return true;
        return size > 16 or alignment > 16;
    }

    pub fn isAggregate(self: *const X86Gen, type_id: u32) bool {
        if (type_id >= self.type_pool.types.items.len) return false;
        return switch (self.type_pool.get(type_id).data) {
            .array, .@"struct", .@"union", .tuple => true,
            else => false,
        };
    }

    pub fn isRegisterAggregate(self: *const X86Gen, type_id: u32) bool {
        if (!self.isAggregate(type_id)) return false;
        const size = self.type_pool.sizeOf(type_id) catch return false;
        const alignment = self.type_pool.alignOf(type_id) catch return false;
        return size > 0 and size <= 16 and alignment <= 16 and !self.typeContainsFloat(type_id);
    }

    pub fn isMemoryAggregate(self: *const X86Gen, type_id: u32) bool {
        return self.isAggregate(type_id) and !self.isRegisterAggregate(type_id);
    }

    /// MEMORY-class results use caller-owned storage. Small integer-compatible
    /// aggregates remain in the normal rax/rdx aggregate return registers.
    pub fn isIndirectReturn(self: *const X86Gen, type_id: u32) bool {
        return self.isMemoryAggregate(type_id) or self.isMemoryErrorUnion(type_id);
    }

    fn typeContainsFloat(self: *const X86Gen, type_id: u32) bool {
        const value_type = self.type_pool.get(type_id);
        if (value_type.isFloat()) return true;
        return switch (value_type.data) {
            .array => |array| self.typeContainsFloat(array.child_type),
            .optional => |optional| self.typeContainsFloat(optional.child_type),
            .@"struct", .@"union", .tuple => blk: {
                const fields = self.type_pool.aggregateFields(type_id) orelse break :blk false;
                for (fields) |field| if (self.typeContainsFloat(field.type_id)) break :blk true;
                break :blk false;
            },
            else => false,
        };
    }

    pub fn isRegisterAggregateErrorUnion(self: *const X86Gen, type_id: u32) bool {
        if (!self.isErrorUnion(type_id) or self.isMemoryErrorUnion(type_id)) return false;
        const payload_id = self.type_pool.get(type_id).data.error_union.payload;
        return switch (self.type_pool.get(payload_id).data) {
            .array, .@"struct", .@"union", .tuple => true,
            else => false,
        };
    }

    pub fn errorPayloadLayout(self: *const X86Gen, type_id: u32) Layout {
        const payload_id = self.type_pool.get(type_id).data.error_union.payload;
        return .{
            .size = @intCast(self.type_pool.sizeOf(payload_id) catch 8),
            .alignment = @intCast(self.type_pool.alignOf(payload_id) catch 8),
        };
    }

    pub fn indirectReturnLayout(self: *const X86Gen, type_id: u32) Layout {
        if (self.isAggregate(type_id)) {
            return .{
                .size = @intCast(self.type_pool.sizeOf(type_id) catch 8),
                .alignment = @intCast(self.type_pool.alignOf(type_id) catch 8),
            };
        }
        return self.errorPayloadLayout(type_id);
    }

    // ── block label helpers ───────────────────────────────────────────────────

    pub fn blockLabel(self: *X86Gen, idx: usize) ![]u8 {
        return std.fmt.allocPrint(self.label_arena.allocator(), ".block_{d}", .{idx});
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  TEXT path (NASM output)
    // ─────────────────────────────────────────────────────────────────────────

    pub fn generate(self: *X86Gen, writer: anytype) !void {
        std.debug.print("[X86Gen] Starting text code generation\n", .{});

        try writer.print("global _start\nsection .text\n", .{});
        try writer.print("_start:\n", .{});
        // Linux enters with argc at [rsp] followed by argv. A Zin slice is
        // passed as pointer then length, so every main may optionally accept
        // `[]const [*]const u8` without affecting no-argument mains.
        try writer.print("  mov rsi, qword [rsp]\n", .{});
        try writer.print("  lea rdi, [rsp + 8]\n", .{});
        try writer.print("  call main\n", .{});
        if (self.mainReturnsVoid())
            try writer.print("  mov rdi, 0\n", .{})
        else
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
        return text_codegen.emit(self, writer, inst_idx);
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
                const content = self.lir.string_literals.items[inst.data.string_literal];
                const str_off: u64 = @as(u64, @intCast(self.rodata.items.len));
                try self.string_offsets.put(@as(lir.Inst.Index, @intCast(idx)), str_off);
                try self.rodata.appendSlice(self.allocator, content);
                try self.rodata.append(self.allocator, 0);
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
            // _start stub: pass argv, call main, then exit(rax) or exit(0)
            // for a void main whose return register is intentionally unspecified.
            try enc.defineSymbol("_start");
            try memory_codegen.emitLoadViaPtr(enc, .rsi, .rsp); // argc / slice length
            try enc.emitMovRegReg(.rdi, .rsp);
            try enc.emitMovRegImm64(.rax, 8);
            try enc.emitAdd(.rdi, .rax); // argv / slice pointer
            try enc.emitCallRel("main");
            if (self.mainReturnsVoid())
                try enc.emitMovRegImm64(.rdi, 0)
            else
                try enc.emitMovRegReg(.rdi, .rax);
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
        return binary_codegen.emit(self, enc, inst_idx);
    }
};

test "pointer loads and stores encode r12/r13 base registers" {
    var enc = Encoder.init(std.testing.allocator);
    defer enc.deinit();

    try memory_codegen.emitStoreViaPtr(&enc, .r13, .r12);
    try std.testing.expectEqualSlices(u8, &.{ 0x4d, 0x89, 0x65, 0x00 }, enc.buf.items);

    enc.buf.clearRetainingCapacity();
    try memory_codegen.emitLoadViaPtr(&enc, .r14, .r12);
    try std.testing.expectEqualSlices(u8, &.{ 0x4d, 0x8b, 0x34, 0x24 }, enc.buf.items);
}

test "byte pointer stores encode extended registers" {
    var enc = Encoder.init(std.testing.allocator);
    defer enc.deinit();

    try memory_codegen.emitStoreByteViaPtr(&enc, .r13, .r12);
    try std.testing.expectEqualSlices(u8, &.{ 0x45, 0x88, 0x65, 0x00 }, enc.buf.items);
}

test "byte pointer load zero-extends" {
    var enc = Encoder.init(std.testing.allocator);
    defer enc.deinit();

    try memory_codegen.emitLoadByteViaPtr(&enc, .r14, .r12);
    try std.testing.expectEqualSlices(u8, &.{ 0x4d, 0x0f, 0xb6, 0x34, 0x24 }, enc.buf.items);
}

test "signed byte pointer load sign-extends" {
    var enc = Encoder.init(std.testing.allocator);
    defer enc.deinit();

    try memory_codegen.emitLoadSignedByteViaPtr(&enc, .r14, .r12);
    try std.testing.expectEqualSlices(u8, &.{ 0x4d, 0x0f, 0xbe, 0x34, 0x24 }, enc.buf.items);
}

test "word pointer loads and stores preserve scalar width" {
    var enc = Encoder.init(std.testing.allocator);
    defer enc.deinit();

    try memory_codegen.emitStoreWordViaPtr(&enc, .r13, .r12);
    try std.testing.expectEqualSlices(u8, &.{ 0x66, 0x45, 0x89, 0x65, 0x00 }, enc.buf.items);

    enc.buf.clearRetainingCapacity();
    try memory_codegen.emitLoadWordViaPtr(&enc, .r14, .r12);
    try std.testing.expectEqualSlices(u8, &.{ 0x4d, 0x0f, 0xb7, 0x34, 0x24 }, enc.buf.items);

    enc.buf.clearRetainingCapacity();
    try memory_codegen.emitLoadSignedWordViaPtr(&enc, .r14, .r12);
    try std.testing.expectEqualSlices(u8, &.{ 0x4d, 0x0f, 0xbf, 0x34, 0x24 }, enc.buf.items);
}

test "dword pointer loads and stores preserve scalar width" {
    var enc = Encoder.init(std.testing.allocator);
    defer enc.deinit();

    try memory_codegen.emitStoreDwordViaPtr(&enc, .r13, .r12);
    try std.testing.expectEqualSlices(u8, &.{ 0x45, 0x89, 0x65, 0x00 }, enc.buf.items);

    enc.buf.clearRetainingCapacity();
    try memory_codegen.emitLoadDwordViaPtr(&enc, .r14, .r12);
    try std.testing.expectEqualSlices(u8, &.{ 0x45, 0x8b, 0x34, 0x24 }, enc.buf.items);

    enc.buf.clearRetainingCapacity();
    try memory_codegen.emitLoadSignedDwordViaPtr(&enc, .r14, .r12);
    try std.testing.expectEqualSlices(u8, &.{ 0x4d, 0x63, 0x34, 0x24 }, enc.buf.items);
}

test "SSE payload transport moves f64 bits through xmm0" {
    var enc = Encoder.init(std.testing.allocator);
    defer enc.deinit();

    try memory_codegen.emitMovqXmm0FromGp(&enc, .r9);
    try memory_codegen.emitMovqGpFromXmm0(&enc, .r10);
    try std.testing.expectEqualSlices(u8, &.{
        0x66, 0x49, 0x0f, 0x6e, 0xc1,
        0x66, 0x49, 0x0f, 0x7e, 0xc2,
    }, enc.buf.items);
}
