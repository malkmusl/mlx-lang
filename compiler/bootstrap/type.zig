const std = @import("std");

pub const Copyability = enum {
    copyable,
    explicit_nocopy,
    contains_nocopy,

    pub fn combine(a: Copyability, b: Copyability) Copyability {
        if (a == .explicit_nocopy or b == .explicit_nocopy) return .contains_nocopy;
        if (a == .contains_nocopy or b == .contains_nocopy) return .contains_nocopy;
        return .copyable;
    }
};

pub const Type = struct {
    pub const Id = u32;

    pub const Primitive = enum {
        void_type,
        noreturn_type,
        type_type,
        anytype_type,
        anyopaque_type,
        bool_type,
        f32_type,
        f64_type,
        comptime_int_type,
        comptime_float_type,
    };

    pub const Int = struct {
        is_signed: bool,
        bits: u16,
    };

    pub const Pointer = struct {
        child_type: Id,
        is_const: bool,
        is_optional: bool,
        size: enum { One, Many, Slice },
    };

    pub const Optional = struct {
        child_type: Id,
    };

    pub const Aggregate = struct {
        // Just a stub for now, will contain fields
        id: u32,
    };

    pub const Tag = enum {
        primitive,
        integer,
        pointer,
        optional,
        array,
        @"struct",
        @"enum",
        @"union",
        tuple,
        error_union,
        function,
    };

    pub const Data = union(Tag) {
        primitive: Primitive,
        integer: Int,
        pointer: Pointer,
        optional: Optional,
        array: struct { child: Id, len: u64 },
        @"struct": Aggregate,
        @"enum": Aggregate,
        @"union": Aggregate,
        tuple: Aggregate,
        error_union: struct { err_set: Id, payload: Id },
        function: struct { ret_type: Id },
    };

    data: Data,
    copyability: Copyability,

    pub fn isCopyable(self: Type) bool {
        return self.copyability == .copyable;
    }
};

pub const TypePool = struct {
    allocator: std.mem.Allocator,
    types: std.ArrayList(Type),
    // For interning, we'd normally use a HashMap mapping from `Type.Data` to `Type.Id`.
    // We'll use a simple ArrayHashMap for deduplication.
    intern_map: std.HashMapUnmanaged(Type.Data, Type.Id, TypeDataHashContext, std.hash_map.default_max_load_percentage),

    pub fn init(allocator: std.mem.Allocator) TypePool {
        return .{
            .allocator = allocator,
            .types = std.ArrayList(Type).empty,
            .intern_map = std.HashMapUnmanaged(Type.Data, Type.Id, TypeDataHashContext, std.hash_map.default_max_load_percentage).empty,
        };
    }

    pub fn deinit(self: *TypePool) void {
        self.types.deinit(self.allocator);
        self.intern_map.deinit(self.allocator);
    }

    pub fn intern(self: *TypePool, data: Type.Data, copyability: Copyability) !Type.Id {
        if (self.intern_map.get(data)) |id| {
            // Note: If copyability differs for the same data, we might need to include it in the hash key.
            // But usually, data defines the structure, and nocopy is a wrapper or derived property.
            // Actually, @nocopy(struct) generates a new type or wraps it? 
            // The spec says @nocopy is a type constructor. Let's assume nocopy is part of the type identity.
            // For now, we'll just check if it exists.
            if (self.types.items[id].copyability == copyability) {
                return id;
            }
        }

        const id = @as(Type.Id, @intCast(self.types.items.len));
        try self.types.append(self.allocator, .{ .data = data, .copyability = copyability });
        try self.intern_map.put(self.allocator, data, id);
        return id;
    }

    pub fn get(self: *const TypePool, id: Type.Id) Type {
        return self.types.items[id];
    }
};

const TypeDataHashContext = struct {
    pub fn hash(self: @This(), data: Type.Data) u64 {
        _ = self;
        var hasher = std.hash.Wyhash.init(0);
        const tag = std.meta.activeTag(data);
        std.hash.autoHash(&hasher, tag);
        switch (data) {
            inline else => |payload| std.hash.autoHash(&hasher, payload),
        }
        return hasher.final();
    }

    pub fn eql(self: @This(), a: Type.Data, b: Type.Data) bool {
        _ = self;
        const a_tag = std.meta.activeTag(a);
        const b_tag = std.meta.activeTag(b);
        if (a_tag != b_tag) return false;
        switch (a) {
            inline else => |a_payload, tag| {
                const b_payload = @field(b, @tagName(tag));
                return std.meta.eql(a_payload, b_payload);
            },
        }
    }
};

test "TypePool interning" {
    const testing = std.testing;
    var pool = TypePool.init(testing.allocator);
    defer pool.deinit();

    const id1 = try pool.intern(.{ .primitive = .void_type }, .copyable);
    const id2 = try pool.intern(.{ .primitive = .void_type }, .copyable);
    
    try testing.expectEqual(id1, id2);

    const int1 = try pool.intern(.{ .integer = .{ .is_signed = true, .bits = 32 } }, .copyable);
    const int2 = try pool.intern(.{ .integer = .{ .is_signed = true, .bits = 32 } }, .copyable);
    const int3 = try pool.intern(.{ .integer = .{ .is_signed = false, .bits = 32 } }, .copyable);

    try testing.expectEqual(int1, int2);
    try testing.expect(int1 != int3);
}

test "Copyability combinations" {
    const testing = std.testing;
    
    try testing.expectEqual(Copyability.combine(.copyable, .copyable), .copyable);
    try testing.expectEqual(Copyability.combine(.copyable, .explicit_nocopy), .contains_nocopy);
    try testing.expectEqual(Copyability.combine(.contains_nocopy, .explicit_nocopy), .contains_nocopy);
}
