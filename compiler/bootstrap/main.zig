const std = @import("std");
const sm = @import("source_manager.zig");
const diag = @import("diagnostics.zig");

pub fn main(init: std.process.Init) !u8 {
    const allocator = init.gpa;
    const io = init.io;
    var stdout_buffer: [1024]u8 = undefined;
    var stdout_file_writer: std.Io.File.Writer = .init(.stdout(), io, &stdout_buffer);
    const out = &stdout_file_writer.interface;

    std.debug.print("zin0 bootstrap compiler\n", .{});

    const args = try init.minimal.args.toSlice(allocator);
    defer allocator.free(args);

    if (args.len < 2) {
        std.debug.print("Usage: zin0 <source.zin> [--emit=asm]\n", .{});
        return 1;
    }
    const path = args[1];

    // Parse flags
    var emit_asm = false;
    var out_path: []const u8 = "out";
    for (args[2..]) |arg| {
        if (std.mem.eql(u8, arg, "--emit=asm")) emit_asm = true;
        if (std.mem.startsWith(u8, arg, "-o")) out_path = arg[2..];
    }

    var source_manager = sm.SourceManager.init(allocator);
    defer source_manager.deinit();

    var engine = diag.DiagnosticEngine.init(allocator, &source_manager);
    defer engine.deinit();

    const max_size = 10 * 1024 * 1024;
    const content_unterm = std.Io.Dir.readFileAlloc(.cwd(), io, path, allocator, @enumFromInt(max_size)) catch |err| {
        std.debug.print("Failed to read file '{s}': {}\n", .{ path, err });
        return 1;
    };
    defer allocator.free(content_unterm);

    var content = try allocator.alloc(u8, content_unterm.len + 1);
    defer allocator.free(content);
    @memcpy(content[0..content_unterm.len], content_unterm);
    content[content_unterm.len] = 0;

    // Stage 1: SourceManager
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
        return 1;
    };
    defer ast.deinit(allocator);

    try engine.render(out);
    try stdout_file_writer.flush();

    if (engine.error_count > 0) {
        std.debug.print("Compilation failed with {d} error(s).\n", .{engine.error_count});
        return 1;
    }

    // Stage 5/6/7: Type + Sema
    const type_mod = @import("type.zig");
    var type_pool = type_mod.TypePool.init(allocator);
    defer type_pool.deinit();

    const scope_mod = @import("scope.zig");
    var root_scope = scope_mod.Scope.init(allocator, null);
    defer root_scope.deinit();

    const sema_mod = @import("sema.zig");
    var sema = sema_mod.Sema.init(allocator, ast, &engine, &type_pool, &root_scope);
    defer sema.deinit();
    sema.analyze() catch |err| {
        std.debug.print("Sema failed: {}\n", .{err});
        return 1;
    };

    try engine.render(out);
    try stdout_file_writer.flush();

    if (engine.error_count > 0) {
        std.debug.print("Compilation failed with {d} error(s).\n", .{engine.error_count});
        return 1;
    }

    // Stage 9: LIR
    const lir_gen_mod = @import("lir_gen.zig");
    var lir_builder = lir_gen_mod.LirBuilder.init(allocator, &sema);
    defer lir_builder.deinit();
    lir_builder.generate() catch |err| {
        std.debug.print("LIR gen failed: {}\n", .{err});
        return 1;
    };
    lir_builder.printLir();

    std.debug.print("AST has {d} nodes\n", .{ast.nodes.len});

    var x86_gen = @import("x86_64_gen.zig").X86Gen.init(
        allocator,
        &lir_builder.lir,
        &type_pool,
        ast,
        source_manager.getFile(0).?.content,
    );
    defer x86_gen.deinit();

    if (emit_asm) {
        // ── Stage 10: NASM text output (legacy / debug) ──────────────────────
        std.debug.print("[zin0] --emit=asm: writing NASM text\n", .{});
        try x86_gen.generate(out);
        try stdout_file_writer.flush();
    } else {
        // ── Stage 12: Binary ELF64 output ────────────────────────────────────
        std.debug.print("[zin0] emitting ELF64 binary → '{s}'\n", .{out_path});
        var enc = @import("x86_64_encoder.zig").Encoder.init(allocator);
        defer enc.deinit();

        // Phase 1: generate binary to discover code size and collect rodata strings.
        // rodata_vaddr is 0 here — string addresses will be wrong, but we need code size.
        x86_gen.generateBinary(&enc) catch |err| {
            std.debug.print("Binary code generation failed: {}\n", .{err});
            return 1;
        };

        // Compute rodata_vaddr from ELF layout:
        //   TEXT_FILE_OFFSET = 0x1000, TEXT_VADDR = 0x401000
        //   rodata starts at page-aligned offset after .text
        const elf64_mod = @import("elf64.zig");
        const text_size: u64 = @as(u64, @intCast(enc.buf.items.len));
        const rodata_file_off: u64 = elf64_mod.alignUp(0x1000 + text_size, 0x1000);
        const rodata_vaddr: u64 = 0x401000 + (rodata_file_off - 0x1000);
        std.debug.print("[zin0] rodata_vaddr = 0x{x} (text_size={d})\n", .{ rodata_vaddr, text_size });

        if (x86_gen.rodata.items.len > 0) {
            // Phase 2: set rodata_vaddr and regenerate with correct string addresses.
            x86_gen.rodata_vaddr = rodata_vaddr;
            // Reset encoder and virtual register allocator for clean re-generation.
            enc.buf.clearRetainingCapacity();
            enc.fixups.clearRetainingCapacity();
            enc.symbols.clearRetainingCapacity();
            x86_gen.vreg_to_op.clearRetainingCapacity();
            x86_gen.addr_to_slot.clearRetainingCapacity();
            x86_gen.error_tag_slots.clearRetainingCapacity();
            x86_gen.error_payload_extra_slots.clearRetainingCapacity();
            x86_gen.next_gp_reg = 0;
            x86_gen.next_stack_slot = 8;
            x86_gen.current_function_return_type = null;
            x86_gen.current_hidden_payload_slot = null;
            // Clear rodata so strings don't accumulate across two generateBinary calls
            x86_gen.rodata.clearRetainingCapacity();
            x86_gen.string_offsets.clearRetainingCapacity();

            x86_gen.generateBinary(&enc) catch |err| {
                std.debug.print("Binary code generation (phase 2) failed: {}\n", .{err});
                return 1;
            };
        }

        // writeExecutable with rodata slice
        elf64_mod.writeExecutable(allocator, io, &enc, "_start", out_path, x86_gen.rodata.items) catch |err| {
            std.debug.print("ELF64 write failed: {}\n", .{err});
            return 1;
        };
        std.debug.print("[zin0] wrote '{s}' — done.\n", .{out_path});
    }

    try stdout_file_writer.flush();
    std.debug.print("Done.\n", .{});
    return 0;
}

test {
    _ = @import("source_manager.zig");
    _ = @import("diagnostics.zig");
    _ = @import("lexer.zig");
    _ = @import("ast.zig");
    _ = @import("builtin.zig");
    _ = @import("parser.zig");
    _ = @import("type.zig");
    _ = @import("scope.zig");
    _ = @import("sema.zig");
    _ = @import("lir.zig");
    _ = @import("lir_gen.zig");
    _ = @import("abi.zig");
    _ = @import("x86_64_encoder.zig");
    _ = @import("x86_64_gen.zig");
    _ = @import("elf64.zig");
    _ = @import("os_linux.zig");
    _ = @import("posix.zig");
}
