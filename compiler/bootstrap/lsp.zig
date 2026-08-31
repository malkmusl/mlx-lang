//! Language Server Protocol support for Zin.
//! The server reuses zin0's lexer, parser and semantic analyser.
const std = @import("std");
const Lexer = @import("syntax/lexer.zig").Lexer;
const Token = @import("syntax/token.zig").Token;
const Parser = @import("syntax/parser.zig").Parser;
const Ast = @import("syntax/ast.zig").Ast;
const Node = @import("syntax/ast.zig").Node;
const SourceManager = @import("source/source_manager.zig").SourceManager;
const diagnostics = @import("source/diagnostics.zig");
const DiagnosticEngine = diagnostics.DiagnosticEngine;
const Severity = diagnostics.Severity;
const TypePool = @import("semantic/type.zig").TypePool;
const Scope = @import("semantic/scope.zig").Scope;
const Sema = @import("semantic/sema.zig").Sema;

const Position = struct { line: usize, character: usize };

const Document = struct {
    allocator: std.mem.Allocator,
    uri: []const u8,
    text: [:0]u8,
    sources: SourceManager,
    engine: DiagnosticEngine,
    ast: ?Ast = null,
    types: TypePool,
    root_scope: Scope,
    sema: ?*Sema = null,
    invalid_tokens: std.ArrayList(Token) = .empty,

    fn init(allocator: std.mem.Allocator, uri: []const u8, text: []const u8) !*Document {
        const document = try allocator.create(Document);
        errdefer allocator.destroy(document);
        document.allocator = allocator;
        document.ast = null;
        document.sema = null;
        document.invalid_tokens = .empty;
        document.uri = try allocator.dupe(u8, uri);
        errdefer allocator.free(document.uri);
        document.text = try allocator.dupeZ(u8, text);
        errdefer allocator.free(document.text);
        document.sources = SourceManager.init(allocator);
        document.engine = DiagnosticEngine.init(allocator, &document.sources);
        document.types = TypePool.init(allocator);
        document.root_scope = Scope.init(allocator, null);
        return document;
    }

    fn deinit(self: *Document) void {
        if (self.sema) |sema| {
            sema.deinit();
            self.allocator.destroy(sema);
        }
        self.root_scope.deinit();
        self.types.deinit();
        if (self.ast) |*ast| ast.deinit(self.allocator);
        self.invalid_tokens.deinit(self.allocator);
        self.engine.deinit();
        self.sources.deinit();
        self.allocator.free(self.text);
        self.allocator.free(self.uri);
        self.allocator.destroy(self);
    }

    fn compile(self: *Document) !void {
        const file_id = try self.sources.addFile(self.uri, self.text);
        var lexer = Lexer.init(self.text);
        var tokens = std.ArrayList(Token).empty;
        defer tokens.deinit(self.allocator);
        while (true) {
            const token = lexer.next();
            try tokens.append(self.allocator, token);
            if (token.tag == .invalid) try self.invalid_tokens.append(self.allocator, token);
            if (token.tag == .eof) break;
        }

        var parser = Parser.init(self.allocator, tokens.items, &self.engine, file_id);
        self.ast = try parser.parse();
        if (self.engine.error_count != 0) return;

        const sema = try self.allocator.create(Sema);
        errdefer self.allocator.destroy(sema);
        sema.* = Sema.init(self.allocator, self.ast.?, &self.engine, &self.types, &self.root_scope);
        sema.analyze() catch {};
        self.sema = sema;
    }
};

const Server = struct {
    allocator: std.mem.Allocator,
    documents: std.StringHashMap(*Document),

    fn init(allocator: std.mem.Allocator) Server {
        return .{ .allocator = allocator, .documents = std.StringHashMap(*Document).init(allocator) };
    }

    fn deinit(self: *Server) void {
        var values = self.documents.valueIterator();
        while (values.next()) |document| document.*.deinit();
        self.documents.deinit();
    }

    fn replaceDocument(self: *Server, uri: []const u8, text: []const u8) !*Document {
        if (self.documents.fetchRemove(uri)) |entry| entry.value.deinit();
        const document = try Document.init(self.allocator, uri, text);
        errdefer document.deinit();
        try document.compile();
        try self.documents.put(document.uri, document);
        return document;
    }

    fn closeDocument(self: *Server, uri: []const u8) void {
        if (self.documents.fetchRemove(uri)) |entry| entry.value.deinit();
    }
};

