//! Recursive source-module loading. This module owns filesystem traversal and
//! parsing; semantic namespaces consume its stable module/import identities.

const std = @import("std");
const Ast = @import("../syntax/ast.zig").Ast;
const Node = @import("../syntax/ast.zig").Node;
const Token = @import("../syntax/token.zig").Token;
const Lexer = @import("../syntax/lexer.zig").Lexer;
const Parser = @import("../syntax/parser.zig").Parser;
const SourceManager = @import("../source/source_manager.zig").SourceManager;
const DiagnosticEngine = @import("../source/diagnostics.zig").DiagnosticEngine;
const resolver = @import("resolver.zig");

pub const LoadedModule = struct {
    source_id: u32,
    ast: ?Ast,
    imports: std.AutoHashMap(Node.Index, resolver.ModuleId),

    fn deinit(self: *LoadedModule, allocator: std.mem.Allocator) void {
        if (self.ast) |*tree| tree.deinit(allocator);
        self.imports.deinit();
    }
};

pub const Loader = struct {
    allocator: std.mem.Allocator,
    io: std.Io,
    source_manager: *SourceManager,
    diagnostics: *DiagnosticEngine,
    graph: resolver.ModuleGraph,
    modules: std.ArrayList(LoadedModule),
    options: resolver.ResolveOptions,

    pub fn init(
        allocator: std.mem.Allocator,
        io: std.Io,
        source_manager: *SourceManager,
        diagnostics: *DiagnosticEngine,
        options: resolver.ResolveOptions,
    ) Loader {
        return .{
            .allocator = allocator,
            .io = io,
            .source_manager = source_manager,
            .diagnostics = diagnostics,
            .graph = resolver.ModuleGraph.init(allocator),
            .modules = .empty,
            .options = options,
        };
    }

    pub fn deinit(self: *Loader) void {
        for (self.modules.items) |*module| module.deinit(self.allocator);
        self.modules.deinit(self.allocator);
        self.graph.deinit();
    }

    pub fn loadRoot(self: *Loader, path: []const u8) !resolver.ModuleId {
        const cwd = try std.process.currentPathAlloc(self.io, self.allocator);
        defer self.allocator.free(cwd);
        const canonical = try std.fs.path.resolve(self.allocator, &.{ cwd, path });
        defer self.allocator.free(canonical);
        return self.loadCanonical(canonical, null, 0, null);
    }

    pub fn loadRootMemory(self: *Loader, path: []const u8, text: []const u8) !resolver.ModuleId {
        const cwd = try std.process.currentPathAlloc(self.io, self.allocator);
        defer self.allocator.free(cwd);
        // LSP passes absolute file paths but we resolve just in case
        const canonical = try std.fs.path.resolve(self.allocator, &.{ cwd, path });
        defer self.allocator.free(canonical);
        return self.loadCanonical(canonical, null, 0, text);
    }


    pub fn get(self: *Loader, id: resolver.ModuleId) *LoadedModule {
        return &self.modules.items[id];
    }

    fn loadCanonical(
        self: *Loader,
        canonical_path: []const u8,
        importer_source: ?u32,
        import_start: u32,
        memory_text: ?[]const u8,
    ) !resolver.ModuleId {
        const entry = try self.graph.getOrAdd(canonical_path);
        if (!entry.is_new) return entry.id;

        try self.modules.append(self.allocator, .{
            .source_id = std.math.maxInt(u32),
            .ast = null,
            .imports = std.AutoHashMap(Node.Index, resolver.ModuleId).init(self.allocator),
        });
        self.graph.setState(entry.id, .loading);

        if (std.mem.eql(u8, canonical_path, "builtin")) {
            self.graph.setState(entry.id, .loaded);
            return entry.id;
        }

        const bytes = if (memory_text) |text|
            try self.allocator.dupe(u8, text)
        else
            std.Io.Dir.readFileAlloc(.cwd(), self.io, canonical_path, self.allocator, @enumFromInt(10 * 1024 * 1024)) catch {
                self.graph.setState(entry.id, .failed);
                if (importer_source) |source_id| try self.reportImportNotFound(source_id, import_start);
                return entry.id;
            };
        defer self.allocator.free(bytes);

        const source_id = try self.source_manager.addFile(canonical_path, bytes);
        self.modules.items[entry.id].source_id = source_id;
        var terminated = try self.allocator.alloc(u8, bytes.len + 1);
        defer self.allocator.free(terminated);
        @memcpy(terminated[0..bytes.len], bytes);
        terminated[bytes.len] = 0;

        var lexer = Lexer.init(terminated[0..bytes.len :0]);
        var tokens = std.ArrayList(Token).empty;
        defer tokens.deinit(self.allocator);
        while (true) {
            const token = lexer.next();
            try tokens.append(self.allocator, token);
            if (token.tag == .eof) break;
        }

        var parser = Parser.init(self.allocator, tokens.items, self.diagnostics, source_id);
        const tree = try parser.parse();
        self.modules.items[entry.id].ast = tree;

        const import_nodes = try collectImports(self.allocator, &tree, bytes);
        defer self.allocator.free(import_nodes);
        for (import_nodes) |import_node| {
            const imported_path = stringLiteralContent(&tree, bytes, import_node.string_node) orelse continue;
            const resolved_path = resolver.resolvePath(self.allocator, canonical_path, imported_path, self.options) catch {
                try self.reportImportNotFound(source_id, tree.tokens[tree.nodes.get(import_node.builtin_node).main_token].start);
                continue;
            };
            defer self.allocator.free(resolved_path);
            const imported_id = try self.loadCanonical(
                resolved_path,
                source_id,
                tree.tokens[tree.nodes.get(import_node.builtin_node).main_token].start,
                null,
            );
            try self.modules.items[entry.id].imports.put(import_node.builtin_node, imported_id);
        }

        self.graph.setState(entry.id, .loaded);
        return entry.id;
    }

    fn reportImportNotFound(self: *Loader, source_id: u32, start: u32) !void {
        try self.diagnostics.report(.{
            .code = 3004,
            .phase = .resolve,
            .severity = .@"error",
            .primary_span = .{ .file_id = source_id, .start_byte = start, .end_byte = start + 1 },
            .message = "Imported module could not be resolved",
        });
    }
};

