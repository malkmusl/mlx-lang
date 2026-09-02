const std = @import("std");
const lir = @import("../../../ir/lir.zig");
const abi = @import("../abi.zig");
const Encoder = @import("../encoder.zig").Encoder;
const Condition = @import("../encoder.zig").Condition;
const Reg = @import("../target.zig").Register;
const integer_instructions = @import("../instructions/integer.zig");
const memory_codegen = @import("memory.zig");

fn nameToReg(name: []const u8) Reg {
    inline for (@typeInfo(Reg).@"enum".fields) |field| {
        if (std.mem.eql(u8, name, field.name)) return @enumFromInt(field.value);
    }
    return .rax;
}

fn opToRegBin(enc: *Encoder, operand: anytype, scratch: Reg) !Reg {
    return switch (operand) {
        .reg => |name| nameToReg(name),
        .mem => |slot| blk: {
            try enc.emitMovRegMem(scratch, slot);
            break :blk scratch;
        },
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

pub fn emit(self: anytype, enc: *Encoder, inst_idx: lir.Inst.Index) !void {
    const inst = self.lir.insts.items[inst_idx];
    if (self.verbose) std.debug.print("[X86Gen-bin]   inst %{d} = {s}\n", .{ inst_idx, @tagName(inst.opcode) });

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

        .aggregate_copy => {
            const op = try self.allocateOp(inst_idx);
            const source = try self.allocateOp(inst.data.aggregate_copy);
            const source_reg = try opToRegBin(enc, source, .rsi);
            try enc.emitMovRegReg(.rsi, source_reg);
            const layout = self.indirectReturnLayout(inst.type_id);
            const slot = try self.getOrAllocBlock(0x9000_0000 | inst_idx, @max(layout.size, 1), layout.alignment);
            try enc.emitLeaRegMem(.rdi, slot);
            try memory_codegen.emitMemoryCopy(enc, layout.size);
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
            switch (self.scalarMemorySize(inst.type_id)) {
                1 => try memory_codegen.emitStoreByteViaPtr(enc, ptr_r, val_r),
                2 => try memory_codegen.emitStoreWordViaPtr(enc, ptr_r, val_r),
                4 => try memory_codegen.emitStoreDwordViaPtr(enc, ptr_r, val_r),
                else => try memory_codegen.emitStoreViaPtr(enc, ptr_r, val_r),
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
            switch (self.scalarMemorySize(inst.type_id)) {
                1 => if (self.isSignedType(inst.type_id))
                    try memory_codegen.emitLoadSignedByteViaPtr(enc, dst_r, ptr_r)
                else
                    try memory_codegen.emitLoadByteViaPtr(enc, dst_r, ptr_r),
                2 => if (self.isSignedType(inst.type_id))
                    try memory_codegen.emitLoadSignedWordViaPtr(enc, dst_r, ptr_r)
                else
                    try memory_codegen.emitLoadWordViaPtr(enc, dst_r, ptr_r),
                4 => if (self.isSignedType(inst.type_id))
                    try memory_codegen.emitLoadSignedDwordViaPtr(enc, dst_r, ptr_r)
                else
                    try memory_codegen.emitLoadDwordViaPtr(enc, dst_r, ptr_r),
                else => try memory_codegen.emitLoadViaPtr(enc, dst_r, ptr_r),
            }
            if (op == .mem) {
                try enc.emitMovMemReg(op.mem, .rcx);
            }
        },

        // ── ABI: parameters ──────────────────────────────────────────────
        .param => {
            const op = try self.allocateOp(inst_idx);
            const param_idx = inst.data.param + @intFromBool(if (self.current_function_return_type) |return_type| self.isIndirectReturn(return_type) else false);
            const arg_regs_bin = [_]Reg{ .rdi, .rsi, .rdx, .rcx, .r8, .r9 };
            if (param_idx < arg_regs_bin.len) {
                const parameter_slot = try self.getOrAllocParameterSlot(param_idx);
                switch (op) {
                    .reg => |r| try enc.emitMovRegMem(nameToReg(r), parameter_slot),
                    .mem => |m| {
                        try enc.emitMovRegMem(.rax, parameter_slot);
                        try enc.emitMovMemReg(m, .rax);
                    },
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
            try enc.emitSubRspImm32(self.beginFunction(inst_idx));
            self.current_function_return_type = inst.type_id;
            const argument_registers = [_]Reg{ .rdi, .rsi, .rdx, .rcx, .r8, .r9 };
            for (argument_registers, 0..) |argument_register, parameter_index| {
                const parameter_slot = try self.getOrAllocParameterSlot(@intCast(parameter_index));
                try enc.emitMovMemReg(parameter_slot, argument_register);
            }
            if (self.isIndirectReturn(inst.type_id)) {
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
            const memory_return = self.isIndirectReturn(inst.type_id);
            const register_aggregate_return = self.isRegisterAggregateErrorUnion(inst.type_id);
            const register_value_return = self.isRegisterAggregate(inst.type_id);
            var memory_payload_slot: ?i32 = null;
            if (memory_return or register_aggregate_return or register_value_return) {
                const layout = if (self.isAggregate(inst.type_id)) self.indirectReturnLayout(inst.type_id) else self.errorPayloadLayout(inst.type_id);
                const slot = try self.getOrAllocBlock(0xa000_0000 | inst_idx, @max(layout.size, 8), layout.alignment);
                memory_payload_slot = slot;
                if (memory_return) try enc.emitLeaRegMem(.rdi, slot);
            }

            const register_offset: u32 = @intFromBool(memory_return);
            const register_capacity: u32 = @as(u32, arg_regs_bin.len) - register_offset;
            const stack_count = num_args -| register_capacity;
            const needs_stack_align = stack_count % 2 != 0;
            // Alignment padding precedes the pushed arguments so the first
            // stack parameter remains at [rbp + 16] in the callee.
            if (needs_stack_align) try enc.emitSubRspImm8(8);
            if (stack_count > 0) {
                var stack_index: u32 = num_args;
                while (stack_index > register_capacity) {
                    stack_index -= 1;
                    const arg_inst = self.lir.extra_data.items[args_extra_start + stack_index];
                    const arg_op = try self.allocateOp(arg_inst);
                    const source = try opToRegBin(enc, arg_op, .rax);
                    try enc.emitPushReg(source);
                }
            }

            // Move register args (last first to avoid clobbering)
            var i: u32 = @min(num_args, register_capacity);
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

            if (stack_count > 0 or needs_stack_align) {
                try enc.emitAddRspImm32(stack_count * 8 + @as(u32, @intFromBool(needs_stack_align)) * 8);
            }

            // Error unions return the tag in rax and the first integer payload in rdx.
            if (self.isAggregate(inst.type_id)) {
                const slot = memory_payload_slot.?;
                if (register_value_return) {
                    const layout = self.indirectReturnLayout(inst.type_id);
                    try enc.emitLeaRegMem(.rdi, slot);
                    try memory_codegen.emitStoreViaPtr(enc, .rdi, .rax);
                    if (layout.size > 8) {
                        try enc.emitMovRegImm64(.r8, 8);
                        try enc.emitAdd(.rdi, .r8);
                        try memory_codegen.emitStoreViaPtr(enc, .rdi, .rdx);
                    }
                }
                switch (op) {
                    .reg => |register| try enc.emitLeaRegMem(nameToReg(register), slot),
                    .mem => |destination| {
                        try enc.emitLeaRegMem(.rax, slot);
                        try enc.emitMovMemReg(destination, .rax);
                    },
                }
            } else if (self.isErrorUnion(inst.type_id)) {
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
                    try memory_codegen.emitStoreViaPtr(enc, .rdi, .rdx);
                    if (layout.size > 8) {
                        try enc.emitMovRegImm64(.r8, 8);
                        try enc.emitAdd(.rdi, .r8);
                        try memory_codegen.emitStoreViaPtr(enc, .rdi, .rcx);
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
                        .reg => |r| try memory_codegen.emitMovqGpFromXmm0(enc, nameToReg(r)),
                        .mem => |m| {
                            try memory_codegen.emitMovqGpFromXmm0(enc, .rax);
                            try enc.emitMovMemReg(m, .rax);
                        },
                    }
                } else switch (op) {
                    .reg => |r| try enc.emitMovRegReg(nameToReg(r), .rdx),
                    .mem => |m| try enc.emitMovMemReg(m, .rdx),
                }
            } else if (self.isSlice(inst.type_id)) {
                const length_slot = try self.getOrAllocErrorPayloadExtraSlot(inst_idx);
                try enc.emitMovMemReg(length_slot, .rdx);
                switch (op) {
                    .reg => |r| try enc.emitMovRegReg(nameToReg(r), .rax),
                    .mem => |m| try enc.emitMovMemReg(m, .rax),
                }
            } else switch (op) {
                .reg => |r| try enc.emitMovRegReg(nameToReg(r), .rax),
                .mem => |m| try enc.emitMovMemReg(m, .rax),
            }
        },

        .syscall => {
            const op = try self.allocateOp(inst_idx);
            const start = inst.data.syscall;
            const count = self.lir.extra_data.items[start];
            const syscall_regs = [_]Reg{ .rax, .rdi, .rsi, .rdx, .r10, .r8, .r9 };
            var index = @min(count, @as(u32, syscall_regs.len));
            while (index > 0) {
                index -= 1;
                const argument = self.lir.extra_data.items[start + 1 + index];
                const argument_op = try self.allocateOp(argument);
                const source = try opToRegBin(enc, argument_op, if (index == 0) .rax else .rcx);
                try enc.emitMovRegReg(syscall_regs[index], source);
            }
            try enc.emitSyscall();
            switch (op) {
                .reg => |register| try enc.emitMovRegReg(nameToReg(register), .rax),
                .mem => |destination| try enc.emitMovMemReg(destination, .rax),
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
            if (self.isAggregate(inst.type_id)) {
                if (inst.data.ret) |r| {
                    const val_op = try self.allocateOp(r);
                    const val_r = try opToRegBin(enc, val_op, .rsi);
                    try enc.emitMovRegReg(.rsi, val_r);
                    const layout = self.indirectReturnLayout(inst.type_id);
                    if (self.isRegisterAggregate(inst.type_id)) {
                        try memory_codegen.emitLoadViaPtr(enc, .rax, .rsi);
                        if (layout.size > 8) {
                            try enc.emitMovRegImm64(.r8, 8);
                            try enc.emitAdd(.rsi, .r8);
                            try memory_codegen.emitLoadViaPtr(enc, .rdx, .rsi);
                        }
                    } else {
                        try enc.emitMovRegMem(.rdi, self.current_hidden_payload_slot.?);
                        try memory_codegen.emitMemoryCopy(enc, layout.size);
                        try enc.emitMovRegMem(.rax, self.current_hidden_payload_slot.?);
                    }
                }
            } else if (self.isErrorUnion(inst.type_id)) {
                if (inst.data.ret) |r| {
                    const val_op = try self.allocateOp(r);
                    const val_r = try opToRegBin(enc, val_op, if (self.isMemoryErrorUnion(inst.type_id)) .rsi else .rdx);
                    if (self.isMemoryErrorUnion(inst.type_id)) {
                        try enc.emitMovRegReg(.rsi, val_r);
                        try enc.emitMovRegMem(.rdi, self.current_hidden_payload_slot.?);
                        try memory_codegen.emitMemoryCopy(enc, self.errorPayloadLayout(inst.type_id).size);
                    } else if (self.isRegisterAggregateErrorUnion(inst.type_id)) {
                        try enc.emitMovRegReg(.rsi, val_r);
                        try memory_codegen.emitLoadViaPtr(enc, .rdx, .rsi);
                        if (self.errorPayloadLayout(inst.type_id).size > 8) {
                            try enc.emitMovRegImm64(.r8, 8);
                            try enc.emitAdd(.rsi, .r8);
                            try memory_codegen.emitLoadViaPtr(enc, .rcx, .rsi);
                        }
                    } else if (self.isFloatErrorUnion(inst.type_id)) {
                        try memory_codegen.emitMovqXmm0FromGp(enc, val_r);
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

        .ret_slice => {
            const pointer = try self.allocateOp(inst.data.ret_slice.ptr);
            const length = try self.allocateOp(inst.data.ret_slice.len);
            const pointer_reg = try opToRegBin(enc, pointer, .rax);
            const length_reg = try opToRegBin(enc, length, .rdx);
            try enc.emitMovRegReg(.rax, pointer_reg);
            try enc.emitMovRegReg(.rdx, length_reg);
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
                try memory_codegen.emitMemoryCopy(enc, self.errorPayloadLayout(inst.type_id).size);
            } else if (self.isRegisterAggregateErrorUnion(inst.type_id)) {
                try enc.emitMovRegReg(.rsi, payload_reg);
                try memory_codegen.emitLoadViaPtr(enc, .rdx, .rsi);
                if (self.errorPayloadLayout(inst.type_id).size > 8) {
                    try enc.emitMovRegImm64(.r8, 8);
                    try enc.emitAdd(.rsi, .r8);
                    try memory_codegen.emitLoadViaPtr(enc, .rcx, .rsi);
                }
            }
            try enc.emitMovRegMem(.rax, tag_slot);
            if (self.isFloatErrorUnion(inst.type_id)) {
                try memory_codegen.emitMovqXmm0FromGp(enc, payload_reg);
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
                if (self.verbose) std.debug.print("[X86Gen-bin]   string_literal %{d}: offset not found!\n", .{inst_idx});
                return;
            };
            const str_vaddr: i64 = @as(i64, @bitCast(self.rodata_vaddr + str_off));
            if (self.verbose) std.debug.print("[X86Gen-bin]   string_literal %{d} vaddr=0x{x}\n", .{ inst_idx, @as(u64, @bitCast(str_vaddr)) });
            switch (op) {
                .reg => |r| try enc.emitMovRegImm64(nameToReg(r), str_vaddr),
                .mem => |m| {
                    try enc.emitMovRegImm64(.rax, str_vaddr);
                    try enc.emitMovMemReg(m, .rax);
                },
            }
        },

        .tuple_literal => {
            if (self.verbose) std.debug.print("[X86Gen-bin]   tuple lowering is not implemented for %{d}\n", .{inst_idx});
        },

        else => {
            if (self.verbose) std.debug.print("[X86Gen-bin]   [unhandled] {s}\n", .{@tagName(inst.opcode)});
        },
    }
}