pub fn main(init: std.process.Init) !u8 {
    const allocator = init.gpa;
    var server = Server.init(allocator);
    defer server.deinit();
    var stdout_storage: [1024]u8 = undefined;
    var stdout = std.Io.File.Writer.init(.stdout(), init.io, &stdout_storage);
    var input = std.ArrayList(u8).empty;
    defer input.deinit(allocator);
    var buffer: [8192]u8 = undefined;

    while (true) {
        const count = std.posix.read(0, &buffer) catch return 1;
        if (count == 0) break;
        try input.appendSlice(allocator, buffer[0..count]);
        while (nextMessage(input.items)) |message| {
            const result = handle(&server, message) catch |err| try internalError(allocator, err);
            defer allocator.free(result);
            try send(&stdout.interface, result);
            consumeMessage(&input, message.len);
        }
    }
    return 0;
}

fn nextMessage(input: []const u8) ?[]const u8 {
    const separator = std.mem.indexOf(u8, input, "\r\n\r\n") orelse return null;
    const header = input[0..separator];
    const index = std.mem.indexOf(u8, header, "Content-Length:") orelse return null;
    const length_start = index + "Content-Length:".len;
    const length = std.fmt.parseInt(usize, std.mem.trim(u8, header[length_start..], " \t\r\n"), 10) catch return null;
    const body_start = separator + 4;
    if (input.len < body_start + length) return null;
    return input[body_start .. body_start + length];
}

fn consumeMessage(input: *std.ArrayList(u8), body_len: usize) void {
    const separator = std.mem.indexOf(u8, input.items, "\r\n\r\n").?;
    const len = separator + 4 + body_len;
    std.mem.copyForwards(u8, input.items[0 .. input.items.len - len], input.items[len..]);
    input.items.len -= len;
}

fn handle(server: *Server, input: []const u8) ![]u8 {
    var parsed = try std.json.parseFromSlice(std.json.Value, server.allocator, input, .{});
    defer parsed.deinit();
    const request = parsed.value;
    const method = stringValue(field(request, "method")) orelse return error.InvalidRequest;
    const id = try jsonId(server.allocator, field(request, "id"));
    defer server.allocator.free(id);
    const params = field(request, "params") orelse .null;

    if (std.mem.eql(u8, method, "initialize")) return response(server.allocator, id,
        \\{"capabilities":{"textDocumentSync":{"openClose":true,"change":1},"completionProvider":{"triggerCharacters":["@","."]},"definitionProvider":true,"hoverProvider":true,"documentSymbolProvider":true}}
    );
    if (std.mem.eql(u8, method, "shutdown")) return response(server.allocator, id, "null");
    if (std.mem.eql(u8, method, "exit")) return server.allocator.dupe(u8, "");

    if (std.mem.eql(u8, method, "textDocument/didOpen")) {
        const text_document = field(params, "textDocument") orelse return error.InvalidRequest;
        const uri = stringValue(field(text_document, "uri")) orelse return error.InvalidRequest;
        const text = stringValue(field(text_document, "text")) orelse return error.InvalidRequest;
        return publishDiagnostics(server.allocator, try server.replaceDocument(uri, text));
    }
    if (std.mem.eql(u8, method, "textDocument/didChange")) {
        const text_document = field(params, "textDocument") orelse return error.InvalidRequest;
        const uri = stringValue(field(text_document, "uri")) orelse return error.InvalidRequest;
        const changes = field(params, "contentChanges") orelse return error.InvalidRequest;
        const change = switch (changes) {
            .array => |items| if (items.items.len != 0) items.items[items.items.len - 1] else return error.InvalidRequest,
            else => return error.InvalidRequest,
        };
        const text = stringValue(field(change, "text")) orelse return error.InvalidRequest;
        return publishDiagnostics(server.allocator, try server.replaceDocument(uri, text));
    }
    if (std.mem.eql(u8, method, "textDocument/didClose")) {
        const text_document = field(params, "textDocument") orelse return error.InvalidRequest;
        const uri = stringValue(field(text_document, "uri")) orelse return error.InvalidRequest;
        server.closeDocument(uri);
        return emptyDiagnostics(server.allocator, uri);
    }
    if (std.mem.eql(u8, method, "textDocument/completion")) return completion(server, id, params);
    if (std.mem.eql(u8, method, "textDocument/documentSymbol")) return documentSymbols(server, id, params);
    if (std.mem.eql(u8, method, "textDocument/definition")) return definition(server, id, params);
    if (std.mem.eql(u8, method, "textDocument/hover")) return hover(server, id, params);
    if (std.mem.eql(u8, method, "zin/ast")) return response(server.allocator, id, "null");
    return server.allocator.dupe(u8, "");
}

