const std = @import("std");
const Type = @import("type.zig").Type;

pub const Opcode = enum {
    const_i,
    const_f,
    copy,

    // Arithmetic
    add,
    sub,
    mul,
    div,
    rem,
    icmp,

    // Memory
    load,
    store,
    addr,
    alloca,
    gep,
    func_sym,
    label,

    // Control Flow
    br,
    condbr,
    call,
    syscall,
    ret,
    ret_error,
    ret_error_union,
    ret_error_slice,
    ret_error_union_slice,
    unreachable_inst,
    param,

    // Error unions
    error_test,
    error_payload,
    error_payload_part,

    // Builtins and Literals
    builtin_sym,
    string_literal,
    tuple_literal,
};

pub const CmpPredicate = enum {
    eq,
    ne,
    lt,
    le,
    gt,
    ge,
};

pub const Inst = struct {
    pub const Index = u32;

    pub const Data = union(Opcode) {
        const_i: u64,
        const_f: f64,
        copy: Index,

        add: struct { lhs: Index, rhs: Index },
        sub: struct { lhs: Index, rhs: Index },
        mul: struct { lhs: Index, rhs: Index },
        div: struct { lhs: Index, rhs: Index },
        rem: struct { lhs: Index, rhs: Index },
        icmp: struct { predicate: CmpPredicate, lhs: Index, rhs: Index },

        load: struct { ptr: Index },
        store: struct { ptr: Index, val: Index },
        addr: u32, // ID of local variable/symbol
        alloca: struct { id: u32, size: u32, alignment: u32 },
        gep: struct { base: Index, index: Index, stride: i32 },
        func_sym: u32, // ID of function identifier node
        label: u32, // ID of function identifier node

        br: struct { dest: BasicBlock.Index },
        condbr: struct { cond: Index, true_dest: BasicBlock.Index, false_dest: BasicBlock.Index },
        call: struct { func: Index, args_start: u32, args_count: u32 },
        syscall: u32, // extra_start, first elem is arg_count
        ret: ?Index,
        ret_error: Index,
        ret_error_union: Index,
        ret_error_slice: struct { ptr: Index, len: Index },
        ret_error_union_slice: struct { source: Index, len: Index },
        unreachable_inst: void,
        param: u32, // Parameter index

        error_test: Index,
        error_payload: Index,
        error_payload_part: struct { source: Index, part: u8 },

        builtin_sym: u32,
        string_literal: u32, // AST node index
        tuple_literal: u32, // AST node index
    };

    opcode: Opcode,
    type_id: Type.Id,
    data: Data,
};

pub const BasicBlock = struct {
    pub const Index = u32;

    insts: std.ArrayList(Inst.Index),

    pub fn init() BasicBlock {
        return .{
            .insts = std.ArrayList(Inst.Index).empty,
        };
    }

    pub fn deinit(self: *BasicBlock, allocator: std.mem.Allocator) void {
        self.insts.deinit(allocator);
    }
};

pub const Lir = struct {
    allocator: std.mem.Allocator,
    insts: std.ArrayList(Inst),
    blocks: std.ArrayList(BasicBlock),
    extra_data: std.ArrayList(u32), // Stores argument lists, etc.

    pub fn init(allocator: std.mem.Allocator) Lir {
        return .{
            .allocator = allocator,
            .insts = std.ArrayList(Inst).empty,
            .blocks = std.ArrayList(BasicBlock).empty,
            .extra_data = std.ArrayList(u32).empty,
        };
    }

    pub fn deinit(self: *Lir) void {
        for (self.blocks.items) |*blk| {
            blk.deinit(self.allocator);
        }
        self.blocks.deinit(self.allocator);
        self.insts.deinit(self.allocator);
        self.extra_data.deinit(self.allocator);
    }
};
