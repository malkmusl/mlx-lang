//! Filesystem-independent import classification. Filesystem loading and graph
//! ownership are added here rather than in parser or semantic analysis.

const std = @import("std");

pub const ImportKind = enum {
    relative,
    package,
    standard,
    builtin,
};

pub const ImportSpec = struct {
    kind: ImportKind,
    path: []const u8,
};

pub const ModuleId = u32;

pub const PackageMap = std.StringHashMap([]const u8);

pub const ResolveOptions = struct {
    std_root: []const u8,
    packages: ?*const PackageMap = null,
};

pub const ResolveError = error{
    UnknownPackage,
    InvalidImport,
};

pub const ModuleState = enum {
    discovered,
    loading,
    loaded,
    failed,
};

pub const Module = struct {
    id: ModuleId,
    path: []const u8,
    state: ModuleState,
};

/// Path-only module graph. Parsing and semantic module namespaces consume this
/// graph but do not own filesystem policy themselves.
pub const ModuleGraph = struct {
    allocator: std.mem.Allocator,
    modules: std.ArrayList(Module),
    by_path: std.StringHashMap(ModuleId),

    pub fn init(allocator: std.mem.Allocator) ModuleGraph {
        return .{
            .allocator = allocator,
            .modules = .empty,
            .by_path = std.StringHashMap(ModuleId).init(allocator),
        };
    }

    pub fn deinit(self: *ModuleGraph) void {
        for (self.modules.items) |module| self.allocator.free(module.path);
        self.modules.deinit(self.allocator);
        self.by_path.deinit();
    }

    pub fn getOrAdd(self: *ModuleGraph, canonical_path: []const u8) !struct { id: ModuleId, is_new: bool } {
        if (self.by_path.get(canonical_path)) |id| return .{ .id = id, .is_new = false };
        const owned_path = try self.allocator.dupe(u8, canonical_path);
        errdefer self.allocator.free(owned_path);
        const id: ModuleId = @intCast(self.modules.items.len);
        try self.modules.append(self.allocator, .{ .id = id, .path = owned_path, .state = .discovered });
        errdefer _ = self.modules.pop();
        try self.by_path.put(owned_path, id);
        return .{ .id = id, .is_new = true };
    }

    pub fn get(self: *const ModuleGraph, id: ModuleId) ?*const Module {
        if (id >= self.modules.items.len) return null;
        return &self.modules.items[id];
    }

    pub fn setState(self: *ModuleGraph, id: ModuleId, state: ModuleState) void {
        self.modules.items[id].state = state;
    }
};

pub fn classify(path: []const u8) ImportSpec {
    if (std.mem.eql(u8, path, "builtin")) return .{ .kind = .builtin, .path = path };
    if (std.mem.startsWith(u8, path, "./") or std.mem.startsWith(u8, path, "../")) {
        return .{ .kind = .relative, .path = path };
    }
    if (std.mem.eql(u8, path, "std") or std.mem.startsWith(u8, path, "std.")) {
        return .{ .kind = .standard, .path = path };
    }
    return .{ .kind = .package, .path = path };
}

/// Resolves all normative import forms into a normalized host path. Package
/// acquisition is deliberately outside the compiler; package roots must be
/// supplied by the build layer.
pub fn resolvePath(
    allocator: std.mem.Allocator,
    importer_path: []const u8,
    import_path: []const u8,
    options: ResolveOptions,
) (std.mem.Allocator.Error || ResolveError)![]u8 {
    if (import_path.len == 0) return error.InvalidImport;
    const spec = classify(import_path);
    return switch (spec.kind) {
        .builtin => allocator.dupe(u8, "builtin"),
        .relative => blk: {
            const parent = std.fs.path.dirname(importer_path) orelse ".";
            break :blk std.fs.path.resolve(allocator, &.{ parent, import_path });
        },
        .standard => blk: {
            const relative = try standardRelativePath(allocator, import_path);
            defer allocator.free(relative);
            break :blk std.fs.path.resolve(allocator, &.{ options.std_root, relative });
        },
        .package => blk: {
            const packages = options.packages orelse return error.UnknownPackage;
            const root = packages.get(import_path) orelse return error.UnknownPackage;
            break :blk std.fs.path.resolve(allocator, &.{root});
        },
    };
}

fn standardRelativePath(allocator: std.mem.Allocator, import_path: []const u8) ![]u8 {
    if (std.mem.eql(u8, import_path, "std")) return allocator.dupe(u8, "std.zin");
    const suffix = import_path["std.".len..];
    if (suffix.len == 0) return allocator.dupe(u8, "std.zin");
    var result = try allocator.alloc(u8, suffix.len + ".zin".len);
    for (suffix, 0..) |byte, index| result[index] = if (byte == '.') std.fs.path.sep else byte;
    @memcpy(result[suffix.len..], ".zin");
    return result;
}

test "classifies normative import forms" {
    try std.testing.expectEqual(ImportKind.relative, classify("./parser.zin").kind);
    try std.testing.expectEqual(ImportKind.relative, classify("../shared.zin").kind);
    try std.testing.expectEqual(ImportKind.standard, classify("std.mem").kind);
    try std.testing.expectEqual(ImportKind.package, classify("example").kind);
    try std.testing.expectEqual(ImportKind.builtin, classify("builtin").kind);
}

test "resolves relative, standard, builtin and mapped package imports" {
    const allocator = std.testing.allocator;

    const relative = try resolvePath(allocator, "/work/compiler/main.zin", "./parser.zin", .{ .std_root = "/work/std" });
    defer allocator.free(relative);
    try std.testing.expectEqualStrings("/work/compiler/parser.zin", relative);

    const standard = try resolvePath(allocator, "/work/compiler/main.zin", "std.mem.Allocator", .{ .std_root = "/work/std" });
    defer allocator.free(standard);
    try std.testing.expectEqualStrings("/work/std/mem/Allocator.zin", standard);

    const builtin = try resolvePath(allocator, "/work/compiler/main.zin", "builtin", .{ .std_root = "/work/std" });
    defer allocator.free(builtin);
    try std.testing.expectEqualStrings("builtin", builtin);

    var packages = PackageMap.init(allocator);
    defer packages.deinit();
    try packages.put("example", "/packages/example/root.zin");
    const package = try resolvePath(allocator, "/work/compiler/main.zin", "example", .{ .std_root = "/work/std", .packages = &packages });
    defer allocator.free(package);
    try std.testing.expectEqualStrings("/packages/example/root.zin", package);
}

test "module graph deduplicates canonical paths and tracks cycles by state" {
    const allocator = std.testing.allocator;
    var graph = ModuleGraph.init(allocator);
    defer graph.deinit();

    const first = try graph.getOrAdd("/work/a.zin");
    const again = try graph.getOrAdd("/work/a.zin");
    try std.testing.expect(first.is_new);
    try std.testing.expect(!again.is_new);
    try std.testing.expectEqual(first.id, again.id);
    graph.setState(first.id, .loading);
    try std.testing.expectEqual(ModuleState.loading, graph.get(first.id).?.state);
}
