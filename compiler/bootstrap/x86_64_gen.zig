const std = @import("std");
const lir = @import("lir.zig");
const x86 = @import("x86_64.zig");
const abi = @import("abi.zig");
const ast_mod = @import("ast.zig");

/// x86_64 assembly generator. Iterates over LIR blocks and emits NASM syntax.
/// Uses the zincc ABI (System V AMD64 on Linux x86_64).
pub const X86Gen = struct {
    pub const Operand = union(enum) {
        reg: []const u8, // register name string
        mem: i32, // offset from rbp (negative = local, positive = incoming stack arg)
    };

    allocator: std.mem.Allocator,
    lir: *lir.Lir,
    ast_tree: ast_mod.Ast,
    src: []const u8,
    vreg_to_op: std.AutoHashMap(lir.Inst.Index, Operand),
    /// Maps addr.data.addr (token/node ID) → stable rbp-relative offset (positive means offset from rbp, slot is [rbp - offset])
    addr_to_slot: std.AutoHashMap(u32, i32),
    next_gp_reg: u8,
    next_stack_slot: i32, // grows downward (starts at 8, increments by 8)

    /// GP regs available for allocation (excludes rax/rcx used as scratch,
    /// rsp/rbp are frame regs).  Caller-saved first so they're preferred.
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
        };
    }

    pub fn deinit(self: *X86Gen) void {
        self.vreg_to_op.deinit();
        self.addr_to_slot.deinit();
    }

    // ── register allocator ──────────────────────────────────────────────────

    fn allocateOp(self: *X86Gen, vreg: lir.Inst.Index) !Operand {
        if (self.vreg_to_op.get(vreg)) |op| return op;

        if (self.next_gp_reg < gp_regs.len) {
            const reg = gp_regs[self.next_gp_reg];
            self.next_gp_reg += 1;
            const op = Operand{ .reg = reg };
            try self.vreg_to_op.put(vreg, op);
            return op;
        }

        // Spill to stack
        const op = Operand{ .mem = @as(i32, @intCast(self.next_stack_slot)) };
        self.next_stack_slot += 8;
        try self.vreg_to_op.put(vreg, op);
        return op;
    }

    // ── operand printing ─────────────────────────────────────────────────────

    fn printOp(writer: anytype, op: Operand) !void {
        switch (op) {
            .reg => |r| try writer.print("{s}", .{r}),
            .mem => |m| try writer.print("qword [rbp - {d}]", .{m}),
        }
    }

    /// Load an operand into `rax` if it is a memory operand.
    /// Returns the effective register name.
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

    // ── main code generation ─────────────────────────────────────────────────

    pub fn generate(self: *X86Gen, writer: anytype) !void {
        std.debug.print("[X86Gen] Starting code generation\n", .{});

        // Emit the ELF entry stub
        try writer.print("global _start\nsection .text\n", .{});
        try writer.print("_start:\n", .{});
        try writer.print("  call main\n", .{});
        try writer.print("  mov rdi, rax\n", .{});
        try writer.print("  mov rax, 60\n", .{}); // sys_exit
        try writer.print("  syscall\n", .{});

        for (self.lir.blocks.items, 0..) |blk, blk_idx| {
            std.debug.print("[X86Gen] Emitting block {d} ({d} insts)\n", .{ blk_idx, blk.insts.items.len });
            try writer.print(".block_{d}:\n", .{blk_idx});

            for (blk.insts.items) |inst_idx| {
                const inst = self.lir.insts.items[inst_idx];
                std.debug.print("[X86Gen]   inst %{d} = {s}\n", .{ inst_idx, @tagName(inst.opcode) });

                switch (inst.opcode) {

                    // ── constants ───────────────────────────────────────────
                    .const_i => {
                        const op = try self.allocateOp(inst_idx);
                        if (op == .mem) {
                            try writer.print("  mov rax, {d}\n", .{inst.data.const_i});
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", rax\n", .{});
                        } else {
                            try writer.print("  mov {s}, {d}\n", .{ op.reg, inst.data.const_i });
                        }
                    },

                    // ── arithmetic ──────────────────────────────────────────
                    .add => {
                        const op  = try self.allocateOp(inst_idx);
                        const lhs = try self.allocateOp(inst.data.add.lhs);
                        const rhs = try self.allocateOp(inst.data.add.rhs);
                        const lreg = try opToReg(writer, lhs, "rax");
                        if (op == .mem) {
                            try writer.print("  mov rax, {s}\n", .{lreg});
                            const rreg = try opToReg(writer, rhs, "rcx");
                            try writer.print("  add rax, {s}\n", .{rreg});
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", rax\n", .{});
                        } else {
                            if (!std.mem.eql(u8, op.reg, lreg)) {
                                try writer.print("  mov {s}, {s}\n", .{ op.reg, lreg });
                            }
                            const rreg = try opToReg(writer, rhs, "rcx");
                            try writer.print("  add {s}, {s}\n", .{ op.reg, rreg });
                        }
                    },

                    .sub => {
                        const op  = try self.allocateOp(inst_idx);
                        const lhs = try self.allocateOp(inst.data.sub.lhs);
                        const rhs = try self.allocateOp(inst.data.sub.rhs);
                        const lreg = try opToReg(writer, lhs, "rax");
                        if (op == .mem) {
                            try writer.print("  mov rax, {s}\n", .{lreg});
                            const rreg = try opToReg(writer, rhs, "rcx");
                            try writer.print("  sub rax, {s}\n", .{rreg});
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", rax\n", .{});
                        } else {
                            if (!std.mem.eql(u8, op.reg, lreg)) {
                                try writer.print("  mov {s}, {s}\n", .{ op.reg, lreg });
                            }
                            const rreg = try opToReg(writer, rhs, "rcx");
                            try writer.print("  sub {s}, {s}\n", .{ op.reg, rreg });
                        }
                    },

                    .mul => {
                        const op  = try self.allocateOp(inst_idx);
                        const lhs = try self.allocateOp(inst.data.mul.lhs);
                        const rhs = try self.allocateOp(inst.data.mul.rhs);
                        // imul must have a register destination
                        const lreg = try opToReg(writer, lhs, "rax");
                        try writer.print("  mov rax, {s}\n", .{lreg});
                        const rreg = try opToReg(writer, rhs, "rcx");
                        try writer.print("  imul rax, {s}\n", .{rreg});
                        if (op == .mem) {
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", rax\n", .{});
                        } else {
                            try writer.print("  mov {s}, rax\n", .{op.reg});
                        }
                    },

                    .div => {
                        const op  = try self.allocateOp(inst_idx);
                        const lhs = try self.allocateOp(inst.data.div.lhs);
                        const rhs = try self.allocateOp(inst.data.div.rhs);
                        // idiv: rax = rdx:rax / operand
                        const lreg = try opToReg(writer, lhs, "rax");
                        try writer.print("  mov rax, {s}\n", .{lreg});
                        try writer.print("  cqo\n", .{}); // sign-extend rax into rdx:rax
                        const rreg = try opToReg(writer, rhs, "rcx");
                        try writer.print("  idiv {s}\n", .{rreg});
                        if (op == .mem) {
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", rax\n", .{});
                        } else {
                            try writer.print("  mov {s}, rax\n", .{op.reg});
                        }
                    },

                    // ── memory ──────────────────────────────────────────────
                    .addr => {
                        const op = try self.allocateOp(inst_idx);
                        // Assign a stable stack slot for this variable ID.
                        // inst.data.addr is a token/node index that uniquely identifies the variable.
                        const var_id = inst.data.addr;
                        const slot = slot_blk: {
                            if (self.addr_to_slot.get(var_id)) |s| break :slot_blk s;
                            const s = self.next_stack_slot;
                            self.next_stack_slot += 8;
                            try self.addr_to_slot.put(var_id, s);
                            break :slot_blk s;
                        };
                        // op holds the address (ptr) value = &variable = rbp - slot
                        if (op == .mem) {
                            try writer.print("  lea rax, [rbp - {d}]\n", .{slot});
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", rax\n", .{});
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
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", rcx\n", .{});
                        } else {
                            try writer.print("  mov {s}, qword [{s}]\n", .{ op.reg, ptr_reg });
                        }
                    },

                    // ── zincc ABI: parameters ───────────────────────────────
                    .param => {
                        const op = try self.allocateOp(inst_idx);
                        const param_idx = inst.data.param;
                        if (param_idx < abi.integer_arg_regs.len) {
                            const arg_reg = abi.integer_arg_regs[param_idx];
                            if (op == .mem) {
                                try writer.print("  mov ", .{}); try printOp(writer, op);
                                try writer.print(", {s}\n", .{arg_reg});
                            } else {
                                try writer.print("  mov {s}, {s}\n", .{ op.reg, arg_reg });
                            }
                        } else {
                            // Stack-passed param: at [rbp + 16 + (n-6)*8]
                            const stack_offset: i32 = 16 + @as(i32, @intCast((param_idx - 6) * 8));
                            try writer.print("  mov rax, qword [rbp + {d}]\n", .{stack_offset});
                            if (op == .mem) {
                                try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", rax\n", .{});
                            } else {
                                try writer.print("  mov {s}, rax\n", .{op.reg});
                            }
                        }
                    },

                    // ── func_sym: load address of a function ─────────────────
                    .func_sym => {
                        const op = try self.allocateOp(inst_idx);
                        const fn_node_idx = inst.data.func_sym;
                        const fn_node = self.ast_tree.nodes.get(fn_node_idx);
                        const fn_tok = self.ast_tree.tokens[fn_node.main_token];
                        const fn_name = self.src[fn_tok.start..fn_tok.end];
                        if (op == .mem) {
                            try writer.print("  lea rax, [rel {s}]\n", .{fn_name});
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", rax\n", .{});
                        } else {
                            try writer.print("  lea {s}, [rel {s}]\n", .{ op.reg, fn_name });
                        }
                    },

                    // ── function label + prologue ────────────────────────────
                    .label => {
                        const fn_tok_idx = inst.data.label;
                        const fn_tok = self.ast_tree.tokens[fn_tok_idx];
                        const fn_name = self.src[fn_tok.start..fn_tok.end];

                        // Emit label (global if not _start)
                        try writer.print("global {s}\n", .{fn_name});
                        try writer.print("{s}:\n", .{fn_name});

                        // Standard function prologue
                        try writer.print("  push rbp\n", .{});
                        try writer.print("  mov rbp, rsp\n", .{});
                        try writer.print("  sub rsp, 256\n", .{}); // 256-byte frame
                    },

                    // ── zincc ABI: call site ─────────────────────────────────
                    .call => {
                        const op = try self.allocateOp(inst_idx);
                        const num_args = inst.data.call.args_count;
                        const args_extra_start = inst.data.call.args_start;

                        // Classify args and build call site descriptor
                        var arg_classes_buf: [32]abi.ArgClass = undefined;
                        const n = @min(num_args, 32);
                        for (0..n) |i| {
                            // For stage 0 just assume INTEGER for all args
                            arg_classes_buf[i] = .INTEGER;
                        }
                        const site = abi.describeCall(arg_classes_buf[0..n]);

                        // Align RSP to 16 before the call if stack args needed
                        if (site.stack_count > 0) {
                            // Push stack args in reverse order
                            var j: i32 = @as(i32, @intCast(num_args)) - 1;
                            while (j >= @as(i32, @intCast(abi.integer_arg_regs.len))) : (j -= 1) {
                                const arg_inst = self.lir.extra_data.items[args_extra_start + @as(u32, @intCast(j))];
                                const arg_op = try self.allocateOp(arg_inst);
                                const arg_reg = try opToReg(writer, arg_op, "rax");
                                try writer.print("  push {s}\n", .{arg_reg});
                            }
                            // Align if needed
                            if (site.needs_stack_align) {
                                try writer.print("  sub rsp, 8\n", .{});
                            }
                        }

                        // Move register args into place (last first to avoid clobbering)
                        var i: u32 = @min(num_args, @as(u32, abi.integer_arg_regs.len));
                        while (i > 0) {
                            i -= 1;
                            const arg_inst = self.lir.extra_data.items[args_extra_start + i];
                            const arg_op = try self.allocateOp(arg_inst);
                            const dest_reg = abi.integer_arg_regs[i];
                            const src_reg = try opToReg(writer, arg_op, "rax");
                            if (!std.mem.eql(u8, dest_reg, src_reg)) {
                                try writer.print("  mov {s}, {s}\n", .{ dest_reg, src_reg });
                            }
                        }

                        // Emit the actual call
                        const func_inst = inst.data.call.func;
                        const func_op = try self.allocateOp(func_inst);
                        switch (func_op) {
                            .reg => |r| try writer.print("  call {s}\n", .{r}),
                            .mem => {
                                try writer.print("  mov rax, ", .{}); try printOp(writer, func_op); try writer.print("\n", .{});
                                try writer.print("  call rax\n", .{});
                            },
                        }

                        // Clean up stack args
                        if (site.stack_bytes > 0) {
                            var adjust = site.stack_bytes;
                            if (site.needs_stack_align) adjust += 8;
                            try writer.print("  add rsp, {d}\n", .{adjust});
                        }

                        // Move return value from rax into result operand
                        if (op == .mem) {
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", rax\n", .{});
                        } else if (!std.mem.eql(u8, op.reg, "rax")) {
                            try writer.print("  mov {s}, rax\n", .{op.reg});
                        }
                    },

                    // ── control flow ────────────────────────────────────────
                    .br => {
                        try writer.print("  jmp .block_{d}\n", .{inst.data.br.dest});
                    },

                    .condbr => {
                        const cond_op = try self.allocateOp(inst.data.condbr.cond);
                        switch (cond_op) {
                            .reg => |r| {
                                try writer.print("  test {s}, {s}\n", .{ r, r });
                            },
                            .mem => {
                                try writer.print("  mov rax, ", .{}); try printOp(writer, cond_op); try writer.print("\n", .{});
                                try writer.print("  test rax, rax\n", .{});
                            },
                        }
                        try writer.print("  jnz .block_{d}\n", .{inst.data.condbr.true_dest});
                        try writer.print("  jmp .block_{d}\n", .{inst.data.condbr.false_dest});
                    },

                    .ret => {
                        // Standard function epilogue
                        if (inst.data.ret) |r| {
                            const val_op = try self.allocateOp(r);
                            switch (val_op) {
                                .reg => |reg| {
                                    if (!std.mem.eql(u8, reg, "rax")) {
                                        try writer.print("  mov rax, {s}\n", .{reg});
                                    }
                                },
                                .mem => {
                                    try writer.print("  mov rax, ", .{}); try printOp(writer, val_op); try writer.print("\n", .{});
                                },
                            }
                        }
                        try writer.print("  mov rsp, rbp\n", .{});
                        try writer.print("  pop rbp\n", .{});
                        try writer.print("  ret\n", .{});
                    },

                    else => {
                        try writer.print("  ; [unhandled] {s}\n", .{@tagName(inst.opcode)});
                    },
                }
            }
        }
        std.debug.print("[X86Gen] Code generation complete\n", .{});
    }
};
