const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Type = @import("../type.zig").Type;

pub const Argument = struct {
    type_id: Type.Id,
    type_value: ?Type.Id = null,
    const_value: ?u64 = null,

    pub fn eql(left: Argument, right: Argument) bool {
        return left.type_id == right.type_id and left.type_value == right.type_value and left.const_value == right.const_value;
    }
};

pub const Instance = struct {
    declaration: Node.Index,
    arguments: []Argument,
    function_type: Type.Id,
    result_type_value: ?Type.Id = null,
    node_types: std.AutoHashMap(Node.Index, Type.Id),
    const_values: std.AutoHashMap(Node.Index, u64),

    pub fn deinit(self: *Instance, allocator: std.mem.Allocator) void {
        allocator.free(self.arguments);
        self.node_types.deinit();
        self.const_values.deinit();
    }
};
