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

    /// Single-token / keyword primitive types — no parameters.
    pub const Primitive = enum {
        void_type,
        noreturn_type,
        type_type,
        anytype_type,
        anyopaque_type,
        bool_type,
        // Floating-point (all five Zig float widths)
        f16_type,
        f32_type,
        f64_type,
        f80_type,
        f128_type,
        // Comptime literals
        comptime_int_type,
        comptime_float_type,
        // null / undefined sentinel
        null_type,
        undefined_type,
    };

    /// Fixed-width signed/unsigned integer: u1/i1 through u4096/i4096.
    pub const Int = struct {
        is_signed: bool,
        bits: u16, // 1–4096
    };

    /// Platform-native pointer-width integer.
    pub const SizeInt = struct {
        is_signed: bool, // false = usize, true = isize
    };

    /// Pointer type: *T, *const T, [*]T, []T etc.
    pub const PointerSize = enum { One, Many, Slice, C };

    pub const Pointer = struct {
        child_type: Id,
        is_const: bool,
        is_volatile: bool,
        is_allowzero: bool,
        is_optional: bool,
        alignment: ?u64,
        size: PointerSize,
        sentinel: ?u64, // only used for sentinel-terminated slices/many-pointers
    };

    pub const Optional = struct {
        child_type: Id,
    };

    pub const Array = struct {
        child_type: Id,
        len: u64,
        sentinel: ?u64,
    };

    /// A complete function type including parameter types and return type.
    pub const Function = struct {
        ret_type: Id,
        params_start: u32, // index into TypePool.extra_type_ids
        params_len: u32,
        is_var_args: bool,
    };

    /// Stub for struct/enum/union — will be expanded when we implement aggregates.
    pub const Aggregate = struct {
        id: u32,
    };

    pub const Vector = struct {
        len: u32,
        child_type: Id,
    };

    pub const Tag = enum {
        primitive,
        integer,
        size_int, // usize / isize
        pointer,
        optional,
        array,
        error_union,
        function,
        @"struct",
        @"enum",
        @"union",
        tuple,
        vector,
    };

    pub const Data = union(Tag) {
        primitive: Primitive,
        integer: Int,
        size_int: SizeInt,
        pointer: Pointer,
        optional: Optional,
        array: Array,
        error_union: struct { err_set: Id, payload: Id },
        function: Function,
        @"struct": Aggregate,
        @"enum": Aggregate,
        @"union": Aggregate,
        tuple: Aggregate,
        vector: Vector,
    };

    data: Data,
    copyability: Copyability,

    pub fn isCopyable(self: Type) bool {
        return self.copyability == .copyable;
    }

    /// Returns true if this type is a comptime-only type.
    pub fn isComptime(self: Type) bool {
        return switch (self.data) {
            .primitive => |p| p == .comptime_int_type or p == .comptime_float_type or p == .type_type,
            else => false,
        };
    }

    /// Returns true if this type is a numeric integer (including comptime_int).
    pub fn isInteger(self: Type) bool {
        return switch (self.data) {
            .integer, .size_int => true,
            .primitive => |p| p == .comptime_int_type,
            else => false,
        };
    }

    /// Returns true if this type is a floating-point (including comptime_float).
    pub fn isFloat(self: Type) bool {
        return switch (self.data) {
            .primitive => |p| switch (p) {
                .f16_type, .f32_type, .f64_type, .f80_type, .f128_type, .comptime_float_type => true,
                else => false,
            },
            else => false,
        };
    }

    /// Check if `from` is coercible to `to` (Zig coercion rules, simplified).
    /// Returns true if assignment `to = from` is valid without an explicit cast.
    pub fn isCoercible(from: Type, to: Type) bool {
        // Same type — always OK
        if (std.meta.eql(from.data, to.data) and from.copyability == to.copyability) return true;

        // comptime_int → any concrete integer or float
        if (from.data == .primitive and from.data.primitive == .comptime_int_type) {
            if (to.isInteger() or to.isFloat()) return true;
        }

        // comptime_float → any concrete float
        if (from.data == .primitive and from.data.primitive == .comptime_float_type) {
            if (to.isFloat()) return true;
        }

        // null → any optional
        if (from.data == .primitive and from.data.primitive == .null_type) {
            if (to.data == .optional) return true;
        }

        // undefined → any type (runtime undefined)
        if (from.data == .primitive and from.data.primitive == .undefined_type) {
            return true;
        }

        // Optional coercion: T → ?T
        if (to.data == .optional) {
            const inner_id = to.data.optional.child_type;
            _ = inner_id; // TODO: recursive check when pool is available
            // For now allow T → ?T if T matches child
        }

        return false;
    }
};

