// Language-server entry point. Kept beside the bootstrap lexer until the
// canonical Zin compiler owns the tooling implementation.
const std = @import("std");
const Lexer = @import("syntax/lexer.zig").Lexer;

pub fn main(init: std.process.Init) !u8 {
    const a = init.gpa;
    var stdout_buffer: [1024]u8 = undefined;
    var stdout = std.Io.File.Writer.init(.stdout(), init.io, &stdout_buffer);
    var input = std.ArrayList(u8).empty;
    defer input.deinit(a);
    var chunk: [8192]u8 = undefined;
    while (true) {
        const n = std.posix.read(0, &chunk) catch return 1;
        if (n == 0) break;
        try input.appendSlice(a, chunk[0..n]);
        while (nextMessage(input.items)) |message| {
            const response = try handle(a, message);
            defer a.free(response);
            try send(&stdout.interface, response);
            const sep = std.mem.indexOf(u8, input.items, "\r\n\r\n").?;
            const total = sep + 4 + message.len;
            std.mem.copyForwards(u8, input.items[0 .. input.items.len - total], input.items[total..]);
            input.items.len -= total;
        }
    }
    return 0;
}

fn nextMessage(buf: []const u8) ?[]const u8 {
    const sep = std.mem.indexOf(u8, buf, "\r\n\r\n") orelse return null;
    const p = std.mem.indexOf(u8, buf[0..sep], "Content-Length:") orelse return null;
    const start = p + "Content-Length:".len;
    const len = std.fmt.parseInt(usize, std.mem.trim(u8, buf[start..sep], " \t"), 10) catch return null;
    if (buf.len < sep + 4 + len) return null;
    return buf[sep + 4 .. sep + 4 + len];
}

fn handle(a: std.mem.Allocator, msg: []const u8) ![]u8 {
    const method = stringField(msg, "method") orelse "";
    if (std.mem.eql(u8, method, "initialize")) return a.dupe(u8, "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"capabilities\":{\"textDocumentSync\":1}}}");
    if (std.mem.eql(u8, method, "shutdown")) return a.dupe(u8, "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":null}");
    if (std.mem.eql(u8, method, "exit")) return a.dupe(u8, "");
    if (std.mem.eql(u8, method, "textDocument/didOpen") or std.mem.eql(u8, method, "textDocument/didChange")) {
        return diagnostics(a, stringField(msg, "uri") orelse "file:///unknown.zin", stringField(msg, "text") orelse "");
    }
    return a.dupe(u8, "");
}

fn stringField(msg: []const u8, field: []const u8) ?[]const u8 {
    var key: [128]u8 = undefined;
    const needle = std.fmt.bufPrint(&key, "\"{s}\"", .{field}) catch return null;
    const p = std.mem.indexOf(u8, msg, needle) orelse return null;
    var start = p + needle.len;
    while (start < msg.len and (msg[start] == ' ' or msg[start] == '\t' or msg[start] == '\n' or msg[start] == '\r' or msg[start] == ':')) start += 1;
    if (start >= msg.len or msg[start] != '"') return null;
    start += 1;
    const end = std.mem.indexOfScalarPos(u8, msg, start, '"') orelse return null;
    return msg[start..end];
}

fn diagnostics(a: std.mem.Allocator, uri: []const u8, text: []const u8) ![]u8 {
    var source = try a.alloc(u8, text.len + 1);
    defer a.free(source);
    @memcpy(source[0..text.len], text); source[text.len] = 0;
    var lex = Lexer.init(source[0..text.len :0]);
    var invalid = false;
    while (true) { const t = lex.next(); invalid = invalid or t.tag == .invalid; if (t.tag == .eof) break; }
    var out = std.ArrayList(u8).empty; defer out.deinit(a);
    try out.appendSlice(a, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":\"");
    try out.appendSlice(a, uri); try out.appendSlice(a, "\",\"diagnostics\":[");
    if (invalid) try out.appendSlice(a, "{\"severity\":1,\"message\":\"invalid token\",\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":1}}}");
    try out.appendSlice(a, "]}}"); return out.toOwnedSlice(a);
}

fn send(out: *std.Io.Writer, body: []const u8) !void {
    if (body.len == 0) return;
    var header: [64]u8 = undefined;
    const h = try std.fmt.bufPrint(&header, "Content-Length: {d}\r\n\r\n", .{body.len});
    try out.writeAll(h); try out.writeAll(body); try out.flush();
}