fn field(value: std.json.Value, name: []const u8) ?std.json.Value {
    return switch (value) { .object => |object| object.get(name), else => null };
}

fn stringValue(value: ?std.json.Value) ?[]const u8 {
    const item = value orelse return null;
    return switch (item) { .string => |text| text, else => null };
}

fn position(params: std.json.Value) ?Position {
    const value = field(params, "position") orelse return null;
    const line = field(value, "line") orelse return null;
    const character = field(value, "character") orelse return null;
    return .{
        .line = switch (line) { .integer => |n| if (n >= 0) @intCast(n) else return null, else => return null },
        .character = switch (character) { .integer => |n| if (n >= 0) @intCast(n) else return null, else => return null },
    };
}

fn documentForParams(server: *Server, params: std.json.Value) ?*Document {
    const text_document = field(params, "textDocument") orelse return null;
    const uri = stringValue(field(text_document, "uri")) orelse return null;
    return server.documents.get(uri);
}

fn jsonId(allocator: std.mem.Allocator, value: ?std.json.Value) ![]u8 {
    const id = value orelse return allocator.dupe(u8, "null");
    return switch (id) {
        .integer => |number| std.fmt.allocPrint(allocator, "{d}", .{number}),
        .string => |text| jsonString(allocator, text),
        .null => allocator.dupe(u8, "null"),
        else => allocator.dupe(u8, "null"),
    };
}

fn response(allocator: std.mem.Allocator, id: []const u8, result: []const u8) ![]u8 {
    return std.fmt.allocPrint(allocator, "{{\"jsonrpc\":\"2.0\",\"id\":{s},\"result\":{s}}}", .{ id, result });
}

fn internalError(allocator: std.mem.Allocator, err: anyerror) ![]u8 {
    const message = try jsonString(allocator, @errorName(err));
    defer allocator.free(message);
    return std.fmt.allocPrint(allocator, "{{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{{\"code\":-32603,\"message\":{s}}}}}", .{message});
}

fn publishDiagnostics(allocator: std.mem.Allocator, document: *const Document) ![]u8 {
    var out = std.ArrayList(u8).empty;
    defer out.deinit(allocator);
    try out.appendSlice(allocator, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":");
    try appendJsonString(&out, allocator, document.uri);
    try out.appendSlice(allocator, ",\"diagnostics\":[");
    var first = true;
    for (document.invalid_tokens.items) |token| {
        try appendDiagnostic(&out, allocator, document.text, token.start, token.end, .@"error", 1000, "Invalid token", &first);
    }
    for (document.engine.diagnostics.items) |diagnostic| {
        try appendDiagnostic(&out, allocator, document.text, diagnostic.primary_span.start_byte, diagnostic.primary_span.end_byte, diagnostic.severity, diagnostic.code, diagnostic.message, &first);
    }
    try out.appendSlice(allocator, "]}}");
    return out.toOwnedSlice(allocator);
}

fn emptyDiagnostics(allocator: std.mem.Allocator, uri: []const u8) ![]u8 {
    var out = std.ArrayList(u8).empty;
    defer out.deinit(allocator);
    try out.appendSlice(allocator, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":");
    try appendJsonString(&out, allocator, uri);
    try out.appendSlice(allocator, ",\"diagnostics\":[]}}");
    return out.toOwnedSlice(allocator);
}

fn appendDiagnostic(out: *std.ArrayList(u8), allocator: std.mem.Allocator, text: []const u8, start: u32, end: u32, severity: Severity, code: u32, message: []const u8, first: *bool) !void {
    if (!first.*) try out.append(allocator, ',');
    first.* = false;
    const start_pos = offsetToPosition(text, start);
    const end_pos = offsetToPosition(text, @max(start, end));
    const level: u8 = switch (severity) { .@"error" => 1, .warning => 2, .note => 3, .help => 4 };
    const prefix: u8 = if (severity == .warning) 'W' else 'E';
    try appendFormat(out, allocator, "{{\"range\":{{\"start\":{{\"line\":{d},\"character\":{d}}},\"end\":{{\"line\":{d},\"character\":{d}}}}},\"severity\":{d},\"code\":\"ZIN-{c}{d:0>4}\",\"source\":\"zin\",\"message\":", .{ start_pos.line, start_pos.character, end_pos.line, end_pos.character, level, prefix, code });
    try appendJsonString(out, allocator, message);
    try out.append(allocator, '}');
}

