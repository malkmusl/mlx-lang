const std = @import("std");
const lir = @import("lir.zig");
const x86 = @import("x86_64.zig");

pub const X86Gen = struct {
    pub const Operand = union(enum) {
        reg: x86.Register,
        mem: i32, // offset from rbp
    };

    allocator: std.mem.Allocator,
    lir: *lir.Lir,
    vreg_to_op: std.AutoHashMap(lir.Inst.Index, Operand),
    next_free_reg: u8,
    next_spill_offset: i32,
    
    pub fn init(allocator: std.mem.Allocator, ir: *lir.Lir) X86Gen {
        return .{
            .allocator = allocator,
            .lir = ir,
            .vreg_to_op = std.AutoHashMap(lir.Inst.Index, Operand).init(allocator),
            .next_free_reg = 0,
            .next_spill_offset = 128, // Start spilling above local variables
        };
    }
    
    pub fn deinit(self: *X86Gen) void {
        self.vreg_to_op.deinit();
    }
    
    fn allocateOp(self: *X86Gen, vreg: lir.Inst.Index) !Operand {
        if (self.vreg_to_op.get(vreg)) |op| {
            return op;
        }
        
        const usable_regs = [_]x86.Register{ .rdx, .rbx, .rsi, .rdi, .r8, .r9, .r10, .r11, .r12, .r13, .r14, .r15 };
        
        if (self.next_free_reg < usable_regs.len) {
            const preg = usable_regs[self.next_free_reg];
            self.next_free_reg += 1;
            const op = Operand{ .reg = preg };
            try self.vreg_to_op.put(vreg, op);
            return op;
        }
        
        // Spill
        const op = Operand{ .mem = self.next_spill_offset };
        self.next_spill_offset += 8;
        try self.vreg_to_op.put(vreg, op);
        return op;
    }
    
    fn printOp(writer: anytype, op: Operand) !void {
        switch (op) {
            .reg => |r| try writer.print("{s}", .{@tagName(r)}),
            .mem => |m| try writer.print("qword [rbp - {d}]", .{m}),
        }
    }
    
    pub fn generate(self: *X86Gen, writer: anytype) !void {
        
        try writer.print("global _start\n", .{});
        try writer.print("section .text\n", .{});
        try writer.print("_start:\n", .{});
        try writer.print("  push rbp\n", .{});
        try writer.print("  mov rbp, rsp\n", .{});
        try writer.print("  sub rsp, 256\n", .{});
        
        for (self.lir.blocks.items, 0..) |blk, blk_idx| {
            try writer.print(".block_{d}:\n", .{blk_idx});
            for (blk.insts.items) |inst_idx| {
                const inst = self.lir.insts.items[inst_idx];
                switch (inst.opcode) {
                    .const_i => {
                        const op = try self.allocateOp(inst_idx);
                        try writer.print("  mov ", .{});
                        try printOp(writer, op);
                        try writer.print(", {d}\n", .{ inst.data.const_i });
                    },
                    .add => {
                        const op = try self.allocateOp(inst_idx);
                        const lhs = try self.allocateOp(inst.data.add.lhs);
                        const rhs = try self.allocateOp(inst.data.add.rhs);
                        
                        if (op == .mem and lhs == .mem) {
                            try writer.print("  mov rax, ", .{}); try printOp(writer, lhs); try writer.print("\n", .{});
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", rax\n", .{});
                        } else {
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", ", .{}); try printOp(writer, lhs); try writer.print("\n", .{});
                        }
                        
                        if (op == .mem and rhs == .mem) {
                            try writer.print("  mov rax, ", .{}); try printOp(writer, rhs); try writer.print("\n", .{});
                            try writer.print("  add ", .{}); try printOp(writer, op); try writer.print(", rax\n", .{});
                        } else {
                            try writer.print("  add ", .{}); try printOp(writer, op); try writer.print(", ", .{}); try printOp(writer, rhs); try writer.print("\n", .{});
                        }
                    },
                    .sub => {
                        const op = try self.allocateOp(inst_idx);
                        const lhs = try self.allocateOp(inst.data.sub.lhs);
                        const rhs = try self.allocateOp(inst.data.sub.rhs);
                        
                        if (op == .mem and lhs == .mem) {
                            try writer.print("  mov rax, ", .{}); try printOp(writer, lhs); try writer.print("\n", .{});
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", rax\n", .{});
                        } else {
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", ", .{}); try printOp(writer, lhs); try writer.print("\n", .{});
                        }
                        
                        if (op == .mem and rhs == .mem) {
                            try writer.print("  mov rax, ", .{}); try printOp(writer, rhs); try writer.print("\n", .{});
                            try writer.print("  sub ", .{}); try printOp(writer, op); try writer.print(", rax\n", .{});
                        } else {
                            try writer.print("  sub ", .{}); try printOp(writer, op); try writer.print(", ", .{}); try printOp(writer, rhs); try writer.print("\n", .{});
                        }
                    },
                    .mul => {
                        const op = try self.allocateOp(inst_idx);
                        const lhs = try self.allocateOp(inst.data.mul.lhs);
                        const rhs = try self.allocateOp(inst.data.mul.rhs);
                        
                        if (op == .mem and lhs == .mem) {
                            try writer.print("  mov rax, ", .{}); try printOp(writer, lhs); try writer.print("\n", .{});
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", rax\n", .{});
                        } else {
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", ", .{}); try printOp(writer, lhs); try writer.print("\n", .{});
                        }
                        
                        if (op == .mem and rhs == .mem) {
                            // imul can't take mem, mem. Actually imul op, rhs doesn't work if op is mem!
                            // imul requires a register destination!
                            try writer.print("  mov rax, ", .{}); try printOp(writer, op); try writer.print("\n", .{});
                            try writer.print("  imul rax, ", .{}); try printOp(writer, rhs); try writer.print("\n", .{});
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", rax\n", .{});
                        } else if (op == .mem) {
                            try writer.print("  mov rax, ", .{}); try printOp(writer, op); try writer.print("\n", .{});
                            try writer.print("  imul rax, ", .{}); try printOp(writer, rhs); try writer.print("\n", .{});
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", rax\n", .{});
                        } else {
                            try writer.print("  imul ", .{}); try printOp(writer, op); try writer.print(", ", .{}); try printOp(writer, rhs); try writer.print("\n", .{});
                        }
                    },
                    .addr => {
                        const op = try self.allocateOp(inst_idx);
                        if (op == .mem) {
                            try writer.print("  lea rax, [rbp - {d}]\n", .{ (inst.data.addr + 1) * 8 });
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", rax\n", .{});
                        } else {
                            try writer.print("  lea ", .{}); try printOp(writer, op); try writer.print(", [rbp - {d}]\n", .{ (inst.data.addr + 1) * 8 });
                        }
                    },
                    .store => {
                        const ptr_op = try self.allocateOp(inst.data.store.ptr);
                        const val_op = try self.allocateOp(inst.data.store.val);
                        
                        var ptr_reg: []const u8 = undefined;
                        if (ptr_op == .mem) {
                            try writer.print("  mov rax, ", .{}); try printOp(writer, ptr_op); try writer.print("\n", .{});
                            ptr_reg = "rax";
                        } else {
                            ptr_reg = @tagName(ptr_op.reg);
                        }
                        
                        if (val_op == .mem) {
                            try writer.print("  mov rcx, ", .{}); try printOp(writer, val_op); try writer.print("\n", .{});
                            try writer.print("  mov qword [{s}], rcx\n", .{ptr_reg});
                        } else {
                            try writer.print("  mov qword [{s}], ", .{ptr_reg}); try printOp(writer, val_op); try writer.print("\n", .{});
                        }
                    },
                    .load => {
                        const op = try self.allocateOp(inst_idx);
                        const ptr_op = try self.allocateOp(inst.data.load.ptr);
                        
                        var ptr_reg: []const u8 = undefined;
                        if (ptr_op == .mem) {
                            try writer.print("  mov rax, ", .{}); try printOp(writer, ptr_op); try writer.print("\n", .{});
                            ptr_reg = "rax";
                        } else {
                            ptr_reg = @tagName(ptr_op.reg);
                        }
                        
                        if (op == .mem) {
                            try writer.print("  mov rcx, qword [{s}]\n", .{ptr_reg});
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", rcx\n", .{});
                        } else {
                            try writer.print("  mov ", .{}); try printOp(writer, op); try writer.print(", qword [{s}]\n", .{ptr_reg});
                        }
                    },
                    .br => {
                        try writer.print("  jmp .block_{d}\n", .{inst.data.br.dest});
                    },
                    .condbr => {
                        const cond_op = try self.allocateOp(inst.data.condbr.cond);
                        if (cond_op == .mem) {
                            try writer.print("  mov rax, ", .{}); try printOp(writer, cond_op); try writer.print("\n", .{});
                            try writer.print("  test rax, rax\n", .{});
                        } else {
                            try writer.print("  test ", .{}); try printOp(writer, cond_op); try writer.print(", ", .{}); try printOp(writer, cond_op); try writer.print("\n", .{});
                        }
                        try writer.print("  jnz .block_{d}\n", .{inst.data.condbr.true_dest});
                        try writer.print("  jmp .block_{d}\n", .{inst.data.condbr.false_dest});
                    },
                    .ret => {
                        if (inst.data.ret) |r| {
                            const val_op = try self.allocateOp(r);
                            try writer.print("  mov rax, ", .{}); try printOp(writer, val_op); try writer.print("\n", .{});
                        }
                        try writer.print("  mov rdi, rax\n", .{});
                        try writer.print("  mov rax, 60\n", .{}); // sys_exit
                        try writer.print("  syscall\n", .{});
                    },
                    else => {
                        try writer.print("  ; unhandled opcode {s}\n", .{ @tagName(inst.opcode) });
                    }
                }
            }
        }
    }
};
