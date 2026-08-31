const std = @import("std");
const lir = @import("../../../ir/lir.zig");
const abi = @import("../abi.zig");
const integer_instructions = @import("../instructions/integer.zig");

fn printOp(writer: anytype, operand: anytype) !void {
    switch (operand) {
        .reg => |name| try writer.print("{s}", .{name}),
        .mem => |slot| try writer.print("qword [rbp - {d}]", .{slot}),
    }
}

fn opToReg(writer: anytype, operand: anytype, scratch: []const u8) ![]const u8 {
    return switch (operand) {
        .reg => |name| name,
        .mem => |slot| blk: {
            try writer.print("  mov {s}, qword [rbp - {d}]\n", .{ scratch, slot });
            break :blk scratch;
        },
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

pub fn emit(self: anytype, writer: anytype, inst_idx: lir.Inst.Index) !void {
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
        .aggregate_copy => {
            const op = try self.allocateOp(inst_idx);
            const source = try self.allocateOp(inst.data.aggregate_copy);
            const source_reg = try opToReg(writer, source, "rsi");
            if (!std.mem.eql(u8, source_reg, "rsi")) try writer.print("  mov rsi, {s}\n", .{source_reg});
            const layout = self.indirectReturnLayout(inst.type_id);
            const slot = try self.getOrAllocBlock(0x9000_0000 | inst_idx, @max(layout.size, 1), layout.alignment);
            try writer.print("  lea rdi, [rbp - {d}]\n", .{slot});
            var offset: u32 = 0;
            while (offset + 8 <= layout.size) : (offset += 8) {
                try writer.print("  mov rax, qword [rsi + {d}]\n  mov qword [rdi + {d}], rax\n", .{ offset, offset });
            }
            while (offset < layout.size) : (offset += 1) {
                try writer.print("  mov al, byte [rsi + {d}]\n  mov byte [rdi + {d}], al\n", .{ offset, offset });
            }
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
            const param_idx = inst.data.param + @intFromBool(if (self.current_function_return_type) |return_type| self.isIndirectReturn(return_type) else false);
            if (param_idx < abi.integer_arg_regs.len) {
                const parameter_slot = try self.getOrAllocParameterSlot(param_idx);
                if (op == .mem) {
                    try writer.print("  mov rax, qword [rbp - {d}]\n  mov ", .{parameter_slot});
                    try printOp(writer, op);
                    try writer.print(", rax\n", .{});
                } else {
                    try writer.print("  mov {s}, qword [rbp - {d}]\n", .{ op.reg, parameter_slot });
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
            const frame_size = self.beginFunction(inst_idx);
            try writer.print("  push rbp\n  mov rbp, rsp\n  sub rsp, {d}\n", .{frame_size});
            self.current_function_return_type = inst.type_id;
            for (abi.integer_arg_regs, 0..) |argument_register, parameter_index| {
                const parameter_slot = try self.getOrAllocParameterSlot(@intCast(parameter_index));
                try writer.print("  mov qword [rbp - {d}], {s}\n", .{ parameter_slot, argument_register });
            }
            if (self.isIndirectReturn(inst.type_id)) {
                const slot = try self.getOrAllocSlot(0xb000_0000 | inst.data.label);
                self.current_hidden_payload_slot = slot;
                try writer.print("  mov qword [rbp - {d}], rdi\n", .{slot});
            }
        },
        .call => {
            const op = try self.allocateOp(inst_idx);
            const num_args = inst.data.call.args_count;
            const args_extra_start = inst.data.call.args_start;
            const memory_return = self.isIndirectReturn(inst.type_id);
            const register_aggregate_return = self.isRegisterAggregateErrorUnion(inst.type_id);
            const register_value_return = self.isRegisterAggregate(inst.type_id);
            var memory_payload_slot: ?i32 = null;
            if (memory_return or register_aggregate_return or register_value_return) {
                const layout = if (self.isAggregate(inst.type_id)) self.indirectReturnLayout(inst.type_id) else self.errorPayloadLayout(inst.type_id);
                const slot = try self.getOrAllocBlock(0xa000_0000 | inst_idx, @max(layout.size, 8), layout.alignment);
                memory_payload_slot = slot;
                if (memory_return) try writer.print("  lea rdi, [rbp - {d}]\n", .{slot});
            }
            const register_offset: u32 = @intFromBool(memory_return);
            const register_capacity: u32 = @as(u32, abi.integer_arg_regs.len) - register_offset;
            const stack_count = num_args -| register_capacity;
            const needs_stack_align = stack_count % 2 != 0;
            if (needs_stack_align) try writer.print("  sub rsp, 8\n", .{});
            if (stack_count > 0) {
                var j: i32 = @as(i32, @intCast(num_args)) - 1;
                while (j >= @as(i32, @intCast(register_capacity))) : (j -= 1) {
                    const arg_inst = self.lir.extra_data.items[args_extra_start + @as(u32, @intCast(j))];
                    const arg_op = try self.allocateOp(arg_inst);
                    const arg_reg = try opToReg(writer, arg_op, "rax");
                    try writer.print("  push {s}\n", .{arg_reg});
                }
            }
            var i: u32 = @min(num_args, register_capacity);
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
            if (stack_count > 0 or needs_stack_align) {
                var adjust = stack_count * 8;
                if (needs_stack_align) adjust += 8;
                try writer.print("  add rsp, {d}\n", .{adjust});
            }
            if (self.isAggregate(inst.type_id)) {
                const slot = memory_payload_slot.?;
                if (register_value_return) {
                    const layout = self.indirectReturnLayout(inst.type_id);
                    try writer.print("  lea rdi, [rbp - {d}]\n  mov qword [rdi], rax\n", .{slot});
                    if (layout.size > 8) try writer.print("  mov qword [rdi + 8], rdx\n", .{});
                }
                if (op == .mem) {
                    try writer.print("  lea rax, [rbp - {d}]\n  mov ", .{slot});
                    try printOp(writer, op);
                    try writer.print(", rax\n", .{});
                } else {
                    try writer.print("  lea {s}, [rbp - {d}]\n", .{ op.reg, slot });
                }
            } else if (self.isErrorUnion(inst.type_id)) {
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
            } else if (self.isSlice(inst.type_id)) {
                const length_slot = try self.getOrAllocErrorPayloadExtraSlot(inst_idx);
                try writer.print("  mov qword [rbp - {d}], rdx\n", .{length_slot});
                if (op == .mem) {
                    try writer.print("  mov ", .{});
                    try printOp(writer, op);
                    try writer.print(", rax\n", .{});
                } else if (!std.mem.eql(u8, op.reg, "rax")) {
                    try writer.print("  mov {s}, rax\n", .{op.reg});
                }
            } else if (op == .mem) {
                try writer.print("  mov ", .{});
                try printOp(writer, op);
                try writer.print(", rax\n", .{});
            } else if (!std.mem.eql(u8, op.reg, "rax")) {
                try writer.print("  mov {s}, rax\n", .{op.reg});
            }
        },
        .syscall => {
            const op = try self.allocateOp(inst_idx);
            const start = inst.data.syscall;
            const count = self.lir.extra_data.items[start];
            const syscall_regs = [_][]const u8{ "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9" };
            var index = @min(count, @as(u32, syscall_regs.len));
            while (index > 0) {
                index -= 1;
                const argument = self.lir.extra_data.items[start + 1 + index];
                const argument_op = try self.allocateOp(argument);
                const source_reg = try opToReg(writer, argument_op, if (index == 0) "rax" else "rcx");
                const destination = syscall_regs[index];
                if (!std.mem.eql(u8, source_reg, destination)) try writer.print("  mov {s}, {s}\n", .{ destination, source_reg });
            }
            try writer.print("  syscall\n", .{});
            if (op == .mem) {
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
            if (self.isAggregate(inst.type_id)) {
                if (inst.data.ret) |r| {
                    const val_op = try self.allocateOp(r);
                    const source_reg = try opToReg(writer, val_op, "rsi");
                    if (!std.mem.eql(u8, source_reg, "rsi")) try writer.print("  mov rsi, {s}\n", .{source_reg});
                    const layout = self.indirectReturnLayout(inst.type_id);
                    if (self.isRegisterAggregate(inst.type_id)) {
                        try writer.print("  mov rax, qword [rsi]\n", .{});
                        if (layout.size > 8) try writer.print("  mov rdx, qword [rsi + 8]\n", .{});
                    } else {
                        try writer.print("  mov rdi, qword [rbp - {d}]\n", .{self.current_hidden_payload_slot.?});
                        var offset: u32 = 0;
                        while (offset + 8 <= layout.size) : (offset += 8) {
                            try writer.print("  mov rax, qword [rsi + {d}]\n  mov qword [rdi + {d}], rax\n", .{ offset, offset });
                        }
                        while (offset < layout.size) : (offset += 1) {
                            try writer.print("  mov al, byte [rsi + {d}]\n  mov byte [rdi + {d}], al\n", .{ offset, offset });
                        }
                        try writer.print("  mov rax, qword [rbp - {d}]\n", .{self.current_hidden_payload_slot.?});
                    }
                }
            } else if (self.isErrorUnion(inst.type_id)) {
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
        .ret_slice => {
            const pointer = try self.allocateOp(inst.data.ret_slice.ptr);
            const length = try self.allocateOp(inst.data.ret_slice.len);
            const pointer_reg = try opToReg(writer, pointer, "rax");
            const length_reg = try opToReg(writer, length, "rdx");
            if (!std.mem.eql(u8, pointer_reg, "rax")) try writer.print("  mov rax, {s}\n", .{pointer_reg});
            if (!std.mem.eql(u8, length_reg, "rdx")) try writer.print("  mov rdx, {s}\n", .{length_reg});
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