fn completion(server: *Server, id: []const u8, params: std.json.Value) ![]u8 {
    const document = documentForParams(server, params) orelse return response(server.allocator, id, "null");
    const cursor = position(params) orelse return response(server.allocator, id, "null");
    const prefix = identifierPrefix(document.text, positionToOffset(document.text, cursor));
    var out = std.ArrayList(u8).empty;
    defer out.deinit(server.allocator);
    try out.appendSlice(server.allocator, "{\"isIncomplete\":false,\"items\":[");
    var first = true;
    const keywords = [_][]const u8{ "const", "var", "fn", "pub", "extern", "comptime", "if", "else", "while", "for", "in", "return", "break", "continue", "defer", "errdefer", "unsafe", "struct", "enum", "union", "error", "true", "false", "null", "undefined" };
    for (keywords) |item| if (matchesPrefix(item, prefix)) try appendCompletion(&out, server.allocator, item, 14, &first);
    const builtins = [_][]const u8{ "@import", "@compileError", "@move", "@nocopy", "@sizeOf", "@alignOf", "@typeInfo" };
    for (builtins) |item| if (matchesPrefix(item, prefix)) try appendCompletion(&out, server.allocator, item, 3, &first);
    if (document.ast) |ast| try appendDeclarations(&out, server.allocator, ast, document.text, prefix, &first);
    try out.appendSlice(server.allocator, "]}");
    return response(server.allocator, id, out.items);
}

fn appendCompletion(out: *std.ArrayList(u8), allocator: std.mem.Allocator, label: []const u8, kind: u8, first: *bool) !void {
    if (!first.*) try out.append(allocator, ',');
    first.* = false;
    try out.appendSlice(allocator, "{\"label\":");
    try appendJsonString(out, allocator, label);
    try appendFormat(out, allocator, ",\"kind\":{d}}}", .{kind});
}

fn appendDeclarations(out: *std.ArrayList(u8), allocator: std.mem.Allocator, ast: Ast, text: []const u8, prefix: []const u8, first: *bool) !void {
    for (ast.nodes.items(.tag), ast.nodes.items(.data), 0..) |tag, data, index| {
        const name_token = declarationNameToken(ast, @intCast(index), tag, data) orelse continue;
        const name = tokenText(ast, text, name_token);
        if (matchesPrefix(name, prefix)) try appendCompletion(out, allocator, name, if (tag == .fn_decl) 3 else 6, first);
    }
}

fn documentSymbols(server: *Server, id: []const u8, params: std.json.Value) ![]u8 {
    const document = documentForParams(server, params) orelse return response(server.allocator, id, "[]");
    const ast = document.ast orelse return response(server.allocator, id, "[]");
    var out = std.ArrayList(u8).empty;
    defer out.deinit(server.allocator);
    try out.append(server.allocator, '[');
    var first = true;
    for (ast.nodes.items(.tag), ast.nodes.items(.data), 0..) |tag, data, index| {
        const name_token = declarationNameToken(ast, @intCast(index), tag, data) orelse continue;
        const token = ast.tokens[name_token];
        if (!first) try out.append(server.allocator, ',');
        first = false;
        const start = offsetToPosition(document.text, token.start);
        const end = offsetToPosition(document.text, token.end);
        try out.appendSlice(server.allocator, "{\"name\":");
        try appendJsonString(&out, server.allocator, tokenText(ast, document.text, name_token));
        try appendFormat(&out, server.allocator, ",\"kind\":{d},\"range\":{{\"start\":{{\"line\":{d},\"character\":{d}}},\"end\":{{\"line\":{d},\"character\":{d}}}}},\"selectionRange\":{{\"start\":{{\"line\":{d},\"character\":{d}}},\"end\":{{\"line\":{d},\"character\":{d}}}}}}}", .{ if (tag == .fn_decl) @as(u8, 12) else if (tag == .const_decl) @as(u8, 14) else @as(u8, 13), start.line, start.character, end.line, end.character, start.line, start.character, end.line, end.character });
    }
    try out.append(server.allocator, ']');
    return response(server.allocator, id, out.items);
}