const ImportNode = struct {
    builtin_node: Node.Index,
    string_node: Node.Index,
};

fn collectImports(allocator: std.mem.Allocator, tree: *const Ast, source: []const u8) ![]ImportNode {
    var imports = std.ArrayList(ImportNode).empty;
    defer imports.deinit(allocator);
    var index: Node.Index = 0;
    while (index < tree.nodes.len) : (index += 1) {
        const node = tree.nodes.get(index);
        if (node.tag != .builtin_call) continue;
        const name_token = tree.tokens[node.data.lhs];
        if (!std.mem.eql(u8, source[name_token.start..name_token.end], "import")) continue;
        const count = tree.extra_data[node.data.rhs];
        if (count != 1) continue;
        const string_node = tree.extra_data[node.data.rhs + 1];
        if (tree.nodes.get(string_node).tag != .string_literal) continue;
        try imports.append(allocator, .{ .builtin_node = index, .string_node = string_node });
    }
    return imports.toOwnedSlice(allocator);
}

fn stringLiteralContent(tree: *const Ast, source: []const u8, node_index: Node.Index) ?[]const u8 {
    const node = tree.nodes.get(node_index);
    if (node.tag != .string_literal) return null;
    const token = tree.tokens[node.main_token];
    const text = source[token.start..token.end];
    if (text.len < 2 or text[0] != '"' or text[text.len - 1] != '"') return null;
    return text[1 .. text.len - 1];
}
