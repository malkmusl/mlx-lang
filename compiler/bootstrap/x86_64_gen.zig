const std = @import("std");
const lir = @import("lir.zig");
const x86 = @import("x86_64.zig");
const abi = @import("abi.zig");
const ast_mod = @import("ast.zig");
const Encoder = @import("x86_64_encoder.zig").Encoder;
const Reg = x86.Register;

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
    ast_tree: ast_mod.Ast,
    src: []const u8,
    vreg_to_op: std.AutoHashMap(lir.Inst.Index, Operand),
    addr_to_slot: std.AutoHashMap(u32, i32),
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

    const gp_regs = [_][]const u8{
        "rdx", "rbx", "r10", "r11", "r12", "r13", "r14", "r15",
    };

    pub fn init(
        allocator: std.mem.Allocator,
        ir: *lir.Lir,
        ast_tree: ast_mod.Ast,
        src: []const u8,
    ) X86Gen {
        return .{
            .allocator = allocator,
            .lir = ir,
            .ast_tree = ast_tree,
            .src = src,
            .vreg_to_op = std.AutoHashMap(lir.Inst.Index, Operand).init(allocator),
            .addr_to_slot = std.AutoHashMap(u32, i32).init(allocator),
            .next_gp_reg = 0,
            .next_stack_slot = 8,
            .label_arena = std.heap.ArenaAllocator.init(allocator),
            .rodata = std.ArrayList(u8).empty,
            .string_offsets = std.AutoHashMap(lir.Inst.Index, u64).init(allocator),
            .rodata_vaddr = 0,
        };
    }

    pub fn deinit(self: *X86Gen) void {
        self.vreg_to_op.deinit();
        self.addr_to_slot.deinit();
        self.label_arena.deinit();
        self.rodata.deinit(self.allocator);
        self.string_offsets.deinit();
    }

    // ── register allocator ───────────────────────────────────────────────────

    fn allocateOp(self: *X86Gen, vreg: lir.Inst.Index) !Operand {
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

    fn printOp(writer: anytype, op: Operand) !void {
        switch (op) {
            .reg => |r| try writer.print("{s}", .{r}),
            .mem => |m| try writer.print("qword [rbp - {d}]", .{m}),
        }
    }

    fn opToReg(writer: anytype, op: Operand, scratch: []const u8) ![]const u8 {
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

    fn nameToReg(name: []const u8) Reg {
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
    fn opToRegBin(enc: *Encoder, op: Operand, scratch: Reg) !Reg {
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
                const op = try self.allocateOp(inst_idx);
                const lhs = try self.allocateOp(inst.data.div.lhs);
                const rhs = try self.allocateOp(inst.data.div.rhs);
                const lreg = try opToReg(writer, lhs, "rax");
                try writer.print("  mov rax, {s}\n", .{lreg});
                try writer.print("  cqo\n", .{});
                const rreg = try opToReg(writer, rhs, "rcx");
                try writer.print("  idiv {s}\n", .{rreg});
                if (op == .mem) {
                    try writer.print("  mov ", .{});
                    try printOp(writer, op);
                    try writer.print(", rax\n", .{});
                } else {
                    try writer.print("  mov {s}, rax\n", .{op.reg});
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
            .store => {
                const ptr_op = try self.allocateOp(inst.data.store.ptr);
                const val_op = try self.allocateOp(inst.data.store.val);
                const ptr_reg = try opToReg(writer, ptr_op, "rax");
                const val_reg = try opToReg(writer, val_op, "rcx");
                try writer.print("  mov qword [{s}], {s}\n", .{ ptr_reg, val_reg });
            },
            .load => {
                const op = try self.allocateOp(inst_idx);
                const ptr_op = try self.allocateOp(inst.data.load.ptr);
                const ptr_reg = try opToReg(writer, ptr_op, "rax");
                if (op == .mem) {
                    try writer.print("  mov rcx, qword [{s}]\n", .{ptr_reg});
                    try writer.print("  mov ", .{});
                    try printOp(writer, op);
                    try writer.print(", rcx\n", .{});
                } else {
                    try writer.print("  mov {s}, qword [{s}]\n", .{ op.reg, ptr_reg });
                }
            },
            .param => {
                const op = try self.allocateOp(inst_idx);
                const param_idx = inst.data.param;
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
                const fn_node_idx = inst.data.func_sym;
                const fn_node = self.ast_tree.nodes.get(fn_node_idx);
                const fn_tok = self.ast_tree.tokens[fn_node.main_token];
                const fn_name = self.src[fn_tok.start..fn_tok.end];
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
                const fn_tok_idx = inst.data.label;
                const fn_tok = self.ast_tree.tokens[fn_tok_idx];
                const fn_name = self.src[fn_tok.start..fn_tok.end];
                try writer.print("global {s}\n{s}:\n", .{ fn_name, fn_name });
                try writer.print("  push rbp\n  mov rbp, rsp\n  sub rsp, 256\n", .{});
            },
            .call => {
                const op = try self.allocateOp(inst_idx);
                const num_args = inst.data.call.args_count;
                const args_extra_start = inst.data.call.args_start;
                var arg_classes_buf: [32]abi.ArgClass = undefined;
                const n = @min(num_args, 32);
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
                var i: u32 = @min(num_args, @as(u32, abi.integer_arg_regs.len));
                while (i > 0) {
                    i -= 1;
                    const arg_inst = self.lir.extra_data.items[args_extra_start + i];
                    const arg_op = try self.allocateOp(arg_inst);
                    const dest_reg = abi.integer_arg_regs[i];
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
                if (op == .mem) {
                    try writer.print("  mov ", .{});
                    try printOp(writer, op);
                    try writer.print(", rax\n", .{});
                } else if (!std.mem.eql(u8, op.reg, "rax")) {
                    try writer.print("  mov {s}, rax\n", .{op.reg});
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
                if (inst.data.ret) |r| {
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
        // We check by scanning for a .label inst whose token text == "main".
        var has_main = false;
        for (self.lir.insts.items) |inst| {
            if (inst.opcode == .label) {
                const tok = self.ast_tree.tokens[inst.data.label];
                const fn_name = self.src[tok.start..tok.end];
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

            .div => {
                // idiv: not in encoder yet — emit as sub rsp, 0 placeholder
                // (Stage 0 programs don't use division yet)
                std.debug.print("[X86Gen-bin] WARNING: div not implemented in binary mode\n", .{});
                _ = try self.allocateOp(inst_idx);
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
                try emitStoreViaPtr(enc, ptr_r, val_r);
            },

            .load => {
                const op = try self.allocateOp(inst_idx);
                const ptr_op = try self.allocateOp(inst.data.load.ptr);
                const ptr_r = try opToRegBin(enc, ptr_op, .rax);
                const dst_r = switch (op) {
                    .reg => |r| nameToReg(r),
                    .mem => .rcx,
                };
                try emitLoadViaPtr(enc, dst_r, ptr_r);
                if (op == .mem) {
                    try enc.emitMovMemReg(op.mem, .rcx);
                }
            },

            // ── ABI: parameters ──────────────────────────────────────────────
            .param => {
                const op = try self.allocateOp(inst_idx);
                const param_idx = inst.data.param;
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
                const fn_node_idx = inst.data.func_sym;
                const fn_node = self.ast_tree.nodes.get(fn_node_idx);
                const fn_tok = self.ast_tree.tokens[fn_node.main_token];
                const fn_name = self.src[fn_tok.start..fn_tok.end];
                const dst_r = switch (op) {
                    .reg => |r| nameToReg(r),
                    .mem => .rax,
                };
                try enc.emitLeaRipRel(dst_r, fn_name);
                if (op == .mem) try enc.emitMovMemReg(op.mem, .rax);
            },

            // ── function label + prologue ────────────────────────────────────
            .label => {
                const fn_tok_idx = inst.data.label;
                const fn_tok = self.ast_tree.tokens[fn_tok_idx];
                const fn_name = self.src[fn_tok.start..fn_tok.end];
                try enc.defineSymbol(fn_name);
                try enc.emitPushRbp();
                try enc.emitMovRbpRsp();
                try enc.emitSubRspImm32(256); // 256-byte frame
            },

            // ── call ─────────────────────────────────────────────────────────
            .call => {
                const op = try self.allocateOp(inst_idx);
                const num_args = inst.data.call.args_count;
                const args_extra_start = inst.data.call.args_start;
                const arg_regs_bin = [_]Reg{ .rdi, .rsi, .rdx, .rcx, .r8, .r9 };

                // Move register args (last first to avoid clobbering)
                var i: u32 = @min(num_args, @as(u32, arg_regs_bin.len));
                while (i > 0) {
                    i -= 1;
                    const arg_inst = self.lir.extra_data.items[args_extra_start + i];
                    const arg_op = try self.allocateOp(arg_inst);
                    const dst_r = arg_regs_bin[i];
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

                // Move return value
                switch (op) {
                    .reg => |r| try enc.emitMovRegReg(nameToReg(r), .rax),
                    .mem => |m| try enc.emitMovMemReg(m, .rax),
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
                if (inst.data.ret) |r| {
                    const val_op = try self.allocateOp(r);
                    const val_r = try opToRegBin(enc, val_op, .rax);
                    try enc.emitMovRegReg(.rax, val_r);
                }
                try enc.emitMovRspRbp();
                try enc.emitPopRbp();
                try enc.emitRet();
            },

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

test "pointer loads and stores encode r12/r13 base registers" {
    var enc = Encoder.init(std.testing.allocator);
    defer enc.deinit();

    try emitStoreViaPtr(&enc, .r13, .r12);
    try std.testing.expectEqualSlices(u8, &.{ 0x4d, 0x89, 0x65, 0x00 }, enc.buf.items);

    enc.buf.clearRetainingCapacity();
    try emitLoadViaPtr(&enc, .r14, .r12);
    try std.testing.expectEqualSlices(u8, &.{ 0x4d, 0x8b, 0x34, 0x24 }, enc.buf.items);
}