fn declarationNameToken(ast: Ast, node_index: Node.Index, tag: Node.Tag, data: Node.Data) ?u32 {
    _ = node_index;
    return switch (tag) {
        .const_decl, .var_decl => data.lhs,
        .fn_decl => ast.nodes.items(.main_token)[data.lhs],
        else => null,
    };
}

fn definition(server: *Server, id: []const u8, params: std.json.Value) ![]u8 {
    const document = documentForParams(server, params) orelse return response(server.allocator, id, "null");
    const ast = document.ast orelse return response(server.allocator, id, "null");
    const sema = document.sema orelse return response(server.allocator, id, "null");
    const cursor = position(params) orelse return response(server.allocator, id, "null");
    const node = findNodeByOffset(ast, positionToOffset(document.text, cursor)) orelse return response(server.allocator, id, "null");
    const declaration = sema.resolved_decls.get(node) orelse return response(server.allocator, id, "null");
    const tag = ast.nodes.items(.tag)[declaration];
    const data = ast.nodes.items(.data)[declaration];
    const name_token = declarationNameToken(ast, declaration, tag, data) orelse ast.nodes.items(.main_token)[declaration];
    const token = ast.tokens[name_token];
    return location(server.allocator, id, document.uri, document.text, token.start, token.end);
}

fn hover(server: *Server, id: []const u8, params: std.json.Value) ![]u8 {
    const document = documentForParams(server, params) orelse return response(server.allocator, id, "null");
    const ast = document.ast orelse return response(server.allocator, id, "null");
    const sema = document.sema orelse return response(server.allocator, id, "null");
    const cursor = position(params) orelse return response(server.allocator, id, "null");
    const node = findNodeByOffset(ast, positionToOffset(document.text, cursor)) orelse return response(server.allocator, id, "null");
    const type_id = sema.node_types.get(node) orelse return response(server.allocator, id, "null");
    var type_buffer: [1024]u8 = undefined;
    const type_name = document.types.typeName(type_id, &type_buffer) catch "<unknown>";
    const markdown = try std.fmt.allocPrint(server.allocator, "~~~zin\n{s}\n~~~", .{type_name});
    defer server.allocator.free(markdown);
    const content = try jsonString(server.allocator, markdown);
    defer server.allocator.free(content);
    return std.fmt.allocPrint(server.allocator, "{{\"jsonrpc\":\"2.0\",\"id\":{s},\"result\":{{\"contents\":{{\"kind\":\"markdown\",\"value\":{s}}}}}}}", .{ id, content });
}

fn location(allocator: std.mem.Allocator, id: []const u8, uri: []const u8, text: []const u8, start_offset: u32, end_offset: u32) ![]u8 {
    const uri_json = try jsonString(allocator, uri);
    defer allocator.free(uri_json);
    const start = offsetToPosition(text, start_offset);
    const end = offsetToPosition(text, end_offset);
    return std.fmt.allocPrint(allocator, "{{\"jsonrpc\":\"2.0\",\"id\":{s},\"result\":{{\"uri\":{s},\"range\":{{\"start\":{{\"line\":{d},\"character\":{d}}},\"end\":{{\"line\":{d},\"character\":{d}}}}}}}}}", .{ id, uri_json, start.line, start.character, end.line, end.character });
}

fn findNodeByOffset(ast: Ast, offset: usize) ?Node.Index {
    var result: ?Node.Index = null;
    for (ast.nodes.items(.main_token), 0..) |token_index, node_index| {
        if (token_index >= ast.tokens.len) continue;
        const token = ast.tokens[token_index];
        if (token.start <= offset and offset <= token.end) result = @intCast(node_index);
    }
    return result;
}

fn tokenText(ast: Ast, text: []const u8, token_index: u32) []const u8 {
    const token = ast.tokens[token_index];
    return text[token.start..token.end];
}

fn identifierPrefix(text: []const u8, offset: usize) []const u8 {
    var start = @min(offset, text.len);
    while (start > 0 and isIdentifierChar(text[start - 1])) start -= 1;
    return text[start..@min(offset, text.len)];
}

