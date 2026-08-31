//! Semantic module namespaces shared between independently analyzed source
//! files. Filesystem loading stays in loader.zig.

const std = @import("std");
const Type = @import("../semantic/type.zig").Type;
const ModuleId = @import("resolver.zig").ModuleId;

pub const Export = struct {
    type_id: Type.Id,
    public: bool,
    const_value: ?u64 = null,
    type_value: ?Type.Id = null,
    module_value: ?ModuleId = null,
    is_function: bool = false,
};

pub const Namespace = struct {
    declarations: std.StringHashMap(Export),

    pub fn init(allocator: std.mem.Allocator) Namespace {
        return .{ .declarations = std.StringHashMap(Export).init(allocator) };
    }

    pub fn deinit(self: *Namespace) void {
        self.declarations.deinit();
    }
};

pub const Registry = struct {
    allocator: std.mem.Allocator,
    namespaces: std.ArrayList(Namespace),

    pub fn init(allocator: std.mem.Allocator, module_count: usize) !Registry {
        var result = Registry{ .allocator = allocator, .namespaces = .empty };
        errdefer result.deinit();
        try result.namespaces.ensureTotalCapacity(allocator, module_count);
        for (0..module_count) |_| result.namespaces.appendAssumeCapacity(Namespace.init(allocator));
        return result;
    }

    pub fn deinit(self: *Registry) void {
        for (self.namespaces.items) |*namespace| namespace.deinit();
        self.namespaces.deinit(self.allocator);
    }

    pub fn put(self: *Registry, module_id: ModuleId, name: []const u8, value: Export) !void {
        try self.namespaces.items[module_id].declarations.put(name, value);
    }

    pub fn get(self: *const Registry, module_id: ModuleId, name: []const u8) ?Export {
        if (module_id >= self.namespaces.items.len) return null;
        return self.namespaces.items[module_id].declarations.get(name);
    }
};
