const std = @import("std");
const ast = @import("ast.zig");
const Type = @import("type.zig").Type;

pub const Scope = struct {
    pub const Symbol = struct {
        name: []const u8,
        decl_node: ast.Node.Index,
        type_id: Type.Id,
        is_const: bool,
    };

    allocator: std.mem.Allocator,
    parent: ?*Scope,
    symbols: std.StringHashMap(Symbol),

    pub fn init(allocator: std.mem.Allocator, parent: ?*Scope) Scope {
        return .{
            .allocator = allocator,
            .parent = parent,
            .symbols = std.StringHashMap(Symbol).init(allocator),
        };
    }

    pub fn deinit(self: *Scope) void {
        self.symbols.deinit();
    }

    pub fn put(self: *Scope, name: []const u8, sym: Symbol) !void {
        try self.symbols.put(name, sym);
    }

    pub fn get(self: *Scope, name: []const u8) ?Symbol {
        if (self.symbols.get(name)) |sym| {
            return sym;
        }
        if (self.parent) |p| {
            return p.get(name);
        }
        return null;
    }
};