fn isIdentifierChar(byte: u8) bool {
    return std.ascii.isAlphanumeric(byte) or byte == '_' or byte == '@';
}

fn matchesPrefix(value: []const u8, prefix: []const u8) bool {
    return prefix.len == 0 or std.mem.startsWith(u8, value, prefix);
}

fn positionToOffset(text: []const u8, target: Position) usize {
    var line: usize = 0;
    var offset: usize = 0;
    while (offset < text.len and line < target.line) : (offset += 1) {
        if (text[offset] == '\n') line += 1;
    }
    var character: usize = 0;
    while (offset < text.len and text[offset] != '\n' and character < target.character) {
        const len = std.unicode.utf8ByteSequenceLength(text[offset]) catch 1;
        const codepoint = std.unicode.utf8Decode(text[offset..@min(text.len, offset + len)]) catch text[offset];
        character += if (codepoint > 0xffff) 2 else 1;
        offset += len;
    }
    return offset;
}

fn offsetToPosition(text: []const u8, target: u32) Position {
    const limit = @min(text.len, @as(usize, target));
    var line: usize = 0;
    var character: usize = 0;
    var offset: usize = 0;
    while (offset < limit) {
        if (text[offset] == '\n') {
            line += 1;
            character = 0;
            offset += 1;
            continue;
        }
        const len = std.unicode.utf8ByteSequenceLength(text[offset]) catch 1;
        const codepoint = std.unicode.utf8Decode(text[offset..@min(limit, offset + len)]) catch text[offset];
        character += if (codepoint > 0xffff) 2 else 1;
        offset += len;
    }
    return .{ .line = line, .character = character };
}

fn jsonString(allocator: std.mem.Allocator, value: []const u8) ![]u8 {
    var out = std.ArrayList(u8).empty;
    defer out.deinit(allocator);
    try appendJsonString(&out, allocator, value);
    return out.toOwnedSlice(allocator);
}

fn appendJsonString(out: *std.ArrayList(u8), allocator: std.mem.Allocator, value: []const u8) !void {
    try out.append(allocator, '"');
    for (value) |byte| switch (byte) {
        '"' => try out.appendSlice(allocator, "\\\""),
        '\\' => try out.appendSlice(allocator, "\\\\"),
        '\n' => try out.appendSlice(allocator, "\\n"),
        '\r' => try out.appendSlice(allocator, "\\r"),
        '\t' => try out.appendSlice(allocator, "\\t"),
        0...7, 11...12, 14...0x1f => try appendFormat(out, allocator, "\\u{X:0>4}", .{byte}),
        else => try out.append(allocator, byte),
    };
    try out.append(allocator, '"');
}

fn appendFormat(out: *std.ArrayList(u8), allocator: std.mem.Allocator, comptime format: []const u8, args: anytype) !void {
    var buffer: [1024]u8 = undefined;
    const rendered = try std.fmt.bufPrint(&buffer, format, args);
    try out.appendSlice(allocator, rendered);
}

fn send(writer: *std.Io.Writer, body: []const u8) !void {
    if (body.len == 0) return;
    var header: [64]u8 = undefined;
    const encoded = try std.fmt.bufPrint(&header, "Content-Length: {d}\r\n\r\n", .{body.len});
    try writer.writeAll(encoded);
    try writer.writeAll(body);
    try writer.flush();
}

test "LSP request JSON preserves decoded document text" {
    var server = Server.init(std.testing.allocator);
    defer server.deinit();
    const request =
        \\{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///test.zin","text":"const value: i32 = 1\n"}}}
    ;

    const result = try handle(&server, request);
    defer std.testing.allocator.free(result);

    try std.testing.expect(std.mem.indexOf(u8, result, "publishDiagnostics") != null);
    const document = server.documents.get("file:///test.zin").?;
    try std.testing.expectEqualStrings("const value: i32 = 1\n", document.text);
}

test "LSP positions use UTF-16 code units" {
    const text = "const emoji = \"😀\"\n";
    const emoji_offset = std.mem.indexOf(u8, text, "😀").?;
    const pos = offsetToPosition(text, @intCast(emoji_offset));
    try std.testing.expectEqual(@as(usize, 0), pos.line);
    try std.testing.expectEqual(@as(usize, 15), pos.character);
    try std.testing.expectEqual(emoji_offset, positionToOffset(text, pos));
}
