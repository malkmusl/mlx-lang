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
    
    // Memory
    load,
    store,
    addr,
    
    // Control Flow
    br,
    condbr,
    call,
    ret,
    unreachable_inst,
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
        
        load: struct { ptr: Index },
        store: struct { ptr: Index, val: Index },
        addr: u32, // ID of local variable/symbol
        
        br: struct { dest: BasicBlock.Index },
        condbr: struct { cond: Index, true_dest: BasicBlock.Index, false_dest: BasicBlock.Index },
        call: struct { func: Index, args_start: u32, args_count: u32 },
        ret: ?Index,
        unreachable_inst: void,
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

    pub fn init(allocator: std.mem.Allocator) Lir {
        return .{
            .allocator = allocator,
            .insts = std.ArrayList(Inst).empty,
            .blocks = std.ArrayList(BasicBlock).empty,
        };
    }

    pub fn deinit(self: *Lir) void {
        for (self.blocks.items) |*blk| {
            blk.deinit(self.allocator);
        }
        self.blocks.deinit(self.allocator);
        self.insts.deinit(self.allocator);
    }
};
