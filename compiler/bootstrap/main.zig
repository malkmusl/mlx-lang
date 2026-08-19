const std = @import("std");
const sm = @import("source_manager.zig");
const diag = @import("diagnostics.zig");

pub fn main(init: std.process.Init) !void {
    const allocator = std.heap.page_allocator;
    const io = init.io;

    var stdout_buffer: [1024]u8 = undefined;
    var stdout_file_writer: std.Io.File.Writer = .init(.stdout(), io, &stdout_buffer);
    const out = &stdout_file_writer.interface;
    
    try out.writeAll(
        "zin0 bootstrap scaffold\n" ++
        "Implement according to AGENT_IMPLEMENTATION.md and spec/.\n",
    );

    var source_manager = sm.SourceManager.init(allocator);
    defer source_manager.deinit();

    var engine = diag.DiagnosticEngine.init(allocator, &source_manager);
    defer engine.deinit();

    // Example usage for testing
    const file_id = try source_manager.addFile("dummy.zin", "const a = 1;\nconst b = 2;\n");
    
    try engine.report(diag.Diagnostic{
        .code = 1001,
        .phase = .lexer,
        .severity = .@"error",
        .primary_span = .{ .file_id = file_id, .start_byte = 6, .end_byte = 7 },
        .message = "Invalid character in variable name",
    });

    try engine.render(out);
    try out.flush();
}

test {
    _ = @import("source_manager.zig");
    _ = @import("diagnostics.zig");
    _ = @import("lexer.zig");
}