pub const TypePool = struct {
    pub const AggregateKind = enum { @"struct", @"enum", @"union", tuple };

    pub const AggregateField = struct {
        name: []const u8,
        type_id: Type.Id,
        offset: u64,
    };

    pub const AggregateFieldInput = struct {
        name: []const u8,
        type_id: Type.Id,
    };

    pub const AggregateInfo = struct {
        kind: AggregateKind,
        fields_start: u32,
        fields_len: u32,
        backing_type: ?Type.Id,
        size: u64,
        alignment: u64,
    };

    allocator: std.mem.Allocator,
    types: std.ArrayList(Type),
    /// Flat list of Type.Id used for function parameter lists.
    extra_type_ids: std.ArrayList(Type.Id),
    aggregate_fields: std.ArrayList(AggregateField),
    aggregates: std.ArrayList(AggregateInfo),
    intern_map: std.HashMapUnmanaged(
        Type.Data,
        Type.Id,
        TypeDataHashContext,
        std.hash_map.default_max_load_percentage,
    ),

    pub fn init(allocator: std.mem.Allocator) TypePool {
        return .{
            .allocator = allocator,
            .types = std.ArrayList(Type).empty,
            .extra_type_ids = std.ArrayList(Type.Id).empty,
            .aggregate_fields = std.ArrayList(AggregateField).empty,
            .aggregates = std.ArrayList(AggregateInfo).empty,
            .intern_map = std.HashMapUnmanaged(
                Type.Data,
                Type.Id,
                TypeDataHashContext,
                std.hash_map.default_max_load_percentage,
            ).empty,
        };
    }

    pub fn deinit(self: *TypePool) void {
        self.types.deinit(self.allocator);
        self.extra_type_ids.deinit(self.allocator);
        self.aggregate_fields.deinit(self.allocator);
        self.aggregates.deinit(self.allocator);
        self.intern_map.deinit(self.allocator);
    }

    /// Intern a type — returns an existing Id if an identical type already exists.
    pub fn intern(self: *TypePool, data: Type.Data, copyability: Copyability) !Type.Id {
        if (self.intern_map.get(data)) |id| {
            if (self.types.items[id].copyability == copyability) {
                return id;
            }

            // The runtime representation is deliberately identical for
            // copyable and @nocopy types, so Type.Data alone cannot key both
            // variants in intern_map. Recover the other variant by scanning;
            // this keeps type identity stable without contaminating layout.
            for (self.types.items, 0..) |existing, existing_id| {
                if (existing.copyability == copyability and std.meta.eql(existing.data, data)) {
                    return @intCast(existing_id);
                }
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

    /// Append param type IDs for a function type; returns the start index.
    pub fn appendParams(self: *TypePool, params: []const Type.Id) !u32 {
        const start = @as(u32, @intCast(self.extra_type_ids.items.len));
        try self.extra_type_ids.appendSlice(self.allocator, params);
        return start;
    }

    pub fn functionParams(self: *const TypePool, function: Type.Function) []const Type.Id {
        const start: usize = @intCast(function.params_start);
        return self.extra_type_ids.items[start .. start + function.params_len];
    }

    /// Intern a named integer type: `u8`, `i32`, etc.
    pub fn internInt(self: *TypePool, signed: bool, bits: u16) !Type.Id {
        return self.intern(.{ .integer = .{ .is_signed = signed, .bits = bits } }, .copyable);
    }

    /// Intern `usize` or `isize`.
    pub fn internSizeInt(self: *TypePool, signed: bool) !Type.Id {
        return self.intern(.{ .size_int = .{ .is_signed = signed } }, .copyable);
    }

    /// Intern a primitive type.
    pub fn internPrimitive(self: *TypePool, prim: Type.Primitive) !Type.Id {
        return self.intern(.{ .primitive = prim }, .copyable);
    }

    /// Intern a simple single-pointer `*T` or `*const T`.
    pub fn internPtr(self: *TypePool, child: Type.Id, is_const: bool) !Type.Id {
        return self.intern(.{ .pointer = .{
            .child_type = child,
            .is_const = is_const,
            .is_volatile = false,
            .is_allowzero = false,
            .is_optional = false,
            .alignment = null,
            .size = .One,
            .sentinel = null,
        } }, .copyable);
    }

    /// Intern a slice `[]T` or `[]const T`.
    pub fn internSlice(self: *TypePool, child: Type.Id, is_const: bool) !Type.Id {
        return self.intern(.{ .pointer = .{
            .child_type = child,
            .is_const = is_const,
            .is_volatile = false,
            .is_allowzero = false,
            .is_optional = false,
            .alignment = null,
            .size = .Slice,
            .sentinel = null,
        } }, .copyable);
    }

    /// Intern `?T`.
    pub fn internOptional(self: *TypePool, child: Type.Id) !Type.Id {
        return self.intern(.{ .optional = .{ .child_type = child } }, .copyable);
    }

    /// Intern `[N]T`.
    pub fn internArray(self: *TypePool, child: Type.Id, len: u64) !Type.Id {
        return self.intern(.{ .array = .{ .child_type = child, .len = len, .sentinel = null } }, .copyable);
    }

    pub fn internAggregate(
        self: *TypePool,
        kind: AggregateKind,
        field_inputs: []const AggregateFieldInput,
        backing_type: ?Type.Id,
        explicit_copyability: ?Copyability,
    ) !Type.Id {
        const fields_start: u32 = @intCast(self.aggregate_fields.items.len);
        var size: u64 = 0;
        var alignment: u64 = 1;
        var copyability: Copyability = explicit_copyability orelse .copyable;

        for (field_inputs) |field| {
            const field_alignment = try self.alignOf(field.type_id);
            const field_size = try self.sizeOf(field.type_id);
            alignment = @max(alignment, field_alignment);
            const offset = switch (kind) {
                .@"struct", .tuple => blk: {
                    size = alignForward(size, field_alignment);
                    break :blk size;
                },
                .@"union", .@"enum" => 0,
            };
            try self.aggregate_fields.append(self.allocator, .{
                .name = field.name,
                .type_id = field.type_id,
                .offset = offset,
            });
            switch (kind) {
                .@"struct", .tuple => size = try std.math.add(u64, size, field_size),
                .@"union" => size = @max(size, field_size),
                .@"enum" => {},
            }
            if (explicit_copyability == null) {
                copyability = Copyability.combine(copyability, self.get(field.type_id).copyability);
            }
        }

        if (kind == .@"enum") {
            const backing = backing_type orelse return error.UnknownLayout;
            size = try self.sizeOf(backing);
            alignment = try self.alignOf(backing);
        } else if (kind == .@"union" and backing_type != null) {
            // A tagged union stores its discriminator followed by one
            // naturally aligned payload area shared by all variants.
            const tag_size = try self.sizeOf(backing_type.?);
            const tag_alignment = try self.alignOf(backing_type.?);
            const payload_alignment = alignment;
            const payload_offset = alignForward(tag_size, payload_alignment);
            const fields_begin: usize = @intCast(fields_start);
            const fields_end = fields_begin + field_inputs.len;
            for (self.aggregate_fields.items[fields_begin..fields_end]) |*field| {
                field.offset = payload_offset;
            }
            alignment = @max(tag_alignment, payload_alignment);
            size = alignForward(try std.math.add(u64, payload_offset, size), alignment);
        } else {
            size = alignForward(size, alignment);
        }

        const aggregate_id: u32 = @intCast(self.aggregates.items.len);
        try self.aggregates.append(self.allocator, .{
            .kind = kind,
            .fields_start = fields_start,
            .fields_len = @intCast(field_inputs.len),
            .backing_type = backing_type,
            .size = size,
            .alignment = alignment,
        });
        const aggregate = Type.Aggregate{ .id = aggregate_id };
        const data: Type.Data = switch (kind) {
            .@"struct" => .{ .@"struct" = aggregate },
            .@"enum" => .{ .@"enum" = aggregate },
            .@"union" => .{ .@"union" = aggregate },
            .tuple => .{ .tuple = aggregate },
        };
        return self.intern(data, copyability);
    }

    pub fn aggregateInfo(self: *const TypePool, type_id: Type.Id) ?AggregateInfo {
        const aggregate_id = switch (self.get(type_id).data) {
            .@"struct" => |aggregate| aggregate.id,
            .@"enum" => |aggregate| aggregate.id,
            .@"union" => |aggregate| aggregate.id,
            .tuple => |aggregate| aggregate.id,
            else => return null,
        };
        return self.aggregates.items[aggregate_id];
    }

    pub fn aggregateFields(self: *const TypePool, type_id: Type.Id) ?[]const AggregateField {
        const info = self.aggregateInfo(type_id) orelse return null;
        return self.aggregate_fields.items[info.fields_start .. info.fields_start + info.fields_len];
    }

    pub fn aggregateField(self: *const TypePool, type_id: Type.Id, name: []const u8) ?AggregateField {
        const fields = self.aggregateFields(type_id) orelse return null;
        for (fields) |field| {
            if (std.mem.eql(u8, field.name, name)) return field;
        }
        return null;
    }

    /// Print a human-readable type name.
    pub fn typeName(self: *const TypePool, id: Type.Id, buf: []u8) ![]const u8 {
        const ty = self.types.items[id];
        return switch (ty.data) {
            .primitive => |p| switch (p) {
                .void_type => "void",
                .noreturn_type => "noreturn",
                .bool_type => "bool",
                .type_type => "type",
                .anytype_type => "anytype",
                .anyopaque_type => "anyopaque",
                .f16_type => "f16",
                .f32_type => "f32",
                .f64_type => "f64",
                .f80_type => "f80",
                .f128_type => "f128",
                .comptime_int_type => "comptime_int",
                .comptime_float_type => "comptime_float",
                .null_type => "@TypeOf(null)",
                .undefined_type => "@TypeOf(undefined)",
            },
            .integer => |i| try std.fmt.bufPrint(buf, "{s}{d}", .{ if (i.is_signed) "i" else "u", i.bits }),
            .size_int => |s| if (s.is_signed) "isize" else "usize",
            .optional => |o| blk: {
                var inner_buf: [64]u8 = undefined;
                const inner = try self.typeName(o.child_type, &inner_buf);
                break :blk try std.fmt.bufPrint(buf, "?{s}", .{inner});
            },
            .pointer => |p| blk: {
                var inner_buf: [64]u8 = undefined;
                const inner = try self.typeName(p.child_type, &inner_buf);
                const prefix: []const u8 = switch (p.size) {
                    .One => "*",
                    .Many => "[*]",
                    .Slice => "[]",
                    .C => "[*c]",
                };
                if (p.alignment) |alignment| {
                    break :blk try std.fmt.bufPrint(buf, "{s}align({d}) {s}{s}{s}", .{
                        prefix,
                        alignment,
                        if (p.is_volatile) "volatile " else "",
                        if (p.is_const) "const " else "",
                        inner,
                    });
                }
                break :blk try std.fmt.bufPrint(buf, "{s}{s}{s}{s}", .{
                    prefix,
                    if (p.is_volatile) "volatile " else "",
                    if (p.is_const) "const " else "",
                    inner,
                });
            },
            .array => |a| blk: {
                var inner_buf: [64]u8 = undefined;
                const inner = try self.typeName(a.child_type, &inner_buf);
                break :blk try std.fmt.bufPrint(buf, "[{d}]{s}", .{ a.len, inner });
            },
            .vector => |v| blk: {
                var inner_buf: [64]u8 = undefined;
                const inner = try self.typeName(v.child_type, &inner_buf);
                break :blk try std.fmt.bufPrint(buf, "@Vector({d},{s})", .{ v.len, inner });
            },
            .@"struct" => "struct",
            .@"enum" => "enum",
            .@"union" => "union",
            .tuple => "tuple",
            else => try std.fmt.bufPrint(buf, "<type:{d}>", .{id}),
        };
    }

    pub fn bitSizeOf(self: *const TypePool, id: Type.Id) error{ UnknownLayout, Overflow }!u64 {
        const ty = self.get(id);
        return switch (ty.data) {
            .integer => |int| int.bits,
            .size_int => 64,
            .pointer => |ptr| if (ptr.size == .Slice) 128 else 64,
            .optional => |optional| blk: {
                const child = self.get(optional.child_type);
                if (child.data == .pointer) break :blk try self.bitSizeOf(optional.child_type);
                return error.UnknownLayout;
            },
            .array => |array| std.math.mul(u64, array.len, try self.bitSizeOf(array.child_type)) catch error.Overflow,
            .vector => |vector| std.math.mul(u64, vector.len, try self.bitSizeOf(vector.child_type)) catch error.Overflow,
            .@"struct", .@"enum", .@"union", .tuple => blk: {
                const info = self.aggregateInfo(id) orelse return error.UnknownLayout;
                break :blk std.math.mul(u64, info.size, 8) catch error.Overflow;
            },
            .primitive => |primitive| switch (primitive) {
                .void_type, .noreturn_type => 0,
                .bool_type => 8,
                .f16_type => 16,
                .f32_type => 32,
                .f64_type => 64,
                .f80_type => 80,
                .f128_type => 128,
                else => error.UnknownLayout,
            },
            else => error.UnknownLayout,
        };
    }

    pub fn sizeOf(self: *const TypePool, id: Type.Id) error{ UnknownLayout, Overflow }!u64 {
        const bits = try self.bitSizeOf(id);
        return std.math.divCeil(u64, bits, 8) catch error.Overflow;
    }

    pub fn alignOf(self: *const TypePool, id: Type.Id) error{UnknownLayout}!u64 {
        const ty = self.get(id);
        return switch (ty.data) {
            .primitive => |primitive| switch (primitive) {
                .void_type, .noreturn_type => 1,
                .bool_type => 1,
                .f16_type => 2,
                .f32_type => 4,
                .f64_type, .f80_type, .f128_type => 8,
                else => error.UnknownLayout,
            },
            .integer => |int| @min(@as(u64, 8), std.math.divCeil(u64, int.bits, 8) catch return error.UnknownLayout),
            .size_int, .pointer => 8,
            .optional => |optional| self.alignOf(optional.child_type),
            .array => |array| self.alignOf(array.child_type),
            .vector => @min(@as(u64, 16), self.sizeOf(id) catch return error.UnknownLayout),
            .@"struct", .@"enum", .@"union", .tuple => (self.aggregateInfo(id) orelse return error.UnknownLayout).alignment,
            else => error.UnknownLayout,
        };
    }
};

fn alignForward(value: u64, alignment: u64) u64 {
    if (alignment <= 1) return value;
    return (value + alignment - 1) & ~(alignment - 1);
}

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

// ──────────────────────────────────────────
//  Tests
// ──────────────────────────────────────────

test "TypePool interning" {
    const testing = std.testing;
    var pool = TypePool.init(testing.allocator);
    defer pool.deinit();

    const id1 = try pool.intern(.{ .primitive = .void_type }, .copyable);
    const id2 = try pool.intern(.{ .primitive = .void_type }, .copyable);
    try testing.expectEqual(id1, id2);

    const int1 = try pool.internInt(true, 32);
    const int2 = try pool.internInt(true, 32);
    const int3 = try pool.internInt(false, 32);
    try testing.expectEqual(int1, int2);
    try testing.expect(int1 != int3);
}

test "Float types" {
    const testing = std.testing;
    var pool = TypePool.init(testing.allocator);
    defer pool.deinit();

    const f16_id = try pool.internPrimitive(.f16_type);
    const f32_id = try pool.internPrimitive(.f32_type);
    const f64_id = try pool.internPrimitive(.f64_type);
    const f80_id = try pool.internPrimitive(.f80_type);
    const f128_id = try pool.internPrimitive(.f128_type);

    try testing.expect(f16_id != f32_id);
    try testing.expect(f32_id != f64_id);
    try testing.expect(f64_id != f80_id);
    try testing.expect(f80_id != f128_id);

    const ty = pool.get(f64_id);
    try testing.expect(ty.isFloat());
}

test "usize/isize" {
    const testing = std.testing;
    var pool = TypePool.init(testing.allocator);
    defer pool.deinit();

    const usize_id = try pool.internSizeInt(false);
    const isize_id = try pool.internSizeInt(true);
    try testing.expect(usize_id != isize_id);
    try testing.expect(pool.get(usize_id).isInteger());
    try testing.expect(pool.get(isize_id).isInteger());
}

test "coercibility" {
    const testing = std.testing;
    var pool = TypePool.init(testing.allocator);
    defer pool.deinit();

    const comptime_int_id = try pool.internPrimitive(.comptime_int_type);
    const i32_id = try pool.internInt(true, 32);
    const f64_id = try pool.internPrimitive(.f64_type);

    const cti = pool.get(comptime_int_id);
    const i32_ty = pool.get(i32_id);
    const f64_ty = pool.get(f64_id);

    try testing.expect(Type.isCoercible(cti, i32_ty)); // comptime_int → i32 ✅
    try testing.expect(Type.isCoercible(cti, f64_ty)); // comptime_int → f64 ✅
    try testing.expect(!Type.isCoercible(f64_ty, i32_ty)); // f64 → i32 ❌
    try testing.expect(Type.isCoercible(i32_ty, i32_ty)); // i32 → i32 ✅
}

test "typeName formatting" {
    const testing = std.testing;
    var pool = TypePool.init(testing.allocator);
    defer pool.deinit();

    var buf: [64]u8 = undefined;
    const i32_id = try pool.internInt(true, 32);
    const name = try pool.typeName(i32_id, &buf);
    try testing.expectEqualStrings("i32", name);

    const u64_id = try pool.internInt(false, 64);
    const uname = try pool.typeName(u64_id, &buf);
    try testing.expectEqualStrings("u64", uname);

    const usize_id = try pool.internSizeInt(false);
    const sname = try pool.typeName(usize_id, &buf);
    try testing.expectEqualStrings("usize", sname);
}

test "Copyability combinations" {
    const testing = std.testing;
    try testing.expectEqual(Copyability.combine(.copyable, .copyable), .copyable);
    try testing.expectEqual(Copyability.combine(.copyable, .explicit_nocopy), .contains_nocopy);
    try testing.expectEqual(Copyability.combine(.contains_nocopy, .explicit_nocopy), .contains_nocopy);
}

test "copyable and nocopy type variants intern independently" {
    const testing = std.testing;
    var pool = TypePool.init(testing.allocator);
    defer pool.deinit();

    const copyable = try pool.internInt(false, 8);
    const nocopy = try pool.intern(pool.get(copyable).data, .explicit_nocopy);
    try testing.expect(copyable != nocopy);
    try testing.expectEqual(copyable, try pool.internInt(false, 8));
    try testing.expectEqual(nocopy, try pool.intern(pool.get(copyable).data, .explicit_nocopy));
}

test "tagged union layout includes discriminator and aligned payload" {
    const testing = std.testing;
    var pool = TypePool.init(testing.allocator);
    defer pool.deinit();

    const u8_type = try pool.internInt(false, 8);
    const u32_type = try pool.internInt(false, 32);
    const void_type = try pool.internPrimitive(.void_type);
    const tagged = try pool.internAggregate(.@"union", &.{
        .{ .name = "none", .type_id = void_type },
        .{ .name = "data", .type_id = u32_type },
    }, u8_type, null);

    try testing.expectEqual(@as(u64, 8), try pool.sizeOf(tagged));
    try testing.expectEqual(@as(u64, 4), try pool.alignOf(tagged));
    try testing.expectEqual(@as(u64, 4), pool.aggregateField(tagged, "none").?.offset);
    try testing.expectEqual(@as(u64, 4), pool.aggregateField(tagged, "data").?.offset);
}
