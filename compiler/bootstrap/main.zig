const std = @import("std");
const sm = @import("source_manager.zig");
const diag = @import("diagnostics.zig");

pub fn main(init: std.process.Init) !void {
    const allocator = init.gpa;
    
    // Zig 0.16.0 Io init
    const io = init.io;
    var stdout_buffer: [1024]u8 = undefined;
    var stdout_file_writer: std.Io.File.Writer = .init(.stdout(), io, &stdout_buffer);
    const out = &stdout_file_writer.interface;
    
    std.debug.print("zin0 bootstrap scaffold\nImplement according to AGENT_IMPLEMENTATION.md and spec/.\n", .{});

    const args = try init.minimal.args.toSlice(allocator);
    defer allocator.free(args);

    if (args.len < 2) {
        std.debug.print("Usage: zin0 <source_file.zin>\n", .{});
        return;
    }
    const path = args[1];

    var source_manager = sm.SourceManager.init(allocator);
    defer source_manager.deinit();

    var engine = diag.DiagnosticEngine.init(allocator, &source_manager);
    defer engine.deinit();

    // Read the file entirely into memory
    const max_size = 10 * 1024 * 1024; // 10MB limit
    const content_unterm = std.Io.Dir.readFileAlloc(.cwd(), io, path, allocator, @enumFromInt(max_size)) catch |err| {
        std.debug.print("Failed to read file '{s}': {}\n", .{ path, err });
        return;
    };
    defer allocator.free(content_unterm);

    var content = try allocator.alloc(u8, content_unterm.len + 1);
    defer allocator.free(content);
    @memcpy(content[0..content_unterm.len], content_unterm);
    content[content_unterm.len] = 0;
    
    // Stage 1: Add to SourceManager
    const file_id = try source_manager.addFile(path, content_unterm);
    
    // Stage 2: Lex
    const lexer = @import("lexer.zig");
    const Token = @import("token.zig").Token;
    var lex = lexer.Lexer.init(content[0..content_unterm.len :0]);
    var tokens = std.ArrayList(Token).empty;
    defer tokens.deinit(allocator);
    
    while (true) {
        const t = lex.next();
        try tokens.append(allocator, t);
        if (t.tag == .eof) break;
    }
    
    // Stage 3: Parse
    const parser = @import("parser.zig");
    var p = parser.Parser.init(allocator, tokens.items, &engine, file_id);
    var ast = p.parse() catch |err| {
        std.debug.print("Fatal parse error: {}\n", .{err});
        try engine.render(out);
        try stdout_file_writer.flush();
        return;
    };
    defer ast.deinit(allocator);
    
    try engine.render(out);
    try stdout_file_writer.flush();
    
    if (engine.error_count == 0) {
        // Stage 5: Type Representation & Stage 6/7: Sema
        const type_mod = @import("type.zig");
        const TypePool = type_mod.TypePool;
        var type_pool = TypePool.init(allocator);
        defer type_pool.deinit();

        const scope_mod = @import("scope.zig");
        var root_scope = scope_mod.Scope.init(allocator, null);
        defer root_scope.deinit();

        const sema_mod = @import("sema.zig");
        var sema = sema_mod.Sema.init(allocator, ast, &engine, &type_pool, &root_scope);
        defer sema.deinit();

        sema.analyze() catch |err| {
            std.debug.print("Semantic analysis failed: {}\n", .{err});
        };

        try engine.render(out);
        try stdout_file_writer.flush();

        // Stage 9: LIR
        const lir_gen_mod = @import("lir_gen.zig");
        var lir_builder = lir_gen_mod.LirBuilder.init(allocator, &sema);
        defer lir_builder.deinit();

        lir_builder.generate() catch |err| {
            std.debug.print("LIR generation failed: {}\n", .{err});
        };
        lir_builder.printLir();

        std.debug.print("AST has {d} nodes\n", .{ast.nodes.len});

        var x86_gen = @import("x86_64_gen.zig").X86Gen.init(allocator, &lir_builder.lir, ast, source_manager.getFile(0).?.content);
        defer x86_gen.deinit();
        try x86_gen.generate(out);
        
        try stdout_file_writer.flush();

        std.debug.print("Done.\n", .{});
    } else {
        std.debug.print("Parsing failed with {d} error(s).\n", .{ engine.error_count });
    }
}

test {
    _ = @import("source_manager.zig");
    _ = @import("diagnostics.zig");
    _ = @import("lexer.zig");
    _ = @import("ast.zig");
    _ = @import("parser.zig");
    _ = @import("type.zig");
    _ = @import("scope.zig");
    _ = @import("sema.zig");
    _ = @import("lir.zig");
    _ = @import("lir_gen.zig");
    _ = @import("abi.zig");
}
